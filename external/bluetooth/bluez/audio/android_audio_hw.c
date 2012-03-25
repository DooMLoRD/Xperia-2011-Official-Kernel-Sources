/*
 *  Copyright (C) 2008-2011 The Android Open Source Project
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#define LOG_TAG "a2dp_audio_hw"
//#define LOG_NDEBUG 0

#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <sys/time.h>

#include <cutils/log.h>
#include <cutils/str_parms.h>

#include <hardware/hardware.h>
#include <system/audio.h>
#include <hardware/audio.h>

#include <hardware_legacy/power.h>

#include "liba2dp.h"

#define A2DP_WAKE_LOCK_NAME            "A2dpOutputStream"
#define MAX_WRITE_RETRIES              5

#define A2DP_SUSPENDED_PARM            "A2dpSuspended"
#define BLUETOOOTH_ENABLED_PARM        "bluetooth_enabled"

#define OUT_SINK_ADDR_PARM             "a2dp_sink_address"

/* number of periods in pcm buffer (one period corresponds to buffer size reported to audio flinger
 * by out_get_buffer_size() */
#define BUF_NUM_PERIODS 6
/* maximum time allowed by out_standby_stream_locked() for 2dp_write() to complete */
#define BUF_WRITE_COMPLETION_TIMEOUT_MS 5000
/* maximum time allowed by out_write() for frames to be available in in write thread buffer */
#define BUF_WRITE_AVAILABILITY_TIMEOUT_MS 1000
/* maximum number of attempts to wait for a write completion in out_standby_stream_locked() */
#define MAX_WRITE_COMPLETION_ATTEMPTS 5

/* NOTE: there are 2 mutexes used by the a2dp output stream.
 *  - lock: protects all calls to a2dp lib functions (a2dp_stop(), a2dp_cleanup()...).
 *    One exception is a2dp_write() which is also protected by the flag write_busy. This is because
 *    out_write() cannot block waiting for a2dp_write() to complete because this function
 *    can sleep to throttle the A2DP bit rate.
 *    This flag is always set/reset and tested with "lock" mutex held.
 *  - buf_lock: protects access to pcm buffer read and write indexes.
 *
 *  The locking order is always as follows:
 *      buf_lock -> lock
 *
 * If you need to hold the adev_a2dp->lock AND the astream_out->lock or astream_out->buf_lock,
 * you MUST take adev_a2dp lock first!!
 */

struct astream_out;
struct adev_a2dp {
    struct audio_hw_device  device;

    audio_mode_t            mode;
    bool                    bt_enabled;
    bool                    suspended;

    pthread_mutex_t         lock;

    struct astream_out      *output;
};

struct astream_out {
    struct audio_stream_out stream;

    uint32_t                sample_rate;
    size_t                  buffer_size;
    uint32_t                channels;
    int                     format;

    int                     fd;
    bool                    standby;
    int                     start_count;
    int                     retry_count;
    void*                   data;

    pthread_mutex_t         lock;   /* see NOTE on mutex locking order above */

    audio_devices_t         device;
    uint64_t                 last_write_time;
    uint32_t                buffer_duration_us;

    bool                    bt_enabled;
    bool                    suspended;
    char                    a2dp_addr[20];

    uint32_t *buf;              /* pcm buffer between audioflinger thread and write thread*/
    size_t buf_size;            /* size of pcm buffer in frames */
    size_t buf_rd_idx;          /* read index in pcm buffer, in frames*/
    size_t buf_wr_idx;          /* write index in pcm buffer, in frames */
    size_t buf_frames_ready;    /* number of frames ready for writing to a2dp sink */
    pthread_mutex_t buf_lock;   /* mutex protecting read and write indexes */
                                /* see NOTE on mutex locking order above */
    pthread_cond_t buf_cond;    /* condition signaling data write/read to/from pcm buffer */
    pthread_t buf_thread;       /* thread reading data from buffer and writing to a2dp sink*/
    bool buf_thread_exit;       /* flag requesting write thread exit */
    bool write_busy;            /* indicates that a write to a2dp sink is in progress and that
                                   standby must wait for this flag to be cleared by write thread */
    pthread_cond_t write_cond;  /* condition associated with write_busy flag */
};

