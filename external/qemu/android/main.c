/* Copyright (C) 2006-2008 The Android Open Source Project
**
** This software is licensed under the terms of the GNU General Public
** License version 2, as published by the Free Software Foundation, and
** may be copied, distributed, and modified under those terms.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
*/

#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>
#ifdef _WIN32
#include <process.h>
#endif
#include "libslirp.h"
#include "sockets.h"

#include "android/android.h"
#include "qemu-common.h"
#include "sysemu.h"
#include "console.h"
#include "user-events.h"

#include <SDL.h>
#include <SDL_syswm.h>

#include "math.h"

#include "android/charmap.h"
#include "modem_driver.h"
#include "proxy_http.h"

#include "android/utils/debug.h"
#include "android/resource.h"
#include "android/config.h"
#include "android/config/config.h"

#include "android/skin/image.h"
#include "android/skin/trackball.h"
#include "android/skin/keyboard.h"
#include "android/skin/file.h"
#include "android/skin/window.h"
#include "android/skin/keyset.h"

#include "android/gps.h"
#include "android/hw-qemud.h"
#include "android/hw-kmsg.h"
#include "android/hw-lcd.h"
#include "android/hw-sensors.h"
#include "android/boot-properties.h"
#include "android/user-config.h"
#include "android/utils/bufprint.h"
#include "android/utils/dirscanner.h"
#include "android/utils/path.h"
#include "android/utils/timezone.h"

#include "android/cmdline-option.h"
#include "android/help.h"
#include "hw/goldfish_nand.h"
#ifdef CONFIG_MEMCHECK
#include "memcheck/memcheck.h"
#endif  // CONFIG_MEMCHECK

#include "android/globals.h"
#include "tcpdump.h"

#include "android/qemulator.h"

/* in vl.c */
extern void  qemu_help(int  code);

#include "framebuffer.h"
AndroidRotation  android_framebuffer_rotation;

#define  STRINGIFY(x)   _STRINGIFY(x)
#define  _STRINGIFY(x)  #x

#ifdef ANDROID_SDK_TOOLS_REVISION
#  define  VERSION_STRING  STRINGIFY(ANDROID_SDK_TOOLS_REVISION)".0"
#else
#  define  VERSION_STRING  "standalone"
#endif

#define  KEYSET_FILE    "default.keyset"
SkinKeyset*      android_keyset;

#define  D(...)  do {  if (VERBOSE_CHECK(init)) dprint(__VA_ARGS__); } while (0)

extern int  control_console_start( int  port );  /* in control.c */

extern int qemu_milli_needed;

/* the default device DPI if none is specified by the skin
 */
#define  DEFAULT_DEVICE_DPI  165

#if 0
static int  opts->flashkeys;      /* forward */
#endif

static void  handle_key_command( void*  opaque, SkinKeyCommand  command, int  param );

#ifdef CONFIG_TRACE
extern void  start_tracing(void);
extern void  stop_tracing(void);
#endif

unsigned long   android_verbose;

int   qemu_cpu_delay = 0;
int   qemu_cpu_delay_count;

/***********************************************************************/
/***********************************************************************/
/*****                                                             *****/
/*****            U T I L I T Y   R O U T I N E S                  *****/
/*****                                                             *****/
/***********************************************************************/
/***********************************************************************/

/***  CONFIGURATION
 ***/

static AUserConfig*  userConfig;

void
emulator_config_init( void )
{
    userConfig = auserConfig_new( android_avdInfo );
}

/* only call this function on normal exits, so that ^C doesn't save the configuration */
void
emulator_config_done( void )
{
    int  win_x, win_y;

    if (!userConfig) {
        D("no user configuration?");
        return;
    }

    SDL_WM_GetPos( &win_x, &win_y );
    auserConfig_setWindowPos(userConfig, win_x, win_y);
    auserConfig_save(userConfig);
}

void *loadpng(const char *fn, unsigned *_width, unsigned *_height);
void *readpng(const unsigned char*  base, size_t  size, unsigned *_width, unsigned *_height);

#ifdef CONFIG_DARWIN
#  define  ANDROID_ICON_PNG  "android_icon_256.png"
#else
#  define  ANDROID_ICON_PNG  "android_icon_16.png"
#endif

static void
sdl_set_window_icon( void )
{
    static int  window_icon_set;

    if (!window_icon_set)
    {
#ifdef _WIN32
        HANDLE         handle = GetModuleHandle( NULL );
        HICON          icon   = LoadIcon( handle, MAKEINTRESOURCE(1) );
        SDL_SysWMinfo  wminfo;

        SDL_GetWMInfo(&wminfo);

        SetClassLong( wminfo.window, GCL_HICON, (LONG)icon );
#else  /* !_WIN32 */
        unsigned              icon_w, icon_h;
        size_t                icon_bytes;
        const unsigned char*  icon_data;
        void*                 icon_pixels;

        window_icon_set = 1;

        icon_data = android_icon_find( ANDROID_ICON_PNG, &icon_bytes );
        if ( !icon_data )
            return;

        icon_pixels = readpng( icon_data, icon_bytes, &icon_w, &icon_h );
        if ( !icon_pixels )
            return;

       /* the data is loaded into memory as RGBA bytes by libpng. we want to manage
        * the values as 32-bit ARGB pixels, so swap the bytes accordingly depending
        * on our CPU endianess
        */
        {
            unsigned*  d     = icon_pixels;
            unsigned*  d_end = d + icon_w*icon_h;

            for ( ; d < d_end; d++ ) {
                unsigned  pix = d[0];
#if HOST_WORDS_BIGENDIAN
                /* R,G,B,A read as RGBA => ARGB */
                pix = ((pix >> 8) & 0xffffff) | (pix << 24);
#else
                /* R,G,B,A read as ABGR => ARGB */
                pix = (pix & 0xff00ff00) | ((pix >> 16) & 0xff) | ((pix & 0xff) << 16);
#endif
                d[0] = pix;
            }
        }

        SDL_Surface* icon = sdl_surface_from_argb32( icon_pixels, icon_w, icon_h );
        if (icon != NULL) {
            SDL_WM_SetIcon(icon, NULL);
            SDL_FreeSurface(icon);
            free( icon_pixels );
        }
#endif	/* !_WIN32 */
    }
}

#define  ONE_MB  (1024*1024)

unsigned convertBytesToMB( uint64_t  size )
{
    if (size == 0)
        return 0;

    size = (size + ONE_MB-1) >> 20;
    if (size > UINT_MAX)
        size = UINT_MAX;

    return (unsigned) size;
}

uint64_t  convertMBToBytes( unsigned  megaBytes )
{
    return ((uint64_t)megaBytes << 20);
}

/***********************************************************************/
/***********************************************************************/
/*****                                                             *****/
/*****            S K I N   I M A G E S                            *****/
/*****                                                             *****/
/***********************************************************************/
/***********************************************************************/

/* called by the emulated framebuffer device each time the content of the
 * framebuffer has changed. the rectangle is the bounding box of all changes
 */
static void
sdl_update(DisplayState *ds, int x, int y, int w, int h)
{
    /* this function is being called from the console code that is currently inactive
    ** simple totally ignore it...
    */
    (void)ds;
    (void)x;
    (void)y;
    (void)w;
    (void)h;
}




/* called by the emulated framebuffer device each time the framebuffer
 * is resized or rotated */
static void
sdl_resize(DisplayState *ds)
{
    //fprintf(stderr, "weird, sdl_resize being called with framebuffer interface\n");
    //exit(1);
}


