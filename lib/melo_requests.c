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

#include "melo_requests.h"

static MeloRequest *
melo_requests_find_request (MeloRequests *requests, MeloAsyncData *async)
{
  MeloRequest *req = NULL;
  GList *l;

  /* Lock list access */
  g_mutex_lock (&requests->mutex);

  /* Find request in list */
  for (l = requests->list; l != NULL; l = l->next) {
    MeloRequest *r = l->data;

    /* Find by asynchronous user data */
    if (r->async.user_data == async->user_data) {
      /* Take a reference */
      req = melo_request_ref (r);
      break;
    }
  }

  /* Unlock list access */
  g_mutex_unlock (&requests->mutex);

  return req;
}

MeloRequest *
melo_requests_new_request (
    MeloRequests *requests, MeloAsyncData *async, GObject *obj)
{
  MeloRequest *req;

  if (!requests || !async)
    return NULL;

  /* Find request */
  req = melo_requests_find_request (requests, async);

  /* Request doesn't exist */
  if (!req)
    req = melo_request_new (requests, async, obj);

  return req;
}

void
melo_requests_cancel_request (MeloRequests *requests, MeloAsyncData *async)
{
  MeloRequest *req;

  if (!requests || !async)
    return;

  /* Find request */
  req = melo_requests_find_request (requests, async);
  if (!req)
    return;

  /* Cancel request */
  melo_request_cancel (req);
  melo_request_unref (req);
}
