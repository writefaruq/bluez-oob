/*
 *
 *  BlueZ - Bluetooth protocol stack for Linux
 *
 *  Copyright (C) 2004-2009  Marcel Holtmann <marcel@holtmann.org>
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

#include <errno.h>
#include <stdlib.h>
#include <unistd.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>
#include <bluetooth/sdp.h>

#include <glib.h>
#include <dbus/dbus.h>
#include <gdbus.h>

#include "adapter.h"
#include "device.h"

#include "error.h"
#include "logging.h"

#include "oob-device.h"

#define check_address(address) bachk(address)

#define DMTX_DEVICE_INTERFACE "org.bluez.Dmtx"

/* Copied from cthakasi/oob */
#define EIR_TAG_TYPE_COD		0x0D
#define EIR_TAG_TYPE_SP_HASH		0x0E
#define EIR_TAG_TYPE_SP_RANDOMIZER	0x0F

/* FIXME: Move to a better place */
struct eir_tag {
	uint8_t len;
	uint8_t type;
	uint8_t data[0];
};

struct oob_data;

void get_local_oobdata(struct oob_data *oob_data)
{
	int dev_id, dd, err;

	dev_id = hci_get_route(&oob_data->bdaddr);
	dd = hci_open_dev(dev_id);
	if (dd < 0) {
		err = errno;
		fprintf(stderr, "Can't open device hci%d: %s (%d)\n",
				dev_id, strerror(err), err);
		return;
	}

	/* FIXME: build warning */
	if (hci_read_local_oob_data(dd, &oob_data->hash,
				&oob_data->randomizer, 1000) < 0) {
		err = errno;
		fprintf(stderr, "Can't read local OOB data on hci%d: %s (%d)\n",
				dev_id, strerror(err), err);
	}
	close(dd);
}

static void oob_unregister(void *data)
{
	/* struct oob_data *loob_data = data; */

	debug("Unregistered interface %s", DMTX_DEVICE_INTERFACE);
	/* TODO */
}

static inline DBusMessage *invalid_args(DBusMessage *msg)
{
	return g_dbus_create_error(msg, ERROR_INTERFACE ".InvalidArguments",
			"Invalid arguments in method call");
}

static DBusMessage *create_oob_device(DBusConnection *conn,
		DBusMessage *msg, void *data)
{
	struct btd_adapter *adapter = data;
	struct btd_device *device;
	const gchar *address;
	DBusMessage *reply;
	const gchar *path;

	if (dbus_message_get_args(msg, NULL, DBUS_TYPE_STRING, &address,
				DBUS_TYPE_INVALID) == FALSE)
		return invalid_args(msg);

	if (check_address(address) < 0)
		return invalid_args(msg);

	if (adapter_find_device(adapter, address))
		return g_dbus_create_error(msg,
				ERROR_INTERFACE ".AlreadyExists",
				"Device already exists");

	/* Device creation via adapter interface */
	debug("DMTX create_device(%s)", address);
	device = adapter_create_device(conn, adapter, address);
	if (!device)
		return NULL;

	/* Return device path */
	path = device_get_path(device);
	debug("DMTX create_device path(%s)", path);

	reply = dbus_message_new_method_return(msg);
	if (!reply)
		return NULL;

	dbus_message_append_args(reply, DBUS_TYPE_OBJECT_PATH, &path,
			DBUS_TYPE_INVALID);
	return reply;
}

static void set_oobdata(struct btd_device *device, struct oob_data *oob_data)
{
        device->oob = TRUE;
        memcpy(device->hash, oob_data->hash, 16);
        memcpy(device->randomnizer, oob_data->randomizer, 16);
}

static void parse_oobtags(uint8_t *oobtags, int len, struct oob_data *roob_data)
{
        struct eir_tag* tag = oobtags;
        while (len) {
                switch (tag->type) {
                        case EIR_TAG_TYPE_COD: /* not handled yet */
                        break;
                        case EIR_TAG_TYPE_SP_HASH:
                        memcpy(roob_data->hash, tag->data, 16);
                        break;
                        case EIR_TAG_TYPE_SP_RANDOMIZER:
                        memcpy(roob_data->randomnizer, tag->data, 16);
                        break;
                        default:
                        debug("parse_oobtags: unknown oob tag type");
                }
                len = len - tag->len; /* FIXME: tag->len type conversion */
                tag = tag + tag->len; /* points to next tag */
        }
}

static DBusMessage *create_paired_oob_device(DBusConnection *conn,
						DBusMessage *msg, void *data)
{
	struct btd_adapter *adapter = data;
	struct btd_device *device;
	const gchar *address, *agent_path;
	uint8_t *oobtags;
	int len;
	struct oob_data roob_data;

	/* See optional OOB tags format for more information */
	if (dbus_message_get_args(msg, NULL, DBUS_TYPE_STRING, &address,
					DBUS_TYPE_OBJECT_PATH, &agent_path,
					DBUS_TYPE_ARRAY, DBUS_TYPE_BYTE, &oobtags, &len,
						DBUS_TYPE_INVALID) == FALSE)
		return invalid_args(msg);

	if (check_address(address) < 0)
		return invalid_args(msg);

	/* TODO: parse oobtags */
	parse_oobtags(oobtags, len, &roob_data);

	device = adapter_get_device(conn, adapter, address);

	if (!device)
		return g_dbus_create_error(msg,
				ERROR_INTERFACE ".Failed",
				"Unable to create a new device object");

	/* Set hash and randomizer in the btd_device */
        set_oobdata(device, &roob_data);

	return device_create_bonding(device, conn, msg,
			agent_path, IO_CAPABILITY_NOINPUTNOOUTPUT);
}


static GDBusMethodTable oob_methods[] = {
	{ "CreateOOBDevice",		"s",	"o",	create_oob_device,
		G_DBUS_METHOD_FLAG_ASYNC },
	{ "CreatePairedOOBDevice",	"soay",	"o",	create_paired_oob_device,
                G_DBUS_METHOD_FLAG_ASYNC},

	{ }
};

void register_oob_interface(DBusConnection *conn,
		struct btd_adapter *adapter, struct oob_data *oob_data)
{
	const gchar *path = adapter_get_path(adapter);

	if (g_dbus_register_interface(conn, path, DMTX_DEVICE_INTERFACE,
				oob_methods, NULL, NULL,
				adapter, oob_unregister) == FALSE) {
		error("Failed to register interface %s on path %s",
				DMTX_DEVICE_INTERFACE, path);
	}

	debug("Registered interface %s on path %s",
			DMTX_DEVICE_INTERFACE, path);
}