static uint64_t system_time(void)
{
    struct timespec t;

    t.tv_sec = t.tv_nsec = 0;
    clock_gettime(CLOCK_MONOTONIC, &t);

    return t.tv_sec*1000000000LL + t.tv_nsec;
}

/** audio_stream_out implementation **/
static uint32_t out_get_sample_rate(const struct audio_stream *stream)
{
    const struct astream_out *out = (const struct astream_out *)stream;
    return out->sample_rate;
}

static int out_set_sample_rate(struct audio_stream *stream, uint32_t rate)
{
    struct astream_out *out = (struct astream_out *)stream;

    LOGE("(%s:%d) %s: Implement me!", __FILE__, __LINE__, __func__);
    return 0;
}

static size_t out_get_buffer_size(const struct audio_stream *stream)
{
    const struct astream_out *out = (const struct astream_out *)stream;
    return out->buffer_size;
}

static uint32_t out_get_channels(const struct audio_stream *stream)
{
    const struct astream_out *out = (const struct astream_out *)stream;
    return out->channels;
}

static int out_get_format(const struct audio_stream *stream)
{
    const struct astream_out *out = (const struct astream_out *)stream;
    return out->format;
}

static int out_set_format(struct audio_stream *stream, int format)
{
    struct astream_out *out = (struct astream_out *)stream;
    LOGE("(%s:%d) %s: Implement me!", __FILE__, __LINE__, __func__);
    return 0;
}

static int out_dump(const struct audio_stream *stream, int fd)
{
    return 0;
}

static uint32_t out_get_latency(const struct audio_stream_out *stream)
{
    const struct astream_out *out = (const struct astream_out *)stream;

    return (out->buffer_duration_us / 1000) + 200;
}

static int out_set_volume(struct audio_stream_out *stream, float left,
                          float right)
{
    return -ENOSYS;
}

static int out_get_render_position(const struct audio_stream_out *stream,
                                   uint32_t *dsp_frames)
{
    return -ENOSYS;
}

static int _out_init_locked(struct astream_out *out, const char *addr)
{
    int ret;

    if (out->data)
        return 0;

    /* XXX: shouldn't this use the sample_rate/channel_count from 'out'? */
    ret = a2dp_init(44100, 2, &out->data);
    if (ret < 0) {
        LOGE("a2dp_init failed err: %d\n", ret);
        out->data = NULL;
        return ret;
    }

    /* XXX: is this even necessary? */
    if (addr)
        strlcpy(out->a2dp_addr, addr, sizeof(out->a2dp_addr));
    a2dp_set_sink(out->data, out->a2dp_addr);

    return 0;
}

static bool _out_validate_parms(struct astream_out *out, int format,
                                uint32_t chans, uint32_t rate)
{
    if ((format && (format != out->format)) ||
        (chans && (chans != out->channels)) ||
        (rate && (rate != out->sample_rate)))
        return false;
    return true;
}

static int out_standby_stream_locked(struct astream_out *out)
{
    int ret = 0;
    int attempts = MAX_WRITE_COMPLETION_ATTEMPTS;

    if (out->standby || !out->data)
        return 0;

    out->standby = true;
    /* wait for write completion if needed */
    while (out->write_busy && attempts--) {
        ret = pthread_cond_timeout_np(&out->write_cond,
                                &out->lock,
                                BUF_WRITE_COMPLETION_TIMEOUT_MS);
        LOGE_IF(ret != 0, "out_standby_stream_locked() wait cond error %d", ret);
    }
    LOGE_IF(attempts == 0, "out_standby_stream_locked() a2dp_write() would not stop!!!");

    LOGV_IF(!out->bt_enabled, "Standby skip stop: enabled %d", out->bt_enabled);
    if (out->bt_enabled) {
        ret = a2dp_stop(out->data);
    }
    release_wake_lock(A2DP_WAKE_LOCK_NAME);

    return ret;
}

static int out_close_stream_locked(struct astream_out *out)
{
    out_standby_stream_locked(out);

    if (out->data) {
        LOGV("%s: calling a2dp_cleanup()", __func__);
        a2dp_cleanup(out->data);
        out->data = NULL;
    }

    return 0;
}

static int out_standby(struct audio_stream *stream)
{
    struct astream_out *out = (struct astream_out *)stream;

    pthread_mutex_lock(&out->lock);
    out_standby_stream_locked(out);
    pthread_mutex_unlock(&out->lock);

    return 0;
}

