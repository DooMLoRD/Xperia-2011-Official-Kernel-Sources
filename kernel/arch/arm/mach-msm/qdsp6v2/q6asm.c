
/*
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
#include <linux/mutex.h>
#include <linux/wait.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/sched.h>
#include <linux/dma-mapping.h>
#include <linux/miscdevice.h>
#include <linux/delay.h>
#include <linux/spinlock.h>
#include <mach/debug_mm.h>
#include <mach/peripheral-loader.h>
#include <asm/atomic.h>
#include <asm/ioctls.h>
#include "q6asm.h"
#include "apr_audio.h"

#define SESSION_MAX 0x08
#define TRUE        0x01
#define FALSE       0x00
#define READDONE_IDX_STATUS 0
#define READDONE_IDX_BUFFER 1
#define READDONE_IDX_SIZE 2
#define READDONE_IDX_OFFSET 3
#define READDONE_IDX_MSW_TS 4
#define READDONE_IDX_LSW_TS 5
#define READDONE_IDX_FLAGS 6
#define READDONE_IDX_NUMFRAMES 7
#define READDONE_IDX_ID 8

static DEFINE_MUTEX(session_lock);

/* session id: 0 reserved */
static struct audio_client *session[SESSION_MAX+1];
static int32_t q6asm_mmapcallback(struct apr_client_data *data, void *priv);
static int32_t q6asm_callback(struct apr_client_data *data, void *priv);
static void q6asm_add_hdr(struct audio_client *ac, struct apr_hdr *hdr,
			uint32_t pkt_size, uint32_t cmd_flg);

static void q6asm_reset_buf_state(struct audio_client *ac);

struct asm_mmap {
	atomic_t ref_cnt;
	atomic_t cmd_state;
	wait_queue_head_t cmd_wait;
	void *apr;
};

static struct asm_mmap this_mmap;

static int q6asm_session_alloc(struct audio_client *ac)
{
	int n;
	mutex_lock(&session_lock);
	for (n = 1; n <= SESSION_MAX; n++) {
		if (!session[n]) {
			session[n] = ac;
			mutex_unlock(&session_lock);
			return n;
		}
	}
	mutex_unlock(&session_lock);
	return -ENOMEM;
}

static void q6asm_session_free(struct audio_client *ac)
{
	pr_debug("sessionid[%d]\n", ac->session);
	mutex_lock(&session_lock);
	session[ac->session] = 0;
	mutex_unlock(&session_lock);
	return;
}

int q6asm_audio_client_buf_free(unsigned int dir,
			struct audio_client *ac)
{
	struct audio_port_data *port;
	int cnt = 0;
	int rc = 0;

	pr_debug("\n");
	mutex_lock(&ac->cmd_lock);
	port = &ac->port[dir];
	if (!port->buf) {
		mutex_unlock(&ac->cmd_lock);
		return 0;
	}
	cnt = port->max_buf_cnt - 1;
	pr_debug("numofbuf[%d] dir[%d]", (cnt+1), dir);

	if (cnt >= 0) {
		rc = q6asm_memory_unmap_regions(ac, dir,
					port->buf[0].size, port->max_buf_cnt);
		if (rc < 0)
			pr_debug("CMD Memory_unmap_regions failed\n");
	}

	while (cnt >= 0) {
		if (port->buf[cnt].data) {
			pr_debug("data[%p]phys[%p][%p] cnt[%d]\n",
				(void *)port->buf[cnt].data,
				(void *)port->buf[cnt].phys,
				(void *)&port->buf[cnt].phys, cnt);
			dma_free_coherent(NULL,
					port->buf[cnt].size,
					port->buf[cnt].data,
					port->buf[cnt].phys);
			port->buf[cnt].data = NULL;
			port->buf[cnt].phys = 0;
			--(port->max_buf_cnt);
		}
		--cnt;
	}
	kfree(port->buf);
	port->buf = NULL;
	mutex_unlock(&ac->cmd_lock);
	return 0;
}

void q6asm_audio_client_free(struct audio_client *ac)
{
	int loopcnt;
	struct audio_port_data *port;
	if (!ac || !ac->session)
		return;

	pr_debug("\n");

	for (loopcnt = 0; loopcnt <= OUT; loopcnt++) {
		port = &ac->port[loopcnt];
		if (!port->buf)
			continue;
		pr_debug("loopcnt = %d\n", loopcnt);
		q6asm_audio_client_buf_free(loopcnt, ac);
	}
	q6asm_session_free(ac);
	apr_deregister(ac->apr);

	pr_debug("APR De-Register\n");

	if (atomic_read(&this_mmap.ref_cnt) <= 0) {
		pr_err("APR Common Port Already Closed\n");
		goto done;
	}

	atomic_dec(&this_mmap.ref_cnt);
	if (atomic_read(&this_mmap.ref_cnt) == 0) {
		apr_deregister(this_mmap.apr);
		pr_debug("APR De-Register common port\n");
	}
done:
	kfree(ac);
	return;
}

struct audio_client *q6asm_audio_client_alloc(app_cb cb, void *priv)
{
	struct audio_client *ac;
	int n;
	int lcnt = 0;

	ac = kzalloc(sizeof(struct audio_client), GFP_KERNEL);
	if (!ac)
		return NULL;
	n = q6asm_session_alloc(ac);
	if (n <= 0)
		goto fail_session;
	ac->session = n;
	ac->cb = cb;
	ac->priv = priv;
	ac->apr = apr_register("ADSP", "ASM", \
				(apr_fn)q6asm_callback,\
				((ac->session) << 8 | 0x0001),\
				ac);

	if (ac->apr == NULL) {
		pr_err("Registration with APR failed\n");
			goto fail;
	}
	pr_debug("Registering the common port with APR\n");
	if (atomic_read(&this_mmap.ref_cnt) == 0) {
		this_mmap.apr = apr_register("ADSP", "ASM", \
					(apr_fn)q6asm_mmapcallback,\
					0x0FFFFFFFF, &this_mmap);
		if (this_mmap.apr == NULL) {
			pr_debug("Unable to register \
				APR ASM common port \n");
			goto fail;
		}
	}

	atomic_inc(&this_mmap.ref_cnt);
	init_waitqueue_head(&ac->cmd_wait);
	mutex_init(&ac->cmd_lock);
	for (lcnt = 0; lcnt <= OUT; lcnt++) {
		mutex_init(&ac->port[lcnt].lock);
		spin_lock_init(&ac->port[lcnt].dsp_lock);
	}
	atomic_set(&ac->cmd_state, 0);

	pr_debug("session[%d]\n", ac->session);

	return ac;
fail:
	q6asm_audio_client_free(ac);
	return NULL;
fail_session:
	kfree(ac);
	return NULL;
}