/* called periodically to poll for user input events */
static void sdl_refresh(DisplayState *ds)
{
    QEmulator*     emulator = ds->opaque;
    SDL_Event      ev;
    SkinWindow*    window   = emulator->window;
    SkinKeyboard*  keyboard = emulator->keyboard;

   /* this will eventually call sdl_update if the content of the VGA framebuffer
    * has changed */
    qframebuffer_check_updates();

    if (window == NULL)
        return;

    while(SDL_PollEvent(&ev)){
        switch(ev.type){
        case SDL_VIDEOEXPOSE:
            skin_window_redraw( window, NULL );
            break;

        case SDL_KEYDOWN:
#ifdef _WIN32
            /* special code to deal with Alt-F4 properly */
            if (ev.key.keysym.sym == SDLK_F4 &&
                ev.key.keysym.mod & KMOD_ALT) {
              goto CleanExit;
            }
#endif
#ifdef __APPLE__
            /* special code to deal with Command-Q properly */
            if (ev.key.keysym.sym == SDLK_q &&
                ev.key.keysym.mod & KMOD_META) {
              goto CleanExit;
            }
#endif
            skin_keyboard_process_event( keyboard, &ev, 1 );
            break;

        case SDL_KEYUP:
            skin_keyboard_process_event( keyboard, &ev, 0 );
            break;

        case SDL_MOUSEMOTION:
            skin_window_process_event( window, &ev );
            break;

        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP:
            {
                int  down = (ev.type == SDL_MOUSEBUTTONDOWN);
                if (ev.button.button == 4)
                {
                    /* scroll-wheel simulates DPad up */
                    AndroidKeyCode  kcode;

                    kcode = // qemulator_rotate_keycode(kKeyCodeDpadUp);
                        android_keycode_rotate(kKeyCodeDpadUp,
                            skin_layout_get_dpad_rotation(qemulator_get_layout(qemulator_get())));
                    user_event_key( kcode, down );
                }
                else if (ev.button.button == 5)
                {
                    /* scroll-wheel simulates DPad down */
                    AndroidKeyCode  kcode;

                    kcode = // qemulator_rotate_keycode(kKeyCodeDpadDown);
                        android_keycode_rotate(kKeyCodeDpadDown,
                            skin_layout_get_dpad_rotation(qemulator_get_layout(qemulator_get())));
                    user_event_key( kcode, down );
                }
                else if (ev.button.button == SDL_BUTTON_LEFT) {
                    skin_window_process_event( window, &ev );
                }
#if 0
                else {
                fprintf(stderr, "... mouse button %s: button=%d state=%04x x=%d y=%d\n",
                                down ? "down" : "up  ",
                                ev.button.button, ev.button.state, ev.button.x, ev.button.y);
                }
#endif
            }
            break;

        case SDL_QUIT:
#if defined _WIN32 || defined __APPLE__
        CleanExit:
#endif
            /* only save emulator config through clean exit */
            qemulator_done(qemulator_get());
            qemu_system_shutdown_request();
            return;
        }
    }

    skin_keyboard_flush( keyboard );
}


/* used to respond to a given keyboard command shortcut
 */
static void
handle_key_command( void*  opaque, SkinKeyCommand  command, int  down )
{
    static const struct { SkinKeyCommand  cmd; AndroidKeyCode  kcode; }  keycodes[] =
    {
        { SKIN_KEY_COMMAND_BUTTON_CALL,   kKeyCodeCall },
        { SKIN_KEY_COMMAND_BUTTON_HOME,   kKeyCodeHome },
        { SKIN_KEY_COMMAND_BUTTON_BACK,   kKeyCodeBack },
        { SKIN_KEY_COMMAND_BUTTON_HANGUP, kKeyCodeEndCall },
        { SKIN_KEY_COMMAND_BUTTON_POWER,  kKeyCodePower },
        { SKIN_KEY_COMMAND_BUTTON_SEARCH,      kKeyCodeSearch },
        { SKIN_KEY_COMMAND_BUTTON_MENU,        kKeyCodeMenu },
        { SKIN_KEY_COMMAND_BUTTON_DPAD_UP,     kKeyCodeDpadUp },
        { SKIN_KEY_COMMAND_BUTTON_DPAD_LEFT,   kKeyCodeDpadLeft },
        { SKIN_KEY_COMMAND_BUTTON_DPAD_RIGHT,  kKeyCodeDpadRight },
        { SKIN_KEY_COMMAND_BUTTON_DPAD_DOWN,   kKeyCodeDpadDown },
        { SKIN_KEY_COMMAND_BUTTON_DPAD_CENTER, kKeyCodeDpadCenter },
        { SKIN_KEY_COMMAND_BUTTON_VOLUME_UP,   kKeyCodeVolumeUp },
        { SKIN_KEY_COMMAND_BUTTON_VOLUME_DOWN, kKeyCodeVolumeDown },
        { SKIN_KEY_COMMAND_BUTTON_CAMERA,      kKeyCodeCamera },
        { SKIN_KEY_COMMAND_NONE, 0 }
    };
    int          nn;
#ifdef CONFIG_TRACE
    static int   tracing = 0;
#endif
    QEmulator*   emulator = opaque;


    for (nn = 0; keycodes[nn].kcode != 0; nn++) {
        if (command == keycodes[nn].cmd) {
            unsigned  code = keycodes[nn].kcode;
            if (down)
                code |= 0x200;
            kbd_put_keycode( code );
            return;
        }
    }

    // for the show-trackball command, handle down events to enable, and
    // up events to disable
    if (command == SKIN_KEY_COMMAND_SHOW_TRACKBALL) {
        emulator->show_trackball = (down != 0);
        skin_window_show_trackball( emulator->window, emulator->show_trackball );
        //qemulator_set_title( emulator );
        return;
    }

    // only handle down events for the rest
    if (down == 0)
        return;

    switch (command)
    {
    case SKIN_KEY_COMMAND_TOGGLE_NETWORK:
        {
            qemu_net_disable = !qemu_net_disable;
            if (android_modem) {
                amodem_set_data_registration(
                        android_modem,
                qemu_net_disable ? A_REGISTRATION_UNREGISTERED
                    : A_REGISTRATION_HOME);
            }
            D( "network is now %s", qemu_net_disable ? "disconnected" : "connected" );
        }
        break;

    case SKIN_KEY_COMMAND_TOGGLE_FULLSCREEN:
        if (emulator->window) {
            skin_window_toggle_fullscreen(emulator->window);
        }
        break;

    case SKIN_KEY_COMMAND_TOGGLE_TRACING:
        {
#ifdef CONFIG_TRACE
            tracing = !tracing;
            if (tracing)
                start_tracing();
            else
                stop_tracing();
#endif
        }
        break;

    case SKIN_KEY_COMMAND_TOGGLE_TRACKBALL:
        emulator->show_trackball = !emulator->show_trackball;
        skin_window_show_trackball( emulator->window, emulator->show_trackball );
        qemulator_set_title(emulator);
        break;

    case SKIN_KEY_COMMAND_ONION_ALPHA_UP:
    case SKIN_KEY_COMMAND_ONION_ALPHA_DOWN:
        if (emulator->onion)
        {
            int  alpha = emulator->onion_alpha;

            if (command == SKIN_KEY_COMMAND_ONION_ALPHA_UP)
                alpha += 16;
            else
                alpha -= 16;

            if (alpha > 256)
                alpha = 256;
            else if (alpha < 0)
                alpha = 0;

            emulator->onion_alpha = alpha;
            skin_window_set_onion( emulator->window, emulator->onion, emulator->onion_rotation, alpha );
            skin_window_redraw( emulator->window, NULL );
            //dprint( "onion alpha set to %d (%.f %%)", alpha, alpha/2.56 );
        }
        break;

    case SKIN_KEY_COMMAND_CHANGE_LAYOUT_PREV:
    case SKIN_KEY_COMMAND_CHANGE_LAYOUT_NEXT:
        {
            SkinLayout*  layout = NULL;

            if (command == SKIN_KEY_COMMAND_CHANGE_LAYOUT_NEXT) {
                layout = emulator->layout->next;
                if (layout == NULL)
                    layout = emulator->layout_file->layouts;
            }
            else if (command == SKIN_KEY_COMMAND_CHANGE_LAYOUT_PREV) {
                layout = emulator->layout_file->layouts;
                while (layout->next && layout->next != emulator->layout)
                    layout = layout->next;
            }
            if (layout != NULL) {
                SkinRotation  rotation;

                emulator->layout = layout;
                skin_window_reset( emulator->window, layout );

                rotation = skin_layout_get_dpad_rotation( layout );

                if (emulator->keyboard)
                    skin_keyboard_set_rotation( emulator->keyboard, rotation );

                if (emulator->trackball) {
                    skin_trackball_set_rotation( emulator->trackball, rotation );
                    skin_window_set_trackball( emulator->window, emulator->trackball );
                    skin_window_show_trackball( emulator->window, emulator->show_trackball );
                }

                skin_window_set_lcd_brightness( emulator->window, emulator->lcd_brightness );

                qframebuffer_invalidate_all();
                qframebuffer_check_updates();
            }
        }
        break;

    default:
        /* XXX: TODO ? */
        ;
    }
}


static void sdl_at_exit(void)
{
    emulator_config_done();
    qemulator_done(qemulator_get());
    SDL_Quit();
}


