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

static GDBusMethodTable oob_methods[] = {
	{ "CreateOOBDevice",		"s",	"o",	create_oob_device,
		G_DBUS_METHOD_FLAG_ASYNC },
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
