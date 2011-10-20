/*
 * Copyright (C) 2009 Google, Inc.
 * Copyright (c) 2010, Code Aurora Forum. All rights reserved.
 * Author: Brian Swetland <swetland@google.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/fs.h>
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/uaccess.h>
#include <linux/wait.h>
#include <linux/msm_audio.h>
#include <asm/atomic.h>
#include <mach/debug_mm.h>
#include "apr_audio.h"
#include "q6asm.h"

#define MAX_BUF 2
#define BUFSZ (4800)

struct pcm {
	struct mutex lock;
	struct mutex write_lock;
	spinlock_t   dsp_lock;
	wait_queue_head_t write_wait;
	struct audio_client *ac;
	uint32_t sample_rate;
	uint32_t channel_count;
	uint32_t buffer_size;
	uint32_t buffer_count;
	uint32_t rec_mode;
	atomic_t out_count;
	atomic_t out_enabled;
	atomic_t out_stopped;
	atomic_t out_prefill;
};

void pcm_out_cb(uint32_t opcode, uint32_t token,
			uint32_t *payload, void *priv)
{
	struct pcm *pcm = (struct pcm *) priv;
	unsigned long flags;

	spin_lock_irqsave(&pcm->dsp_lock, flags);
	switch (opcode) {
	case ASM_DATA_EVENT_WRITE_DONE:
		atomic_inc(&pcm->out_count);
		wake_up(&pcm->write_wait);
		break;
	default:
		break;
	}
	spin_unlock_irqrestore(&pcm->dsp_lock, flags);
}

static int pcm_out_enable(struct pcm *pcm)
{
	if (atomic_read(&pcm->out_enabled))
		return 0;
	return q6asm_run(pcm->ac, 0, 0, 0);
}

static int pcm_out_disable(struct pcm *pcm)
{
	int rc = 0;

	if (atomic_read(&pcm->out_enabled)) {
		atomic_set(&pcm->out_enabled, 0);
		rc = q6asm_cmd(pcm->ac, CMD_CLOSE);

		atomic_set(&pcm->out_stopped, 1);
		wake_up(&pcm->write_wait);
	}
	return rc;
}

static int config(struct pcm *pcm)
{
	int rc = 0;
	if (!atomic_read(&pcm->out_prefill)) {
		pr_debug("%s: pcm prefill\n", __func__);
		rc = q6asm_audio_client_buf_alloc(IN, pcm->ac,
				pcm->buffer_size, pcm->buffer_count);
		if (rc < 0) {
			pr_err("Audio Start: Buffer Allocation failed \
							rc = %d\n", rc);
			goto fail;
		}

		rc = q6asm_media_format_block_pcm(pcm->ac, pcm->sample_rate,
							pcm->channel_count);
		if (rc < 0)
			pr_err("%s: CMD Format block failed\n", __func__);

		atomic_set(&pcm->out_prefill, 1);
		atomic_set(&pcm->out_count, pcm->buffer_count);
	}
fail:
	return rc;
}

static long pcm_out_ioctl(struct file *file, unsigned int cmd,
				unsigned long arg)
{
	struct pcm *pcm = file->private_data;
	int rc = 0;

	if (cmd == AUDIO_GET_STATS) {
		struct msm_audio_stats stats;
		memset(&stats, 0, sizeof(stats));
		if (copy_to_user((void *) arg, &stats, sizeof(stats)))
			return -EFAULT;
		return 0;
	}

	mutex_lock(&pcm->lock);
	switch (cmd) {
	case AUDIO_SET_VOLUME: {
		int vol;
		if (copy_from_user(&vol, (void *) arg, sizeof(vol))) {
			rc = -EFAULT;
			break;
		}
		break;
	}
	case AUDIO_START: {
		pr_info("%s: AUDIO_START\n", __func__);
		rc = config(pcm);
		if (rc) {
			pr_err("%s: Out Configuration failed\n", __func__);
			rc = -EFAULT;
			break;
		}

		rc = pcm_out_enable(pcm);
		if (rc) {
			pr_err("Out enable failed\n");
			rc = -EFAULT;
			break;
		}
		atomic_set(&pcm->out_enabled, 1);
		break;
	}
	case AUDIO_GET_SESSION_ID: {
		if (copy_to_user((void *) arg, &pcm->ac->session,
					sizeof(unsigned short)))
			rc = -EFAULT;
		break;
	}
	case AUDIO_STOP:
		break;
	case AUDIO_FLUSH:
		break;
	case AUDIO_SET_CONFIG: {
		struct msm_audio_config config;
		pr_info("AUDIO_SET_CONFIG\n");
		if (copy_from_user(&config, (void *) arg, sizeof(config))) {
			rc = -EFAULT;
			break;
		}
		if (config.channel_count < 1 || config.channel_count > 2) {
			rc = -EINVAL;
			break;
		}
		if (config.sample_rate < 8000 || config.sample_rate > 48000) {
			rc = -EINVAL;
			break;
		}
		if (config.buffer_size < 128) {
			rc = -EINVAL;
			break;
		}
		pcm->sample_rate = config.sample_rate;
		pcm->channel_count = config.channel_count;
		pcm->buffer_size = config.buffer_size;
		pcm->buffer_count = config.buffer_count;
		break;
	}
	case AUDIO_GET_CONFIG: {
		struct msm_audio_config config;
		pr_info("AUDIO_GET_CONFIG\n");
		config.buffer_size = pcm->buffer_size;
		config.buffer_count = pcm->buffer_count;
		config.sample_rate = pcm->sample_rate;
		config.channel_count = pcm->channel_count;
		config.unused[0] = 0;
		config.unused[1] = 0;
		config.unused[2] = 0;
		if (copy_to_user((void *) arg, &config, sizeof(config)))
			rc = -EFAULT;
		break;
	}
	case AUDIO_SET_EQ: {
		struct msm_audio_eq_stream_config eq_config;
		if (copy_from_user(&eq_config, (void *) arg,
						sizeof(eq_config))) {
			rc = -EFAULT;
			break;
		}
		break;
	}
	default:
		rc = -EINVAL;
	}
	mutex_unlock(&pcm->lock);
	return rc;
}

static int pcm_out_open(struct inode *inode, struct file *file)
{
	struct pcm *pcm;
	int rc = 0;
	pr_info("[%s:%s] open\n", __MM_FILE__, __func__);
	pcm = kzalloc(sizeof(struct pcm), GFP_KERNEL);
	if (!pcm) {
		pr_info("%s: Failed to allocated memory\n", __func__);
		return -ENOMEM;
	}

	pcm->channel_count = 2;
	pcm->sample_rate = 44100;
	pcm->buffer_size = BUFSZ;
	pcm->buffer_count = MAX_BUF;

	pcm->ac = q6asm_audio_client_alloc((app_cb)pcm_out_cb, (void *)pcm);
	if (!pcm->ac) {
		pr_err("%s: Could not allocate memory\n", __func__);
		rc = -ENOMEM;
		goto fail;
	}

	rc = q6asm_open_write(pcm->ac, FORMAT_LINEAR_PCM);
	if (rc < 0) {
		pr_info("%s: pcm out open failed\n", __func__);
		rc = -EINVAL;
		goto fail;
	}

	mutex_init(&pcm->lock);
	mutex_init(&pcm->write_lock);
	init_waitqueue_head(&pcm->write_wait);
	spin_lock_init(&pcm->dsp_lock);
	atomic_set(&pcm->out_enabled, 0);
	atomic_set(&pcm->out_stopped, 0);
	atomic_set(&pcm->out_count, pcm->buffer_count);
	atomic_set(&pcm->out_prefill, 0);
	file->private_data = pcm;
	return 0;
fail:
	if (pcm->ac)
		q6asm_audio_client_free(pcm->ac);
	kfree(pcm);
	return rc;
}

static ssize_t pcm_out_write(struct file *file, const char __user *buf,
					size_t count, loff_t *pos)
{
	struct pcm *pcm = file->private_data;
	const char __user *start = buf;
	int xfer;
	char *bufptr;
	uint32_t idx;
	void *data;
	int rc = 0;
	uint32_t size;

	if (!pcm->ac)
		return -ENODEV;

	if (!atomic_read(&pcm->out_enabled)) {
		rc = config(pcm);
		if (rc < 0)
			return rc;
	}

	mutex_lock(&pcm->write_lock);
	while (count > 0) {
		rc = wait_event_timeout(pcm->write_wait,
				(atomic_read(&pcm->out_count) ||
				atomic_read(&pcm->out_stopped)), 5 * HZ);
		if (rc < 0) {
			pr_info("%s: wait_event_timeout failed\n", __func__);
			goto fail;
		}

		if (atomic_read(&pcm->out_stopped) &&
					!atomic_read(&pcm->out_count)) {
			pr_info("%s: pcm stopped out_count 0\n", __func__);
			mutex_unlock(&pcm->write_lock);
			return 0;
		}

		data = q6asm_is_cpu_buf_avail(IN, pcm->ac, &size, &idx);
		bufptr = data;
		if (bufptr) {
			xfer = count;
			if (xfer > BUFSZ)
				xfer = BUFSZ;

			if (copy_from_user(bufptr, buf, xfer)) {
				rc = -EFAULT;
				goto fail;
			}
			buf += xfer;
			count -= xfer;
			rc = q6asm_write(pcm->ac, xfer, 0, 0, 0);
			wmb();
			if (rc < 0) {
				rc = -EFAULT;
				goto fail;
			}
		}
		atomic_dec(&pcm->out_count);
	}

	rc = buf - start;
fail:
	mutex_unlock(&pcm->write_lock);
	return rc;
}

static int pcm_out_release(struct inode *inode, struct file *file)
{
	struct pcm *pcm = file->private_data;
	if (pcm->ac)
		pcm_out_disable(pcm);
	q6asm_audio_client_free(pcm->ac);
	kfree(pcm);
	pr_info("[%s:%s] release\n", __MM_FILE__, __func__);
	return 0;
}

static const struct file_operations pcm_out_fops = {
	.owner		= THIS_MODULE,
	.open		= pcm_out_open,
	.write		= pcm_out_write,
	.release	= pcm_out_release,
	.unlocked_ioctl	= pcm_out_ioctl,
};

struct miscdevice pcm_out_misc = {
	.minor	= MISC_DYNAMIC_MINOR,
	.name	= "msm_pcm_out",
	.fops	= &pcm_out_fops,
};

static int __init pcm_out_init(void)
{
	return misc_register(&pcm_out_misc);
}

device_initcall(pcm_out_init);