void sdl_display_init(DisplayState *ds, int full_screen, int  no_frame)
{
    QEmulator*    emulator = qemulator_get();
    SkinDisplay*  disp     = skin_layout_get_display(emulator->layout);
    DisplayChangeListener*  dcl;
    int           width, height;

    if (disp->rotation & 1) {
        width  = disp->rect.size.h;
        height = disp->rect.size.w;
    } else {
        width  = disp->rect.size.w;
        height = disp->rect.size.h;
    }

    /* Register a display state object for the emulated framebuffer */
    ds->allocator = &default_allocator;
    ds->opaque    = emulator;
    ds->surface   = qemu_create_displaysurface(ds, width, height);
    register_displaystate(ds);

    /* Register a change listener for it */
    dcl = (DisplayChangeListener *) qemu_mallocz(sizeof(DisplayChangeListener));
    dcl->dpy_update      = sdl_update;
    dcl->dpy_resize      = sdl_resize;
    dcl->dpy_refresh     = sdl_refresh;
    dcl->dpy_text_cursor = NULL;
    register_displaychangelistener(ds, dcl);

    skin_keyboard_enable( emulator->keyboard, 1 );
    skin_keyboard_on_command( emulator->keyboard, handle_key_command, emulator );
}


static const char*  skin_network_speed = NULL;
static const char*  skin_network_delay = NULL;

/* list of skin aliases */
static const struct {
    const char*  name;
    const char*  alias;
} skin_aliases[] = {
    { "QVGA-L", "320x240" },
    { "QVGA-P", "240x320" },
    { "HVGA-L", "480x320" },
    { "HVGA-P", "320x480" },
    { "QVGA", "320x240" },
    { "HVGA", "320x480" },
    { NULL, NULL }
};

/* this is used by hw/events_device.c to send the charmap name to the system */
const char*    android_skin_keycharmap = NULL;

void init_skinned_ui(const char *path, const char *name, AndroidOptions*  opts)
{
    char      tmp[1024];
    AConfig*  root;
    AConfig*  n;
    int       win_x, win_y, flags;

    signal(SIGINT, SIG_DFL);
#ifndef _WIN32
    signal(SIGQUIT, SIG_DFL);
#endif

    /* we're not a game, so allow the screensaver to run */
    putenv("SDL_VIDEO_ALLOW_SCREENSAVER=1");

    flags = SDL_INIT_NOPARACHUTE;
    if (!opts->no_window)
        flags |= SDL_INIT_VIDEO;

    if(SDL_Init(flags)){
        fprintf(stderr, "SDL init failure, reason is: %s\n", SDL_GetError() );
        exit(1);
    }

    if (!opts->no_window) {
        SDL_EnableUNICODE(!opts->raw_keys);
        SDL_EnableKeyRepeat(0,0);

        sdl_set_window_icon();
    }
    else
    {
#ifndef _WIN32
       /* prevent SIGTTIN and SIGTTOUT from stopping us. this is necessary to be
        * able to run the emulator in the background (e.g. "emulator &").
        * despite the fact that the emulator should not grab input or try to
        * write to the output in normal cases, we're stopped on some systems
        * (e.g. OS X)
        */
        signal(SIGTTIN, SIG_IGN);
        signal(SIGTTOU, SIG_IGN);
#endif
    }
    atexit(sdl_at_exit);

    root = aconfig_node("", "");

    if(name) {
        /* Support skin aliases like QVGA-H QVGA-P, etc...
           But first we check if it's a directory that exist before applying
           the alias */
        int  checkAlias = 1;

        if (path != NULL) {
            bufprint(tmp, tmp+sizeof(tmp), "%s/%s", path, name);
            if (path_exists(tmp)) {
                checkAlias = 0;
            } else {
                D("there is no '%s' skin in '%s'", name, path);
            }
        }

        if (checkAlias) {
            int  nn;

            for (nn = 0; ; nn++ ) {
                const char*  skin_name  = skin_aliases[nn].name;
                const char*  skin_alias = skin_aliases[nn].alias;

                if ( !skin_name )
                    break;

                if ( !strcasecmp( skin_name, name ) ) {
                    D("skin name '%s' aliased to '%s'", name, skin_alias);
                    name = skin_alias;
                    break;
                }
            }
        }

        /* Magically support skins like "320x240" */
        if(isdigit(name[0])) {
            char *x = strchr(name, 'x');
            if(x && isdigit(x[1])) {
                int width = atoi(name);
                int height = atoi(x + 1);
                sprintf(tmp,"display {\n  width %d\n  height %d\n}\n",
                        width, height);
                aconfig_load(root, strdup(tmp));
                path = ":";
                goto found_a_skin;
            }
        }

        if (path == NULL) {
            derror("unknown skin name '%s'", name);
            exit(1);
        }

        sprintf(tmp, "%s/%s/layout", path, name);
        D("trying to load skin file '%s'", tmp);

        if(aconfig_load_file(root, tmp) >= 0) {
            sprintf(tmp, "%s/%s/", path, name);
            path = tmp;
            goto found_a_skin;
        } else {
            dwarning("could not load skin file '%s', using built-in one\n",
                     tmp);
        }
    }

    {
        const unsigned char*  layout_base;
        size_t                layout_size;

        name = "<builtin>";

        layout_base = android_resource_find( "layout", &layout_size );
        if (layout_base != NULL) {
            char*  base = malloc( layout_size+1 );
            memcpy( base, layout_base, layout_size );
            base[layout_size] = 0;

            D("parsing built-in skin layout file (size=%d)", (int)layout_size);
            aconfig_load(root, base);
            path = ":";
        } else {
            fprintf(stderr, "Couldn't load builtin skin\n");
            exit(1);
        }
    }

found_a_skin:
    {
        win_x = 10;
        win_y = 10;

        if (userConfig)
            auserConfig_getWindowPos(userConfig, &win_x, &win_y);
    }

    if ( qemulator_init(qemulator_get(), root, path, win_x, win_y, opts ) < 0 ) {
        fprintf(stderr, "### Error: could not load emulator skin '%s'\n", name);
        exit(1);
    }

    android_skin_keycharmap = skin_keyboard_charmap_name(qemulator_get()->keyboard);

    /* the default network speed and latency can now be specified by the device skin */
    n = aconfig_find(root, "network");
    if (n != NULL) {
        skin_network_speed = aconfig_str(n, "speed", 0);
        skin_network_delay = aconfig_str(n, "delay", 0);
    }

#if 0
    /* create a trackball if needed */
    n = aconfig_find(root, "trackball");
    if (n != NULL) {
        SkinTrackBallParameters  params;

        params.x        = aconfig_unsigned(n, "x", 0);
        params.y        = aconfig_unsigned(n, "y", 0);
        params.diameter = aconfig_unsigned(n, "diameter", 20);
        params.ring     = aconfig_unsigned(n, "ring", 1);

        params.ball_color = aconfig_unsigned(n, "ball-color", 0xffe0e0e0);
        params.dot_color  = aconfig_unsigned(n, "dot-color",  0xff202020 );
        params.ring_color = aconfig_unsigned(n, "ring-color", 0xff000000 );

        qemu_disp->trackball = skin_trackball_create( &params );
        skin_trackball_refresh( qemu_disp->trackball );
    }
#endif

    /* add an onion overlay image if needed */
    if (opts->onion) {
        SkinImage*  onion = skin_image_find_simple( opts->onion );
        int         alpha, rotate;

        if ( opts->onion_alpha && 1 == sscanf( opts->onion_alpha, "%d", &alpha ) ) {
            alpha = (256*alpha)/100;
        } else
            alpha = 128;

        if ( opts->onion_rotation && 1 == sscanf( opts->onion_rotation, "%d", &rotate ) ) {
            rotate &= 3;
        } else
            rotate = SKIN_ROTATION_0;

        qemulator_get()->onion          = onion;
        qemulator_get()->onion_alpha    = alpha;
        qemulator_get()->onion_rotation = rotate;
    }
}

int qemu_main(int argc, char **argv);

/* this function dumps the QEMU help */
extern void  help( void );
extern void  emulator_help( void );

#define  VERBOSE_OPT(str,var)   { str, &var }

#define  _VERBOSE_TAG(x,y)   { #x, VERBOSE_##x, y },
static const struct { const char*  name; int  flag; const char*  text; }
verbose_options[] = {
    VERBOSE_TAG_LIST
    { 0, 0, 0 }
};

static int
load_keyset(const char*  path)
{
    if (path_can_read(path)) {
        AConfig*  root = aconfig_node("","");
        if (!aconfig_load_file(root, path)) {
            android_keyset = skin_keyset_new(root);
            if (android_keyset != NULL) {
                D( "keyset loaded from: %s", path);
                return 0;
            }
        }
    }
    return -1;
}

