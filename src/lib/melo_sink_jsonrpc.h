/*
 * melo_sink_jsonrpc.h: Global audio sink JSON-RPC interface
 *
 * Copyright (C) 2016 Alexandre Dilly <dillya@sparod.com>
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

#ifndef __MELO_SINK_JSONRPC_H__
#define __MELO_SINK_JSONRPC_H__

#include "melo_sink.h"
#include "melo_jsonrpc.h"

/* JSON-RPC methods */
void melo_sink_jsonrpc_register_methods (void);
void melo_sink_jsonrpc_unregister_methods (void);

#endif /* __MELO_SINK_JSONRPC_H__ */
