/*
 * Copyright (C) 2020 Alexandre Dilly <dillya@sparod.com>
 *
 * This library is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation; either version 2.1 of the License, or any later version.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.
 * See the GNU Lesser General Public License for more details.

 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA.
 */

#ifndef _MELO_REQUESTS_H_
#define _MELO_REQUESTS_H

#include <stdatomic.h>

#include <gmodule.h>

#include "melo/melo_async.h"
#include "melo/melo_request.h"

typedef struct _MeloRequests {
  GMutex mutex;
  GList *list;
} MeloRequests;

struct _MeloRequest {
  /* Parent instance */
  GObject parent_instance;

  MeloRequests *parent;
  bool is_canceled;

  MeloAsyncData async;
  GObject *obj;
  void *user_data;
};

MeloRequest *melo_requests_new_request (
    MeloRequests *requests, MeloAsyncData *async, GObject *obj);
void melo_requests_cancel_request (
    MeloRequests *requests, MeloAsyncData *async);

MeloRequest *melo_request_new (
    MeloRequests *requests, MeloAsyncData *async, GObject *obj);
void melo_request_cancel (MeloRequest *req);

#endif /* !_MELO_REQUESTS_H_ */