static void
parse_keyset(const char*  keyset, AndroidOptions*  opts)
{
    char   kname[MAX_PATH];
    char   temp[MAX_PATH];
    char*  p;
    char*  end;

    /* append .keyset suffix if needed */
    if (strchr(keyset, '.') == NULL) {
        p   =  kname;
        end = p + sizeof(kname);
        p   = bufprint(p, end, "%s.keyset", keyset);
        if (p >= end) {
            derror( "keyset name too long: '%s'\n", keyset);
            exit(1);
        }
        keyset = kname;
    }

    /* look for a the keyset file */
    p   = temp;
    end = p + sizeof(temp);
    p = bufprint_config_file(p, end, keyset);
    if (p < end && load_keyset(temp) == 0)
        return;

    p = temp;
    p = bufprint(p, end, "%s" PATH_SEP "keysets" PATH_SEP "%s", opts->sysdir, keyset);
    if (p < end && load_keyset(temp) == 0)
        return;

    p = temp;
    p = bufprint_app_dir(p, end);
    p = bufprint(p, end, PATH_SEP "keysets" PATH_SEP "%s", keyset);
    if (p < end && load_keyset(temp) == 0)
        return;

    return;
}

static void
write_default_keyset( void )
{
    char   path[MAX_PATH];

    bufprint_config_file( path, path+sizeof(path), KEYSET_FILE );

    /* only write if there is no file here */
    if ( !path_exists(path) ) {
        int          fd = open( path, O_WRONLY | O_CREAT, 0666 );
        int          ret;
        const char*  ks = skin_keyset_get_default();


        D( "writing default keyset file to %s", path );

        if (fd < 0) {
            D( "%s: could not create file: %s", __FUNCTION__, strerror(errno) );
            return;
        }
        CHECKED(ret, write(fd, ks, strlen(ks)));
        close(fd);
    }
}

#ifdef CONFIG_NAND_LIMITS

static uint64_t
parse_nand_rw_limit( const char*  value )
{
    char*     end;
    uint64_t  val = strtoul( value, &end, 0 );

    if (end == value) {
        derror( "bad parameter value '%s': expecting unsigned integer", value );
        exit(1);
    }

    switch (end[0]) {
        case 'K':  val <<= 10; break;
        case 'M':  val <<= 20; break;
        case 'G':  val <<= 30; break;
        case 0: break;
        default:
            derror( "bad read/write limit suffix: use K, M or G" );
            exit(1);
    }
    return val;
}

static void
parse_nand_limits(char*  limits)
{
    int      pid = -1, signal = -1;
    int64_t  reads = 0, writes = 0;
    char*    item = limits;

    /* parse over comma-separated items */
    while (item && *item) {
        char*  next = strchr(item, ',');
        char*  end;

        if (next == NULL) {
            next = item + strlen(item);
        } else {
            *next++ = 0;
        }

        if ( !memcmp(item, "pid=", 4) ) {
            pid = strtol(item+4, &end, 10);
            if (end == NULL || *end) {
                derror( "bad parameter, expecting pid=<number>, got '%s'",
                        item );
                exit(1);
            }
            if (pid <= 0) {
                derror( "bad parameter: process identifier must be > 0" );
                exit(1);
            }
        }
        else if ( !memcmp(item, "signal=", 7) ) {
            signal = strtol(item+7,&end, 10);
            if (end == NULL || *end) {
                derror( "bad parameter: expecting signal=<number>, got '%s'",
                        item );
                exit(1);
            }
            if (signal <= 0) {
                derror( "bad parameter: signal number must be > 0" );
                exit(1);
            }
        }
        else if ( !memcmp(item, "reads=", 6) ) {
            reads = parse_nand_rw_limit(item+6);
        }
        else if ( !memcmp(item, "writes=", 7) ) {
            writes = parse_nand_rw_limit(item+7);
        }
        else {
            derror( "bad parameter '%s' (see -help-nand-limits)", item );
            exit(1);
        }
        item = next;
    }
    if (pid < 0) {
        derror( "bad paramater: missing pid=<number>" );
        exit(1);
    }
    else if (signal < 0) {
        derror( "bad parameter: missing signal=<number>" );
        exit(1);
    }
    else if (reads == 0 && writes == 0) {
        dwarning( "no read or write limit specified. ignoring -nand-limits" );
    } else {
        nand_threshold*  t;

        t  = &android_nand_read_threshold;
        t->pid     = pid;
        t->signal  = signal;
        t->counter = 0;
        t->limit   = reads;

        t  = &android_nand_write_threshold;
        t->pid     = pid;
        t->signal  = signal;
        t->counter = 0;
        t->limit   = writes;
    }
}
#endif /* CONFIG_NAND_LIMITS */

void emulator_help( void )
{
    STRALLOC_DEFINE(out);
    android_help_main(out);
    printf( "%.*s", out->n, out->s );
    stralloc_reset(out);
    exit(1);
}

static int
add_dns_server( const char*  server_name )
{
    SockAddress   addr;

    if (sock_address_init_resolve( &addr, server_name, 55, 0 ) < 0) {
        fprintf(stderr,
                "### WARNING: can't resolve DNS server name '%s'\n",
                server_name );
        return -1;
    }

    D( "DNS server name '%s' resolved to %s", server_name, sock_address_to_string(&addr) );

    if ( slirp_add_dns_server( &addr ) < 0 ) {
        fprintf(stderr,
                "### WARNING: could not add DNS server '%s' to the network stack\n", server_name);
        return -1;
    }
    return 0;
}


/* this function is used to perform auto-detection of the
 * system directory in the case of a SDK installation.
 *
 * we want to deal with several historical usages, hence
 * the slightly complicated logic.
 *
 * NOTE: the function returns the path to the directory
 *       containing 'fileName'. this is *not* the full
 *       path to 'fileName'.
 */
static char*
_getSdkImagePath( const char*  fileName )
{
    char   temp[MAX_PATH];
    char*  p   = temp;
    char*  end = p + sizeof(temp);
    char*  q;
    char*  app;

    static const char* const  searchPaths[] = {
        "",                                  /* program's directory */
        "/lib/images",                       /* this is for SDK 1.0 */
        "/../platforms/android-1.1/images",  /* this is for SDK 1.1 */
        NULL
    };

    app = bufprint_app_dir(temp, end);
    if (app >= end)
        return NULL;

    do {
        int  nn;

        /* first search a few well-known paths */
        for (nn = 0; searchPaths[nn] != NULL; nn++) {
            p = bufprint(app, end, "%s", searchPaths[nn]);
            q = bufprint(p, end, "/%s", fileName);
            if (q < end && path_exists(temp)) {
                *p = 0;
                goto FOUND_IT;
            }
        }

        /* hmmm. let's assume that we are in a post-1.1 SDK
         * scan ../platforms if it exists
         */
        p = bufprint(app, end, "/../platforms");
        if (p < end) {
            DirScanner*  scanner = dirScanner_new(temp);
            if (scanner != NULL) {
                int          found = 0;
                const char*  subdir;

                for (;;) {
                    subdir = dirScanner_next(scanner);
                    if (!subdir) break;

                    q = bufprint(p, end, "/%s/images/%s", subdir, fileName);
                    if (q >= end || !path_exists(temp))
                        continue;

                    found = 1;
                    p = bufprint(p, end, "/%s/images", subdir);
                    break;
                }
                dirScanner_free(scanner);
                if (found)
                    break;
            }
        }

        /* I'm out of ideas */
        return NULL;

    } while (0);

FOUND_IT:
    //D("image auto-detection: %s/%s", temp, fileName);
    return qemu_strdup(temp);
}

static char*
_getSdkImage( const char*  path, const char*  file )
{
    char  temp[MAX_PATH];
    char  *p = temp, *end = p + sizeof(temp);

    p = bufprint(temp, end, "%s/%s", path, file);
    if (p >= end || !path_exists(temp))
        return NULL;

    return qemu_strdup(temp);
}

static char*
_getSdkSystemImage( const char*  path, const char*  optionName, const char*  file )
{
    char*  image = _getSdkImage(path, file);

    if (image == NULL) {
        derror("Your system directory is missing the '%s' image file.\n"
               "Please specify one with the '%s <filepath>' option",
               file, optionName);
        exit(2);
    }
    return image;
}

static void
_forceAvdImagePath( AvdImageType  imageType,
                   const char*   path,
                   const char*   description,
                   int           required )
{
    if (path == NULL)
        return;

    if (required && !path_exists(path)) {
        derror("Cannot find %s image file: %s", description, path);
        exit(1);
    }
    android_avdParams->forcePaths[imageType] = path;
}

