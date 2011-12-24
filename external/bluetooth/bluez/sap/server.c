/*
 *  BlueZ - Bluetooth protocol stack for Linux
 *
 *  Copyright (C) 2010 Instituto Nokia de Tecnologia - INdT
 *  Copyright (C) 2010 ST-Ericsson SA
 *  Copyright (C) 2011 Tieto Poland
 *
 *  Author: Marek Skowron <marek.skowron@tieto.com> for ST-Ericsson.
 *  Author: Waldemar Rymarkiewicz <waldemar.rymarkiewicz@tieto.com>
 *          for ST-Ericsson.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <errno.h>
#include <glib.h>
#include <netinet/in.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/sdp.h>
#include <bluetooth/sdp_lib.h>

#include "adapter.h"
#include "btio.h"
#include "sdpd.h"
#include "log.h"
#include "error.h"
#include "dbus-common.h"
#include "sap.h"
#include "server.h"

#define SAP_SERVER_INTERFACE	"org.bluez.SimAccess"
#define SAP_UUID		"0000112D-0000-1000-8000-00805F9B34FB"
#define SAP_SERVER_CHANNEL	8
#define SAP_BUF_SIZE		512

#define PADDING4(x) (4 - (x & 0x03))
#define PARAMETER_SIZE(x) (sizeof(struct sap_parameter) + x + PADDING4(x))

#define SAP_NO_REQ 0xFF

#define SAP_TIMER_GRACEFUL_DISCONNECT 30
#define SAP_TIMER_NO_ACTIVITY 30

enum {
	SAP_STATE_DISCONNECTED,
	SAP_STATE_CONNECT_IN_PROGRESS,
	SAP_STATE_CONNECTED,
	SAP_STATE_GRACEFUL_DISCONNECT,
	SAP_STATE_IMMEDIATE_DISCONNECT,
	SAP_STATE_CLIENT_DISCONNECT
};

struct sap_connection {
	GIOChannel *io;
	uint32_t state;
	uint8_t processing_req;
	guint timer_id;
};

struct sap_server {
	bdaddr_t src;
	char *path;
	uint32_t record_id;
	GIOChannel *listen_io;
	struct sap_connection *conn;
};

static DBusConnection *connection;
static struct sap_server *server;

static void start_guard_timer(struct sap_connection *conn, guint interval);
static void stop_guard_timer(struct sap_connection *conn);
static gboolean guard_timeout(gpointer data);

static size_t add_result_parameter(uint8_t result,
					struct sap_parameter *param)
{
	param->id = SAP_PARAM_ID_RESULT_CODE;
	param->len = htons(SAP_PARAM_ID_RESULT_CODE_LEN);
	*param->val = result;

	return PARAMETER_SIZE(SAP_PARAM_ID_RESULT_CODE_LEN);
}

static int is_power_sim_off_req_allowed(uint8_t processing_req)
{
	switch (processing_req) {
	case SAP_NO_REQ:
	case SAP_TRANSFER_APDU_REQ:
	case SAP_TRANSFER_ATR_REQ:
	case SAP_POWER_SIM_ON_REQ:
	case SAP_RESET_SIM_REQ:
	case SAP_TRANSFER_CARD_READER_STATUS_REQ:
		return 1;
	default:
		return 0;
	}
}

static int is_reset_sim_req_allowed(uint8_t processing_req)
{
	switch (processing_req) {
	case SAP_NO_REQ:
	case SAP_TRANSFER_APDU_REQ:
	case SAP_TRANSFER_ATR_REQ:
	case SAP_TRANSFER_CARD_READER_STATUS_REQ:
		return 1;
	default:
		return 0;
	}
}

static int check_msg(struct sap_message *msg)
{
	if (!msg)
		return -EINVAL;

	switch (msg->id) {
	case SAP_CONNECT_REQ:
		if (msg->nparam != 0x01)
			return -EBADMSG;

		if (msg->param->id != SAP_PARAM_ID_MAX_MSG_SIZE)
			return -EBADMSG;

		if (ntohs(msg->param->len) != SAP_PARAM_ID_MAX_MSG_SIZE_LEN)
			return -EBADMSG;

		break;

	case SAP_TRANSFER_APDU_REQ:
		if (msg->nparam != 0x01)
			return -EBADMSG;

		if (msg->param->id != SAP_PARAM_ID_COMMAND_APDU)
			if ( msg->param->id != SAP_PARAM_ID_COMMAND_APDU7816)
				return -EBADMSG;

		if (msg->param->len == 0x00)
			return -EBADMSG;

		break;

	case SAP_SET_TRANSPORT_PROTOCOL_REQ:
		if (msg->nparam != 0x01)
			return -EBADMSG;

		if (msg->param->id != SAP_PARAM_ID_TRANSPORT_PROTOCOL)
			return -EBADMSG;

		if (ntohs(msg->param->len) != SAP_PARAM_ID_TRANSPORT_PROTO_LEN)
			return -EBADMSG;

		if (*msg->param->val != SAP_TRANSPORT_PROTOCOL_T0)
			if (*msg->param->val != SAP_TRANSPORT_PROTOCOL_T1)
				return -EBADMSG;

		break;

	case SAP_DISCONNECT_REQ:
	case SAP_TRANSFER_ATR_REQ:
	case SAP_POWER_SIM_OFF_REQ:
	case SAP_POWER_SIM_ON_REQ:
	case SAP_RESET_SIM_REQ:
	case SAP_TRANSFER_CARD_READER_STATUS_REQ:
		if (msg->nparam != 0x00)
			return -EBADMSG;

		break;
	}

	return 0;
}

static sdp_record_t *create_sap_record(uint8_t channel)
{
	sdp_list_t *apseq, *aproto, *profiles, *proto[2], *root, *svclass_id;
	uuid_t sap_uuid, gt_uuid, root_uuid, l2cap, rfcomm;
	sdp_profile_desc_t profile;
	sdp_record_t *record;
	sdp_data_t *ch;

	record = sdp_record_alloc();
	if (!record)
		return NULL;

	sdp_uuid16_create(&root_uuid, PUBLIC_BROWSE_GROUP);
	root = sdp_list_append(NULL, &root_uuid);
	sdp_set_browse_groups(record, root);
	sdp_list_free(root, NULL);

	sdp_uuid16_create(&sap_uuid, SAP_SVCLASS_ID);
	svclass_id = sdp_list_append(NULL, &sap_uuid);
	sdp_uuid16_create(&gt_uuid, GENERIC_TELEPHONY_SVCLASS_ID);
	svclass_id = sdp_list_append(svclass_id, &gt_uuid);

	sdp_set_service_classes(record, svclass_id);
	sdp_list_free(svclass_id, NULL);

	sdp_uuid16_create(&profile.uuid, SAP_PROFILE_ID);
	profile.version = SAP_VERSION;
	profiles = sdp_list_append(NULL, &profile);
	sdp_set_profile_descs(record, profiles);
	sdp_list_free(profiles, NULL);

	sdp_uuid16_create(&l2cap, L2CAP_UUID);
	proto[0] = sdp_list_append(NULL, &l2cap);
	apseq = sdp_list_append(NULL, proto[0]);

	sdp_uuid16_create(&rfcomm, RFCOMM_UUID);
	proto[1] = sdp_list_append(NULL, &rfcomm);
	ch = sdp_data_alloc(SDP_UINT8, &channel);
	proto[1] = sdp_list_append(proto[1], ch);
	apseq = sdp_list_append(apseq, proto[1]);

	aproto = sdp_list_append(NULL, apseq);
	sdp_set_access_protos(record, aproto);

	sdp_set_info_attr(record, "SIM Access Server",
			NULL, NULL);

	sdp_data_free(ch);
	sdp_list_free(proto[0], NULL);
	sdp_list_free(proto[1], NULL);
	sdp_list_free(apseq, NULL);
	sdp_list_free(aproto, NULL);

	return record;
}

static int send_message(struct sap_connection *conn, void *buf, size_t size)
{
	size_t written = 0;
	GError *gerr = NULL;
	GIOStatus gstatus;

	if (!conn || !buf)
		return -EINVAL;

	DBG("size %zu", size);

	gstatus = g_io_channel_write_chars(conn->io, buf, size, &written,
						&gerr);
	if (gstatus != G_IO_STATUS_NORMAL) {
		if (gerr)
			g_error_free(gerr);

		error("write error (0x%02x).", gstatus);
		return -EINVAL;
	}

	if (written != size)
		error("write error.(written %zu size %zu)", written, size);

	return 0;
}

static int disconnect_ind(void *sap_device, uint8_t disc_type)
{
	struct sap_connection *conn = sap_device;
	char buf[SAP_BUF_SIZE];
	struct sap_message *msg = (struct sap_message *) buf;
	struct sap_parameter *param = (struct sap_parameter *) msg->param;
	size_t size = sizeof(struct sap_message);

	if (!conn)
		return -EINVAL;

	DBG("data %p state %d disc_type 0x%02x", conn, conn->state, disc_type);

	if (conn->state != SAP_STATE_GRACEFUL_DISCONNECT &&
			conn->state != SAP_STATE_IMMEDIATE_DISCONNECT) {
		error("Processing error (state %d pr 0x%02x)", conn->state,
							conn->processing_req);
		return -EPERM;
	}

	memset(buf, 0, sizeof(buf));
	msg->id = SAP_DISCONNECT_IND;
	msg->nparam = 0x01;

	/* Add disconnection type param. */
	param->id  = SAP_PARAM_ID_DISCONNECT_IND;
	param->len = htons(SAP_PARAM_ID_DISCONNECT_IND_LEN);
	*param->val = disc_type;
	size += PARAMETER_SIZE(SAP_PARAM_ID_DISCONNECT_IND_LEN);

	return send_message(sap_device, buf, size);
}

