/*
 *
 *  BlueZ - Bluetooth protocol stack for Linux
 *
 *  Copyright (C) 2006-2010  Nokia Corporation
 *  Copyright (C) 2004-2010  Marcel Holtmann <marcel@holtmann.org>
 *  Copyright (C) 2010 Sony Ericsson Mobile Communications AB
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#define _GNU_SOURCE
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>
#include <bluetooth/sdp.h>

#include <glib.h>
#include <dbus/dbus.h>
#include <gdbus.h>

#include "log.h"
#include "textfile.h"

#include "hcid.h"
#include "manager.h"
#include "adapter.h"
#include "device.h"
#include "error.h"
#include "glib-helper.h"
#include "dbus-common.h"
#include "agent.h"
#include "storage.h"
#include "dbus-hci.h"

struct oob_availability_req {
        struct btd_device *device;
        uint8_t auth;
        uint8_t capa;
};

static DBusConnection *connection = NULL;

gboolean get_adapter_and_device(bdaddr_t *src, bdaddr_t *dst,
					struct btd_adapter **adapter,
					struct btd_device **device,
					gboolean create)
{
	char peer_addr[18];

	*adapter = manager_find_adapter(src);
	if (!*adapter) {
		error("Unable to find matching adapter");
		return FALSE;
	}

	ba2str(dst, peer_addr);

	if (create)
		*device = adapter_get_device(connection, *adapter, peer_addr);
	else
		*device = adapter_find_device(*adapter, peer_addr);

	if (create && !*device) {
		error("Unable to get device object!");
		return FALSE;
	}

	return TRUE;
}

const char *class_to_icon(uint32_t class)
{
	switch ((class & 0x1f00) >> 8) {
	case 0x01:
		return "computer";
	case 0x02:
		switch ((class & 0xfc) >> 2) {
		case 0x01:
		case 0x02:
		case 0x03:
		case 0x05:
			return "phone";
		case 0x04:
			return "modem";
		}
		break;
	case 0x03:
		return "network-wireless";
	case 0x04:
		switch ((class & 0xfc) >> 2) {
		case 0x01:
		case 0x02:
			return "audio-card";	/* Headset */
		case 0x06:
			return "audio-card";	/* Headphone */
		case 0x0b: /* VCR */
		case 0x0c: /* Video Camera */
		case 0x0d: /* Camcorder */
			return "camera-video";
		default:
			return "audio-card";	/* Other audio device */
		}
		break;
	case 0x05:
		switch ((class & 0xc0) >> 6) {
		case 0x00:
			switch ((class & 0x1e) >> 2) {
			case 0x01:
			case 0x02:
				return "input-gaming";
			}
			break;
		case 0x01:
			return "input-keyboard";
		case 0x02:
			switch ((class & 0x1e) >> 2) {
			case 0x05:
				return "input-tablet";
			default:
				return "input-mouse";
			}
		}
		break;
	case 0x06:
		if (class & 0x80)
			return "printer";
		if (class & 0x20)
			return "camera-photo";
		break;
	}

	return NULL;
}

/*****************************************************************
 *
 *  Section reserved to HCI commands confirmation handling and low
 *  level events(eg: device attached/dettached.
 *
 *****************************************************************/

static void pincode_cb(struct agent *agent, DBusError *err,
				const char *pincode, struct btd_device *device)
{
	struct btd_adapter *adapter = device_get_adapter(device);
	pin_code_reply_cp pr;
	bdaddr_t sba, dba;
	size_t len;
	int dev;
	uint16_t dev_id = adapter_get_dev_id(adapter);

	dev = hci_open_dev(dev_id);
	if (dev < 0) {
		error("hci_open_dev(%d): %s (%d)", dev_id,
				strerror(errno), errno);
		return;
	}

	adapter_get_address(adapter, &sba);
	device_get_address(device, &dba);

	if (err) {
		hci_send_cmd(dev, OGF_LINK_CTL,
				OCF_PIN_CODE_NEG_REPLY, 6, &dba);
		goto done;
	}

	len = strlen(pincode);

	set_pin_length(&sba, len);

	memset(&pr, 0, sizeof(pr));
	bacpy(&pr.bdaddr, &dba);
	memcpy(pr.pin_code, pincode, len);
	pr.pin_len = len;
	hci_send_cmd(dev, OGF_LINK_CTL, OCF_PIN_CODE_REPLY,
						PIN_CODE_REPLY_CP_SIZE, &pr);

done:
	hci_close_dev(dev);
}