static uint64_t
_adjustPartitionSize( const char*  description,
                      uint64_t     imageBytes,
                      uint64_t     defaultBytes,
                      int          inAndroidBuild )
{
    char      temp[64];
    unsigned  imageMB;
    unsigned  defaultMB;

    if (imageBytes <= defaultBytes)
        return defaultBytes;

    imageMB   = convertBytesToMB(imageBytes);
    defaultMB = convertBytesToMB(defaultBytes);

    if (imageMB > defaultMB) {
        snprintf(temp, sizeof temp, "(%d MB > %d MB)", imageMB, defaultMB);
    } else {
        snprintf(temp, sizeof temp, "(%lld bytes > %lld bytes)", imageBytes, defaultBytes);
    }

    if (inAndroidBuild) {
        dwarning("%s partition size adjusted to match image file %s\n", description, temp);
    }

    /* Add extra space on the partition to allow for adb push. */
    return convertMBToBytes(imageMB + (imageMB >> 3));
}


int main(int argc, char **argv)
{
    char   tmp[MAX_PATH];
    char*  tmpend = tmp + sizeof(tmp);
    char*  args[128];
    int    n;
    char*  opt;
    int    use_sdcard_img = 0;
    int    serial = 0;
    int    gps_serial = 0;
    int    radio_serial = 0;
    int    qemud_serial = 0;
    int    shell_serial = 0;
    int    dns_count = 0;
    unsigned  cachePartitionSize = 0;
    unsigned  systemPartitionSize = 0;
    unsigned  dataPartitionSize = 0;
    unsigned  defaultPartitionSize = convertMBToBytes(66);

    AndroidHwConfig*  hw;
    AvdInfo*          avd;

    //const char *appdir = get_app_dir();
    char*       android_build_root = NULL;
    char*       android_build_out  = NULL;

    AndroidOptions  opts[1];

    args[0] = argv[0];

    if ( android_parse_options( &argc, &argv, opts ) < 0 ) {
        exit(1);
    }

    while (argc-- > 1) {
        opt = (++argv)[0];

        if(!strcmp(opt, "-qemu")) {
            argc--;
            argv++;
            break;
        }

        if (!strcmp(opt, "-help")) {
            emulator_help();
        }

        if (!strncmp(opt, "-help-",6)) {
            STRALLOC_DEFINE(out);
            opt += 6;

            if (!strcmp(opt, "all")) {
                android_help_all(out);
            }
            else if (android_help_for_option(opt, out) == 0) {
                /* ok */
            }
            else if (android_help_for_topic(opt, out) == 0) {
                /* ok */
            }
            if (out->n > 0) {
                printf("\n%.*s", out->n, out->s);
                exit(0);
            }

            fprintf(stderr, "unknown option: -help-%s\n", opt);
            fprintf(stderr, "please use -help for a list of valid topics\n");
            exit(1);
        }

        if (opt[0] == '-') {
            fprintf(stderr, "unknown option: %s\n", opt);
            fprintf(stderr, "please use -help for a list of valid options\n");
            exit(1);
        }

        fprintf(stderr, "invalid command-line parameter: %s.\n", opt);
        fprintf(stderr, "Hint: use '@foo' to launch a virtual device named 'foo'.\n");
        fprintf(stderr, "please use -help for more information\n");
        exit(1);
    }

    /* special case, if -qemu -h is used, directly invoke the QEMU-specific help */
    if (argc > 0) {
        int  nn;
        for (nn = 0; nn < argc; nn++)
            if (!strcmp(argv[nn], "-h")) {
                qemu_help(0);
                break;
            }
    }

    if (android_charmap_setup(opts->charmap)) {
        exit(1);
    }

    if (opts->version) {
        printf("Android emulator version %s\n"
               "Copyright (C) 2006-2008 The Android Open Source Project and many others.\n"
               "This program is a derivative of the QEMU CPU emulator (www.qemu.org).\n\n",
#if defined ANDROID_BUILD_ID
               VERSION_STRING " (build_id " STRINGIFY(ANDROID_BUILD_ID) ")" );
#else
               VERSION_STRING);
#endif
        printf("  This software is licensed under the terms of the GNU General Public\n"
               "  License version 2, as published by the Free Software Foundation, and\n"
               "  may be copied, distributed, and modified under those terms.\n\n"
               "  This program is distributed in the hope that it will be useful,\n"
               "  but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
               "  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n"
               "  GNU General Public License for more details.\n\n");

        exit(0);
    }

    if (opts->timezone) {
        if ( timezone_set(opts->timezone) < 0 ) {
            fprintf(stderr, "emulator: it seems the timezone '%s' is not in zoneinfo format\n", opts->timezone);
        }
    }

    /* legacy support: we used to use -system <dir> and -image <file>
     * instead of -sysdir <dir> and -system <file>, so handle this by checking
     * whether the options point to directories or files.
     */
    if (opts->image != NULL) {
        if (opts->system != NULL) {
            if (opts->sysdir != NULL) {
                derror( "You can't use -sysdir, -system and -image at the same time.\n"
                        "You should probably use '-sysdir <path> -system <file>'.\n" );
                exit(2);
            }
        }
        dwarning( "Please note that -image is obsolete and that -system is now used to point\n"
                  "to the system image. Next time, try using '-sysdir <path> -system <file>' instead.\n" );
        opts->sysdir = opts->system;
        opts->system = opts->image;
        opts->image  = NULL;
    }
    else if (opts->system != NULL && path_is_dir(opts->system)) {
        if (opts->sysdir != NULL) {
            derror( "Option -system should now be followed by a file path, not a directory one.\n"
                    "Please use '-sysdir <path>' to point to the system directory.\n" );
            exit(1);
        }
        dwarning( "Please note that the -system option should now be used to point to the initial\n"
                  "system image (like the obsolete -image option). To point to the system directory\n"
                  "please now use '-sysdir <path>' instead.\n" );

        opts->sysdir = opts->system;
        opts->system = NULL;
    }

    if (opts->nojni)
        opts->no_jni = opts->nojni;

    if (opts->nocache)
        opts->no_cache = opts->nocache;

    if (opts->noaudio)
        opts->no_audio = opts->noaudio;

    if (opts->noskin)
        opts->no_skin = opts->noskin;

    if (opts->initdata) {
        opts->init_data = opts->initdata;
        opts->initdata  = NULL;
    }

    /* If no AVD name was given, try to find the top of the
     * Android build tree
     */
    if (opts->avd == NULL) {
        do {
            char*  out = getenv("ANDROID_PRODUCT_OUT");

            if (out == NULL || out[0] == 0)
                break;

            if (!path_exists(out)) {
                derror("Can't access ANDROID_PRODUCT_OUT as '%s'\n"
                    "You need to build the Android system before launching the emulator",
                    out);
                exit(2);
            }

            android_build_root = path_parent( out, 4 );
            if (android_build_root == NULL || !path_exists(android_build_root)) {
                derror("Can't find the Android build root from '%s'\n"
                    "Please check the definition of the ANDROID_PRODUCT_OUT variable.\n"
                    "It should point to your product-specific build output directory.\n",
                    out );
                exit(2);
            }
            android_build_out = out;
            D( "found Android build root: %s", android_build_root );
            D( "found Android build out:  %s", android_build_out );
        } while (0);
    }
    /* if no virtual device name is given, and we're not in the
     * Android build system, we'll need to perform some auto-detection
     * magic :-)
     */
    if (opts->avd == NULL && !android_build_out)
    {
        char   dataDirIsSystem = 0;

        if (!opts->sysdir) {
            opts->sysdir = _getSdkImagePath("system.img");
            if (!opts->sysdir) {
                derror(
                "You did not specify a virtual device name, and the system\n"
                "directory could not be found.\n\n"
                "If you are an Android SDK user, please use '@<name>' or '-avd <name>'\n"
                "to start a given virtual device (see -help-avd for details).\n\n"

                "Otherwise, follow the instructions in -help-disk-images to start the emulator\n"
                );
                exit(2);
            }
            D("autoconfig: -sysdir %s", opts->sysdir);
        }

        if (!opts->system) {
            opts->system = _getSdkSystemImage(opts->sysdir, "-image", "system.img");
            D("autoconfig: -image %s", opts->image);
        }

        if (!opts->kernel) {
            opts->kernel = _getSdkSystemImage(opts->sysdir, "-kernel", "kernel-qemu");
            D("autoconfig: -kernel %s", opts->kernel);
        }

        if (!opts->ramdisk) {
            opts->ramdisk = _getSdkSystemImage(opts->sysdir, "-ramdisk", "ramdisk.img");
            D("autoconfig: -ramdisk %s", opts->ramdisk);
        }

        /* if no data directory is specified, use the system directory */
        if (!opts->datadir) {
            opts->datadir   = qemu_strdup(opts->sysdir);
            dataDirIsSystem = 1;
            D("autoconfig: -datadir %s", opts->sysdir);
        }

        if (!opts->data) {
            /* check for userdata-qemu.img in the data directory */
            bufprint(tmp, tmpend, "%s/userdata-qemu.img", opts->datadir);
            if (!path_exists(tmp)) {
                derror(
                "You did not provide the name of an Android Virtual Device\n"
                "with the '-avd <name>' option. Read -help-avd for more information.\n\n"

                "If you *really* want to *NOT* run an AVD, consider using '-data <file>'\n"
                "to specify a data partition image file (I hope you know what you're doing).\n"
                );
                exit(2);
            }

            opts->data = qemu_strdup(tmp);
            D("autoconfig: -data %s", opts->data);
        }

        if (!opts->sdcard && opts->datadir) {
            bufprint(tmp, tmpend, "%s/sdcard.img", opts->datadir);
            if (path_exists(tmp)) {
                opts->sdcard = qemu_strdup(tmp);
                D("autoconfig: -sdcard %s", opts->sdcard);
            }
        }
    }

    /* setup the virtual device parameters from our options
     */
    if (opts->no_cache) {
        android_avdParams->flags |= AVDINFO_NO_CACHE;
    }
    if (opts->wipe_data) {
        android_avdParams->flags |= AVDINFO_WIPE_DATA | AVDINFO_WIPE_CACHE;
    }

    /* if certain options are set, we can force the path of
        * certain kernel/disk image files
        */
    _forceAvdImagePath(AVD_IMAGE_KERNEL,     opts->kernel, "kernel", 1);
    _forceAvdImagePath(AVD_IMAGE_INITSYSTEM, opts->system, "system", 1);
    _forceAvdImagePath(AVD_IMAGE_RAMDISK,    opts->ramdisk,"ramdisk", 1);
    _forceAvdImagePath(AVD_IMAGE_USERDATA,   opts->data,   "user data", 0);
    _forceAvdImagePath(AVD_IMAGE_CACHE,      opts->cache,  "cache", 0);
    _forceAvdImagePath(AVD_IMAGE_SDCARD,     opts->sdcard, "SD Card", 0);

    /* we don't accept -skindir without -skin now
     * to simplify the autoconfig stuff with virtual devices
     */
    if (opts->no_skin) {
        opts->skin    = "320x480";
        opts->skindir = NULL;
    }

    if (opts->skindir) {
        if (!opts->skin) {
            derror( "the -skindir <path> option requires a -skin <name> option");
            exit(1);
        }
    }
    android_avdParams->skinName     = opts->skin;
    android_avdParams->skinRootPath = opts->skindir;

    /* setup the virtual device differently depending on whether
     * we are in the Android build system or not
     */
    if (opts->avd != NULL)
    {
        android_avdInfo = avdInfo_new( opts->avd, android_avdParams );
        if (android_avdInfo == NULL) {
            /* an error message has already been printed */
            dprint("could not find virtual device named '%s'", opts->avd);
            exit(1);
        }
    }
    else
    {
        if (!android_build_out) {
            android_build_out = android_build_root = opts->sysdir;
        }
        android_avdInfo = avdInfo_newForAndroidBuild(
                            android_build_root,
                            android_build_out,
                            android_avdParams );

        if(android_avdInfo == NULL) {
            D("could not start virtual device\n");
            exit(1);
        }
    }

    avd = android_avdInfo;

    /* get the skin from the virtual device configuration */
    opts->skin    = (char*) avdInfo_getSkinName( avd );
    opts->skindir = (char*) avdInfo_getSkinDir( avd );

    if (opts->skin) {
        D("autoconfig: -skin %s", opts->skin);
    }
    if (opts->skindir) {
        D("autoconfig: -skindir %s", opts->skindir);
    }

    /* Read hardware configuration */
    hw = android_hw;
    if (avdInfo_getHwConfig(avd, hw) < 0) {
        derror("could not read hardware configuration ?");
        exit(1);
    }