static void connect_req(struct sap_connection *conn,
				struct sap_parameter *param)
{
	uint16_t maxmsgsize, *val;

	DBG("conn %p state %d", conn, conn->state);

	if (!param)
		goto error_rsp;

	if (conn->state != SAP_STATE_DISCONNECTED)
		goto error_rsp;

	stop_guard_timer(conn);

	val = (uint16_t *) &param->val;
	maxmsgsize = ntohs(*val);

	DBG("Connect MaxMsgSize: 0x%04x", maxmsgsize);

	conn->state = SAP_STATE_CONNECT_IN_PROGRESS;

	if (maxmsgsize <= SAP_BUF_SIZE) {
		conn->processing_req = SAP_CONNECT_REQ;
		sap_connect_req(conn, maxmsgsize);
	} else {
		sap_connect_rsp(conn, SAP_STATUS_MAX_MSG_SIZE_NOT_SUPPORTED,
								SAP_BUF_SIZE);
	}

	return;

error_rsp:
	error("Processing error (param %p state %d pr 0x%02x)", param,
					conn->state, conn->processing_req);
	sap_error_rsp(conn);
}

static int disconnect_req(struct sap_connection *conn, uint8_t disc_type)
{
	DBG("conn %p state %d disc_type 0x%02x", conn, conn->state, disc_type);

	switch (disc_type) {
	case SAP_DISCONNECTION_TYPE_GRACEFUL:
		if (conn->state == SAP_STATE_DISCONNECTED)
			goto error_req;

		if (conn->state == SAP_STATE_CONNECT_IN_PROGRESS)
			goto error_req;

		if (conn->state == SAP_STATE_CONNECTED) {
			conn->state = SAP_STATE_GRACEFUL_DISCONNECT;
			conn->processing_req = SAP_NO_REQ;

			disconnect_ind(conn, disc_type);
			/* Timer will disconnect if client won't do.*/
			start_guard_timer(conn, SAP_TIMER_GRACEFUL_DISCONNECT);
		}

		return 0;

	case SAP_DISCONNECTION_TYPE_IMMEDIATE:
		if (conn->state == SAP_STATE_DISCONNECTED)
			goto error_req;

		if (conn->state == SAP_STATE_CONNECT_IN_PROGRESS)
			goto error_req;

		if (conn->state == SAP_STATE_CONNECTED ||
				conn->state == SAP_STATE_GRACEFUL_DISCONNECT) {
			conn->state = SAP_STATE_IMMEDIATE_DISCONNECT;
			conn->processing_req = SAP_NO_REQ;

			stop_guard_timer(conn);
			disconnect_ind(conn, disc_type);
			sap_disconnect_req(conn, 0);
		}

		return 0;

	case SAP_DISCONNECTION_TYPE_CLIENT:
		if (conn->state != SAP_STATE_CONNECTED &&
				conn->state != SAP_STATE_GRACEFUL_DISCONNECT)
			goto error_rsp;

		conn->state = SAP_STATE_CLIENT_DISCONNECT;
		conn->processing_req = SAP_NO_REQ;

		stop_guard_timer(conn);
		sap_disconnect_req(conn, 0);

		return 0;

	default:
		error("Unknown disconnection type (0x%02x).", disc_type);
		return -EINVAL;
	}

error_rsp:
	sap_error_rsp(conn);
error_req:
	error("Processing error (state %d pr 0x%02x)", conn->state,
						conn->processing_req);
	return -EPERM;
}