int hcid_dbus_request_pin(int dev, bdaddr_t *sba, struct hci_conn_info *ci)
{
	struct btd_adapter *adapter;
	struct btd_device *device;

	if (!get_adapter_and_device(sba, &ci->bdaddr, &adapter, &device, TRUE))
		return -ENODEV;

	/* Check if the adapter is not pairable and if there isn't a bonding in
	 * progress */
	if (!adapter_is_pairable(adapter) && !device_is_bonding(device, NULL))
		return -EPERM;

	return device_request_authentication(device, AUTH_TYPE_PINCODE, 0,
								pincode_cb);
}

static int confirm_reply(struct btd_adapter *adapter,
				struct btd_device *device, gboolean success)
{
	int dd;
	user_confirm_reply_cp cp;
	uint16_t dev_id = adapter_get_dev_id(adapter);

	dd = hci_open_dev(dev_id);
	if (dd < 0) {
		error("Unable to open hci%d", dev_id);
		return dd;
	}

	memset(&cp, 0, sizeof(cp));
	device_get_address(device, &cp.bdaddr);

	if (success)
		hci_send_cmd(dd, OGF_LINK_CTL, OCF_USER_CONFIRM_REPLY,
					USER_CONFIRM_REPLY_CP_SIZE, &cp);
	else
		hci_send_cmd(dd, OGF_LINK_CTL, OCF_USER_CONFIRM_NEG_REPLY,
					USER_CONFIRM_REPLY_CP_SIZE, &cp);

	hci_close_dev(dd);

	return 0;
}

static void confirm_cb(struct agent *agent, DBusError *err, void *user_data)
{
	struct btd_device *device = user_data;
	struct btd_adapter *adapter = device_get_adapter(device);
	gboolean success = (err == NULL) ? TRUE : FALSE;

	confirm_reply(adapter, device, success);
}

static void passkey_cb(struct agent *agent, DBusError *err, uint32_t passkey,
			void *user_data)
{
	struct btd_device *device = user_data;
	struct btd_adapter *adapter = device_get_adapter(device);
	user_passkey_reply_cp cp;
	bdaddr_t dba;
	int dd;
	uint16_t dev_id = adapter_get_dev_id(adapter);

	dd = hci_open_dev(dev_id);
	if (dd < 0) {
		error("Unable to open hci%d", dev_id);
		return;
	}

	device_get_address(device, &dba);

	memset(&cp, 0, sizeof(cp));
	bacpy(&cp.bdaddr, &dba);
	cp.passkey = passkey;

	if (err)
		hci_send_cmd(dd, OGF_LINK_CTL,
				OCF_USER_PASSKEY_NEG_REPLY, 6, &dba);
	else
		hci_send_cmd(dd, OGF_LINK_CTL, OCF_USER_PASSKEY_REPLY,
					USER_PASSKEY_REPLY_CP_SIZE, &cp);

	hci_close_dev(dd);
}

static void oob_data_cb(struct  agent *agent, DBusError *err, uint8_t *hash,
				uint8_t *randomizer, void *user_data)
{
	struct btd_device *device = user_data;
	struct btd_adapter *adapter = device_get_adapter(device);
	remote_oob_data_reply_cp cp;
	bdaddr_t dba;
	int dd;
	uint16_t dev_id = adapter_get_dev_id(adapter);

	dd = hci_open_dev(dev_id);
	if (dd < 0) {
		error("Unable to open hci%d", dev_id);
		return;
	}

	device_get_address(device, &dba);
	memset(&cp, 0, sizeof(cp));
	bacpy(&cp.bdaddr, &dba);

	memcpy(&cp.hash, hash, 16);
	memcpy(&cp.randomizer, randomizer, 16);

	if (err)
                hci_send_cmd(dd, OGF_LINK_CTL, OCF_REMOTE_OOB_DATA_NEG_REPLY,
                                6, &dba);

	else
                hci_send_cmd(dd, OGF_LINK_CTL, OCF_REMOTE_OOB_DATA_REPLY,
                                REMOTE_OOB_DATA_REPLY_CP_SIZE, &cp);

	hci_close_dev(dd);
}

