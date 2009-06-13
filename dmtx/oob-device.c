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

#include "logging.h"
#include "textfile.h"
#include "uinput.h"

#include "../src/storage.h"
#include "../src/manager.h"
#include "../src/dbus-common.h"
#include "adapter.h"
#include "../src/device.h"


#include "error.h"
#include "glib-helper.h"
#include "btio.h"

#include "oob-device.h"

#define check_address(address) bachk(address)

#define DMTX_DEVICE_INTERFACE "org.bluez.Dmtx"

struct oob_data;

void get_local_oobdata(struct oob_data *oob_data)
{
        int dev_id, dd, i;
        int timeout = 1000;

        dev_id = hci_get_route(&oob_data->bdaddr);
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

static inline DBusMessage *invalid_args(DBusMessage *msg)
{
	return g_dbus_create_error(msg, ERROR_INTERFACE ".InvalidArguments",
			"Invalid arguments in method call");
}
/*
int add_oob_device(DBusConnection *conn, const char *sender,
                        struct btd_adapter *adapter, const char *xmltext,
                        dbus_uint32_t *handle)
{
        struct oob_data *roob_data;
        char gchar *address;
        struct btd_device *device;

 	roob_data = dmtx_xml_parse_oob(xmltext, strlen(xmltext));
	if (!roob_data) {
		error("Parsing of XML OOB data failed");
		return -EIO;
	}

        ba2str(&roob_data->bdaddr, address);

	if (check_address(address) < 0)
		return invalid_args(msg);

	if (adapter_find_device(adapter, address))
		return g_dbus_create_error(msg,
				ERROR_INTERFACE ".AlreadyExists",
				"Device already exists");

	debug("create_device(%s)", address);

	device = adapter_create_device(conn, adapter, address);
	if (!device)
		return errno;
        handle = device->handle;
        return handle;
}

static DBusMessage *create_oob_device(DBusConnection *conn,
					DBusMessage *msg, void *data)
{
        struct btd_adapter *adapter = data;
        DBusMessage *reply;
	const char *sender, *xmltext;
	dbus_uint32_t handle;
	int err;

	if (dbus_message_get_args(msg, NULL,
			DBUS_TYPE_STRING, &xmltext, DBUS_TYPE_INVALID) == FALSE)
		return invalid_args(msg);

	sender = dbus_message_get_sender(msg);

	err = add_oob_device(conn, sender, adapter, xmltext, &handle);
	if (err < 0)
		return failed_strerror(msg, err);

	reply = dbus_message_new_method_return(msg);
	if (!reply)
		return NULL;

	dbus_message_append_args(reply, DBUS_TYPE_UINT32, &handle,
							DBUS_TYPE_INVALID);

	return reply;
}
*/

static DBusMessage *create_device2(DBusConnection *conn,
					DBusMessage *msg, void *data)
{
	struct btd_adapter *adapter = data;
	struct btd_device *device;
	const gchar *address;

	if (dbus_message_get_args(msg, NULL, DBUS_TYPE_STRING, &address,
						DBUS_TYPE_INVALID) == FALSE)
		return invalid_args(msg);

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
	{ "CreateOOBDevice",		"s",	"o",	create_device2,
						G_DBUS_METHOD_FLAG_ASYNC },
	{ }
};

void register_oob_interface(DBusConnection *conn, struct btd_adapter *adapter, struct oob_data* oob_data)
{
        const gchar *path;
        path = adapter_get_path(adapter);

        if (g_dbus_register_interface(conn, path, DMTX_DEVICE_INTERFACE,
					oob_methods, NULL, NULL,
					adapter, oob_unregister) == FALSE) {
		error("Failed to register interface %s on path %s",
			DMTX_DEVICE_INTERFACE, path);
	}

	debug("Registered interface %s on path %s",
			DMTX_DEVICE_INTERFACE, path);

}