static void transfer_apdu_req(struct sap_connection *conn,
					struct sap_parameter *param)
{
	DBG("conn %p state %d", conn, conn->state);

	if (!param)
		goto error_rsp;

	param->len = ntohs(param->len);

	if (conn->state != SAP_STATE_CONNECTED &&
			conn->state != SAP_STATE_GRACEFUL_DISCONNECT)
		goto error_rsp;

	if (conn->processing_req != SAP_NO_REQ)
		goto error_rsp;

	conn->processing_req = SAP_TRANSFER_APDU_REQ;
	sap_transfer_apdu_req(conn, param);

	return;

error_rsp:
	error("Processing error (param %p state %d pr 0x%02x)", param,
					conn->state, conn->processing_req);
	sap_error_rsp(conn);
}

static void transfer_atr_req(struct sap_connection *conn)
{
	DBG("conn %p state %d", conn, conn->state);

	if (conn->state != SAP_STATE_CONNECTED)
		goto error_rsp;

	if (conn->processing_req != SAP_NO_REQ)
		goto error_rsp;

	conn->processing_req = SAP_TRANSFER_ATR_REQ;
	sap_transfer_atr_req(conn);

	return;

error_rsp:
	error("Processing error (state %d pr 0x%02x)", conn->state,
						conn->processing_req);
	sap_error_rsp(conn);
}

static void power_sim_off_req(struct sap_connection *conn)
{
	DBG("conn %p state %d", conn, conn->state);

	if (conn->state != SAP_STATE_CONNECTED)
		goto error_rsp;

	if (!is_power_sim_off_req_allowed(conn->processing_req))
		goto error_rsp;

	conn->processing_req = SAP_POWER_SIM_OFF_REQ;
	sap_power_sim_off_req(conn);

	return;

error_rsp:
	error("Processing error (state %d pr 0x%02x)", conn->state,
						conn->processing_req);
	sap_error_rsp(conn);
}

static void power_sim_on_req(struct sap_connection *conn)
{
	DBG("conn %p state %d", conn, conn->state);

	if (conn->state != SAP_STATE_CONNECTED)
		goto error_rsp;

	if (conn->processing_req != SAP_NO_REQ)
		goto error_rsp;

	conn->processing_req = SAP_POWER_SIM_ON_REQ;
	sap_power_sim_on_req(conn);

	return;

error_rsp:
	error("Processing error (state %d pr 0x%02x)", conn->state,
						conn->processing_req);
	sap_error_rsp(conn);
}

static void reset_sim_req(struct sap_connection *conn)
{
	DBG("conn %p state %d", conn, conn->state);

	if (conn->state != SAP_STATE_CONNECTED)
		goto error_rsp;

	if (!is_reset_sim_req_allowed(conn->processing_req))
		goto error_rsp;

	conn->processing_req = SAP_RESET_SIM_REQ;
	sap_reset_sim_req(conn);

	return;

error_rsp:
	error("Processing error (state %d pr 0x%02x param)", conn->state,
						conn->processing_req);
	sap_error_rsp(conn);
}