static void io_capa_oob_response(struct btd_adapter *adapter, struct btd_device *device,
					uint8_t cap, uint8_t auth, gboolean oob)
{
        io_capability_reply_cp cp;
	int dd;
	uint16_t dev_id = adapter_get_dev_id(adapter);

	dd = hci_open_dev(dev_id);
	if (dd < 0) {
		error("Unable to open hci%d", dev_id);
		return;
	}
	memset(&cp, 0, sizeof(cp));
	device_get_address(device, &cp.bdaddr);

	cp.capability = cap;
	if (oob)
		cp.oob_data = 0x01;
	else
		cp.oob_data = 0x00;
	cp.authentication = auth;

	hci_send_cmd(dd, OGF_LINK_CTL, OCF_IO_CAPABILITY_REPLY,
			IO_CAPABILITY_REPLY_CP_SIZE, &cp);
	hci_close_dev(dd);
}

static void oob_availability_cb(struct agent *agent, DBusError *err,
					void *user_data)
{
	struct oob_availability_req *oob = user_data;
	struct btd_device *device = oob->device;
	struct btd_adapter *adapter = device_get_adapter(device);

	if (err) {
		io_capa_oob_response(adapter, device, oob->capa, oob->auth, FALSE);
	} else {
		io_capa_oob_response(adapter, device, oob->capa, oob->auth, TRUE);
	}
}


static void pairing_consent_cb(struct agent *agent, DBusError *err,
					void *user_data)
{
	struct btd_device *device = user_data;
	struct btd_adapter *adapter = device_get_adapter(device);
	user_confirm_reply_cp cp;
	int dd;
	uint16_t dev_id = adapter_get_dev_id(adapter);

	dd = hci_open_dev(dev_id);
	if (dd < 0) {
		error("Unable to open hci%d", dev_id);
		return;
	}

	memset(&cp, 0, sizeof(cp));
	device_get_address(device, &cp.bdaddr);

	if (err)
		hci_send_cmd(dd, OGF_LINK_CTL, OCF_USER_CONFIRM_NEG_REPLY,
					USER_CONFIRM_REPLY_CP_SIZE, &cp);
	else
		hci_send_cmd(dd, OGF_LINK_CTL, OCF_USER_CONFIRM_REPLY,
					USER_CONFIRM_REPLY_CP_SIZE, &cp);

	hci_close_dev(dd);
}

static int get_auth_requirements(bdaddr_t *local, bdaddr_t *remote,
							uint8_t *auth)
{
	struct hci_auth_info_req req;
	char addr[18];
	int err, dd, dev_id;

	ba2str(local, addr);

	dev_id = hci_devid(addr);
	if (dev_id < 0)
		return dev_id;

	dd = hci_open_dev(dev_id);
	if (dd < 0)
		return dd;

	memset(&req, 0, sizeof(req));
	bacpy(&req.bdaddr, remote);

	err = ioctl(dd, HCIGETAUTHINFO, (unsigned long) &req);
	if (err < 0) {
		DBG("HCIGETAUTHINFO failed: %s (%d)",
					strerror(errno), errno);
		hci_close_dev(dd);
		return err;
	}

	hci_close_dev(dd);

	if (auth)
		*auth = req.type;

	return 0;
}