static int out_set_parameters(struct audio_stream *stream, const char *kvpairs)
{
    struct astream_out *out = (struct astream_out *)stream;
    struct str_parms *parms;
    char *str;
    char value[32];
    int ret;

    parms = str_parms_create_str(kvpairs);

    pthread_mutex_lock(&out->lock);

    ret = str_parms_get_str(parms, OUT_SINK_ADDR_PARM, value, sizeof(value));
    if (ret >= 0) {
        /* strlen(00:00:00:00:00:00) == 17 */
        if (strlen(value) == 17) {
            strlcpy(out->a2dp_addr, value, sizeof(out->a2dp_addr));
            if (out->data)
                a2dp_set_sink(out->data, out->a2dp_addr);
        } else
            ret = -EINVAL;
    }

    pthread_mutex_unlock(&out->lock);
    str_parms_destroy(parms);
    return ret;
}

static audio_devices_t out_get_device(const struct audio_stream *stream)
{
    const struct astream_out *out = (const struct astream_out *)stream;
    return out->device;
}


static int out_set_device(struct audio_stream *stream, audio_devices_t device)
{
    struct astream_out *out = (struct astream_out *)stream;

    if (!audio_is_a2dp_device(device))
        return -EINVAL;

    /* XXX: if out->device ever starts getting used for anything, need to
     * grab the out->lock */
    out->device = device;
    return 0;
}

static char * out_get_parameters(const struct audio_stream *stream,
                                 const char *keys)
{
    struct astream_out *out = (struct astream_out *)stream;
    struct str_parms *parms;
    struct str_parms *out_parms;
    char *str;
    char value[20];
    int ret;

    parms = str_parms_create_str(keys);
    out_parms = str_parms_create();

    pthread_mutex_lock(&out->lock);

    ret = str_parms_get_str(parms, OUT_SINK_ADDR_PARM, value, sizeof(value));
    if (ret >= 0)
        str_parms_add_str(out_parms, OUT_SINK_ADDR_PARM, out->a2dp_addr);

    pthread_mutex_unlock(&out->lock);

    str = str_parms_to_str(out_parms);
    str_parms_destroy(out_parms);
    str_parms_destroy(parms);

    return str;
}

size_t _out_frames_available_locked(struct astream_out *out)
{

    size_t frames = out->buf_size - out->buf_frames_ready;
    if (frames > out->buf_size - out->buf_wr_idx) {
        frames = out->buf_size - out->buf_wr_idx;
    }
    return frames;
}

size_t _out_frames_ready_locked(struct astream_out *out)
{
    size_t frames = out->buf_frames_ready;

    if (frames > out->buf_size - out->buf_rd_idx) {
        frames = out->buf_size - out->buf_rd_idx;
    }
    return frames;
}

void _out_inc_wr_idx_locked(struct astream_out *out, size_t frames)
{
    out->buf_wr_idx += frames;
    out->buf_frames_ready += frames;
    if (out->buf_wr_idx == out->buf_size) {
        out->buf_wr_idx = 0;
    }
    pthread_cond_signal(&out->buf_cond);
}

void _out_inc_rd_idx_locked(struct astream_out *out, size_t frames)
{
    out->buf_rd_idx += frames;
    out->buf_frames_ready -= frames;
    if (out->buf_rd_idx == out->buf_size) {
        out->buf_rd_idx = 0;
    }
    pthread_cond_signal(&out->buf_cond);
}