static void transfer_card_reader_status_req(struct sap_connection *conn)
{
	DBG("conn %p state %d", conn, conn->state);

	if (conn->state != SAP_STATE_CONNECTED)
		goto error_rsp;

	if (conn->processing_req != SAP_NO_REQ)
		goto error_rsp;

	conn->processing_req = SAP_TRANSFER_CARD_READER_STATUS_REQ;
	sap_transfer_card_reader_status_req(conn);

	return;

error_rsp:
	error("Processing error (state %d pr 0x%02x)", conn->state,
						conn->processing_req);
	sap_error_rsp(conn);
}

static void set_transport_protocol_req(struct sap_connection *conn,
					struct sap_parameter *param)
{
	if (!param)
		goto error_rsp;

	DBG("conn %p state %d param %p", conn, conn->state, param);

	if (conn->state != SAP_STATE_CONNECTED)
		goto error_rsp;

	if (conn->processing_req != SAP_NO_REQ)
		goto error_rsp;

	conn->processing_req = SAP_SET_TRANSPORT_PROTOCOL_REQ;
	sap_set_transport_protocol_req(conn, param);

	return;

error_rsp:
	error("Processing error (param %p state %d pr 0x%02x)", param,
					conn->state, conn->processing_req);
	sap_error_rsp(conn);
}

static void start_guard_timer(struct sap_connection *conn, guint interval)
{
	if (!conn)
		return;

	if (!conn->timer_id)
		conn->timer_id = g_timeout_add_seconds(interval, guard_timeout,
									conn);
	else
		error("Timer is already active.");
}

static void stop_guard_timer(struct sap_connection *conn)
{
	if (conn  && conn->timer_id) {
		g_source_remove(conn->timer_id);
		conn->timer_id = 0;
	}
}

static gboolean guard_timeout(gpointer data)
{
	struct sap_connection *conn = data;

	if (!conn)
		return FALSE;

	DBG("conn %p state %d pr 0x%02x", conn, conn->state,
						conn->processing_req);

	conn->timer_id = 0;

	switch (conn->state) {
	case SAP_STATE_DISCONNECTED:
		/* Client opened RFCOMM channel but didn't send CONNECT_REQ,
		 * in fixed time or client disconnected SAP connection but
		 * didn't closed RFCOMM channel in fixed time.*/
		if (conn->io) {
			g_io_channel_shutdown(conn->io, TRUE, NULL);
			g_io_channel_unref(conn->io);
		}
		break;

	case SAP_STATE_GRACEFUL_DISCONNECT:
		/* Client didn't disconnect SAP connection in fixed time,
		 * so close SAP connection immediately. */
		disconnect_req(conn, SAP_DISCONNECTION_TYPE_IMMEDIATE);
		break;

	default:
		error("Unexpected state (%d).", conn->state);
		break;
	}

	return FALSE;
}

int sap_connect_rsp(void *sap_device, uint8_t status, uint16_t maxmsgsize)
{
	struct sap_connection *conn = sap_device;
	char buf[SAP_BUF_SIZE];
	struct sap_message *msg = (struct sap_message *) buf;
	struct sap_parameter *param = (struct sap_parameter *) msg->param;
	size_t size = sizeof(struct sap_message);

	if (!conn)
		return -EINVAL;

	DBG("state %d pr 0x%02x status 0x%02x", conn->state,
						conn->processing_req, status);

	if (conn->state != SAP_STATE_CONNECT_IN_PROGRESS)
		return -EPERM;

	memset(buf, 0, sizeof(buf));
	msg->id = SAP_CONNECT_RESP;
	msg->nparam = 0x01;

	/* Add connection status */
	param->id = SAP_PARAM_ID_CONN_STATUS;
	param->len = htons(SAP_PARAM_ID_CONN_STATUS_LEN);
	*param->val = status;
	size += PARAMETER_SIZE(SAP_PARAM_ID_CONN_STATUS_LEN);

	/* Add MaxMsgSize */
	if (maxmsgsize && (status == SAP_STATUS_MAX_MSG_SIZE_NOT_SUPPORTED ||
				status == SAP_STATUS_MAX_MSG_SIZE_TOO_SMALL)) {
		uint16_t *len;

		msg->nparam++;
		param = (struct sap_parameter *) &buf[size];
		param->id = SAP_PARAM_ID_MAX_MSG_SIZE;
		param->len = htons(SAP_PARAM_ID_MAX_MSG_SIZE_LEN);
		len = (uint16_t *) &param->val;
		*len = htons(maxmsgsize);
		size += PARAMETER_SIZE(SAP_PARAM_ID_MAX_MSG_SIZE_LEN);
	}

	if (status == SAP_STATUS_OK) {
		gboolean connected = TRUE;

		emit_property_changed(connection, server->path,
						SAP_SERVER_INTERFACE,
			"Connected", DBUS_TYPE_BOOLEAN, &connected);

		conn->state = SAP_STATE_CONNECTED;
	} else {
		conn->state = SAP_STATE_DISCONNECTED;

		/* Timer will shutdown channel if client doesn't send
		 * CONNECT_REQ or doesn't shutdown channel itself.*/
		start_guard_timer(conn, SAP_TIMER_NO_ACTIVITY);
	}

	conn->processing_req = SAP_NO_REQ;

	return send_message(sap_device, buf, size);
}