int q6asm_audio_client_buf_alloc(unsigned int dir,
			struct audio_client *ac,
			unsigned int bufsz,
			unsigned int bufcnt)
{
	int cnt = 0;
	int rc = 0;
	struct audio_buffer *buf;

	pr_debug("bufsz[%d]bufcnt[%d]\n", bufsz, bufcnt);

	if (!(ac) || ((dir != IN) && (dir != OUT)))
		return -EINVAL;

	if (ac->session <= 0 || ac->session > 8)
		goto fail;

	if (ac->port[dir].buf) {
		pr_debug("buffer already allocated\n");
		return 0;
	}
	mutex_lock(&ac->cmd_lock);
	buf = kzalloc(((sizeof(struct audio_buffer))*bufcnt), GFP_KERNEL);

	if (!buf) {
		mutex_unlock(&ac->cmd_lock);
		goto fail;
	}

	ac->port[dir].buf = buf;

	while (cnt < bufcnt) {
		if (bufsz > 0) {
			buf[cnt].data = dma_alloc_coherent(NULL, bufsz,
						&buf[cnt].phys, GFP_KERNEL);
			if (!buf[cnt].data) {
				pr_err("Buf alloc failed\n");
				mutex_unlock(&ac->cmd_lock);
				goto fail;
			}
			buf[cnt].used = 1;
			buf[cnt].size = bufsz;
			buf[cnt].actual_size = bufsz;
			pr_debug("data[%p]phys[%p][%p]\n",
					(void *)buf[cnt].data,
					(void *)buf[cnt].phys,
					(void *)&buf[cnt].phys);
		}
		cnt++;
	}
	ac->port[dir].max_buf_cnt = cnt;

	rc = q6asm_memory_map_regions(ac, dir, bufsz, cnt);
	if (rc < 0) {
		pr_err("CMD Memory_map_regions failed\n");
		mutex_unlock(&ac->cmd_lock);
		goto fail;
	}
	mutex_unlock(&ac->cmd_lock);
	return 0;
fail:
	q6asm_audio_client_buf_free(dir, ac);
	return -EINVAL;
}

static int32_t q6asm_mmapcallback(struct apr_client_data *data, void *priv)
{
	uint32_t token;
	uint32_t *payload = data->payload;

	pr_debug("ptr0[0x%x]ptr1[0x%x]opcode[0x%x]"
		"token[0x%x]payload_s[%d] src[%d] dest[%d]\n",
		payload[0], payload[1], data->opcode, data->token,
		data->payload_size, data->src_port, data->dest_port);

	if (data->opcode == APR_BASIC_RSP_RESULT) {
		token = data->token;
		switch (payload[0]) {
		case ASM_SESSION_CMD_MEMORY_MAP:
		case ASM_SESSION_CMD_MEMORY_UNMAP:
		case ASM_SESSION_CMD_MEMORY_MAP_REGIONS:
		case ASM_SESSION_CMD_MEMORY_UNMAP_REGIONS:
			pr_debug("command[0x%x]success [0x%x]",
						payload[0], payload[1]);
			if (atomic_read(&this_mmap.cmd_state)) {
				atomic_set(&this_mmap.cmd_state, 0);
				wake_up(&this_mmap.cmd_wait);
			}
			break;
		default:
			pr_debug("command[0x%x] not expecting rsp", payload[0]);
			break;
		}
	}
	return 0;
}


static int32_t q6asm_callback(struct apr_client_data *data, void *priv)
{
	int i = 0;
	struct audio_client *ac = (struct audio_client *)priv;
	uint32_t token;
	unsigned long dsp_flags;
	uint32_t *payload = data->payload;

	pr_debug("session[%d]ptr0[0x%x]ptr1[0x%x]opcode[0x%x] "
		"token[0x%x]payload_s[%d] src[%d] dest[%d]\n",
		ac->session, payload[0], payload[1], data->opcode,
		data->token, data->payload_size, data->src_port,
		data->dest_port);

	if (ac == NULL) {
		pr_err("ac NULL\n");
		return -EINVAL;
	}

	if (data->opcode == APR_BASIC_RSP_RESULT) {
		token = data->token;
		switch (payload[0]) {
		case ASM_SESSION_CMD_PAUSE:
		case ASM_DATA_CMD_EOS:
		case ASM_STREAM_CMD_CLOSE:
		case ASM_STREAM_CMD_FLUSH:
		case ASM_SESSION_CMD_RUN:
		case ASM_SESSION_CMD_REGISTER_FOR_TX_OVERFLOW_EVENTS:
		if (token != ac->session) {
			pr_err("Invalid session[%d] rxed expected[%d]",
					token, ac->session);
			break;
		}
		case ASM_STREAM_CMD_OPEN_READ:
		case ASM_STREAM_CMD_OPEN_WRITE:
		case ASM_STREAM_CMD_OPEN_READWRITE:
		case ASM_DATA_CMD_MEDIA_FORMAT_UPDATE:
		case ASM_STREAM_CMD_SET_ENCDEC_PARAM:
			pr_debug("command[0x%x]success [0x%x]",
						payload[0], payload[1]);
			if (atomic_read(&ac->cmd_state)) {
				atomic_set(&ac->cmd_state, 0);
				wake_up(&ac->cmd_wait);
			}
			if (ac->cb)
				ac->cb(data->opcode, data->token,
					(uint32_t *)data->payload, ac->priv);
			break;
		default:
			pr_debug("command[0x%x] not expecting rsp", payload[0]);
			break;
		}
		return 0;
	}

	switch (data->opcode) {
	case ASM_DATA_EVENT_WRITE_DONE:{
		struct audio_port_data *port = &ac->port[IN];
		pr_debug("Rxed opcode[0x%x] status[0x%x] token[%d]",
				payload[0], payload[1], data->token);
		spin_lock_irqsave(&port->dsp_lock, dsp_flags);
		token = data->token;
		port->buf[token].used = 1;
		spin_unlock_irqrestore(&port->dsp_lock, dsp_flags);
		for (i = 0; i < port->max_buf_cnt; i++)
			pr_debug("%d ", port->buf[i].used);

		break;
	}
	case ASM_DATA_EVENT_READ_DONE:{

		struct audio_port_data *port = &ac->port[OUT];

		pr_debug("R-D: status=%d buff_add=%x act_size=%d offset=%d\n",
				payload[READDONE_IDX_STATUS],
				payload[READDONE_IDX_BUFFER],
				payload[READDONE_IDX_SIZE],
				payload[READDONE_IDX_OFFSET]);
		pr_debug("R-D:msw_ts=%d lsw_ts=%d flags=%d id=%d num=%d\n",
				payload[READDONE_IDX_MSW_TS],
				payload[READDONE_IDX_LSW_TS],
				payload[READDONE_IDX_FLAGS],
				payload[READDONE_IDX_ID],
				payload[READDONE_IDX_NUMFRAMES]);

		spin_lock_irqsave(&port->dsp_lock, dsp_flags);
		token = data->token;
		if (port->buf[token].phys != payload[READDONE_IDX_BUFFER]) {
			pr_err("Buf expected[%p]rxed[%p]\n",\
				(void *)port->buf[token].phys,\
				(void *)payload[READDONE_IDX_BUFFER]);
			spin_unlock_irqrestore(&port->dsp_lock, dsp_flags);
			break;
		}
		port->buf[token].actual_size = payload[READDONE_IDX_SIZE];
		spin_unlock_irqrestore(&port->dsp_lock, dsp_flags);
		break;
	}
	case ASM_DATA_EVENT_EOS:
	case ASM_DATA_CMDRSP_EOS:
		pr_debug("rxed opcode[0x%x]eos[%d]",
			data->opcode, atomic_read(&ac->eos_state));

		if (atomic_read(&ac->eos_state)) {
			atomic_set(&ac->eos_state, 0);
			wake_up(&ac->cmd_wait);
		} else
			return 0;
		break;
	case ASM_STREAM_CMDRSP_GET_ENCDEC_PARAM:
		break;
	case ASM_STREAM_CMDRSP_GET_PP_PARAMS:
		break;
	case ASM_SESSION_EVENT_TX_OVERFLOW:
		pr_err("ASM_SESSION_EVENT_TX_OVERFLOW\n");
		break;
	}
	if (ac->cb)
		ac->cb(data->opcode, data->token,
			data->payload, ac->priv);

	return 0;
}