static ssize_t out_write(struct audio_stream_out *stream, const void* buffer,
                         size_t bytes)
{
    struct astream_out *out = (struct astream_out *)stream;
    int ret;
    size_t frames_total = bytes / sizeof(uint32_t); // always stereo 16 bit
    uint32_t *buf = (uint32_t *)buffer;
    size_t frames_written = 0;

    pthread_mutex_lock(&out->buf_lock);
    pthread_mutex_lock(&out->lock);
    if (!out->bt_enabled || out->suspended) {
        LOGV("a2dp write: bluetooth disabled bt_en %d, suspended %d",
             out->bt_enabled, out->suspended);
        ret = -1;
        goto err_bt_disabled;
    }

    if (out->standby) {
        acquire_wake_lock(PARTIAL_WAKE_LOCK, A2DP_WAKE_LOCK_NAME);
        out->standby = false;
        out->last_write_time = system_time();
        out->buf_rd_idx = 0;
        out->buf_wr_idx = 0;
        out->buf_frames_ready = 0;
    }

    ret = _out_init_locked(out, NULL);
    if (ret < 0) {
        goto err_init;
    }

    pthread_mutex_unlock(&out->lock);

    while (frames_written < frames_total) {
        size_t frames = _out_frames_available_locked(out);
        if (frames == 0) {
            int ret = pthread_cond_timeout_np(&out->buf_cond,
                                              &out->buf_lock,
                                              BUF_WRITE_AVAILABILITY_TIMEOUT_MS);
            if (ret != 0) {
                pthread_mutex_lock(&out->lock);
                goto err_write;
            }
            frames  = _out_frames_available_locked(out);
        }
        if (frames > frames_total - frames_written) {
            frames = frames_total - frames_written;
        }
        memcpy(out->buf + out->buf_wr_idx, buf + frames_written, frames * sizeof(uint32_t));
        frames_written += frames;
        _out_inc_wr_idx_locked(out, frames);
        pthread_mutex_lock(&out->lock);
        if (out->standby) {
            goto err_write;
        }
        pthread_mutex_unlock(&out->lock);
    }
    pthread_mutex_unlock(&out->buf_lock);

    return bytes;

/* out->lock must be locked and out->buf_lock unlocked when jumping here */
err_write:
err_init:
err_bt_disabled:
    pthread_mutex_unlock(&out->buf_lock);
    LOGV("!!!! write error");
    out_standby_stream_locked(out);
    pthread_mutex_unlock(&out->lock);

    /* XXX: simulate audio output timing in case of error?!?! */
    usleep(out->buffer_duration_us);
    return ret;
}


static void *_out_buf_thread_func(void *context)
{
    struct astream_out *out = (struct astream_out *)context;

    pthread_mutex_lock(&out->buf_lock);

    while(!out->buf_thread_exit) {
        size_t frames;

        frames = _out_frames_ready_locked(out);
        while (frames && !out->buf_thread_exit) {
            int retries = MAX_WRITE_RETRIES;
            uint64_t now;
            uint32_t elapsed_us;

            while (frames > 0 && !out->buf_thread_exit) {
                int ret;
                uint32_t buffer_duration_us;
                /* PCM format is always 16bit stereo */
                size_t bytes = frames * sizeof(uint32_t);
                if (bytes > out->buffer_size) {
                    bytes = out->buffer_size;
                }

                pthread_mutex_lock(&out->lock);
                if (out->standby) {
                    /* abort and clear all pending frames if standby requested */
                    pthread_mutex_unlock(&out->lock);
                    frames = _out_frames_ready_locked(out);
                    _out_inc_rd_idx_locked(out, frames);
                    goto wait;
                }
                /* indicate to out_standby_stream_locked() that a2dp_write() is active */
                out->write_busy = true;
                pthread_mutex_unlock(&out->lock);
                pthread_mutex_unlock(&out->buf_lock);

                ret = a2dp_write(out->data, out->buf + out->buf_rd_idx, bytes);

                /* clear write_busy condition */
                pthread_mutex_lock(&out->buf_lock);
                pthread_mutex_lock(&out->lock);
                out->write_busy = false;
                pthread_cond_signal(&out->write_cond);
                pthread_mutex_unlock(&out->lock);

                if (ret < 0) {
                    LOGE("%s: a2dp_write failed (%d)\n", __func__, ret);
                    /* skip pending frames in case of write error */
                    _out_inc_rd_idx_locked(out, frames);
                    break;
                } else if (ret == 0) {
                    if (retries-- == 0) {
                        /* skip pending frames in case of multiple time out */
                        _out_inc_rd_idx_locked(out, frames);
                        break;
                    }
                    continue;
                }
                ret /= sizeof(uint32_t);
                _out_inc_rd_idx_locked(out, ret);
                frames -= ret;

                /* XXX: PLEASE FIX ME!!!! */

                /* if A2DP sink runs abnormally fast, sleep a little so that
                 * audioflinger mixer thread does no spin and starve other threads. */
                /* NOTE: It is likely that the A2DP headset is being disconnected */
                now = system_time();
                elapsed_us = (now - out->last_write_time) / 1000UL;
                buffer_duration_us = ((ret * 1000) / out->sample_rate) * 1000;

                if (elapsed_us < (buffer_duration_us / 4)) {
                    LOGV("A2DP sink runs too fast");
                    usleep(buffer_duration_us - elapsed_us);
                }
                out->last_write_time = now;

            }
            frames = _out_frames_ready_locked(out);
        }
wait:
        if (!out->buf_thread_exit) {
            pthread_cond_wait(&out->buf_cond, &out->buf_lock);
        }
    }
    pthread_mutex_unlock(&out->buf_lock);
    return NULL;
}