int sap_disconnect_rsp(void *sap_device)
{
	struct sap_connection *conn = sap_device;
	struct sap_message msg;

	if (!conn)
		return -EINVAL;

	DBG("state %d pr 0x%02x", conn->state, conn->processing_req);

	switch (conn->state) {
	case SAP_STATE_CLIENT_DISCONNECT:
		memset(&msg, 0, sizeof(msg));
		msg.id = SAP_DISCONNECT_RESP;

		conn->state = SAP_STATE_DISCONNECTED;
		conn->processing_req = SAP_NO_REQ;

		/* Timer will close channel if client doesn't do it.*/
		start_guard_timer(conn, SAP_TIMER_NO_ACTIVITY);

		return send_message(sap_device, &msg, sizeof(msg));

	case SAP_STATE_IMMEDIATE_DISCONNECT:
		conn->state = SAP_STATE_DISCONNECTED;
		conn->processing_req = SAP_NO_REQ;

		if (conn->io) {
			g_io_channel_shutdown(conn->io, TRUE, NULL);
			g_io_channel_unref(conn->io);
		}

		return 0;

	default:
		break;
	}

	return 0;
}

int sap_transfer_apdu_rsp(void *sap_device, uint8_t result, uint8_t *apdu,
					uint16_t length)
{
	struct sap_connection *conn = sap_device;
	char buf[SAP_BUF_SIZE];
	struct sap_message *msg = (struct sap_message *) buf;
	struct sap_parameter *param = (struct sap_parameter *) msg->param;
	size_t size = sizeof(struct sap_message);

	if (!conn)
		return -EINVAL;

	DBG("state %d pr 0x%02x", conn->state, conn->processing_req);

	if (conn->processing_req != SAP_TRANSFER_APDU_REQ)
		return 0;

	if (result == SAP_RESULT_OK && (!apdu || (apdu && length == 0x00)))
		return -EINVAL;

	memset(buf, 0, sizeof(buf));
	msg->id = SAP_TRANSFER_APDU_RESP;
	msg->nparam = 0x01;
	size += add_result_parameter(result, param);

	/* Add APDU response. */
	if (result == SAP_RESULT_OK) {
		msg->nparam++;
		param = (struct sap_parameter *) &buf[size];
		param->id = SAP_PARAM_ID_RESPONSE_APDU;
		param->len = htons(length);

		size += PARAMETER_SIZE(length);

		if (size > SAP_BUF_SIZE)
			return -EOVERFLOW;

		memcpy(param->val, apdu, length);
	}

	conn->processing_req = SAP_NO_REQ;

	return send_message(sap_device, buf, size);
}

int sap_transfer_atr_rsp(void *sap_device, uint8_t result, uint8_t *atr,
					uint16_t length)
{
	struct sap_connection *conn = sap_device;
	char buf[SAP_BUF_SIZE];
	struct sap_message *msg = (struct sap_message *) buf;
	struct sap_parameter *param = (struct sap_parameter *) msg->param;
	size_t size = sizeof(struct sap_message);

	if (!conn)
		return -EINVAL;

	DBG("result 0x%02x state %d pr 0x%02x len %d", result, conn->state,
			conn->processing_req, length);

	if (conn->processing_req != SAP_TRANSFER_ATR_REQ)
		return 0;

	if (result == SAP_RESULT_OK && (!atr || (atr && length == 0x00)))
		return -EINVAL;

	memset(buf, 0, sizeof(buf));
	msg->id = SAP_TRANSFER_ATR_RESP;
	msg->nparam = 0x01;
	size += add_result_parameter(result, param);

	/* Add ATR response */
	if (result == SAP_RESULT_OK) {
		msg->nparam++;
		param = (struct sap_parameter *) &buf[size];
		param->id = SAP_PARAM_ID_ATR;
		param->len = htons(length);
		size += PARAMETER_SIZE(length);

		if (size > SAP_BUF_SIZE)
			return -EOVERFLOW;

		memcpy(param->val, atr, length);
	}

	conn->processing_req = SAP_NO_REQ;

	return send_message(sap_device, buf, size);
}

int sap_power_sim_off_rsp(void *sap_device, uint8_t result)
{
	struct sap_connection *conn = sap_device;
	char buf[SAP_BUF_SIZE];
	struct sap_message *msg = (struct sap_message *) buf;
	size_t size = sizeof(struct sap_message);

	if (!conn)
		return -EINVAL;

	DBG("state %d pr 0x%02x", conn->state, conn->processing_req);

	if (conn->processing_req != SAP_POWER_SIM_OFF_REQ)
		return 0;

	memset(buf, 0, sizeof(buf));
	msg->id = SAP_POWER_SIM_OFF_RESP;
	msg->nparam = 0x01;
	size += add_result_parameter(result, msg->param);

	conn->processing_req = SAP_NO_REQ;

	return send_message(sap_device, buf, size);
}

int sap_power_sim_on_rsp(void *sap_device, uint8_t result)
{
	struct sap_connection *conn = sap_device;
	char buf[SAP_BUF_SIZE];
	struct sap_message *msg = (struct sap_message *) buf;
	size_t size = sizeof(struct sap_message);

	if (!conn)
		return -EINVAL;

	DBG("state %d pr 0x%02x", conn->state, conn->processing_req);

	if (conn->processing_req != SAP_POWER_SIM_ON_REQ)
		return 0;

	memset(buf, 0, sizeof(buf));
	msg->id = SAP_POWER_SIM_ON_RESP;
	msg->nparam = 0x01;
	size += add_result_parameter(result, msg->param);

	conn->processing_req = SAP_NO_REQ;

	return send_message(sap_device, buf, size);
}

