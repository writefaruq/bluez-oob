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

#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>
#include <bluetooth/hidp.h>
#include <bluetooth/l2cap.h>
#include <bluetooth/rfcomm.h>
#include <bluetooth/sdp.h>
#include <bluetooth/sdp_lib.h>

#include <glib.h>
#include <dbus/dbus.h>
#include <gdbus.h>

#include "../src/storage.h"
#include "../src/manager.h"
#include "../src/dbus-common.h"
#include "adapter.h"
#include "../src/device.h"

#include "oob-device.h"
#include "error.h"
#include "glib-helper.h"

#define DMTX_DEVICE_INTERFACE "org.bluez.Dmtx"

struct oob_data {
        bdaddr_t        bdaddr;
        uint8_t         hash[16];
        uint8_t         randomizer[16];
};

static void get_local_oobdata(struct oob_data* oob_data)
{
        int dev_id, dd, i;
        int timeout = 1000;

        dev_id = hci_get_route(&oob_data->bd_addr);
        dd = hci_open_dev(dev_id);
	if (dd < 0) {
		fprintf(stderr, "Can't open device hci%d: %s (%d)\n",
						dev_id, strerror(errno), errno);
	}

	if (hci_read_local_oob_data(dd, &oob_data->hash, &oob_data->randomizer, timeout) < 0) {
		fprintf(stderr, "Can't read local OOB data on hci%d: %s (%d)\n",
						dev_id, strerror(errno), errno);
	}
}

static void oob_unregister(void *data)
{
	struct oob_data *loob_data = data;

	debug("Unregistered interface %s", DMTX_DEVICE_INTERFACE);
	/* TODO */
}

static DBusMessage *create_oob_device(DBusConnection *conn,
					DBusMessage *msg, void *data)
{
        struct oob_data *roob_data = data;
        struct btd_adapter *adapter;
	struct btd_device *device;
	const gchar *address;

        /* TODO: parse supplied xml content
	if (dbus_message_get_args(msg, NULL, DBUS_TYPE_STRING, &address,
						DBUS_TYPE_INVALID) == FALSE)
		return invalid_args(msg);
        */

	if (check_address(address) < 0)
		return invalid_args(msg);

	if (adapter_find_device(adapter, address))
		return g_dbus_create_error(msg,
				ERROR_INTERFACE ".AlreadyExists",
				"Device already exists");

	debug("create_device(%s)", address);

	device = adapter_create_device(conn, adapter, address);
	if (!device)
		return NULL;

	return NULL;
}

static GDBusMethodTable oob_methods[] = {
	{ "CreateOOBDevice",		"a{sv}",	"u",	create_oob_device,
						G_DBUS_METHOD_FLAG_ASYNC },
	{ }
};

static void register_oob_interface(DBusConnection *conn, const gchar *path, struct oob_data* oob_data)
{
        if (g_dbus_register_interface(conn, path, DMTX_DEVICE_INTERFACE,
					oob_methods, NULL, NULL,
					oob_data, oob_unregister) == FALSE) {
		error("Failed to register interface %s on path %s",
			DMTX_DEVICE_INTERFACE, path);
		return NULL;
	}

	debug("Registered interface %s on path %s",
			DMTX_DEVICE_INTERFACE, path);

}