void *q6asm_is_cpu_buf_avail(int dir, struct audio_client *ac, uint32_t *size,
				uint32_t *index)
{
	void *data;
	unsigned char idx;
	struct audio_port_data *port;

	if (!ac || ((dir != IN) && (dir != OUT)))
		return NULL;

	port = &ac->port[dir];

	mutex_lock(&port->lock);
	idx = port->cpu_buf;
	/*  dir 0: used = 0 means buf in use
	    dir 1: used = 1 means buf in use */
	if (port->buf[idx].used == dir) {
		/* To make it more robust, we could loop and get the
		next avail buf, its risky though */
		pr_debug("Next buf idx[0x%x] not available, dir[%d]\n",
								idx, dir);
		mutex_unlock(&port->lock);
		return NULL;
	}
	*size = port->buf[idx].actual_size;
	*index = port->cpu_buf;
	data = port->buf[idx].data;
	pr_debug("session[%d]index[%d] data[%p]size[%d]\n",
						ac->session,
						port->cpu_buf,
						data, *size);
	/* By default increase the cpu_buf cnt
	user accesses this function,increase cpu buf(to avoid another api)*/
	port->buf[idx].used = dir;
	port->cpu_buf = ((port->cpu_buf + 1) & (port->max_buf_cnt - 1));
	mutex_unlock(&port->lock);
	return data;
}

int q6asm_is_dsp_buf_avail(int dir, struct audio_client *ac)
{
	int ret = -1;
	struct audio_port_data *port;
	uint32_t idx;

	if (!ac || (dir != OUT))
		return ret;

	port = &ac->port[dir];

	mutex_lock(&port->lock);
	idx = port->dsp_buf;

	if (port->buf[idx].used == (dir ^ 1)) {
		/* To make it more robust, we could loop and get the
		next avail buf, its risky though */
		pr_err("Next buf idx[0x%x] not available, dir[%d]\n",
								idx, dir);
		mutex_unlock(&port->lock);
		return ret;
	}
	pr_debug("session[%d]dsp_buf=%d cpu_buf=%d\n", ac->session,
				port->dsp_buf, port->cpu_buf);
	ret = ((port->dsp_buf != port->cpu_buf) ? 0 : -1);
	mutex_unlock(&port->lock);
	return ret;
}

static void q6asm_add_hdr(struct audio_client *ac, struct apr_hdr *hdr,
			uint32_t pkt_size, uint32_t cmd_flg)
{
	pr_debug("pkt size=%d cmd_flg=%d\n", pkt_size, cmd_flg);
	mutex_lock(&ac->cmd_lock);
	pr_debug("**************\n");
	hdr->hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD, \
				APR_HDR_LEN(sizeof(struct apr_hdr)),\
				APR_PKT_VER);
	hdr->src_svc = ((struct apr_svc *)ac->apr)->id;
	hdr->src_domain = APR_DOMAIN_APPS;
	hdr->dest_svc = APR_SVC_ASM;
	hdr->dest_domain = APR_DOMAIN_ADSP;
	hdr->src_port = (ac->session << 8) | 0x0001;
	hdr->dest_port = (ac->session << 8) | 0x0001;
	if (cmd_flg) {
		hdr->token = ac->session;
		atomic_set(&ac->cmd_state, 1);
	}
	hdr->pkt_size  = APR_PKT_SIZE(APR_HDR_SIZE, pkt_size);
	mutex_unlock(&ac->cmd_lock);
	return;
}

static void q6asm_add_mmaphdr(struct apr_hdr *hdr, uint32_t pkt_size,
							uint32_t cmd_flg)
{
	pr_debug("pkt size=%d cmd_flg=%d\n", pkt_size, cmd_flg);
	pr_debug("**************\n");
	hdr->hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD, \
				APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	hdr->src_port = 0;
	hdr->dest_port = 0;
	if (cmd_flg) {
		hdr->token = 0;
		atomic_set(&this_mmap.cmd_state, 1);
	}
	hdr->pkt_size  = pkt_size;
	return;
}

int q6asm_open_read(struct audio_client *ac,
		uint32_t format)
{
	int rc = 0x00;
	struct asm_stream_cmd_open_read open;

	pr_debug("session[%d]", ac->session);

	if ((ac == NULL) || (ac->apr == NULL)) {
		pr_err("APR handle NULL\n");
		return -EINVAL;
	}

	q6asm_add_hdr(ac, &open.hdr, sizeof(open), TRUE);
	open.hdr.opcode = ASM_STREAM_CMD_OPEN_READ;
	/* Stream prio : High, provide meta info with encoded frames */
	open.src_endpoint = ASM_END_POINT_DEVICE_MATRIX;
	open.pre_proc_top = DEFAULT_TOPOLOGY;

	switch (format) {
	case FORMAT_LINEAR_PCM:
		open.uMode = STREAM_PRIORITY_HIGH;
		open.format = LINEAR_PCM;
		break;
	case FORMAT_MPEG4_AAC:
		open.uMode = BUFFER_META_ENABLE | STREAM_PRIORITY_HIGH;
		open.format = MPEG4_AAC;
		break;
	case FORMAT_V13K:
		open.uMode = BUFFER_META_ENABLE | STREAM_PRIORITY_HIGH;
		open.format = V13K_FS;
		break;
	case FORMAT_EVRC:
		open.uMode = BUFFER_META_ENABLE | STREAM_PRIORITY_HIGH;
		open.format = EVRC_FS;
		break;
	case FORMAT_AMRNB:
		open.uMode = BUFFER_META_ENABLE | STREAM_PRIORITY_HIGH;
		open.format = AMRNB_FS;
		break;
	default:
		pr_err("Invalid format[%d]\n", format);
		goto fail_cmd;
	}
	rc = apr_send_pkt(ac->apr, (uint32_t *) &open);
	if (rc < 0) {
		pr_err("open failed op[0x%x]rc[%d]\n", \
						open.hdr.opcode, rc);
		goto fail_cmd;
	}
	rc = wait_event_timeout(ac->cmd_wait,
			(atomic_read(&ac->cmd_state) == 0), 5*HZ);
	if (rc < 0) {
		pr_err("timeout. waited for OPEN_WRITR rc[%d]\n", rc);
		goto fail_cmd;
	}
	return 0;
fail_cmd:
	return -EINVAL;
}