int sap_reset_sim_rsp(void *sap_device, uint8_t result)
{
	struct sap_connection *conn = sap_device;
	char buf[SAP_BUF_SIZE];
	struct sap_message *msg = (struct sap_message *) buf;
	size_t size = sizeof(struct sap_message);

	if (!conn)
		return -EINVAL;

	DBG("state %d pr 0x%02x result 0x%02x", conn->state,
					conn->processing_req, result);

	if (conn->processing_req != SAP_RESET_SIM_REQ)
		return 0;

	memset(buf, 0, sizeof(buf));
	msg->id = SAP_RESET_SIM_RESP;
	msg->nparam = 0x01;
	size += add_result_parameter(result, msg->param);

	conn->processing_req = SAP_NO_REQ;

	return send_message(sap_device, buf, size);
}

int sap_transfer_card_reader_status_rsp(void *sap_device, uint8_t result,
						uint8_t status)
{
	struct sap_connection *conn = sap_device;
	char buf[SAP_BUF_SIZE];
	struct sap_message *msg = (struct sap_message *) buf;
	struct sap_parameter *param = (struct sap_parameter *) msg->param;
	size_t size = sizeof(struct sap_message);

	if (!conn)
		return -EINVAL;

	DBG("state %d pr 0x%02x result 0x%02x", conn->state,
					conn->processing_req, result);

	if (conn->processing_req != SAP_TRANSFER_CARD_READER_STATUS_REQ)
		return 0;

	memset(buf, 0, sizeof(buf));
	msg->id = SAP_TRANSFER_CARD_READER_STATUS_RESP;
	msg->nparam = 0x01;
	size += add_result_parameter(result, param);

	/* Add card reader status. */
	if (result == SAP_RESULT_OK) {
		msg->nparam++;
		param = (struct sap_parameter *) &buf[size];
		param->id = SAP_PARAM_ID_CARD_READER_STATUS;
		param->len = htons(SAP_PARAM_ID_CARD_READER_STATUS_LEN);
		*param->val = status;
		size += PARAMETER_SIZE(SAP_PARAM_ID_CARD_READER_STATUS_LEN);
	}

	conn->processing_req = SAP_NO_REQ;

	return send_message(sap_device, buf, size);
}

int sap_transport_protocol_rsp(void *sap_device, uint8_t result)
{
	struct sap_connection *conn = sap_device;
	char buf[SAP_BUF_SIZE];
	struct sap_message *msg = (struct sap_message *) buf;
	size_t size = sizeof(struct sap_message);

	if (!conn)
		return -EINVAL;

	DBG("state %d pr 0x%02x result 0x%02x", conn->state,
					conn->processing_req, result);

	if (conn->processing_req != SAP_SET_TRANSPORT_PROTOCOL_REQ)
		return 0;

	memset(buf, 0, sizeof(buf));
	msg->id = SAP_SET_TRANSPORT_PROTOCOL_RESP;
	msg->nparam = 0x01;
	size += add_result_parameter(result, msg->param);

	conn->processing_req = SAP_NO_REQ;

	return send_message(sap_device, buf, size);
}

int sap_error_rsp(void *sap_device)
{
	struct sap_message msg;
	struct sap_connection *conn = sap_device;

	memset(&msg, 0, sizeof(msg));
	msg.id = SAP_ERROR_RESP;

	return send_message(conn, &msg, sizeof(msg));
}

int sap_status_ind(void *sap_device, uint8_t status_change)
{
	struct sap_connection *conn = sap_device;
	char buf[SAP_BUF_SIZE];
	struct sap_message *msg = (struct sap_message *) buf;
	struct sap_parameter *param = (struct sap_parameter *) msg->param;
	size_t size = sizeof(struct sap_message);

	if (!conn)
		return -EINVAL;

	DBG("state %d pr 0x%02x sc 0x%02x", conn->state, conn->processing_req,
				status_change);

	if (conn->state != SAP_STATE_CONNECTED &&
			conn->state != SAP_STATE_GRACEFUL_DISCONNECT)
		return 0;

	memset(buf, 0, sizeof(buf));
	msg->id = SAP_STATUS_IND;
	msg->nparam = 0x01;

	/* Add status change. */
	param->id  = SAP_PARAM_ID_STATUS_CHANGE;
	param->len = htons(SAP_PARAM_ID_STATUS_CHANGE_LEN);
	*param->val = status_change;
	size += PARAMETER_SIZE(SAP_PARAM_ID_STATUS_CHANGE_LEN);

	return send_message(sap_device, buf, size);
}

int sap_disconnect_ind(void *sap_device, uint8_t disc_type)
{
	struct sap_connection *conn = sap_device;

	return disconnect_req(conn, SAP_DISCONNECTION_TYPE_IMMEDIATE);
}

static int handle_cmd(void *data, void *buf, size_t size)
{
	struct sap_message *msg = buf;
	struct sap_connection *conn = data;

	if (!conn)
		return -EINVAL;

	if (size < sizeof(struct sap_message))
		goto error_rsp;

	if (msg->nparam != 0 && size < (sizeof(struct sap_message) +
					sizeof(struct sap_parameter) + 4))
		goto error_rsp;

	if (check_msg(msg) < 0)
		goto error_rsp;

	switch (msg->id) {
	case SAP_CONNECT_REQ:
		connect_req(conn, msg->param);
		return 0;
	case SAP_DISCONNECT_REQ:
		disconnect_req(conn, SAP_DISCONNECTION_TYPE_CLIENT);
		return 0;
	case SAP_TRANSFER_APDU_REQ:
		transfer_apdu_req(conn, msg->param);
		return 0;
	case SAP_TRANSFER_ATR_REQ:
		transfer_atr_req(conn);
		return 0;
	case SAP_POWER_SIM_OFF_REQ:
		power_sim_off_req(conn);
		return 0;
	case SAP_POWER_SIM_ON_REQ:
		power_sim_on_req(conn);
		return 0;
	case SAP_RESET_SIM_REQ:
		reset_sim_req(conn);
		return 0;
	case SAP_TRANSFER_CARD_READER_STATUS_REQ:
		transfer_card_reader_status_req(conn);
		return 0;
	case SAP_SET_TRANSPORT_PROTOCOL_REQ:
		set_transport_protocol_req(conn, msg->param);
		return 0;
	default:
		DBG("SAP unknown message.");
		break;
	}

error_rsp:
	DBG("Bad request message format.");
	sap_error_rsp(conn);
	return -EBADMSG;
}