int hcid_dbus_user_confirm(bdaddr_t *sba, bdaddr_t *dba, uint32_t passkey)
{
	struct btd_adapter *adapter;
	struct btd_device *device;
	struct agent *agent;
	uint8_t rem_cap, rem_auth, loc_cap, loc_auth;
	gboolean bonding_initiator;

	if (!get_adapter_and_device(sba, dba, &adapter, &device, TRUE))
		return -ENODEV;

	if (get_auth_requirements(sba, dba, &loc_auth) < 0) {
		error("Unable to get local authentication requirements");
		goto fail;
	}

	agent = device_get_agent(device);
	if (agent == NULL) {
		error("No agent available for user confirmation");
		goto fail;
	}

	loc_cap = agent_get_io_capability(agent);

	DBG("confirm IO capabilities are 0x%02x", loc_cap);
	DBG("confirm authentication requirement is 0x%02x", loc_auth);

	rem_cap = device_get_cap(device);
	rem_auth = device_get_auth(device);

	DBG("remote IO capabilities are 0x%02x", rem_cap);
	DBG("remote authentication requirement is 0x%02x", rem_auth);

	/* If we require MITM but the remote device can't provide that
	 * (it has NoInputNoOutput) then reject the confirmation
	 * request. The only exception is when we're dedicated bonding
	 * initiators since then we always have the MITM bit set. */
	bonding_initiator = device_is_bonding(device, NULL);
	if (!bonding_initiator && (loc_auth & 0x01) && rem_cap == 0x03) {
		error("Rejecting request: remote device can't provide MITM");
		goto fail;
	}

	/* If local IO capabilities are DisplayYesNo and remote IO
	 * capabiltiies are DisplayOnly or NoInputNoOutput;
	 * call PairingConsent callback for incoming requests. */
	if ((loc_cap == 0x01) &&
		        (rem_cap == 0x00 || rem_cap == 0x03))
		return device_request_authentication(device,
				AUTH_TYPE_PAIRING_CONSENT, 0,
				pairing_consent_cb);

	/* If no side requires MITM protection; auto-accept */
	if ((loc_auth == 0xff || !(loc_auth & 0x01) || rem_cap == 0x03) &&
				(!(rem_auth & 0x01) || loc_cap == 0x03)) {
		DBG("auto accept of confirmation");

		/* Wait 5 milliseconds before doing auto-accept */
		usleep(5000);

		if (confirm_reply(adapter, device, TRUE) < 0)
			return -EIO;

		return device_request_authentication(device, AUTH_TYPE_AUTO,
								0, NULL);
	}

	return device_request_authentication(device, AUTH_TYPE_CONFIRM,
							passkey, confirm_cb);

fail:
	return confirm_reply(adapter, device, FALSE);
}

int hcid_dbus_user_passkey(bdaddr_t *sba, bdaddr_t *dba)
{
	struct btd_adapter *adapter;
	struct btd_device *device;

	if (!get_adapter_and_device(sba, dba, &adapter, &device, TRUE))
		return -ENODEV;

	return device_request_authentication(device, AUTH_TYPE_PASSKEY, 0,
								passkey_cb);
}

int hcid_dbus_user_notify(bdaddr_t *sba, bdaddr_t *dba, uint32_t passkey)
{
	struct btd_adapter *adapter;
	struct btd_device *device;

	if (!get_adapter_and_device(sba, dba, &adapter, &device, TRUE))
		return -ENODEV;

	return device_request_authentication(device, AUTH_TYPE_NOTIFY,
								passkey, NULL);
}

void hcid_dbus_bonding_process_complete(bdaddr_t *local, bdaddr_t *peer,
								uint8_t status)
{
	struct btd_adapter *adapter;
	struct btd_device *device;

	DBG("status=%02x", status);

	if (!get_adapter_and_device(local, peer, &adapter, &device, TRUE))
		return;

	if (!device_is_authenticating(device) && !device_is_bonding(device, NULL)) {
		/* This means that there was no pending PIN or SSP token
		 * request from the controller, i.e. this is not a new
		 * pairing */
		DBG("no pending auth request");
		return;
	}

	/* If this is a new pairing send the appropriate reply and signal for
	 * it and proceed with service discovery */
	device_bonding_complete(device, status);
}

void hcid_dbus_simple_pairing_complete(bdaddr_t *local, bdaddr_t *peer,
								uint8_t status)
{
	struct btd_adapter *adapter;
	struct btd_device *device;

	DBG("status=%02x", status);

	if (!get_adapter_and_device(local, peer, &adapter, &device, TRUE))
		return;

	device_simple_pairing_complete(device, status);
}

static char *extract_eir_name(uint8_t *data, uint8_t *type)
{
	if (!data || !type)
		return NULL;

	if (data[0] == 0)
		return NULL;

	*type = data[1];

	switch (*type) {
	case 0x08:
	case 0x09:
		return strndup((char *) (data + 2), data[0] - 1);
	}

	return NULL;
}