int q6asm_open_write(struct audio_client *ac, uint32_t format)
{
	int rc = 0x00;
	struct asm_stream_cmd_open_write open;

	pr_debug("session[%d]", ac->session);

	if ((ac == NULL) || (ac->apr == NULL)) {
		pr_err("APR handle NULL\n");
		return -EINVAL;
	}
	q6asm_add_hdr(ac, &open.hdr, sizeof(open), TRUE);

	open.hdr.opcode = ASM_STREAM_CMD_OPEN_WRITE;
	open.uMode = STREAM_PRIORITY_HIGH;
	/* source endpoint : matrix */
	open.sink_endpoint = ASM_END_POINT_DEVICE_MATRIX;
	open.stream_handle = 0x00;
	open.post_proc_top = DEFAULT_TOPOLOGY;

	switch (format) {
	case FORMAT_LINEAR_PCM:
		open.format = LINEAR_PCM;
		break;
	case FORMAT_MPEG4_AAC:
		open.format = MPEG4_AAC;
		break;
	default:
		pr_err("Invalid format[%d]\n", format);
		goto fail_cmd;
	}
	rc = apr_send_pkt(ac->apr, (uint32_t *) &open);
	if (rc < 0) {
		pr_err("open failed op[0x%x]rc[%d]\n", \
						open.hdr.opcode, rc);
		goto fail_cmd;
	}
	rc = wait_event_timeout(ac->cmd_wait,
			(atomic_read(&ac->cmd_state) == 0), 5*HZ);
	if (rc < 0) {
		pr_err("timeout. waited for OPEN_WRITR rc[%d]\n", rc);
		goto fail_cmd;
	}
	return 0;
fail_cmd:
	return -EINVAL;
}

int q6asm_open_read_write(struct audio_client *ac,
			uint32_t rd_format,
			uint32_t wr_format)
{
	int rc = 0x00;
	struct asm_stream_cmd_open_read_write open;

	pr_debug("session[%d]", ac->session);
	pr_debug("wr_format[0x%x]rd_format[0x%x]",
				wr_format, rd_format);

	if ((ac == NULL) || (ac->apr == NULL)) {
		pr_err("APR handle NULL\n");
		return -EINVAL;
	}
	q6asm_add_hdr(ac, &open.hdr, sizeof(open), TRUE);
	open.hdr.opcode = ASM_STREAM_CMD_OPEN_READWRITE;

	open.uMode = BUFFER_META_ENABLE | STREAM_PRIORITY_NORMAL;
	/* source endpoint : matrix */
	open.post_proc_top = DEFAULT_TOPOLOGY;
	switch (wr_format) {
	case FORMAT_LINEAR_PCM:
		open.write_format = LINEAR_PCM;
		break;
	default:
		pr_err("Invalid format[%d]\n", wr_format);
		goto fail_cmd;
	}

	switch (rd_format) {
	case FORMAT_LINEAR_PCM:
		open.read_format = LINEAR_PCM;
		break;
	case FORMAT_MPEG4_AAC:
		open.read_format = MPEG4_AAC;
		break;
	case FORMAT_V13K:
		open.read_format = V13K_FS;
		break;
	case FORMAT_EVRC:
		open.read_format = EVRC_FS;
		break;
	case FORMAT_AMRNB:
		open.read_format = AMRNB_FS;
		break;
	default:
		pr_err("Invalid format[%d]\n", rd_format);
		goto fail_cmd;
	}
	pr_debug("rdformat[0x%x]wrformat[0x%x]\n",
			open.read_format, open.write_format);

	rc = apr_send_pkt(ac->apr, (uint32_t *) &open);
	if (rc < 0) {
		pr_err("open failed op[0x%x]rc[%d]\n", \
						open.hdr.opcode, rc);
		goto fail_cmd;
	}
	rc = wait_event_timeout(ac->cmd_wait,
			(atomic_read(&ac->cmd_state) == 0), 5*HZ);
	if (rc < 0) {
		pr_err("timeout. waited for OPEN_WRITR rc[%d]\n", rc);
		goto fail_cmd;
	}
	return 0;
fail_cmd:
	return -EINVAL;
}

int q6asm_run(struct audio_client *ac, uint32_t flags,
		uint32_t msw_ts, uint32_t lsw_ts)
{
	struct asm_stream_cmd_run run;
	int rc;
	pr_debug("session[%d]", ac->session);
	if (!ac || ac->apr == NULL) {
		pr_err("APR handle NULL\n");
		return -EINVAL;
	}
	q6asm_add_hdr(ac, &run.hdr, sizeof(run), TRUE);

	run.hdr.opcode = ASM_SESSION_CMD_RUN;
	run.flags    = flags;
	run.msw_ts   = msw_ts;
	run.lsw_ts   = lsw_ts;

	rc = apr_send_pkt(ac->apr, (uint32_t *) &run);
	if (rc < 0) {
		pr_err("Commmand run failed[%d]", rc);
		goto fail_cmd;
	}

	rc = wait_event_timeout(ac->cmd_wait,
			(atomic_read(&ac->cmd_state) == 0), 5*HZ);
	if (rc < 0) {
		pr_err("timeout. waited for run success rc[%d]", rc);
		goto fail_cmd;
	}

	return 0;
fail_cmd:
	return -EINVAL;
}