static int out_add_audio_effect(const struct audio_stream *stream, effect_handle_t effect)
{
    return 0;
}

static int out_remove_audio_effect(const struct audio_stream *stream, effect_handle_t effect)
{
    return 0;
}

static int _out_bt_enable(struct astream_out *out, bool enable)
{
    int ret = 0;

    pthread_mutex_lock(&out->lock);
    out->bt_enabled = enable;
    if (!enable)
        ret = out_close_stream_locked(out);
    pthread_mutex_unlock(&out->lock);

    return ret;
}

static int _out_a2dp_suspend(struct astream_out *out, bool suspend)
{
    pthread_mutex_lock(&out->lock);
    out->suspended = suspend;
    out_standby_stream_locked(out);
    pthread_mutex_unlock(&out->lock);

    return 0;
}

static int adev_open_output_stream(struct audio_hw_device *dev,
                                   uint32_t devices, int *format,
                                   uint32_t *channels, uint32_t *sample_rate,
                                   struct audio_stream_out **stream_out)
{
    struct adev_a2dp *adev = (struct adev_a2dp *)dev;
    struct astream_out *out;
    int ret;

    pthread_mutex_lock(&adev->lock);

    /* one output stream at a time */
    if (adev->output) {
        LOGV("output exists");
        ret = -EBUSY;
        goto err_output_exists;
    }

    out = calloc(1, sizeof(struct astream_out));
    if (!out) {
        ret = -ENOMEM;
        goto err_alloc;
    }

    pthread_mutex_init(&out->lock, NULL);

    out->stream.common.get_sample_rate = out_get_sample_rate;
    out->stream.common.set_sample_rate = out_set_sample_rate;
    out->stream.common.get_buffer_size = out_get_buffer_size;
    out->stream.common.get_channels = out_get_channels;
    out->stream.common.get_format = out_get_format;
    out->stream.common.set_format = out_set_format;
    out->stream.common.standby = out_standby;
    out->stream.common.dump = out_dump;
    out->stream.common.set_parameters = out_set_parameters;
    out->stream.common.get_parameters = out_get_parameters;
    out->stream.common.set_device = out_set_device;
    out->stream.common.get_device = out_get_device;
    out->stream.common.add_audio_effect = out_add_audio_effect;
    out->stream.common.remove_audio_effect = out_remove_audio_effect;
    out->stream.get_latency = out_get_latency;
    out->stream.set_volume = out_set_volume;
    out->stream.write = out_write;
    out->stream.get_render_position = out_get_render_position;

    out->sample_rate = 44100;
    out->buffer_size = 512 * 20;
    out->channels = AUDIO_CHANNEL_OUT_STEREO;
    out->format = AUDIO_FORMAT_PCM_16_BIT;

    out->fd = -1;
    out->device = devices;
    out->bt_enabled = adev->bt_enabled;
    out->suspended = adev->suspended;

    /* for now, buffer_duration_us is precalculated and never changed.
     * if the sample rate or the format ever changes on the fly, we'd have
     * to recalculate this */
    out->buffer_duration_us = ((out->buffer_size * 1000 ) /
                               audio_stream_frame_size(&out->stream.common) /
                               out->sample_rate) * 1000;
    if (!_out_validate_parms(out, format ? *format : 0,
                             channels ? *channels : 0,
                             sample_rate ? *sample_rate : 0)) {
        LOGV("invalid parameters");
        ret = -EINVAL;
        goto err_validate_parms;
    }

    int err = pthread_create(&out->buf_thread, (const pthread_attr_t *) NULL, _out_buf_thread_func, out);
    if (err != 0) {
        goto err_validate_parms;
    }

    /* PCM format is always 16bit, stereo */
    out->buf_size = (out->buffer_size * BUF_NUM_PERIODS) / sizeof(int32_t);
    out->buf = (uint32_t *)malloc(out->buf_size * sizeof(int32_t));
    if (!out->buf) {
        goto err_validate_parms;
    }