void hcid_dbus_inquiry_result(bdaddr_t *local, bdaddr_t *peer, uint32_t class,
				int8_t rssi, uint8_t *data)
{
	char filename[PATH_MAX + 1];
	struct btd_adapter *adapter;
	struct btd_device *device;
	char local_addr[18], peer_addr[18], *alias, *name, *tmp_name;
	struct remote_dev_info *dev, match;
	uint8_t name_type = 0x00;
	name_status_t name_status;
	int state;
	dbus_bool_t legacy;
	unsigned char features[8];

	ba2str(local, local_addr);
	ba2str(peer, peer_addr);

	if (!get_adapter_and_device(local, peer, &adapter, &device, FALSE)) {
		error("No matching adapter found");
		return;
	}

	write_remote_class(local, peer, class);

	if (data)
		write_remote_eir(local, peer, data);

	/*
	 * workaround to identify situation when the daemon started and
	 * a standard inquiry or periodic inquiry was already running
	 */
	if (!(adapter_get_state(adapter) & STD_INQUIRY) &&
			!(adapter_get_state(adapter) & PERIODIC_INQUIRY)) {
		state = adapter_get_state(adapter);
		state |= PERIODIC_INQUIRY;
		adapter_set_state(adapter, state);
	}

	memset(&match, 0, sizeof(struct remote_dev_info));
	bacpy(&match.bdaddr, peer);
	match.name_status = NAME_SENT;
	/* if found: don't send the name again */
	dev = adapter_search_found_devices(adapter, &match);
	if (dev) {
		adapter_update_found_devices(adapter, peer, rssi, class,
						NULL, NULL, dev->legacy,
						NAME_NOT_REQUIRED);
		return;
	}

	/* the inquiry result can be triggered by NON D-Bus client */
	if (adapter_get_state(adapter) & RESOLVE_NAME)
		name_status = NAME_REQUIRED;
	else
		name_status = NAME_NOT_REQUIRED;

	create_name(filename, PATH_MAX, STORAGEDIR, local_addr, "aliases");
	alias = textfile_get(filename, peer_addr);

	create_name(filename, PATH_MAX, STORAGEDIR, local_addr, "names");
	name = textfile_get(filename, peer_addr);

	if (data)
		legacy = FALSE;
	else if (name == NULL)
		legacy = TRUE;
	else if (read_remote_features(local, peer, NULL, features) == 0) {
		if (features[0] & 0x01)
			legacy = FALSE;
		else
			legacy = TRUE;
	} else
		legacy = TRUE;

	tmp_name = extract_eir_name(data, &name_type);
	if (tmp_name) {
		if (name_type == 0x09) {
			write_device_name(local, peer, tmp_name);
			name_status = NAME_NOT_REQUIRED;

			if (name)
				g_free(name);

			name = tmp_name;
		} else {
			if (name)
				free(tmp_name);
			else
				name = tmp_name;
		}
	}

	if (name && name_type != 0x08)
		name_status = NAME_SENT;

	/* add in the list to track name sent/pending */
	adapter_update_found_devices(adapter, peer, rssi, class, name, alias,
					legacy, name_status);

	g_free(name);
	g_free(alias);
}

void hcid_dbus_set_legacy_pairing(bdaddr_t *local, bdaddr_t *peer,
							gboolean legacy)
{
	struct btd_adapter *adapter;
	struct btd_device *device;
	struct remote_dev_info *dev, match;

	if (!get_adapter_and_device(local, peer, &adapter, &device, FALSE)) {
		error("No matching adapter found");
		return;
	}

	memset(&match, 0, sizeof(struct remote_dev_info));
	bacpy(&match.bdaddr, peer);
	match.name_status = NAME_ANY;

	dev = adapter_search_found_devices(adapter, &match);
	if (dev)
		dev->legacy = legacy;
}

void hcid_dbus_remote_class(bdaddr_t *local, bdaddr_t *peer, uint32_t class)
{
	uint32_t old_class = 0;
	struct btd_adapter *adapter;
	struct btd_device *device;
	const gchar *dev_path;

	read_remote_class(local, peer, &old_class);

	if (old_class == class)
		return;

	if (!get_adapter_and_device(local, peer, &adapter, &device, FALSE))
		return;

	if (!device)
		return;

	dev_path = device_get_path(device);

	emit_property_changed(connection, dev_path, DEVICE_INTERFACE, "Class",
				DBUS_TYPE_UINT32, &class);
}