int q6asm_enc_cfg_blk_aac(struct audio_client *ac,
			 uint32_t frames_per_buf,
			uint32_t sample_rate, uint32_t channels,
			uint32_t bit_rate, uint32_t mode, uint32_t format)
{
	struct asm_stream_cmd_encdec_cfg_blk enc_cfg;
	int rc = 0;

	pr_debug("frames[%d]SR[%d]ch[%d]bitrate[%d]mode[%d]format[%d]",
		frames_per_buf, sample_rate, channels, bit_rate, mode, format);

	q6asm_add_hdr(ac, &enc_cfg.hdr, (sizeof(enc_cfg) - APR_HDR_SIZE), TRUE);

	enc_cfg.hdr.opcode = ASM_STREAM_CMD_SET_ENCDEC_PARAM;
	enc_cfg.param_id = ASM_ENCDEC_CFG_BLK_ID;
	enc_cfg.param_size = sizeof(struct asm_encode_cfg_blk);
	enc_cfg.enc_blk.frames_per_buf = frames_per_buf;
	enc_cfg.enc_blk.format_id = MPEG4_AAC;
	enc_cfg.enc_blk.cfg_size  = sizeof(struct asm_aac_read_cfg);
	enc_cfg.enc_blk.cfg.aac.bitrate = bit_rate;
	enc_cfg.enc_blk.cfg.aac.enc_mode = mode;
	enc_cfg.enc_blk.cfg.aac.format = format;
	enc_cfg.enc_blk.cfg.aac.ch_cfg = channels;
	enc_cfg.enc_blk.cfg.aac.sample_rate = sample_rate;

	rc = apr_send_pkt(ac->apr, (uint32_t *) &enc_cfg);
	if (rc < 0) {
		pr_err("Comamnd %d failed\n", ASM_STREAM_CMD_SET_ENCDEC_PARAM);
		rc = -EINVAL;
		goto fail_cmd;
	}
	rc = wait_event_timeout(ac->cmd_wait,
			(atomic_read(&ac->cmd_state) == 0), 5*HZ);
	if (rc < 0) {
		pr_err("timeout. waited for FORMAT_UPDATE\n");
		goto fail_cmd;
	}
	return 0;
fail_cmd:
	return -EINVAL;
}

int q6asm_enc_cfg_blk_pcm(struct audio_client *ac,
			uint32_t rate, uint32_t channels)
{
	struct asm_stream_cmd_encdec_cfg_blk  enc_cfg;

	int rc = 0;

	pr_debug("\n");

	q6asm_add_hdr(ac, &enc_cfg.hdr, (sizeof(enc_cfg) - APR_HDR_SIZE), TRUE);

	enc_cfg.hdr.opcode = ASM_STREAM_CMD_SET_ENCDEC_PARAM;
	enc_cfg.param_id = ASM_ENCDEC_CFG_BLK_ID;
	enc_cfg.param_size = sizeof(struct asm_encode_cfg_blk);
	enc_cfg.enc_blk.frames_per_buf = 1;
	enc_cfg.enc_blk.format_id = LINEAR_PCM;
	enc_cfg.enc_blk.cfg_size = sizeof(struct asm_pcm_cfg);
	enc_cfg.enc_blk.cfg.pcm.ch_cfg = channels;
	enc_cfg.enc_blk.cfg.pcm.bits_per_sample = 16;
	enc_cfg.enc_blk.cfg.pcm.sample_rate = rate;
	enc_cfg.enc_blk.cfg.pcm.is_signed = 1;
	enc_cfg.enc_blk.cfg.pcm.interleaved = 1;

	rc = apr_send_pkt(ac->apr, (uint32_t *) &enc_cfg);
	if (rc < 0) {
		pr_err("Comamnd open failed\n");
		rc = -EINVAL;
		goto fail_cmd;
	}
	rc = wait_event_timeout(ac->cmd_wait,
			(atomic_read(&ac->cmd_state) == 0), 5*HZ);
	if (rc < 0) {
		pr_err("timeout opcode[0x%x] ", enc_cfg.hdr.opcode);
		goto fail_cmd;
	}
	return 0;
fail_cmd:
	return -EINVAL;
}

int q6asm_enc_cfg_blk_qcelp(struct audio_client *ac, uint32_t frames_per_buf,
		uint16_t min_rate, uint16_t max_rate,
		uint16_t reduced_rate_level, uint16_t rate_modulation_cmd)
{
	struct asm_stream_cmd_encdec_cfg_blk enc_cfg;
	int rc = 0;

	pr_debug("frames[%d]min_rate[0x%4x]max_rate[0x%4x] \
		reduced_rate_level[0x%4x]rate_modulation_cmd[0x%4x]",
		frames_per_buf, min_rate, max_rate,
		reduced_rate_level, rate_modulation_cmd);

	q6asm_add_hdr(ac, &enc_cfg.hdr, (sizeof(enc_cfg) - APR_HDR_SIZE), TRUE);

	enc_cfg.hdr.opcode = ASM_STREAM_CMD_SET_ENCDEC_PARAM;

	enc_cfg.param_id = ASM_ENCDEC_CFG_BLK_ID;
	enc_cfg.param_size = sizeof(struct asm_encode_cfg_blk);

	enc_cfg.enc_blk.frames_per_buf = frames_per_buf;
	enc_cfg.enc_blk.format_id = V13K_FS;
	enc_cfg.enc_blk.cfg_size  = sizeof(struct asm_qcelp13_read_cfg);
	enc_cfg.enc_blk.cfg.qcelp13.min_rate = min_rate;
	enc_cfg.enc_blk.cfg.qcelp13.max_rate = max_rate;
	enc_cfg.enc_blk.cfg.qcelp13.reduced_rate_level = reduced_rate_level;
	enc_cfg.enc_blk.cfg.qcelp13.rate_modulation_cmd = rate_modulation_cmd;

	rc = apr_send_pkt(ac->apr, (uint32_t *) &enc_cfg);
	if (rc < 0) {
		pr_err("Comamnd %d failed\n", ASM_STREAM_CMD_SET_ENCDEC_PARAM);
		goto fail_cmd;
	}
	rc = wait_event_timeout(ac->cmd_wait,
			(atomic_read(&ac->cmd_state) == 0), 5*HZ);
	if (rc < 0) {
		pr_err("timeout. waited for FORMAT_UPDATE\n");
		goto fail_cmd;
	}
	return 0;
fail_cmd:
	return -EINVAL;
}