    /* XXX: check return code? */
    if (adev->bt_enabled)
        _out_init_locked(out, "00:00:00:00:00:00");

    adev->output = out;

    if (format)
        *format = out->format;
    if (channels)
        *channels = out->channels;
    if (sample_rate)
        *sample_rate = out->sample_rate;

    pthread_mutex_unlock(&adev->lock);

    *stream_out = &out->stream;

    return 0;

err_validate_parms:
    free(out);
err_alloc:
err_output_exists:
    pthread_mutex_unlock(&adev->lock);
    *stream_out = NULL;
    return ret;
}

/* needs the adev->lock held */
static void adev_close_output_stream_locked(struct adev_a2dp *dev,
                                            struct astream_out *stream)
{
    struct adev_a2dp *adev = (struct adev_a2dp *)dev;
    struct astream_out *out = (struct astream_out *)stream;

    /* invalid stream? */
    if (!adev->output || adev->output != out) {
        LOGE("%s: unknown stream %p (ours is %p)", __func__, out, adev->output);
        return;
    }

    pthread_mutex_lock(&out->lock);
    /* out_write() must not be executed from now on */
    out->bt_enabled = false;
    out_close_stream_locked(out);
    pthread_mutex_unlock(&out->lock);
    if (out->buf_thread) {
        pthread_mutex_lock(&out->buf_lock);
        out->buf_thread_exit = true;
        pthread_cond_broadcast(&out->buf_cond);
        pthread_mutex_unlock(&out->buf_lock);
        pthread_join(out->buf_thread, (void **) NULL);
        pthread_cond_destroy(&out->buf_cond);
        pthread_mutex_destroy(&out->buf_lock);
    }
    if (out->buf) {
        free(out->buf);
    }

    adev->output = NULL;
    free(out);
}

static void adev_close_output_stream(struct audio_hw_device *dev,
                                     struct audio_stream_out *stream)
{
    struct adev_a2dp *adev = (struct adev_a2dp *)dev;
    struct astream_out *out = (struct astream_out *)stream;

    pthread_mutex_lock(&adev->lock);
    adev_close_output_stream_locked(adev, out);
    pthread_mutex_unlock(&adev->lock);
}

static int adev_set_parameters(struct audio_hw_device *dev, const char *kvpairs)
{
    struct adev_a2dp *adev = (struct adev_a2dp *)dev;
    struct str_parms *parms;
    char *str;
    char value[8];
    int ret;

    parms = str_parms_create_str(kvpairs);

    pthread_mutex_lock(&adev->lock);

    ret = str_parms_get_str(parms, BLUETOOOTH_ENABLED_PARM, value,
                            sizeof(value));
    if (ret >= 0) {
        adev->bt_enabled = !strcmp(value, "true");
        if (adev->output)
            _out_bt_enable(adev->output, adev->bt_enabled);
    }

    ret = str_parms_get_str(parms, A2DP_SUSPENDED_PARM, value, sizeof(value));
    if (ret >= 0) {
        adev->suspended = !strcmp(value, "true");
        if (adev->output)
            _out_a2dp_suspend(adev->output, adev->suspended);
    }

    pthread_mutex_unlock(&adev->lock);

    str_parms_destroy(parms);

    return ret;
}

static char * adev_get_parameters(const struct audio_hw_device *dev,
                                  const char *keys)
{
    struct adev_a2dp *adev = (struct adev_a2dp *)dev;
    struct str_parms *parms;
    struct str_parms *out_parms;
    char *str;
    char value[8];
    int ret;

    parms = str_parms_create_str(keys);
    out_parms = str_parms_create();

    pthread_mutex_lock(&adev->lock);

    ret = str_parms_get_str(parms, BLUETOOOTH_ENABLED_PARM, value,
                            sizeof(value));
    if (ret >= 0)
        str_parms_add_str(out_parms, BLUETOOOTH_ENABLED_PARM,
                          adev->bt_enabled ? "true" : "false");

    ret = str_parms_get_str(parms, A2DP_SUSPENDED_PARM, value, sizeof(value));
    if (ret >= 0)
        str_parms_add_str(out_parms, A2DP_SUSPENDED_PARM,
                          adev->suspended ? "true" : "false");

    pthread_mutex_unlock(&adev->lock);

    str = str_parms_to_str(out_parms);
    str_parms_destroy(out_parms);
    str_parms_destroy(parms);

    return str;
}

