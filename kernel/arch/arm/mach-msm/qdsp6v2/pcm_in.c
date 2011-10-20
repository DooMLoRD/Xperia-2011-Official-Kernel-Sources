/*
 * Copyright (C) 2009 Google, Inc.
 * Copyright (C) 2009 HTC Corporation
 * Copyright (c) 2010, Code Aurora Forum. All rights reserved.
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
#include <linux/spinlock.h>
#include <linux/wait.h>
#include <linux/msm_audio.h>
#include <asm/atomic.h>
#include <mach/debug_mm.h>
#include "apr_audio.h"
#include "q6asm.h"

#define MAX_BUF 2
#define BUFSZ (480 * 8)

struct pcm {
	struct mutex lock;
	struct mutex read_lock;
	wait_queue_head_t wait;
	spinlock_t dsp_lock;
	struct audio_client *ac;
	uint32_t sample_rate;
	uint32_t channel_count;
	uint32_t buffer_size;
	uint32_t buffer_count;
	uint32_t rec_mode;
	uint32_t in_frame_info[MAX_BUF][2];
	atomic_t in_count;
	atomic_t in_enabled;
	atomic_t in_stopped;
};

static void pcm_in_get_dsp_buffers(struct pcm*,
				uint32_t token, uint32_t *payload);

void pcm_in_cb(uint32_t opcode, uint32_t token,
		uint32_t *payload, void *priv)
{
	struct pcm *pcm = (struct pcm *) priv;
	unsigned long flags;

	spin_lock_irqsave(&pcm->dsp_lock, flags);
	switch (opcode) {
	case ASM_DATA_EVENT_READ_DONE:
		pcm_in_get_dsp_buffers(pcm, token, payload);
		break;
	default:
		break;
	}
	spin_unlock_irqrestore(&pcm->dsp_lock, flags);
}

static void pcm_in_get_dsp_buffers(struct pcm *pcm,
				uint32_t token, uint32_t *payload)
{
	pcm->in_frame_info[token][0] = payload[7];
	pcm->in_frame_info[token][1] = payload[3];
	if (atomic_read(&pcm->in_count) <= pcm->buffer_count)
		atomic_inc(&pcm->in_count);
	wake_up(&pcm->wait);
}

static int pcm_in_enable(struct pcm *pcm)
{
	if (atomic_read(&pcm->in_enabled))
		return 0;
	return q6asm_run(pcm->ac, 0, 0, 0);
}

static int pcm_in_disable(struct pcm *pcm)
{
	int rc = 0;

	if (atomic_read(&pcm->in_enabled)) {
		atomic_set(&pcm->in_enabled, 0);
		rc = q6asm_cmd(pcm->ac, CMD_CLOSE);

		atomic_set(&pcm->in_stopped, 1);
		memset(pcm->in_frame_info, 0,
				sizeof(char) * pcm->buffer_count * 2);
		wake_up(&pcm->wait);
	}
	return rc;
}

static int config(struct pcm *pcm)
{
	int rc = 0;

	pr_debug("%s: pcm prefill\n", __func__);
	rc = q6asm_audio_client_buf_alloc(OUT, pcm->ac,
				pcm->buffer_size, pcm->buffer_count);
	if (rc < 0) {
		pr_err("Audio Start: Buffer Allocation failed \
						rc = %d\n", rc);
		goto fail;
	}

	rc = q6asm_enc_cfg_blk_pcm(pcm->ac, pcm->sample_rate,
						pcm->channel_count);
	if (rc < 0) {
		pr_err("%s: cmd media format block failed", __func__);
		goto fail;
	}
fail:
	return rc;
}

static long pcm_in_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct pcm *pcm = file->private_data;
	int rc = 0;

	mutex_lock(&pcm->lock);
	switch (cmd) {
	case AUDIO_SET_VOLUME:
		break;
	case AUDIO_GET_STATS: {
		struct msm_audio_stats stats;
		memset(&stats, 0, sizeof(stats));
		if (copy_to_user((void *) arg, &stats, sizeof(stats)))
			rc = -EFAULT;
		break;
	}
	case AUDIO_START: {
		int cnt = 0;

		rc = config(pcm);
		if (rc) {
			pr_err("%s: IN Configuration failed\n", __func__);
			rc = -EFAULT;
			break;
		}

		rc = pcm_in_enable(pcm);
		if (rc) {
			pr_err("%s: In Enable failed\n", __func__);
			rc = -EFAULT;
			break;
		}

		atomic_set(&pcm->in_enabled, 1);

		while (cnt++ < pcm->buffer_count)
			q6asm_read(pcm->ac);
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

		if (copy_from_user(&config, (void *) arg, sizeof(config))) {
			rc = -EFAULT;
			break;
		}

		if (!config.channel_count || config.channel_count > 2) {
			rc = -EINVAL;
			break;
		}

		if (config.sample_rate < 8000 || config.sample_rate > 48000) {
			rc = -EINVAL;
			break;
		}

		if (config.buffer_size < 480 || config.buffer_size % 480) {
			pr_err("%s: Buffer Size should be multiple of 480\n",
								__func__);
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
	default:
		rc = -EINVAL;
		break;
	}
	mutex_unlock(&pcm->lock);
	return rc;
}

static int pcm_in_open(struct inode *inode, struct file *file)
{
	struct pcm *pcm;
	int rc = 0;

	pcm = kzalloc(sizeof(struct pcm), GFP_KERNEL);
	if (!pcm)
		return -ENOMEM;

	pcm->channel_count = 1;
	pcm->sample_rate = 8000;
	pcm->buffer_size = BUFSZ;
	pcm->buffer_count = MAX_BUF;

	pcm->ac = q6asm_audio_client_alloc((app_cb)pcm_in_cb, (void *)pcm);
	if (!pcm->ac) {
		pr_err("%s: Could not allocate memory\n", __func__);
		rc = -ENOMEM;
		goto fail;
	}

	mutex_init(&pcm->lock);
	mutex_init(&pcm->read_lock);
	spin_lock_init(&pcm->dsp_lock);
	init_waitqueue_head(&pcm->wait);

	rc = q6asm_open_read(pcm->ac, FORMAT_LINEAR_PCM);
	if (rc < 0) {
		pr_err("%s: Cmd Open Failed\n", __func__);
		goto fail;
	}

	atomic_set(&pcm->in_stopped, 0);
	atomic_set(&pcm->in_enabled, 0);
	atomic_set(&pcm->in_count, 0);
	file->private_data = pcm;
	return 0;
fail:
	if (pcm->ac)
		q6asm_audio_client_free(pcm->ac);
	kfree(pcm);
	return rc;
}

static ssize_t pcm_in_read(struct file *file, char __user *buf,
			  size_t count, loff_t *pos)
{
	struct pcm *pcm = file->private_data;
	const char __user *start = buf;
	void *data;
	uint32_t offset = 0;
	uint32_t size = 0;
	uint32_t idx;
	int rc = 0;

	mutex_lock(&pcm->read_lock);
	while (count > 0) {
		rc = wait_event_timeout(pcm->wait,
				(atomic_read(&pcm->in_count) ||
				atomic_read(&pcm->in_stopped)), 5 * HZ);
		if (rc < 0) {
			pr_err("%s: wait_event_timeout failed\n", __func__);
			goto fail;
		}

		if (atomic_read(&pcm->in_stopped) &&
					!atomic_read(&pcm->in_count)) {
			mutex_unlock(&pcm->read_lock);
			return 0;
		}

		data = q6asm_is_cpu_buf_avail(OUT, pcm->ac, &size, &idx);
		if ((count >= size) && data) {
			offset = pcm->in_frame_info[idx][1];
			if (copy_to_user(buf, data+offset, size)) {
				pr_err("%s copy_to_user failed\n", __func__);
				rc = -EFAULT;
				goto fail;
			}

			count -= size;
			buf += size;
			atomic_dec(&pcm->in_count);
			memset(&pcm->in_frame_info[idx], 0,
						sizeof(uint32_t) * 2);

			rc = q6asm_read(pcm->ac);
			if (rc < 0) {
				pr_err("%s q6asm_read faile\n", __func__);
				goto fail;
			}
			rmb();
			break;
		} else {
			pr_err("%s: short read data[%p] size[%d]\n",\
						__func__, data, size);
			break;
		}
	}
	rc = buf-start;
fail:
	mutex_unlock(&pcm->read_lock);
	return rc;
}

static int pcm_in_release(struct inode *inode, struct file *file)
{
	int rc = 0;
	struct pcm *pcm = file->private_data;

	if (atomic_read(&pcm->in_enabled))
		rc = pcm_in_disable(pcm);

	q6asm_audio_client_free(pcm->ac);
	kfree(pcm);
	pr_info("[%s:%s] release\n", __MM_FILE__, __func__);
	return rc;
}

static const struct file_operations pcm_in_fops = {
	.owner		= THIS_MODULE,
	.open		= pcm_in_open,
	.read		= pcm_in_read,
	.release	= pcm_in_release,
	.unlocked_ioctl	= pcm_in_ioctl,
};

struct miscdevice pcm_in_misc = {
	.minor	= MISC_DYNAMIC_MINOR,
	.name	= "msm_pcm_in",
	.fops	= &pcm_in_fops,
};

static int __init pcm_in_init(void)
{
	return misc_register(&pcm_in_misc);
}

device_initcall(pcm_in_init);