void hcid_dbus_remote_name(bdaddr_t *local, bdaddr_t *peer, uint8_t status,
				char *name)
{
	struct btd_adapter *adapter;
	char srcaddr[18], dstaddr[18];
	int state;
	struct btd_device *device;
	struct remote_dev_info match, *dev_info;

	if (!get_adapter_and_device(local, peer, &adapter, &device, FALSE))
		return;

	ba2str(local, srcaddr);
	ba2str(peer, dstaddr);

	if (status != 0)
		goto proceed;

	bacpy(&match.bdaddr, peer);
	match.name_status = NAME_ANY;

	dev_info = adapter_search_found_devices(adapter, &match);
	if (dev_info) {
		g_free(dev_info->name);
		dev_info->name = g_strdup(name);
		adapter_emit_device_found(adapter, dev_info);
	}

	if (device)
		device_set_name(device, name);

proceed:
	/* remove from remote name request list */
	adapter_remove_found_device(adapter, peer);

	/* check if there is more devices to request names */
	if (adapter_resolve_names(adapter) == 0)
		return;

	state = adapter_get_state(adapter);
	state &= ~PERIODIC_INQUIRY;
	state &= ~STD_INQUIRY;
	adapter_set_state(adapter, state);
}

int hcid_dbus_link_key_notify(bdaddr_t *local, bdaddr_t *peer,
				uint8_t *key, uint8_t key_type,
				int pin_length, uint8_t old_key_type)
{
	struct btd_device *device;
	struct btd_adapter *adapter;
	uint8_t local_auth = 0xff, remote_auth, new_key_type;
	gboolean bonding, temporary = FALSE;

	if (!get_adapter_and_device(local, peer, &adapter, &device, TRUE))
		return -ENODEV;

	new_key_type = key_type;

	if (key_type == 0x06) {
		if (device_get_debug_key(device, NULL))
			old_key_type = 0x03;
		if (old_key_type != 0xff)
			new_key_type = old_key_type;
		else
			/* This is Changed Combination Link Key for
			 * a temporary link key.*/
			return 0;
	}

	get_auth_requirements(local, peer, &local_auth);
	remote_auth = device_get_auth(device);
	bonding = device_is_bonding(device, NULL);

	DBG("key type 0x%02x old key type 0x%02x new key type 0x%02x",
					key_type, old_key_type, new_key_type);

	DBG("local auth 0x%02x and remote auth 0x%02x",
					local_auth, remote_auth);

	/* Clear any previous debug key */
	device_set_debug_key(device, NULL);

	/* If this is not the first link key set a flag so a subsequent auth
	 * complete event doesn't trigger SDP and remove any stored key */
	if (old_key_type != 0xff) {
		device_set_renewed_key(device, TRUE);
		device_remove_bonding(device);
	}

	/* Store the link key only in runtime memory if it's a debug
	 * key, else store the link key persistently if one of the
	 * following is true:
	 * 1. this is a legacy link key
	 * 2. this is a changed combination key and there was a previously
	 *    stored one
	 * 3. neither local nor remote side had no-bonding as a requirement
	 * 4. the local side had dedicated bonding as a requirement
	 * 5. the remote side is using dedicated bonding since in that case
	 *    also the local requirements are set to dedicated bonding
	 * If none of the above match only keep the link key around for
	 * this connection and set the temporary flag for the device.
	 */
	if (new_key_type == 0x03) {
		DBG("Storing debug key in runtime memory");
		device_set_debug_key(device, key);
	} else if (key_type < 0x03 ||
				(key_type == 0x06 && old_key_type != 0xff) ||
				(local_auth > 0x01 && remote_auth > 0x01) ||
				(local_auth == 0x02 || local_auth == 0x03) ||
				(remote_auth == 0x02 || remote_auth == 0x03)) {
		int err;

		DBG("storing link key of type 0x%02x", key_type);

		err = write_link_key(local, peer, key, new_key_type,
								pin_length);
		if (err < 0) {
			error("write_link_key: %s (%d)", strerror(-err), -err);
			return err;
		}
	} else
		temporary = TRUE;

	if (!device_is_connected(device))
		device_set_secmode3_conn(device, TRUE);
	else if (!bonding && old_key_type == 0xff)
		hcid_dbus_bonding_process_complete(local, peer, 0);

	device_set_temporary(device, temporary);

	return 0;
}

int hcid_dbus_get_oob_data(bdaddr_t *sba, bdaddr_t * dba)
{
	struct btd_adapter *adapter;
	struct btd_device *device;
	struct agent *agent = NULL;

        if (!get_adapter_and_device(sba, dba, &adapter, &device, TRUE))
	        return -ENODEV;

	agent = device_get_agent(device);
	if (agent == NULL) {
		error("No agent available for device");
		return -1;
	}

	return device_request_authentication(device, AUTH_TYPE_OOB, 0,
						oob_data_cb);
}

