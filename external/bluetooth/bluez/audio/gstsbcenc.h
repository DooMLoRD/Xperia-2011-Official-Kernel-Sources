/*
 *
 *  BlueZ - Bluetooth protocol stack for Linux
 *
 *  Copyright (C) 2004-2010  Marcel Holtmann <marcel@holtmann.org>
 *
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

#include <gst/gst.h>
#include <gst/base/gstadapter.h>

#include "sbc.h"

G_BEGIN_DECLS

#define GST_TYPE_SBC_ENC \
	(gst_sbc_enc_get_type())
#define GST_SBC_ENC(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_SBC_ENC,GstSbcEnc))
#define GST_SBC_ENC_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_SBC_ENC,GstSbcEncClass))
#define GST_IS_SBC_ENC(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_SBC_ENC))
#define GST_IS_SBC_ENC_CLASS(obj) \
	(G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_SBC_ENC))

typedef struct _GstSbcEnc GstSbcEnc;
typedef struct _GstSbcEncClass GstSbcEncClass;

struct _GstSbcEnc {
	GstElement element;

	GstPad *sinkpad;
	GstPad *srcpad;
	GstAdapter *adapter;

	gint rate;
	gint channels;
	gint mode;
	gint blocks;
	gint allocation;
	gint subbands;
	gint bitpool;

	guint codesize;
	gint frame_length;
	gint frame_duration;

	sbc_t sbc;
};

struct _GstSbcEncClass {
	GstElementClass parent_class;
};

GType gst_sbc_enc_get_type(void);

gboolean gst_sbc_enc_plugin_init(GstPlugin *plugin);

G_END_DECLS