static int adev_init_check(const struct audio_hw_device *dev)
{
    return 0;
}

static int adev_set_voice_volume(struct audio_hw_device *dev, float volume)
{
    return -ENOSYS;
}

static int adev_set_master_volume(struct audio_hw_device *dev, float volume)
{
    return -ENOSYS;
}

static int adev_set_mode(struct audio_hw_device *dev, int mode)
{
    /* TODO: do we care for the mode? */
    return 0;
}

static int adev_set_mic_mute(struct audio_hw_device *dev, bool state)
{
    return -ENOSYS;
}

static int adev_get_mic_mute(const struct audio_hw_device *dev, bool *state)
{
    return -ENOSYS;
}

static size_t adev_get_input_buffer_size(const struct audio_hw_device *dev,
                                         uint32_t sample_rate, int format,
                                         int channel_count)
{
    /* no input */
    return 0;
}

static int adev_open_input_stream(struct audio_hw_device *dev, uint32_t devices,
                                  int *format, uint32_t *channels,
                                  uint32_t *sample_rate,
                                  audio_in_acoustics_t acoustics,
                                  struct audio_stream_in **stream_in)
{
    return -ENOSYS;
}

static void adev_close_input_stream(struct audio_hw_device *dev,
                                   struct audio_stream_in *in)
{
    return;
}

static int adev_dump(const audio_hw_device_t *device, int fd)
{
    return 0;
}

static int adev_close(hw_device_t *device)
{
    struct adev_a2dp *adev = (struct adev_a2dp *)device;

    pthread_mutex_lock(&adev->lock);
    if (adev->output)
        adev_close_output_stream_locked(adev, adev->output);
    pthread_mutex_unlock(&adev->lock);
    free(adev);

    return 0;
}

static uint32_t adev_get_supported_devices(const struct audio_hw_device *dev)
{
    return AUDIO_DEVICE_OUT_ALL_A2DP;
}

static int adev_open(const hw_module_t* module, const char* name,
                     hw_device_t** device)
{
    struct adev_a2dp *adev;
    int ret;

    if (strcmp(name, AUDIO_HARDWARE_INTERFACE) != 0)
        return -EINVAL;

    adev = calloc(1, sizeof(struct adev_a2dp));
    if (!adev)
        return -ENOMEM;

    adev->bt_enabled = true;
    adev->suspended = false;
    pthread_mutex_init(&adev->lock, NULL);
    adev->output = NULL;

    adev->device.common.tag = HARDWARE_DEVICE_TAG;
    adev->device.common.version = 0;
    adev->device.common.module = (struct hw_module_t *) module;
    adev->device.common.close = adev_close;

    adev->device.get_supported_devices = adev_get_supported_devices;
    adev->device.init_check = adev_init_check;
    adev->device.set_voice_volume = adev_set_voice_volume;
    adev->device.set_master_volume = adev_set_master_volume;
    adev->device.set_mode = adev_set_mode;
    adev->device.set_mic_mute = adev_set_mic_mute;
    adev->device.get_mic_mute = adev_get_mic_mute;
    adev->device.set_parameters = adev_set_parameters;
    adev->device.get_parameters = adev_get_parameters;
    adev->device.get_input_buffer_size = adev_get_input_buffer_size;
    adev->device.open_output_stream = adev_open_output_stream;
    adev->device.close_output_stream = adev_close_output_stream;
    adev->device.open_input_stream = adev_open_input_stream;
    adev->device.close_input_stream = adev_close_input_stream;
    adev->device.dump = adev_dump;

    *device = &adev->device.common;

    return 0;

err_str_parms_create:
    free(adev);
    return ret;
}

static struct hw_module_methods_t hal_module_methods = {
    .open = adev_open,
};

struct audio_module HAL_MODULE_INFO_SYM = {
    .common = {
        .tag = HARDWARE_MODULE_TAG,
        .version_major = 1,
        .version_minor = 0,
        .id = AUDIO_HARDWARE_MODULE_ID,
        .name = "A2DP Audio HW HAL",
        .author = "The Android Open Source Project",
        .methods = &hal_module_methods,
    },
};