void hcid_dbus_conn_complete(bdaddr_t *local, uint8_t status, uint16_t handle,
				bdaddr_t *peer)
{
	struct btd_adapter *adapter;
	struct btd_device *device;

	if (!get_adapter_and_device(local, peer, &adapter, &device, TRUE))
		return;

	if (status) {
		gboolean secmode3 = device_get_secmode3_conn(device);

		device_set_secmode3_conn(device, FALSE);

		if (status == 0x0e || status == 0x18) {
			device_set_temporary(device, TRUE);
			secmode3 = TRUE;
		}
		if (device_is_bonding(device, NULL))
			device_bonding_complete(device, status);
		if (device_is_temporary(device))
			adapter_remove_device(connection, adapter, device,
								secmode3);
		return;
	}

	/* add in the device connetions list */
	adapter_add_connection(adapter, device, handle);
}

void hcid_dbus_disconn_complete(bdaddr_t *local, uint8_t status,
				uint16_t handle, uint8_t reason)
{
	struct btd_adapter *adapter;
	struct btd_device *device;

	if (status) {
		error("Disconnection failed: 0x%02x", status);
		return;
	}

	adapter = manager_find_adapter(local);
	if (!adapter) {
		error("No matching adapter found");
		return;
	}

	device = adapter_find_connection(adapter, handle);
	if (!device) {
		DBG("No matching connection found for handle %u", handle);
		return;
	}

	adapter_remove_connection(adapter, device, handle);
}

/* Section reserved to device HCI callbacks */

void hcid_dbus_setscan_enable_complete(bdaddr_t *local)
{
	struct btd_adapter *adapter;
	read_scan_enable_rp rp;
	struct hci_request rq;
	int dd = -1;
	uint16_t dev_id;

	adapter = manager_find_adapter(local);
	if (!adapter) {
		error("No matching adapter found");
		return;
	}

	if (adapter_powering_down(adapter))
		return;

	dev_id = adapter_get_dev_id(adapter);

	dd = hci_open_dev(dev_id);
	if (dd < 0) {
		error("HCI device open failed: hci%d", dev_id);
		return;
	}

	memset(&rq, 0, sizeof(rq));
	rq.ogf    = OGF_HOST_CTL;
	rq.ocf    = OCF_READ_SCAN_ENABLE;
	rq.rparam = &rp;
	rq.rlen   = READ_SCAN_ENABLE_RP_SIZE;
	rq.event  = EVT_CMD_COMPLETE;

	if (hci_send_req(dd, &rq, HCI_REQ_TIMEOUT) < 0) {
		error("Sending read scan enable command failed: %s (%d)",
				strerror(errno), errno);
		goto failed;
	}

	if (rp.status) {
		error("Getting scan enable failed with status 0x%02x",
				rp.status);
		goto failed;
	}

	adapter_mode_changed(adapter, rp.enable);

failed:
	if (dd >= 0)
		hci_close_dev(dd);
}

void hcid_dbus_write_simple_pairing_mode_complete(bdaddr_t *local)
{
	struct btd_adapter *adapter;
	int dd;
	uint8_t mode;
	uint16_t dev_id;
	const gchar *path;

	adapter = manager_find_adapter(local);
	if (!adapter) {
		error("No matching adapter found");
		return;
	}

	dev_id = adapter_get_dev_id(adapter);
	path = adapter_get_path(adapter);

	dd = hci_open_dev(dev_id);
	if (dd < 0) {
		error("HCI adapter open failed: %s", path);
		return;
	}

	if (hci_read_simple_pairing_mode(dd, &mode,
						HCI_REQ_TIMEOUT) < 0) {
		error("Can't read simple pairing mode for %s: %s(%d)",
					path, strerror(errno), errno);
		hci_close_dev(dd);
		return;
	}

	hci_close_dev(dd);

	adapter_update_ssp_mode(adapter, mode);
}

void hcid_dbus_returned_link_key(bdaddr_t *local, bdaddr_t *peer)
{
	struct btd_adapter *adapter;
	struct btd_device *device;

	if (!get_adapter_and_device(local, peer, &adapter, &device, TRUE))
		return;

	device_set_paired(device, TRUE);
}

