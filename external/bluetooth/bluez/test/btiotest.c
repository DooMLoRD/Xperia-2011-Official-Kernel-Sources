/*
 *
 *  BlueZ - Bluetooth protocol stack for Linux
 *
 *  Copyright (C) 2009-2010  Marcel Holtmann <marcel@holtmann.org>
 *  Copyright (C) 2009-2010  Nokia Corporation
 *
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
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>
#include <signal.h>

#include <glib.h>

#include "btio.h"

#define DEFAULT_ACCEPT_TIMEOUT 2

struct io_data {
	guint ref;
	GIOChannel *io;
	BtIOType type;
	gint reject;
	gint disconn;
	gint accept;
};

static void io_data_unref(struct io_data *data)
{
	data->ref--;

	if (data->ref)
		return;

	if (data->io)
		g_io_channel_unref(data->io);

	g_free(data);
}

static struct io_data *io_data_ref(struct io_data *data)
{
	data->ref++;
	return data;
}

static struct io_data *io_data_new(GIOChannel *io, BtIOType type, gint reject,
						gint disconn, gint accept)
{
	struct io_data *data;

	data = g_new0(struct io_data, 1);
	data->io = io;
	data->type = type;
	data->reject = reject;
	data->disconn = disconn;
	data->accept = accept;

	return io_data_ref(data);
}

static gboolean io_watch(GIOChannel *io, GIOCondition cond, gpointer user_data)
{
	printf("Disconnected\n");
	return FALSE;
}

static gboolean disconn_timeout(gpointer user_data)
{
	struct io_data *data = user_data;

	printf("Disconnecting\n");

	g_io_channel_shutdown(data->io, TRUE, NULL);

	return FALSE;
}

static void connect_cb(GIOChannel *io, GError *err, gpointer user_data)
{
	struct io_data *data = user_data;
	GIOCondition cond;
	char addr[18];
	uint16_t handle;
	uint8_t cls[3];

	if (err) {
		printf("Connecting failed: %s\n", err->message);
		return;
	}

	if (!bt_io_get(io, data->type, &err,
			BT_IO_OPT_DEST, addr,
			BT_IO_OPT_HANDLE, &handle,
			BT_IO_OPT_CLASS, cls,
			BT_IO_OPT_INVALID)) {
		printf("Unable to get destination address: %s\n",
								err->message);
		g_clear_error(&err);
		strcpy(addr, "(unknown)");
	}

	printf("Successfully connected to %s. handle=%u, class=%02x%02x%02x\n",
			addr, handle, cls[0], cls[1], cls[2]);

	if (data->type == BT_IO_L2CAP || data->type == BT_IO_SCO) {
		uint16_t omtu, imtu;

		if (!bt_io_get(io, data->type, &err,
					BT_IO_OPT_OMTU, &omtu,
					BT_IO_OPT_IMTU, &imtu,
					BT_IO_OPT_INVALID)) {
			printf("Unable to get L2CAP MTU sizes: %s\n",
								err->message);
			g_clear_error(&err);
		} else
			printf("imtu=%u, omtu=%u\n", imtu, omtu);
	}

	if (data->disconn == 0) {
		g_io_channel_shutdown(io, TRUE, NULL);
		printf("Disconnected\n");
		return;
	}

	if (data->io == NULL)
		data->io = g_io_channel_ref(io);

	if (data->disconn > 0) {
		io_data_ref(data);
		g_timeout_add_seconds_full(G_PRIORITY_DEFAULT, data->disconn,
					disconn_timeout, data,
					(GDestroyNotify) io_data_unref);
	}


	io_data_ref(data);
	cond = G_IO_NVAL | G_IO_HUP | G_IO_ERR;
	g_io_add_watch_full(io, G_PRIORITY_DEFAULT, cond, io_watch, data,
					(GDestroyNotify) io_data_unref);
}

static gboolean confirm_timeout(gpointer user_data)
{
	struct io_data *data = user_data;

	if (data->reject >= 0) {
		printf("Rejecting connection\n");
		g_io_channel_shutdown(data->io, TRUE, NULL);
		return FALSE;
	}

	printf("Accepting connection\n");

	io_data_ref(data);

	if (!bt_io_accept(data->io, connect_cb, data,
				(GDestroyNotify) io_data_unref, NULL)) {
		printf("bt_io_accept() failed\n");
		io_data_unref(data);
	}

	return FALSE;
}

static void confirm_cb(GIOChannel *io, gpointer user_data)
{
	char addr[18];
	struct io_data *data = user_data;
	GError *err = NULL;

	if (!bt_io_get(io, data->type, &err, BT_IO_OPT_DEST, addr,
							BT_IO_OPT_INVALID)) {
		printf("bt_io_get(OPT_DEST): %s\n", err->message);
		g_clear_error(&err);
	} else
		printf("Got confirmation request for %s\n", addr);

	if (data->accept < 0 && data->reject < 0)
		return;

	if (data->reject == 0) {
		printf("Rejecting connection\n");
		g_io_channel_shutdown(io, TRUE, NULL);
		return;
	}

	data->io = g_io_channel_ref(io);
	io_data_ref(data);

	if (data->accept == 0) {
		if (!bt_io_accept(io, connect_cb, data,
					(GDestroyNotify) io_data_unref,
					&err)) {
			printf("bt_io_accept() failed: %s\n", err->message);
			g_clear_error(&err);
			io_data_unref(data);
			return;
		}
	} else {
		gint seconds = (data->reject > 0) ?
						data->reject : data->accept;
		g_timeout_add_seconds_full(G_PRIORITY_DEFAULT, seconds,
					confirm_timeout, data,
					(GDestroyNotify) io_data_unref);
	}
}

static void l2cap_connect(const char *src, const char *dst, uint16_t psm,
						gint disconn, gint sec)
{
	struct io_data *data;
	GError *err = NULL;

	printf("Connecting to %s L2CAP PSM %u\n", dst, psm);

	data = io_data_new(NULL, BT_IO_L2CAP, -1, disconn, -1);

	if (src)
		data->io = bt_io_connect(BT_IO_L2CAP, connect_cb, data,
						(GDestroyNotify) io_data_unref,
						&err,
						BT_IO_OPT_SOURCE, src,
						BT_IO_OPT_DEST, dst,
						BT_IO_OPT_PSM, psm,
						BT_IO_OPT_SEC_LEVEL, sec,
						BT_IO_OPT_INVALID);
	else
		data->io = bt_io_connect(BT_IO_L2CAP, connect_cb, data,
						(GDestroyNotify) io_data_unref,
						&err,
						BT_IO_OPT_DEST, dst,
						BT_IO_OPT_PSM, psm,
						BT_IO_OPT_SEC_LEVEL, sec,
						BT_IO_OPT_INVALID);

	if (!data->io) {
		printf("Connecting to %s failed: %s\n", dst, err->message);
		g_error_free(err);
		exit(EXIT_FAILURE);
	}
}

static void l2cap_listen(const char *src, uint16_t psm, gint defer,
				gint reject, gint disconn, gint accept,
				gint sec, gboolean master)
{
	struct io_data *data;
	BtIOConnect conn;
	BtIOConfirm cfm;
	GIOChannel *l2_srv;
	GError *err = NULL;

	if (defer) {
		conn = NULL;
		cfm = confirm_cb;
	} else {
		conn = connect_cb;
		cfm = NULL;
	}

	printf("Listening on L2CAP PSM %u\n", psm);

	data = io_data_new(NULL, BT_IO_L2CAP, reject, disconn, accept);

	if (src)
		l2_srv = bt_io_listen(BT_IO_L2CAP, conn, cfm,
					data, (GDestroyNotify) io_data_unref,
					&err,
					BT_IO_OPT_SOURCE, src,
					BT_IO_OPT_PSM, psm,
					BT_IO_OPT_SEC_LEVEL, sec,
					BT_IO_OPT_MASTER, master,
					BT_IO_OPT_INVALID);
	else
		l2_srv = bt_io_listen(BT_IO_L2CAP, conn, cfm,
					data, (GDestroyNotify) io_data_unref,
					&err,
					BT_IO_OPT_PSM, psm,
					BT_IO_OPT_SEC_LEVEL, sec,
					BT_IO_OPT_MASTER, master,
					BT_IO_OPT_INVALID);

	if (!l2_srv) {
		printf("Listening failed: %s\n", err->message);
		g_error_free(err);
		exit(EXIT_FAILURE);
	}

	g_io_channel_unref(l2_srv);
}

static void rfcomm_connect(const char *src, const char *dst, uint8_t ch,
						gint disconn, gint sec)
{
	struct io_data *data;
	GError *err = NULL;

	printf("Connecting to %s RFCOMM channel %u\n", dst, ch);

	data = io_data_new(NULL, BT_IO_RFCOMM, -1, disconn, -1);

	if (src)
		data->io = bt_io_connect(BT_IO_RFCOMM, connect_cb, data,
						(GDestroyNotify) io_data_unref,
						&err,
						BT_IO_OPT_SOURCE, src,
						BT_IO_OPT_DEST, dst,
						BT_IO_OPT_CHANNEL, ch,
						BT_IO_OPT_SEC_LEVEL, sec,
						BT_IO_OPT_INVALID);
	else
		data->io = bt_io_connect(BT_IO_RFCOMM, connect_cb, data,
						(GDestroyNotify) io_data_unref,
						&err,
						BT_IO_OPT_DEST, dst,
						BT_IO_OPT_CHANNEL, ch,
						BT_IO_OPT_SEC_LEVEL, sec,
						BT_IO_OPT_INVALID);

	if (!data->io) {
		printf("Connecting to %s failed: %s\n", dst, err->message);
		g_error_free(err);
		exit(EXIT_FAILURE);
	}
}

static void rfcomm_listen(const char *src, uint8_t ch, gboolean defer,
				gint reject, gint disconn, gint accept,
				gint sec, gboolean master)
{
	struct io_data *data;
	BtIOConnect conn;
	BtIOConfirm cfm;
	GIOChannel *rc_srv;
	GError *err = NULL;

	if (defer) {
		conn = NULL;
		cfm = confirm_cb;
	} else {
		conn = connect_cb;
		cfm = NULL;
	}

	data = io_data_new(NULL, BT_IO_RFCOMM, reject, disconn, accept);

	if (src)
		rc_srv = bt_io_listen(BT_IO_RFCOMM, conn, cfm,
					data, (GDestroyNotify) io_data_unref,
					&err,
					BT_IO_OPT_SOURCE, src,
					BT_IO_OPT_CHANNEL, ch,
					BT_IO_OPT_SEC_LEVEL, sec,
					BT_IO_OPT_MASTER, master,
					BT_IO_OPT_INVALID);
	else
		rc_srv = bt_io_listen(BT_IO_RFCOMM, conn, cfm,
					data, (GDestroyNotify) io_data_unref,
					&err,
					BT_IO_OPT_CHANNEL, ch,
					BT_IO_OPT_SEC_LEVEL, sec,
					BT_IO_OPT_MASTER, master,
					BT_IO_OPT_INVALID);

	if (!rc_srv) {
		printf("Listening failed: %s\n", err->message);
		g_error_free(err);
		exit(EXIT_FAILURE);
	}

	bt_io_get(rc_srv, BT_IO_RFCOMM, &err,
			BT_IO_OPT_CHANNEL, &ch,
			BT_IO_OPT_INVALID);

	printf("Listening on RFCOMM channel %u\n", ch);

	g_io_channel_unref(rc_srv);
}

static void sco_connect(const char *src, const char *dst, gint disconn)
{
	struct io_data *data;
	GError *err = NULL;

	printf("Connecting SCO to %s\n", dst);

	data = io_data_new(NULL, BT_IO_SCO, -1, disconn, -1);

	if (src)
		data->io = bt_io_connect(BT_IO_SCO, connect_cb, data,
						(GDestroyNotify) io_data_unref,
						&err,
						BT_IO_OPT_SOURCE, src,
						BT_IO_OPT_DEST, dst,
						BT_IO_OPT_INVALID);
	else
		data->io = bt_io_connect(BT_IO_SCO, connect_cb, data,
						(GDestroyNotify) io_data_unref,
						&err,
						BT_IO_OPT_DEST, dst,
						BT_IO_OPT_INVALID);

	if (!data->io) {
		printf("Connecting to %s failed: %s\n", dst, err->message);
		g_error_free(err);
		exit(EXIT_FAILURE);
	}
}

static void sco_listen(const char *src, gint disconn)
{
	struct io_data *data;
	GIOChannel *sco_srv;
	GError *err = NULL;

	printf("Listening for SCO connections\n");

	data = io_data_new(NULL, BT_IO_SCO, -1, disconn, -1);

	if (src)
		sco_srv = bt_io_listen(BT_IO_SCO, connect_cb, NULL,
					data, (GDestroyNotify) io_data_unref,
					&err,
					BT_IO_OPT_SOURCE, src,
					BT_IO_OPT_INVALID);
	else
		sco_srv = bt_io_listen(BT_IO_SCO, connect_cb, NULL,
					data, (GDestroyNotify) io_data_unref,
					&err, BT_IO_OPT_INVALID);

	if (!sco_srv) {
		printf("Listening failed: %s\n", err->message);
		g_error_free(err);
		exit(EXIT_FAILURE);
	}

	g_io_channel_unref(sco_srv);
}

static gint opt_channel = -1;
static gint opt_psm = 0;
static gboolean opt_sco = FALSE;
static gboolean opt_defer = FALSE;
static char *opt_dev = NULL;
static gint opt_reject = -1;
static gint opt_disconn = -1;
static gint opt_accept = DEFAULT_ACCEPT_TIMEOUT;
static gint opt_sec = 0;
static gboolean opt_master = FALSE;

static GMainLoop *main_loop;

static GOptionEntry options[] = {
	{ "channel", 'c', 0, G_OPTION_ARG_INT, &opt_channel,
				"RFCOMM channel" },
	{ "psm", 'p', 0, G_OPTION_ARG_INT, &opt_psm,
				"L2CAP PSM" },
	{ "sco", 's', 0, G_OPTION_ARG_NONE, &opt_sco,
				"Use SCO" },
	{ "defer", 'd', 0, G_OPTION_ARG_NONE, &opt_defer,
				"Use DEFER_SETUP for incoming connections" },
	{ "sec-level", 'S', 0, G_OPTION_ARG_INT, &opt_sec,
				"Security level" },
	{ "dev", 'i', 0, G_OPTION_ARG_STRING, &opt_dev,
				"Which HCI device to use" },
	{ "reject", 'r', 0, G_OPTION_ARG_INT, &opt_reject,
				"Reject connection after N seconds" },
	{ "disconnect", 'D', 0, G_OPTION_ARG_INT, &opt_disconn,
				"Disconnect connection after N seconds" },
	{ "accept", 'a', 0, G_OPTION_ARG_INT, &opt_accept,
				"Accept connection after N seconds" },
	{ "master", 'm', 0, G_OPTION_ARG_NONE, &opt_master,
				"Master role switch (incoming connections)" },
	{ NULL },
};

static void sig_term(int sig)
{
	g_main_loop_quit(main_loop);
}

int main(int argc, char *argv[])
{
	GOptionContext *context;

	context = g_option_context_new(NULL);
	g_option_context_add_main_entries(context, options, NULL);

	if (!g_option_context_parse(context, &argc, &argv, NULL))
		exit(EXIT_FAILURE);

	g_option_context_free(context);

	printf("accept=%d, reject=%d, discon=%d, defer=%d, sec=%d\n",
		opt_accept, opt_reject, opt_disconn, opt_defer, opt_sec);

	if (opt_psm) {
		if (argc > 1)
			l2cap_connect(opt_dev, argv[1], opt_psm,
							opt_disconn, opt_sec);
		else
			l2cap_listen(opt_dev, opt_psm, opt_defer, opt_reject,
					opt_disconn, opt_accept, opt_sec,
					opt_master);
	}

	if (opt_channel != -1) {
		if (argc > 1)
			rfcomm_connect(opt_dev, argv[1], opt_channel,
							opt_disconn, opt_sec);
		else
			rfcomm_listen(opt_dev, opt_channel, opt_defer,
					opt_reject, opt_disconn, opt_accept,
					opt_sec, opt_master);
	}

	if (opt_sco) {
		if (argc > 1)
			sco_connect(opt_dev, argv[1], opt_disconn);
		else
			sco_listen(opt_dev, opt_disconn);
	}

	signal(SIGTERM, sig_term);
	signal(SIGINT, sig_term);

	main_loop = g_main_loop_new(NULL, FALSE);

	g_main_loop_run(main_loop);

	g_main_loop_unref(main_loop);

	printf("Exiting\n");

	exit(EXIT_SUCCESS);
}