#ifdef CONFIG_NAND_LIMITS
    if (opts->nand_limits)
        parse_nand_limits(opts->nand_limits);
#endif

    if (opts->keyset) {
        parse_keyset(opts->keyset, opts);
        if (!android_keyset) {
            fprintf(stderr,
                    "emulator: WARNING: could not find keyset file named '%s',"
                    " using defaults instead\n",
                    opts->keyset);
        }
    }
    if (!android_keyset) {
        parse_keyset("default", opts);
        if (!android_keyset) {
            android_keyset = skin_keyset_new_from_text( skin_keyset_get_default() );
            if (!android_keyset) {
                fprintf(stderr, "PANIC: default keyset file is corrupted !!\n" );
                fprintf(stderr, "PANIC: please update the code in android/skin/keyset.c\n" );
                exit(1);
            }
            if (!opts->keyset)
                write_default_keyset();
        }
    }

    /* the purpose of -no-audio is to disable sound output from the emulator,
     * not to disable Audio emulation. So simply force the 'none' backends */
    if (opts->no_audio)
        opts->audio = "none";

    if (opts->audio) {
        if (opts->audio_in || opts->audio_out) {
            derror( "you can't use -audio with -audio-in or -audio-out\n" );
            exit(1);
        }
        if ( !audio_check_backend_name( 0, opts->audio ) ) {
            derror( "'%s' is not a valid audio output backend. see -help-audio-out\n",
                    opts->audio);
            exit(1);
        }
        opts->audio_out = opts->audio;
        opts->audio_in  = opts->audio;

        if ( !audio_check_backend_name( 1, opts->audio ) ) {
            fprintf(stderr,
                    "emulator: warning: '%s' is not a valid audio input backend. audio record disabled\n",
                    opts->audio);
            opts->audio_in = "none";
        }
    }

    if (opts->audio_in) {
        static char  env[64]; /* note: putenv needs a static unique string buffer */
        if ( !audio_check_backend_name( 1, opts->audio_in ) ) {
            derror( "'%s' is not a valid audio input backend. see -help-audio-in\n",
                    opts->audio_in);
            exit(1);
        }
        bufprint( env, env+sizeof(env), "QEMU_AUDIO_IN_DRV=%s", opts->audio_in );
        putenv( env );

        if (!hw->hw_audioInput) {
            dwarning( "Emulated hardware doesn't have audio input.");
        }
    }
    if (opts->audio_out) {
        static char  env[64]; /* note: putenv needs a static unique string buffer */
        if ( !audio_check_backend_name( 0, opts->audio_out ) ) {
            derror( "'%s' is not a valid audio output backend. see -help-audio-out\n",
                    opts->audio_out);
            exit(1);
        }
        bufprint( env, env+sizeof(env), "QEMU_AUDIO_OUT_DRV=%s", opts->audio_out );
        putenv( env );
        if (!hw->hw_audioOutput) {
            dwarning( "Emulated hardware doesn't have audio output");
        }
    }

    if (opts->cpu_delay) {
        char*   end;
        long    delay = strtol(opts->cpu_delay, &end, 0);
        if (end == NULL || *end || delay < 0 || delay > 1000 ) {
            fprintf(stderr, "option -cpu-delay must be an integer between 0 and 1000\n" );
            exit(1);
        }
        if (delay > 0)
            delay = (1000-delay);

        qemu_cpu_delay = (int) delay;
    }

    if (opts->shared_net_id) {
        char*  end;
        long   shared_net_id = strtol(opts->shared_net_id, &end, 0);
        if (end == NULL || *end || shared_net_id < 1 || shared_net_id > 255) {
            fprintf(stderr, "option -shared-net-id must be an integer between 1 and 255\n");
            exit(1);
        }
        char ip[11];
        snprintf(ip, 11, "10.1.2.%ld", shared_net_id);
        boot_property_add("net.shared_net_ip",ip);
    }


    emulator_config_init();
    init_skinned_ui(opts->skindir, opts->skin, opts);

    if (!opts->netspeed) {
        if (skin_network_speed)
            D("skin network speed: '%s'", skin_network_speed);
        opts->netspeed = (char*)skin_network_speed;
    }
    if (!opts->netdelay) {
        if (skin_network_delay)
            D("skin network delay: '%s'", skin_network_delay);
        opts->netdelay = (char*)skin_network_delay;
    }

    if ( android_parse_network_speed(opts->netspeed) < 0 ) {
        fprintf(stderr, "invalid -netspeed parameter '%s', see emulator -usage\n", opts->netspeed);
        emulator_help();
    }

    if ( android_parse_network_latency(opts->netdelay) < 0 ) {
        fprintf(stderr, "invalid -netdelay parameter '%s', see emulator -usage\n", opts->netdelay);
        emulator_help();
    }

    if (opts->netfast) {
        qemu_net_download_speed = 0;
        qemu_net_upload_speed = 0;
        qemu_net_min_latency = 0;
        qemu_net_max_latency = 0;
    }

    if (opts->trace) {
        char*   tracePath = avdInfo_getTracePath(avd, opts->trace);
        int     ret;

        if (tracePath == NULL) {
            derror( "bad -trace parameter" );
            exit(1);
        }
        ret = path_mkdir_if_needed( tracePath, 0755 );
        if (ret < 0) {
            fprintf(stderr, "could not create directory '%s'\n", tmp);
            exit(2);
        }
        opts->trace = tracePath;
    }