int hcid_dbus_get_io_cap(bdaddr_t *local, bdaddr_t *remote)
{
	struct btd_adapter *adapter;
	struct btd_device *device;
	struct oob_availability_req *oob_req;
	struct agent *agent = NULL;
	uint8_t agent_cap, auth, cap;
	gboolean oob = FALSE;
	int ret;

	if (!get_adapter_and_device(local, remote, &adapter, &device, TRUE))
		return -ENODEV;

	if (get_auth_requirements(local, remote, &auth) < 0)
		return -1;

	DBG("initial authentication requirement is 0x%02x", auth);

	if (auth == 0xff)
		auth = device_get_auth(device);

	/* Check if the adapter is not pairable and if there isn't a bonding
	 * in progress */
	if (!adapter_is_pairable(adapter) &&
				!device_is_bonding(device, NULL)) {
		if (device_get_auth(device) < 0x02) {
			DBG("Allowing no bonding in non-bondable mode");
			/* No input, no output */
			cap = 0x03;
			/* Kernel defaults to general bonding and so
			 * overwrite for this special case. Otherwise
			 * non-pairable test cases will fail. */
			auth = 0x00;
			goto done;
		}
		return -EPERM;
	}

	/* For CreatePairedDevice use dedicated bonding */
	agent = device_get_agent(device);
	if (!agent) {
		/* This is the non bondable mode case */
		if (device_get_auth(device) > 0x01) {
			DBG("Bonding request, but no agent present");
			return -1;
		}

		/* No agent available, and no bonding case */
		if (auth == 0x00 || auth == 0x04) {
			DBG("Allowing no bonding without agent");
			/* No input, no output */
			cap = 0x03;
			/* If kernel defaults to general bonding, set it
			 * back to no bonding */
			auth = 0x00;
			goto done;
		}

		error("No agent available for IO capability");
		return -1;
	}

	agent_cap = agent_get_io_capability(agent);

	if (auth == 0x00 || auth == 0x04) {
		/* If remote requests dedicated bonding follow that lead */
		if (device_get_auth(device) == 0x02 ||
				device_get_auth(device) == 0x03) {

			/* If both remote and local IO capabilities allow MITM
			 * then require it, otherwise don't */
			if (device_get_cap(device) == 0x03 ||
							agent_cap == 0x03)
				auth = 0x02;
			else
				auth = 0x03;
		}

		/* If remote indicates no bonding then follow that. This
		 * is important since the kernel might give general bonding
		 * as default. */
		if (device_get_auth(device) == 0x00 ||
					device_get_auth(device) == 0x01)
			auth = 0x00;

		/* If remote requires MITM then also require it, unless
		 * our IO capability is NoInputNoOutput (so some
		 * just-works security cases can be tested) */
		if (device_get_auth(device) != 0xff &&
					(device_get_auth(device) & 0x01) &&
					agent_cap != 0x03)
			auth |= 0x01;
	}

	DBG("final authentication requirement is 0x%02x", auth);
	cap = agent_get_io_capability(agent);
	oob = agent_get_oob_capability(agent);

	// if pairing is not locally initiated
	if (oob && agent == adapter_get_agent(adapter)) {
		oob_req = g_new0(struct oob_availability_req, 1);
		oob_req->device = device;
		oob_req->auth = auth;
		oob_req->capa = cap;

		ret = device_request_oob_availability(device, oob_availability_cb,
							oob_req);
		if (ret < 0) {
			g_free(oob_req);
			oob = FALSE;
			goto done;
		}
		return ret;
	}

done:
	io_capa_oob_response(adapter, device, cap, auth, oob);
	return 0;
}

int hcid_dbus_set_io_cap(bdaddr_t *local, bdaddr_t *remote,
						uint8_t cap, uint8_t auth)
{
	struct btd_adapter *adapter;
	struct btd_device *device;

	if (!get_adapter_and_device(local, remote, &adapter, &device, TRUE))
		return -ENODEV;

	device_set_cap(device, cap);
	device_set_auth(device, auth);

	return 0;
}

/* Most of the functions in this module require easy access to a connection so
 * we keep it global here and provide these access functions the other (few)
 * modules that require access to it */

void set_dbus_connection(DBusConnection *conn)
{
	connection = conn;
}

DBusConnection *get_dbus_connection(void)
{
	return connection;
}