static void sap_conn_remove(struct sap_connection *conn)
{
	DBG("conn %p", conn);

	if (!conn)
		return;

	if (conn->io) {
		g_io_channel_shutdown(conn->io, TRUE, NULL);
		g_io_channel_unref(conn->io);
	}

	conn->io = NULL;
	g_free(conn);
	server->conn = NULL;
}

static gboolean sap_io_cb(GIOChannel *io, GIOCondition cond, gpointer data)
{
	char buf[SAP_BUF_SIZE];
	size_t bytes_read = 0;
	GError *gerr = NULL;
	GIOStatus gstatus;

	DBG("io %p", io);

	if (cond & G_IO_NVAL) {
		DBG("ERR (G_IO_NVAL) on rfcomm socket.");
		return FALSE;
	}

	if (cond & G_IO_ERR) {
		DBG("ERR (G_IO_ERR) on rfcomm socket.");
		return FALSE;
	}

	if (cond & G_IO_HUP) {
		DBG("HUP on rfcomm socket.");
		return FALSE;
	}

	gstatus = g_io_channel_read_chars(io, buf, sizeof(buf) - 1,
				&bytes_read, &gerr);
	if (gstatus != G_IO_STATUS_NORMAL) {
		if (gerr)
			g_error_free(gerr);

		return TRUE;
	}

	if (handle_cmd(data, buf, bytes_read) < 0)
		error("Invalid SAP message.");

	return TRUE;
}

static void sap_io_destroy(void *data)
{
	struct sap_connection *conn = data;

	DBG("conn %p", conn);

	if (conn && conn->io) {
		gboolean connected = FALSE;

		stop_guard_timer(conn);

		if (conn->state != SAP_STATE_CONNECT_IN_PROGRESS)
			emit_property_changed(connection, server->path,
					SAP_SERVER_INTERFACE, "Connected",
					DBUS_TYPE_BOOLEAN, &connected);

		if (conn->state == SAP_STATE_CONNECT_IN_PROGRESS ||
				conn->state == SAP_STATE_CONNECTED ||
				conn->state == SAP_STATE_GRACEFUL_DISCONNECT)
			sap_disconnect_req(NULL, 1);

		conn->io = NULL;
		sap_conn_remove(conn);
	}
}

static void sap_connect_cb(GIOChannel *io, GError *gerr, gpointer data)
{
	struct sap_connection *conn = data;

	DBG("io %p gerr %p data %p ", io, gerr, data);

	if (!conn)
		return;

	/* Timer will shutdown the channel in case of lack of client
	   activity */
	start_guard_timer(conn, SAP_TIMER_NO_ACTIVITY);

	g_io_add_watch_full(io, G_PRIORITY_DEFAULT,
			G_IO_IN | G_IO_ERR | G_IO_HUP | G_IO_NVAL,
			sap_io_cb, conn, sap_io_destroy);
}

static void connect_auth_cb(DBusError *derr, void *data)
{
	struct sap_connection *conn = data;
	GError *gerr = NULL;

	DBG("derr %p data %p ", derr, data);

	if (!conn)
		return;

	if (derr && dbus_error_is_set(derr)) {
		error("Access denied: %s", derr->message);
		sap_conn_remove(conn);
		return;
	}

	if (!bt_io_accept(conn->io, sap_connect_cb, conn, NULL, &gerr)) {
		error("bt_io_accept: %s", gerr->message);
		g_error_free(gerr);
		sap_conn_remove(conn);
		return;
	}

	DBG("Client has been authorized.");
}

static void connect_confirm_cb(GIOChannel *io, gpointer data)
{
	struct sap_connection *conn = server->conn;
	GError *gerr = NULL;
	bdaddr_t src, dst;
	int err;

	DBG("io %p data %p ", io, data);

	if (!io)
		return;

	if (conn) {
		g_io_channel_shutdown(io, TRUE, NULL);
		return;
	}

	conn = g_try_new0(struct sap_connection, 1);
	if (!conn) {
		error("Can't allocate memory for incomming SAP connection.");
		g_io_channel_shutdown(io, TRUE, NULL);
		return;
	}

	g_io_channel_set_encoding(io, NULL, NULL);
	g_io_channel_set_buffered(io, FALSE);

	server->conn = conn;
	conn->io = g_io_channel_ref(io);
	conn->state = SAP_STATE_DISCONNECTED;

	bt_io_get(io, BT_IO_RFCOMM, &gerr,
			BT_IO_OPT_SOURCE_BDADDR, &src,
			BT_IO_OPT_DEST_BDADDR, &dst,
			BT_IO_OPT_INVALID);
	if (gerr) {
		error("%s", gerr->message);
		g_error_free(gerr);
		sap_conn_remove(conn);
		return;
	}

	err = btd_request_authorization(&src, &dst, SAP_UUID,
					connect_auth_cb, conn);
	if (err < 0) {
		DBG("Authorization denied: %d %s", err,  strerror(err));
		sap_conn_remove(conn);
		return;
	}

	DBG("SAP incoming connection (sock %d) authorization.",
				g_io_channel_unix_get_fd(io));
}

