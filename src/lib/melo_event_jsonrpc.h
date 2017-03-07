/*
 * melo_event_jsonrpc.h: Event JSON-RPC interface
 *
 * Copyright (C) 2017 Alexandre Dilly <dillya@sparod.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#ifndef __MELO_EVENT_JSONRPC_H__
#define __MELO_EVENT_JSONRPC_H__

#include "melo_event.h"
#include "melo_jsonrpc.h"

JsonObject *melo_event_jsonrpc_evnet_to_object (MeloEventType type, guint event,
                                                const gchar *id, gpointer data);

#endif /* __MELO_EVENT_JSONRPC_H__ */
