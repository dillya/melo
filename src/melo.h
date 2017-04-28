/*
 * melo.h: Shared definition of Melo program
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

#ifndef __MELO_H__
#define __MELO_H__

#include "melo_httpd.h"
#include "melo_discover.h"

typedef struct _MeloContext MeloContext;

struct _MeloContext {
  MeloDiscover *disco;
  /* Audio settings */
  struct {
    gint64 rate;
    gint64 channels;
  } audio;
  /* HTTP server settings */
  MeloHTTPD *server;
  gchar *name;
  gint64 port;
};

#endif /* __MELO_H__ */