#ifdef CONFIG_MEMCHECK
    if (opts->memcheck) {
        memcheck_init(opts->memcheck);
    }
#endif  // CONFIG_MEMCHECK

    if (opts->tcpdump) {
        if (qemu_tcpdump_start(opts->tcpdump) < 0) {
            dwarning( "could not start packet capture: %s", strerror(errno));
        }
    }

    if (opts->no_cache)
        opts->cache = 0;

    if (opts->dns_server) {
        char*  x = strchr(opts->dns_server, ',');
        dns_count = 0;
        if (x == NULL)
        {
            if ( add_dns_server( opts->dns_server ) == 0 )
                dns_count = 1;
        }
        else
        {
            x = strdup(opts->dns_server);
            while (*x) {
                char*  y = strchr(x, ',');

                if (y != NULL)
                    *y = 0;

                if (y == NULL || y > x) {
                    if ( add_dns_server( x ) == 0 )
                        dns_count += 1;
                }

                if (y == NULL)
                    break;

                x = y+1;
            }
        }
        if (dns_count == 0)
            fprintf( stderr, "### WARNING: will use system default DNS server\n" );
    }

    if (dns_count == 0)
        dns_count = slirp_get_system_dns_servers();

    n = 1;
    /* generate arguments for the underlying qemu main() */
    {
        const char*  kernelFile    = avdInfo_getImageFile(avd, AVD_IMAGE_KERNEL);
        int          kernelFileLen = strlen(kernelFile);

        args[n++] = "-kernel";
        args[n++] = (char*)kernelFile;

        /* If the kernel image name ends in "-armv7", then change the cpu
         * type automatically. This is a poor man's approach to configuration
         * management, but should allow us to get past building ARMv7
         * system images with dex preopt pass without introducing too many
         * changes to the emulator sources.
         *
         * XXX:
         * A 'proper' change would require adding some sort of hardware-property
         * to each AVD config file, then automatically determine its value for
         * full Android builds (depending on some environment variable), plus
         * some build system changes. I prefer not to do that for now for reasons
         * of simplicity.
         */
         if (kernelFileLen > 6 && !memcmp(kernelFile + kernelFileLen - 6, "-armv7", 6)) {
            args[n++] = "-cpu";
            args[n++] = "cortex-a8";
         }
    }

    args[n++] = "-initrd";
    args[n++] = (char*) avdInfo_getImageFile(avd, AVD_IMAGE_RAMDISK);

    if (opts->partition_size) {
        char*  end;
        long   sizeMB = strtol(opts->partition_size, &end, 0);
        long   minSizeMB = 10;
        long   maxSizeMB = LONG_MAX / ONE_MB;

        if (sizeMB < 0 || *end != 0) {
            derror( "-partition-size must be followed by a positive integer" );
            exit(1);
        }
        if (sizeMB < minSizeMB || sizeMB > maxSizeMB) {
            derror( "partition-size (%d) must be between %dMB and %dMB",
                    sizeMB, minSizeMB, maxSizeMB );
            exit(1);
        }
        defaultPartitionSize = sizeMB * ONE_MB;
    }

    /* Check the size of the system partition image.
     * If we have an AVD, it must be smaller than
     * the disk.systemPartition.size hardware property.
     *
     * Otherwise, we need to adjust the systemPartitionSize
     * automatically, and print a warning.
     *
     */
    {
        uint64_t   systemBytes  = avdInfo_getImageFileSize(avd, AVD_IMAGE_INITSYSTEM);
        uint64_t   defaultBytes = defaultPartitionSize;

        if (defaultBytes == 0 || opts->partition_size)
            defaultBytes = defaultPartitionSize;

        systemPartitionSize = _adjustPartitionSize("system", systemBytes, defaultBytes,
                                                   android_build_out != NULL);
    }

    /* Check the size of the /data partition. The only interesting cases here are:
     * - when the USERDATA image already exists and is larger than the default
     * - when we're wiping data and the INITDATA is larger than the default.
     */

    {
        const char*  dataPath     = avdInfo_getImageFile(avd, AVD_IMAGE_USERDATA);
        uint64_t     defaultBytes = defaultPartitionSize;

        if (defaultBytes == 0 || opts->partition_size)
            defaultBytes = defaultPartitionSize;

        if (dataPath == NULL || !path_exists(dataPath) || opts->wipe_data) {
            dataPath = avdInfo_getImageFile(avd, AVD_IMAGE_INITDATA);
        }
        if (dataPath == NULL || !path_exists(dataPath)) {
            dataPartitionSize = defaultBytes;
        }
        else {
            uint64_t  dataBytes;
            path_get_size(dataPath, &dataBytes);

            dataPartitionSize = _adjustPartitionSize("data", dataBytes, defaultBytes,
                                                     android_build_out != NULL);
        }
    }

    {
        const char*  filetype = "file";

        if (avdInfo_isImageReadOnly(avd, AVD_IMAGE_INITSYSTEM))
            filetype = "initfile";

        bufprint(tmp, tmpend,
             "system,size=0x%x,%s=%s", systemPartitionSize, filetype,
             avdInfo_getImageFile(avd, AVD_IMAGE_INITSYSTEM));

        args[n++] = "-nand";
        args[n++] = strdup(tmp);
    }

    bufprint(tmp, tmpend,
             "userdata,size=0x%x,file=%s",
             dataPartitionSize,
             avdInfo_getImageFile(avd, AVD_IMAGE_USERDATA));

    args[n++] = "-nand";
    args[n++] = strdup(tmp);

    if (hw->disk_cachePartition) {
        opts->cache = (char*) avdInfo_getImageFile(avd, AVD_IMAGE_CACHE);
        cachePartitionSize = hw->disk_cachePartition_size;
    }
    else if (opts->cache) {
        dwarning( "Emulated hardware doesn't support a cache partition" );
        opts->cache    = NULL;
        opts->no_cache = 1;
    }

    if (opts->cache) {
        /* use a specific cache file */
        sprintf(tmp, "cache,size=0x%0x,file=%s", cachePartitionSize, opts->cache);
        args[n++] = "-nand";
        args[n++] = strdup(tmp);
    }
    else if (!opts->no_cache) {
        /* create a temporary cache partition file */
        sprintf(tmp, "cache,size=0x%0x", cachePartitionSize);
        args[n++] = "-nand";
        args[n++] = strdup(tmp);
    }

    if (hw->hw_sdCard != 0)
        opts->sdcard = (char*) avdInfo_getImageFile(avd, AVD_IMAGE_SDCARD);
    else if (opts->sdcard) {
        dwarning( "Emulated hardware doesn't support SD Cards" );
        opts->sdcard = NULL;
    }

    if(opts->sdcard) {
        uint64_t  size;
        if (path_get_size(opts->sdcard, &size) == 0) {
            /* see if we have an sdcard image.  get its size if it exists */
            /* due to what looks like limitations of the MMC protocol, one has
             * to use an SD Card image that is equal or larger than 9 MB
             */
            if (size < 9*1024*1024ULL) {
                fprintf(stderr, "### WARNING: SD Card files must be at least 9MB, ignoring '%s'\n", opts->sdcard);
            } else {
                args[n++] = "-hda";
                args[n++] = opts->sdcard;
                use_sdcard_img = 1;
            }
        } else {
            D("no SD Card image at '%s'", opts->sdcard);
        }
    }

    if (!opts->logcat || opts->logcat[0] == 0) {
        opts->logcat = getenv("ANDROID_LOG_TAGS");
        if (opts->logcat && opts->logcat[0] == 0)
            opts->logcat = NULL;
    }

