/* Copyright (C) 2008 The Android Open Source Project
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
#ifndef ANDROID_AVD_INFO_H
#define ANDROID_AVD_INFO_H

#include "android/utils/ini.h"
#include "android/avd/hw-config.h"

/* An Android Virtual Device (AVD for short) corresponds to a
 * directory containing all kernel/disk images for a given virtual
 * device, as well as information about its hardware capabilities,
 * SDK version number, skin, etc...
 *
 * Each AVD has a human-readable name and is backed by a root
 * configuration file and a content directory. For example, an
 *  AVD named 'foo' will correspond to the following:
 *
 *  - a root configuration file named ~/.android/avd/foo.ini
 *    describing where the AVD's content can be found
 *
 *  - a content directory like ~/.android/avd/foo/ containing all
 *    disk image and configuration files for the virtual device.
 *
 * the 'foo.ini' file should contain at least one line of the form:
 *
 *    rootPath=<content-path>
 *
 * it may also contain other lines that cache stuff found in the
 * content directory, like hardware properties or SDK version number.
 *
 * it is possible to move the content directory by updating the foo.ini
 * file to point to the new location. This can be interesting when your
 * $HOME directory is located on a network share or in a roaming profile
 * (Windows), given that the content directory of a single virtual device
 * can easily use more than 100MB of data.
 *
 */

/* a macro used to define the list of disk images managed by the
 * implementation. This macro will be expanded several times with
 * varying definitions of _AVD_IMG
 */
#define  AVD_IMAGE_LIST \
    _AVD_IMG(KERNEL,"kernel-qemu","kernel") \
    _AVD_IMG(RAMDISK,"ramdisk.img","ramdisk") \
    _AVD_IMG(INITSYSTEM,"system.img","init system") \
    _AVD_IMG(INITDATA,"userdata.img","init data") \
    _AVD_IMG(USERSYSTEM,"system-qemu.img","user system") \
    _AVD_IMG(USERDATA,"userdata-qemu.img", "user data") \
    _AVD_IMG(CACHE,"cache.img","cache") \
    _AVD_IMG(SDCARD,"sdcard.img","SD Card") \

/* define the enumared values corresponding to each AVD image type
 * examples are: AVD_IMAGE_KERNEL, AVD_IMAGE_SYSTEM, etc..
 */
#define _AVD_IMG(x,y,z)   AVD_IMAGE_##x ,
typedef enum {
    AVD_IMAGE_LIST
    AVD_IMAGE_MAX /* do not remove */
} AvdImageType;
#undef  _AVD_IMG

/* AvdInfo is an opaque structure used to model the information
 * corresponding to a given AVD instance
 */
typedef struct AvdInfo  AvdInfo;

/* various flags used when creating an AvdInfo object */
typedef enum {
    /* use to force a data wipe */
    AVDINFO_WIPE_DATA = (1 << 0),
    /* use to ignore the cache partition */
    AVDINFO_NO_CACHE  = (1 << 1),
    /* use to wipe cache partition, ignored if NO_CACHE is set */
    AVDINFO_WIPE_CACHE = (1 << 2),
    /* use to ignore ignore SDCard image (default or provided) */
    AVDINFO_NO_SDCARD = (1 << 3),
    /* use to wipe the system image with new initial values */
    AVDINFO_WIPE_SYSTEM = (1 << 4),
} AvdFlags;

typedef struct {
    unsigned     flags;
    const char*  skinName;
    const char*  skinRootPath;
    const char*  forcePaths[AVD_IMAGE_MAX];
} AvdInfoParams;

/* Creates a new AvdInfo object from a name. Returns NULL if name is NULL
 * or contains characters that are not part of the following list:
 * letters, digits, underscores, dashes and periods
 */
AvdInfo*  avdInfo_new( const char*  name, AvdInfoParams*  params );

/* A special function used to setup an AvdInfo for use when starting
 * the emulator from the Android build system. In this specific instance
 * we're going to create temporary files to hold all writable image
 * files, and activate all hardware features by default
 *
 * 'androidBuildRoot' must be the absolute path to the root of the
 * Android build system (i.e. the 'android' directory)
 *
 * 'androidOut' must be the target-specific out directory where
 * disk images will be looked for.
 */
AvdInfo*  avdInfo_newForAndroidBuild( const char*     androidBuildRoot,
                                      const char*     androidOut,
                                      AvdInfoParams*  params );

/* Frees an AvdInfo object and the corresponding strings that may be
 * returned by its getXXX() methods
 */
void        avdInfo_free( AvdInfo*  i );

/* Return the name of the Android Virtual Device
 */
const char*  avdInfo_getName( AvdInfo*  i );

/* Try to find the path of a given image file, returns NULL
 * if the corresponding file could not be found. the string
 * belongs to the AvdInfo object.
 */
const char*  avdInfo_getImageFile( AvdInfo*  i, AvdImageType  imageType );

/* Return the size of a given image file. Returns 0 if the file
 * does not exist or could not be accessed.
 */
uint64_t     avdInfo_getImageFileSize( AvdInfo*  i, AvdImageType  imageType );

/* Returns 1 if the corresponding image file is read-only
 */
int          avdInfo_isImageReadOnly( AvdInfo*  i, AvdImageType  imageType );

/* lock an image file if it is writable. returns 0 on success, or -1
 * otherwise. note that if the file is read-only, it doesn't need to
 * be locked and the function will return success.
 */
int          avdInfo_lockImageFile( AvdInfo*  i, AvdImageType  imageType, int  abortOnError);

/* Manually set the path of a given image file. */
void         avdInfo_setImageFile( AvdInfo*  i, AvdImageType  imageType, const char*  imagePath );

/* Returns the path of the skin directory */
/* the string belongs to the AvdInfo object */
const char*  avdInfo_getSkinPath( AvdInfo*  i );

/* Returns the name of the virtual device's skin */
const char*  avdInfo_getSkinName( AvdInfo*  i );

/* Returns the root skin directory for this device */
const char*  avdInfo_getSkinDir ( AvdInfo*  i );

/* Returns the content path of the virtual device */
const char*  avdInfo_getContentPath( AvdInfo*  i );

/* Returns TRUE iff in the Android build system */
int          avdInfo_inAndroidBuild( AvdInfo*  i );

/* Reads the AVD's hardware configuration into 'hw'. returns -1 on error, 0 otherwise */
int          avdInfo_getHwConfig( AvdInfo*  i, AndroidHwConfig*  hw );

/* Returns a *copy* of the path used to store trace 'foo'. result must be freed by caller */
char*        avdInfo_getTracePath( AvdInfo*  i, const char*  traceName );

/* */

#endif /* ANDROID_AVD_INFO_H */
