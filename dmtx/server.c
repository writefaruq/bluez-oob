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
#include <glib.h>
#include <unistd.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/sdp.h>
#include <dbus/dbus.h>

#include "logging.h"
#include "adapter.h"
#include "oob-device.h"
#include "server.h"

static GSList *servers = NULL;

struct dmtx_server {
	bdaddr_t src;
	GIOChannel *ctrl;
	GIOChannel *intr;
	GIOChannel *confirm;
};

struct oob_data;

static gint server_cmp(gconstpointer s, gconstpointer user_data)
{
	const struct dmtx_server *server = s;
	const bdaddr_t *src = user_data;

	return bacmp(&server->src, src);
}


int server_start(struct btd_adapter *adapter, DBusConnection *conn)
{
	struct dmtx_server *server;
	bdaddr_t src;
	const gchar *path;
	struct oob_data *loob_data; /* local oob data*/

	adapter_get_address(adapter, &src);
	path = adapter_get_path(adapter);

	server = g_new0(struct dmtx_server, 1);
	bacpy(&server->src, &src);

	servers = g_slist_append(servers, server);

	/* Initialize local OOB data */
	loob_data = g_new0(struct oob_data, 1);
	bacpy(&loob_data->bdaddr, &src);
	get_local_oobdata(loob_data);

	/* Export oob-device specific D-Bus APIs */
	register_oob_interface(conn, adapter, loob_data);

	return 0;
}

void server_stop(const bdaddr_t *src)
{
	struct dmtx_server *server;
	GSList *l = g_slist_find_custom(servers, src, server_cmp);

	if (!l)
		return;

	server = l->data;

	servers = g_slist_remove(servers, server);
	g_free(server);
}