int q6asm_enc_cfg_blk_evrc(struct audio_client *ac, uint32_t frames_per_buf,
		uint16_t min_rate, uint16_t max_rate,
		uint16_t rate_modulation_cmd)
{
	struct asm_stream_cmd_encdec_cfg_blk enc_cfg;
	int rc = 0;

	pr_debug("frames[%d]min_rate[0x%4x]max_rate[0x%4x] \
		rate_modulation_cmd[0x%4x]", frames_per_buf,
		min_rate, max_rate, rate_modulation_cmd);

	q6asm_add_hdr(ac, &enc_cfg.hdr, (sizeof(enc_cfg) - APR_HDR_SIZE), TRUE);

	enc_cfg.hdr.opcode = ASM_STREAM_CMD_SET_ENCDEC_PARAM;

	enc_cfg.param_id = ASM_ENCDEC_CFG_BLK_ID;
	enc_cfg.param_size = sizeof(struct asm_encode_cfg_blk);

	enc_cfg.enc_blk.frames_per_buf = frames_per_buf;
	enc_cfg.enc_blk.format_id = EVRC_FS;
	enc_cfg.enc_blk.cfg_size  = sizeof(struct asm_evrc_read_cfg);
	enc_cfg.enc_blk.cfg.evrc.min_rate = min_rate;
	enc_cfg.enc_blk.cfg.evrc.max_rate = max_rate;
	enc_cfg.enc_blk.cfg.evrc.rate_modulation_cmd = rate_modulation_cmd;
	enc_cfg.enc_blk.cfg.evrc.reserved = 0;

	rc = apr_send_pkt(ac->apr, (uint32_t *) &enc_cfg);
	if (rc < 0) {
		pr_err("Comamnd %d failed\n", ASM_STREAM_CMD_SET_ENCDEC_PARAM);
		goto fail_cmd;
	}
	rc = wait_event_timeout(ac->cmd_wait,
			(atomic_read(&ac->cmd_state) == 0), 5*HZ);
	if (rc < 0) {
		pr_err("timeout. waited for FORMAT_UPDATE\n");
		goto fail_cmd;
	}
	return 0;
fail_cmd:
	return -EINVAL;
}

int q6asm_enc_cfg_blk_amrnb(struct audio_client *ac, uint32_t frames_per_buf,
			uint16_t band_mode, uint16_t dtx_enable)
{
	struct asm_stream_cmd_encdec_cfg_blk enc_cfg;
	int rc = 0;

	pr_debug("frames[%d]band_mode[0x%4x]dtx_enable[0x%4x]",
		frames_per_buf, band_mode, dtx_enable);

	q6asm_add_hdr(ac, &enc_cfg.hdr, (sizeof(enc_cfg) - APR_HDR_SIZE), TRUE);

	enc_cfg.hdr.opcode = ASM_STREAM_CMD_SET_ENCDEC_PARAM;

	enc_cfg.param_id = ASM_ENCDEC_CFG_BLK_ID;
	enc_cfg.param_size = sizeof(struct asm_encode_cfg_blk);

	enc_cfg.enc_blk.frames_per_buf = frames_per_buf;
	enc_cfg.enc_blk.format_id = AMRNB_FS;
	enc_cfg.enc_blk.cfg_size  = sizeof(struct asm_amrnb_read_cfg);
	enc_cfg.enc_blk.cfg.amrnb.mode = band_mode;
	enc_cfg.enc_blk.cfg.amrnb.dtx_mode = dtx_enable;

	rc = apr_send_pkt(ac->apr, (uint32_t *) &enc_cfg);
	if (rc < 0) {
		pr_err("Comamnd %d failed\n", ASM_STREAM_CMD_SET_ENCDEC_PARAM);
		goto fail_cmd;
	}
	rc = wait_event_timeout(ac->cmd_wait,
			(atomic_read(&ac->cmd_state) == 0), 5*HZ);
	if (rc < 0) {
		pr_err("timeout. waited for FORMAT_UPDATE\n");
		goto fail_cmd;
	}
	return 0;
fail_cmd:
	return -EINVAL;
}

int q6asm_media_format_block_pcm(struct audio_client *ac,
				uint32_t rate, uint32_t channels)
{
	struct asm_stream_media_format_update fmt;
	int rc = 0;

	pr_debug("rate[%d]ch[%d]\n", rate, channels);

	q6asm_add_hdr(ac, &fmt.hdr, (sizeof(fmt) - APR_HDR_SIZE), TRUE);

	fmt.hdr.opcode = ASM_DATA_CMD_MEDIA_FORMAT_UPDATE;

	fmt.format = LINEAR_PCM;
	fmt.cfg_size = sizeof(struct asm_pcm_cfg);
	fmt.write_cfg.pcm_cfg.ch_cfg = channels;
	fmt.write_cfg.pcm_cfg.bits_per_sample = 16;
	fmt.write_cfg.pcm_cfg.sample_rate = rate;
	fmt.write_cfg.pcm_cfg.is_signed = 1;
	fmt.write_cfg.pcm_cfg.interleaved = 1;

	rc = apr_send_pkt(ac->apr, (uint32_t *) &fmt);
	if (rc < 0) {
		pr_err("Comamnd open failed\n");
		rc = -EINVAL;
		goto fail_cmd;
	}
	rc = wait_event_timeout(ac->cmd_wait,
			(atomic_read(&ac->cmd_state) == 0), 5*HZ);
	if (rc < 0) {
		pr_err("timeout. waited for FORMAT_UPDATE\n");
		rc = -EINVAL;
	}
	return 0;
fail_cmd:
	return -EINVAL;
}

int q6asm_memory_map(struct audio_client *ac, uint32_t buf_add, int dir,
					uint32_t bufsz, uint32_t bufcnt)
{
	struct asm_stream_cmd_memory_map mem_map;
	int rc = 0;

	pr_debug("\n");

	if (!ac || ac->apr == NULL || this_mmap.apr == NULL) {
		pr_err("APR handle NULL\n");
		return -EINVAL;
	}

	q6asm_add_mmaphdr(&mem_map.hdr,
			sizeof(struct asm_stream_cmd_memory_map), TRUE);
	mem_map.hdr.opcode = ASM_SESSION_CMD_MEMORY_MAP;

	mem_map.buf_add = buf_add;
	mem_map.buf_size = bufsz * bufcnt;
	mem_map.mempool_id = 0;

	q6asm_add_mmaphdr(&mem_map.hdr,
			sizeof(struct asm_stream_cmd_memory_map), TRUE);

	pr_debug("buf add[%x]  buf_add_parameter[%x]\n",
					mem_map.buf_add, buf_add);

	rc = apr_send_pkt(this_mmap.apr, (uint32_t *) &mem_map);
	if (rc < 0) {
		pr_err("mem_map op[0x%x]rc[%d]\n",
				mem_map.hdr.opcode, rc);
		rc = -EINVAL;
		goto fail_cmd;
	}

	rc = wait_event_timeout(this_mmap.cmd_wait,
		(atomic_read(&this_mmap.cmd_state) == 0), 5 * HZ);
	if (rc < 0) {
		pr_err("timeout. waited for memory_map\n");
		rc = -EINVAL;
		goto fail_cmd;
	}
fail_cmd:
	return rc;
}