#if 0
    if (opts->console) {
        derror( "option -console is obsolete. please use -shell instead" );
        exit(1);
    }
#endif

    /* we always send the kernel messages from ttyS0 to android_kmsg */
    {
        AndroidKmsgFlags  flags = 0;

        if (opts->show_kernel)
            flags |= ANDROID_KMSG_PRINT_MESSAGES;

        android_kmsg_init( flags );
        args[n++] = "-serial";
        args[n++] = "android-kmsg";
        serial++;
    }

    /* XXXX: TODO: implement -shell and -logcat through qemud instead */
    if (!opts->shell_serial) {
#ifdef _WIN32
        opts->shell_serial = "con:";
#else
        opts->shell_serial = "stdio";
#endif
    }
    else
        opts->shell = 1;

    if (opts->shell || opts->logcat) {
        args[n++] = "-serial";
        args[n++] = opts->shell_serial;
        shell_serial = serial++;
    }

    if (opts->old_system)
    {
        if (opts->radio) {
            args[n++] = "-serial";
            args[n++] = opts->radio;
            radio_serial = serial++;
        }
        else {
            args[n++] = "-serial";
            args[n++] = "android-modem";
            radio_serial = serial++;
        }
        if (opts->gps) {
            args[n++] = "-serial";
            args[n++] = opts->gps;
            gps_serial = serial++;
        }
    }
    else /* !opts->old_system */
    {
        args[n++] = "-serial";
        args[n++] = "android-qemud";
        qemud_serial = serial++;

        if (opts->radio) {
            CharDriverState*  cs = qemu_chr_open("radio",opts->radio,NULL);
            if (cs == NULL) {
                derror( "unsupported character device specification: %s\n"
                        "used -help-char-devices for list of available formats\n", opts->radio );
                exit(1);
            }
            android_qemud_set_channel( ANDROID_QEMUD_GSM, cs);
        }
        else if ( hw->hw_gsmModem != 0 ) {
            if ( android_qemud_get_channel( ANDROID_QEMUD_GSM, &android_modem_cs ) < 0 ) {
                derror( "could not initialize qemud 'gsm' channel" );
                exit(1);
            }
        }

        if (opts->gps) {
            CharDriverState*  cs = qemu_chr_open("gps",opts->gps,NULL);
            if (cs == NULL) {
                derror( "unsupported character device specification: %s\n"
                        "used -help-char-devices for list of available formats\n", opts->gps );
                exit(1);
            }
            android_qemud_set_channel( ANDROID_QEMUD_GPS, cs);
        }
        else if ( hw->hw_gps != 0 ) {
            if ( android_qemud_get_channel( "gps", &android_gps_cs ) < 0 ) {
                derror( "could not initialize qemud 'gps' channel" );
                exit(1);
            }
        }
    }

    if (opts->memory) {
        char*  end;
        long   ramSize = strtol(opts->memory, &end, 0);
        if (ramSize < 0 || *end != 0) {
            derror( "-memory must be followed by a positive integer" );
            exit(1);
        }
        if (ramSize < 32 || ramSize > 4096) {
            derror( "physical memory size must be between 32 and 4096 MB" );
            exit(1);
        }
    }
    if (!opts->memory) {
        bufprint(tmp, tmpend, "%d", hw->hw_ramSize);
        opts->memory = qemu_strdup(tmp);
    }

    if (opts->trace) {
        args[n++] = "-trace";
        args[n++] = opts->trace;
        args[n++] = "-tracing";
        args[n++] = "off";
    }

    args[n++] = "-append";

    if (opts->bootchart) {
        char*  end;
        int    timeout = strtol(opts->bootchart, &end, 10);
        if (timeout == 0)
            opts->bootchart = NULL;
        else if (timeout < 0 || timeout > 15*60) {
            derror( "timeout specified for -bootchart option is invalid.\n"
                    "please use integers between 1 and 900\n");
            exit(1);
        }
    }

    /* start the 'boot-properties service, and parse the -prop
     * options, if any.
     */
    boot_property_init_service();

    hwLcd_setBootProperty(get_device_dpi(opts));

    /* Set the VM's max heap size, passed as a boot property */
    if (hw->vm_heapSize > 0) {
        char  tmp[32], *p=tmp, *end=p + sizeof(tmp);
        p = bufprint(p, end, "%dm", hw->vm_heapSize);

        boot_property_add("dalvik.vm.heapsize",tmp);
    }

    if (opts->prop != NULL) {
        ParamList*  pl = opts->prop;
        for ( ; pl != NULL; pl = pl->next ) {
            boot_property_parse_option(pl->param);
        }
    }

    /* Setup the kernel init options
     */
    {
        static char  params[1024];
        char        *p = params, *end = p + sizeof(params);

        p = bufprint(p, end, "qemu=1 console=ttyS0" );

        if (opts->shell || opts->logcat) {
            p = bufprint(p, end, " androidboot.console=ttyS%d", shell_serial );
        }

        if (opts->trace) {
            p = bufprint(p, end, " android.tracing=1");
        }

#ifdef CONFIG_MEMCHECK
        if (opts->memcheck) {
            /* This will set ro.kernel.memcheck system property
             * to memcheck's tracing flags. */
            p = bufprint(p, end, " memcheck=%s", opts->memcheck);
        }
#endif  // CONFIG_MEMCHECK

        if (!opts->no_jni) {
            p = bufprint(p, end, " android.checkjni=1");
        }

        if (opts->no_boot_anim) {
            p = bufprint( p, end, " android.bootanim=0" );
        }

        if (opts->logcat) {
            char*  q = bufprint(p, end, " androidboot.logcat=%s", opts->logcat);

            if (q < end) {
                /* replace any space by a comma ! */
                {
                    int  nn;
                    for (nn = 1; p[nn] != 0; nn++)
                        if (p[nn] == ' ' || p[nn] == '\t')
                            p[nn] = ',';
                    p += nn;
                }
            }
            p = q;
        }

        if (opts->old_system)
        {
            p = bufprint(p, end, " android.ril=ttyS%d", radio_serial);

            if (opts->gps) {
                p = bufprint(p, end, " android.gps=ttyS%d", gps_serial);
            }
        }
        else
        {
            p = bufprint(p, end, " android.qemud=ttyS%d", qemud_serial);
        }

        if (dns_count > 0) {
            p = bufprint(p, end, " android.ndns=%d", dns_count);
        }

        if (opts->bootchart) {
            p = bufprint(p, end, " androidboot.bootchart=%s", opts->bootchart);
        }

        if (p >= end) {
            fprintf(stderr, "### ERROR: kernel parameters too long\n");
            exit(1);
        }

        args[n++] = strdup(params);
    }

    if (opts->ports) {
        args[n++] = "-android-ports";
        args[n++] = opts->ports;
    }

    if (opts->port) {
        args[n++] = "-android-port";
        args[n++] = opts->port;
    }

    if (opts->report_console) {
        args[n++] = "-android-report-console";
        args[n++] = opts->report_console;
    }

    if (opts->http_proxy) {
        args[n++] = "-http-proxy";
        args[n++] = opts->http_proxy;
    }

    /* physical memory */
    args[n++] = "-m";
    args[n++] = opts->memory;

    /* on Linux, the 'dynticks' clock sometimes doesn't work
     * properly. this results in the UI freezing while emulation
     * continues, for several seconds...
     */
#ifdef __linux__
    args[n++] = "-clock";
    args[n++] = "unix";
#endif

    /* Set up the interfaces for inter-emulator networking */
    if (opts->shared_net_id) {
        unsigned int shared_net_id = atoi(opts->shared_net_id);
        char nic[37];

        args[n++] = "-net";
        args[n++] = "nic,vlan=0";
        args[n++] = "-net";
        args[n++] = "user,vlan=0";

        args[n++] = "-net";
        snprintf(nic, sizeof nic, "nic,vlan=1,macaddr=52:54:00:12:34:%02x", shared_net_id);
        args[n++] = strdup(nic);
        args[n++] = "-net";
        args[n++] = "socket,vlan=1,mcast=230.0.0.10:1234";
    }

    while(argc-- > 0) {
        args[n++] = *argv++;
    }
    args[n] = 0;

    if(VERBOSE_CHECK(init)) {
        int i;
        for(i = 0; i < n; i++) {
            fprintf(stdout, "emulator: argv[%02d] = \"%s\"\n", i, args[i]);
        }
    }
    return qemu_main(n, args);
}
