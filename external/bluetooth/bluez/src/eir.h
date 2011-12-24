/*
 *
 *  BlueZ - Bluetooth protocol stack for Linux
 *
 *  Copyright (C) 2011  Nokia Corporation
 *  Copyright (C) 2011  Marcel Holtmann <marcel@holtmann.org>
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

struct uuid_info {
	uuid_t uuid;
	uint8_t svc_hint;
};

struct eir_data {
	GSList *services;
	int flags;
	char *name;
	gboolean name_complete;
};

void eir_data_free(struct eir_data *eir);
int eir_parse(struct eir_data *eir, uint8_t *eir_data);
void eir_create(const char *name, int8_t tx_power, uint16_t did_vendor,
			uint16_t did_product, uint16_t did_version,
			GSList *uuids, uint8_t *data);