int q6asm_memory_unmap(struct audio_client *ac, uint32_t buf_add, int dir)
{
	struct asm_stream_cmd_memory_unmap mem_unmap;
	int rc = 0;

	pr_debug("\n");

	if (!ac || ac->apr == NULL || this_mmap.apr == NULL) {
		pr_err("APR handle NULL\n");
		return -EINVAL;
	}

	q6asm_add_mmaphdr(&mem_unmap.hdr,
			sizeof(struct asm_stream_cmd_memory_unmap), TRUE);
	mem_unmap.hdr.opcode = ASM_SESSION_CMD_MEMORY_UNMAP;
	mem_unmap.buf_add = buf_add;

	rc = apr_send_pkt(this_mmap.apr, (uint32_t *) &mem_unmap);
	if (rc < 0) {
		pr_err("mem_unmap op[0x%x]rc[%d]\n",
					mem_unmap.hdr.opcode, rc);
		rc = -EINVAL;
		goto fail_cmd;
	}

	rc = wait_event_timeout(this_mmap.cmd_wait,
			(atomic_read(&this_mmap.cmd_state) == 0), 5 * HZ);
	if (rc < 0) {
		pr_err("timeout. waited for memory_map\n");
		rc = -EINVAL;
		goto fail_cmd;
	}
fail_cmd:
	return rc;
}

int q6asm_memory_map_regions(struct audio_client *ac, int dir,
				uint32_t bufsz, uint32_t bufcnt)
{
	struct	 asm_stream_cmd_memory_map_regions *mmap_regions = NULL;
	struct asm_memory_map_regions *mregions = NULL;
	struct audio_port_data *port = NULL;
	struct audio_buffer *ab = NULL;
	void	*mmap_region_cmd = NULL;
	void	*payload = NULL;
	int	rc = 0;
	int	i = 0;
	int	cmd_size = 0;

	pr_debug("\n");

	if (!ac || ac->apr == NULL || this_mmap.apr == NULL) {
		pr_err("APR handle NULL\n");
		return -EINVAL;
	}

	cmd_size = sizeof(struct asm_stream_cmd_memory_map_regions)
			+ sizeof(struct asm_memory_map_regions) * bufcnt;

	mmap_region_cmd = kzalloc(cmd_size, GFP_KERNEL);
	mmap_regions = (struct asm_stream_cmd_memory_map_regions *)
							mmap_region_cmd;
	q6asm_add_mmaphdr(&mmap_regions->hdr, cmd_size, TRUE);
	mmap_regions->hdr.opcode = ASM_SESSION_CMD_MEMORY_MAP_REGIONS;
	mmap_regions->mempool_id = 0;
	mmap_regions->nregions = bufcnt & 0x00ff;
	pr_debug("map_regions->nregions = %d\n", mmap_regions->nregions);
	payload = ((u8 *) mmap_region_cmd +
		sizeof(struct asm_stream_cmd_memory_map_regions));
	mregions = (struct asm_memory_map_regions *)payload;

	port = &ac->port[dir];
	for (i = 0; i < bufcnt; i++) {
		ab = &port->buf[i];
		mregions->phys = ab->phys;
		mregions->buf_size = ab->size;
		++mregions;
	}

	rc = apr_send_pkt(this_mmap.apr, (uint32_t *) mmap_region_cmd);
	if (rc < 0) {
		pr_err("mmap_regions op[0x%x]rc[%d]\n",
					mmap_regions->hdr.opcode, rc);
		rc = -EINVAL;
		goto fail_cmd;
	}

	rc = wait_event_timeout(this_mmap.cmd_wait,
			(atomic_read(&this_mmap.cmd_state) == 0), 5*HZ);
	if (rc < 0) {
		pr_err("timeout. waited for memory_map\n");
		rc = -EINVAL;
		goto fail_cmd;
	}
fail_cmd:
	kfree(mmap_region_cmd);
	return rc;
}

int q6asm_memory_unmap_regions(struct audio_client *ac, int dir,
				uint32_t bufsz, uint32_t bufcnt)
{
	struct asm_stream_cmd_memory_unmap_regions *unmap_regions = NULL;
	struct asm_memory_unmap_regions *mregions = NULL;
	struct audio_port_data *port = NULL;
	struct audio_buffer *ab = NULL;
	void	*unmap_region_cmd = NULL;
	void	*payload = NULL;
	int	rc = 0;
	int	i = 0;
	int	cmd_size = 0;

	pr_debug("\n");
	if (!ac || ac->apr == NULL || this_mmap.apr == NULL) {
		pr_err("APR handle NULL\n");
		return -EINVAL;
	}

	cmd_size = sizeof(struct asm_stream_cmd_memory_unmap_regions) +
			sizeof(struct asm_memory_unmap_regions) * bufcnt;

	unmap_region_cmd = kzalloc(cmd_size, GFP_KERNEL);
	unmap_regions = (struct asm_stream_cmd_memory_unmap_regions *)
							unmap_region_cmd;
	q6asm_add_mmaphdr(&unmap_regions->hdr, cmd_size, TRUE);
	unmap_regions->hdr.opcode = ASM_SESSION_CMD_MEMORY_UNMAP_REGIONS;
	unmap_regions->nregions = bufcnt & 0x00ff;
	pr_debug("unmap_regions->nregions = %d\n", unmap_regions->nregions);
	payload = ((u8 *) unmap_region_cmd +
			sizeof(struct asm_stream_cmd_memory_unmap_regions));
	mregions = (struct asm_memory_unmap_regions *)payload;
	port = &ac->port[dir];
	for (i = 0; i < bufcnt; i++) {
		ab = &port->buf[i];
		mregions->phys = ab->phys;
		++mregions;
	}

	rc = apr_send_pkt(this_mmap.apr, (uint32_t *) unmap_region_cmd);
	if (rc < 0) {
		pr_err("mmap_regions op[0x%x]rc[%d]\n",
					unmap_regions->hdr.opcode, rc);
		goto fail_cmd;
	}

	rc = wait_event_timeout(this_mmap.cmd_wait,
			(atomic_read(&this_mmap.cmd_state) == 0), 5*HZ);
	if (rc < 0) {
		pr_err("timeout. waited for memory_unmap\n");
		goto fail_cmd;
	}

fail_cmd:
	kfree(unmap_region_cmd);
	return rc;
}

int q6asm_read(struct audio_client *ac)
{
	struct asm_stream_cmd_read read;
	struct audio_buffer        *ab;
	int dsp_buf;
	struct audio_port_data     *port;
	int rc;
	if (!ac || ac->apr == NULL) {
		pr_err("APR handle NULL\n");
		return -EINVAL;
	}
	port = &ac->port[OUT];

	q6asm_add_hdr(ac, &read.hdr, (sizeof(read) - APR_HDR_SIZE), FALSE);

	mutex_lock(&port->lock);

	dsp_buf = port->dsp_buf;
	ab = &port->buf[dsp_buf];
	port->buf[dsp_buf].used = 0;
	pr_debug("session[%d]dsp-buf[%d][%p]cpu_buf[%d][%p]\n",
					ac->session,
					dsp_buf,
					(void *)port->buf[dsp_buf].data,
					port->cpu_buf,
					(void *)port->buf[port->cpu_buf].phys);

	read.hdr.opcode = ASM_DATA_CMD_READ;
	read.buf_add = ab->phys;
	read.buf_size = ab->size;
	read.uid = port->dsp_buf;
	read.hdr.token = port->dsp_buf;

	port->dsp_buf = (port->dsp_buf + 1) & (port->max_buf_cnt - 1);
	mutex_unlock(&port->lock);
	pr_debug("buf add[0x%x] token[%d] uid[%d]\n",
						read.buf_add,
						read.hdr.token,
						read.uid);
	rc = apr_send_pkt(ac->apr, (uint32_t *) &read);
	if (rc < 0) {
		pr_err("read op[0x%x]rc[%d]\n", read.hdr.opcode, rc);
		goto fail_cmd;
	}
	return 0;
fail_cmd:
	return -EINVAL;
}