static inline DBusMessage *message_failed(DBusMessage *msg,
					const char *description)
{
	return g_dbus_create_error(msg, ERROR_INTERFACE ".Failed",
				"%s", description);
}

static DBusMessage *disconnect(DBusConnection *conn, DBusMessage *msg,
								void *data)
{
	struct sap_server *server = data;

	DBG("server %p", server);

	if (!server)
		return message_failed(msg, "Server internal error.");

	DBG("conn %p", server->conn);

	if (!server->conn)
		return message_failed(msg, "Client already disconnected");

	if (disconnect_req(server->conn, SAP_DISCONNECTION_TYPE_GRACEFUL) < 0)
		return g_dbus_create_error(msg, ERROR_INTERFACE	".Failed",
				"There is no active connection");

	return dbus_message_new_method_return(msg);
}

static DBusMessage *get_properties(DBusConnection *c,
				DBusMessage *msg, void *data)
{
	struct sap_connection *conn = data;
	DBusMessage *reply;
	DBusMessageIter iter;
	DBusMessageIter dict;
	dbus_bool_t connected;

	if (!conn)
		return message_failed(msg, "Server internal error.");

	reply = dbus_message_new_method_return(msg);
	if (!reply)
		return NULL;

	dbus_message_iter_init_append(reply, &iter);

	dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY,
			DBUS_DICT_ENTRY_BEGIN_CHAR_AS_STRING
			DBUS_TYPE_STRING_AS_STRING DBUS_TYPE_VARIANT_AS_STRING
			DBUS_DICT_ENTRY_END_CHAR_AS_STRING, &dict);

	connected = (conn->state == SAP_STATE_CONNECTED ||
			conn->state == SAP_STATE_GRACEFUL_DISCONNECT);
	dict_append_entry(&dict, "Connected", DBUS_TYPE_BOOLEAN, &connected);

	dbus_message_iter_close_container(&iter, &dict);

	return reply;
}

static GDBusMethodTable server_methods[] = {
	{"GetProperties", "", "a{sv}", get_properties},
	{"Disconnect", "", "", disconnect},
	{ }
};

static GDBusSignalTable server_signals[] = {
	{ "PropertyChanged", "sv"},
	{ }
};

static void server_free(struct sap_server *server)
{
	if (!server)
		return;

	sap_conn_remove(server->conn);
	g_free(server->path);
	g_free(server);
	server = NULL;
}

static void destroy_sap_interface(void *data)
{
	struct sap_server *server = data;

	DBG("Unregistered interface %s on path %s",
			SAP_SERVER_INTERFACE, server->path);

	server_free(server);
}

int sap_server_register(const char *path, bdaddr_t *src)
{
	sdp_record_t *record = NULL;
	GError *gerr = NULL;
	GIOChannel *io;

	if (sap_init() < 0) {
		error("Sap driver initialization failed.");
		return -1;
	}

	server = g_try_new0(struct sap_server, 1);
	if (!server) {
		sap_exit();
		return -ENOMEM;
	}

	bacpy(&server->src, src);
	server->path = g_strdup(path);

	record = create_sap_record(SAP_SERVER_CHANNEL);
	if (!record) {
		error("Creating SAP SDP record failed.");
		goto sdp_err;
	}

	if (add_record_to_server(&server->src, record) < 0) {
		error("Adding SAP SDP record to the SDP server failed.");
		sdp_record_free(record);
		goto sdp_err;
	}

	server->record_id = record->handle;

	io = bt_io_listen(BT_IO_RFCOMM, NULL, connect_confirm_cb, server,
			NULL, &gerr,
			BT_IO_OPT_SOURCE_BDADDR, &server->src,
			BT_IO_OPT_CHANNEL, SAP_SERVER_CHANNEL,
			BT_IO_OPT_SEC_LEVEL, BT_IO_SEC_HIGH,
			BT_IO_OPT_MASTER, TRUE,
			BT_IO_OPT_INVALID);
	if (!io) {
		error("Can't listen at channel %d.", SAP_SERVER_CHANNEL);
		g_error_free(gerr);
		goto server_err;
	}

	DBG("Listen socket 0x%02x", g_io_channel_unix_get_fd(io));

	server->listen_io = io;
	server->conn = NULL;

	if (!g_dbus_register_interface(connection, path, SAP_SERVER_INTERFACE,
				server_methods, server_signals, NULL,
				server, destroy_sap_interface)) {
		error("D-Bus failed to register %s interface",
						SAP_SERVER_INTERFACE);
		goto server_err;
	}

	return 0;

server_err:
	remove_record_from_server(server->record_id);
sdp_err:
	server_free(server);
	sap_exit();

	return -1;
}

int sap_server_unregister(const char *path)
{
	if (!server)
		return -EINVAL;

	remove_record_from_server(server->record_id);

	if (server->conn)
		sap_conn_remove(server->conn);

	if (server->listen_io) {
		g_io_channel_shutdown(server->listen_io, TRUE, NULL);
		g_io_channel_unref(server->listen_io);
		server->listen_io = NULL;
	}

	g_dbus_unregister_interface(connection, path, SAP_SERVER_INTERFACE);

	sap_exit();

	return 0;
}

int sap_server_init(DBusConnection *conn)
{
	connection = dbus_connection_ref(conn);
	return 0;
}

void sap_server_exit(void)
{
	dbus_connection_unref(connection);
	connection = NULL;
}