int q6asm_write(struct audio_client *ac, uint32_t len, uint32_t msw_ts,
		uint32_t lsw_ts, uint32_t flags)
{
	int rc = 0;
	struct asm_stream_cmd_write write;
	struct audio_port_data *port;
	struct audio_buffer    *ab;
	int dsp_buf = 0;

	pr_debug("session[%d] len=%d", ac->session, len);
	if (!ac || ac->apr == NULL) {
		pr_err("APR handle NULL\n");
		return -EINVAL;
	}
	port = &ac->port[IN];

	q6asm_add_hdr(ac, &write.hdr, (sizeof(write) - APR_HDR_SIZE), FALSE);
	mutex_lock(&port->lock);

	dsp_buf = port->dsp_buf;
	ab = &port->buf[dsp_buf];

	write.hdr.token = port->dsp_buf;
	write.hdr.opcode = ASM_DATA_CMD_WRITE;
	write.buf_add = ab->phys;
	write.avail_bytes = len;
	write.msw_ts = msw_ts;
	write.lsw_ts = lsw_ts;
	write.uflags = (0x8000 | flags);
	write.uid = port->dsp_buf;
	port->dsp_buf = (port->dsp_buf + 1) & (port->max_buf_cnt - 1);

	pr_debug("ab->phys[0x%x]bufadd[0x%x]token[0x%x]buf_id[0x%x]",
							ab->phys,
							write.buf_add,
							write.hdr.token,
							write.uid);
	mutex_unlock(&port->lock);

	rc = apr_send_pkt(ac->apr, (uint32_t *) &write);
	if (rc < 0) {
		pr_err("write op[0x%x]rc[%d]\n", write.hdr.opcode, rc);
		goto fail_cmd;
	}
	pr_debug("WRITE SUCCESS\n");
	return 0;
fail_cmd:
	return -EINVAL;
}

int q6asm_cmd(struct audio_client *ac, int cmd)
{
	struct apr_hdr hdr;
	int rc;
	unsigned char state;

	if (!ac || ac->apr == NULL) {
		pr_err("APR handle NULL\n");
		return -EINVAL;
	}
	q6asm_add_hdr(ac, &hdr, (sizeof(hdr) - APR_HDR_SIZE), TRUE);
	switch (cmd) {
	case CMD_PAUSE:
		hdr.opcode = ASM_SESSION_CMD_PAUSE;
		state = atomic_read(&ac->cmd_state);
		break;
	case CMD_FLUSH:
		hdr.opcode = ASM_STREAM_CMD_FLUSH;
		state = atomic_read(&ac->cmd_state);
		break;
	case CMD_EOS:
		hdr.opcode = ASM_DATA_CMD_EOS;
		atomic_set(&ac->eos_state, 1);
		state = atomic_read(&ac->eos_state);
		atomic_set(&ac->cmd_state, 0);
		break;
	case CMD_CLOSE:
		hdr.opcode = ASM_STREAM_CMD_CLOSE;
		state = atomic_read(&ac->cmd_state);
		break;
	default:
		pr_err("Invalid format[%d]\n", cmd);
		goto fail_cmd;
	}
	pr_debug("session[%d]opcode[0x%x] ", ac->session,
						hdr.opcode);
	rc = apr_send_pkt(ac->apr, (uint32_t *) &hdr);
	if (rc < 0) {
		pr_err("Commmand 0x%x failed\n", hdr.opcode);
		goto fail_cmd;
	}
	rc = wait_event_timeout(ac->cmd_wait, (state == 0), 5*HZ);
	if (rc < 0) {
		pr_err("timeout. waited for response opcode[0x%x]\n",
							hdr.opcode);
		goto fail_cmd;
	}
	if (cmd == CMD_FLUSH)
		q6asm_reset_buf_state(ac);
	return 0;
fail_cmd:
	return -EINVAL;
}

static void q6asm_reset_buf_state(struct audio_client *ac)
{
	int cnt = 0;
	int loopcnt = 0;
	struct audio_port_data *port = NULL;

	mutex_lock(&ac->cmd_lock);
	for (loopcnt = 0; loopcnt <= OUT; loopcnt++) {
		port = &ac->port[loopcnt];
		cnt = port->max_buf_cnt - 1;
		port->dsp_buf = 0;
		port->cpu_buf = 0;
		while (cnt >= 0) {
			if (!port->buf)
				continue;
			port->buf[cnt].used = 1;
			cnt--;
		}
	}
	mutex_unlock(&ac->cmd_lock);
}

int q6asm_reg_tx_overflow(struct audio_client *ac, uint16_t enable)
{
	struct asm_stream_cmd_reg_tx_overflow_event tx_overflow;
	int rc;

	if (!ac || ac->apr == NULL) {
		pr_err("APR handle NULL\n");
		return -EINVAL;
	}
	pr_debug("session[%d]enable[%d]\n", ac->session, enable);
	q6asm_add_hdr(ac, &tx_overflow.hdr, sizeof(tx_overflow), TRUE);

	tx_overflow.hdr.opcode = \
			ASM_SESSION_CMD_REGISTER_FOR_TX_OVERFLOW_EVENTS;
	/* tx overflow event: enable */
	tx_overflow.enable = enable;

	rc = apr_send_pkt(ac->apr, (uint32_t *) &tx_overflow);
	if (rc < 0) {
		pr_err("tx overflow op[0x%x]rc[%d]\n", \
						tx_overflow.hdr.opcode, rc);
		goto fail_cmd;
	}
	rc = wait_event_timeout(ac->cmd_wait,
				(atomic_read(&ac->cmd_state) == 0), 5*HZ);
	if (rc < 0) {
		pr_err("timeout. waited for tx overflow\n");
		goto fail_cmd;
	}
	return 0;
fail_cmd:
	return -EINVAL;
}

static int __init q6asm_init(void)
{
	pr_info("%s\n", __func__);
	init_waitqueue_head(&this_mmap.cmd_wait);
	memset(session, 0, sizeof(session));
	return 0;
}

static void __exit q6asm_exit(void)
{
	pr_info("%s\n", __func__);
	return;
}
device_initcall(q6asm_init);
