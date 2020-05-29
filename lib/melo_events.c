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

#define MELO_LOG_TAG "events"
#include "melo/melo_log.h"

#include "melo_events.h"

bool
melo_events_add_listener (MeloEvents *events, MeloAsyncCb cb, void *user_data)
{
  MeloAsyncData *async;
  GList *l;

  if (!events || !cb)
    return false;

  /* Check listener list */
  for (l = events->list; l != NULL; l = l->next) {
    async = l->data;

    /* Event already registered */
    if (async->cb == cb && async->user_data == user_data) {
      MELO_LOGE ("event %p:%p already registered", cb, user_data);
      return false;
    }
  }

  /* Add new listener */
  async = malloc (sizeof (*async));
  if (!async) {
    MELO_LOGE ("failed to register event");
    return false;
  }

  /* Fill event listener */
  async->cb = cb;
  async->user_data = user_data;
  events->list = g_list_prepend (events->list, async);

  return true;
}

bool
melo_events_remove_listener (
    MeloEvents *events, MeloAsyncCb cb, void *user_data)
{
  GList *l;

  if (!events || !cb)
    return false;

  /* Find listener */
  for (l = events->list; l != NULL; l = l->next) {
    MeloAsyncData *async = l->data;

    /* Remove listener */
    if (async->cb == cb && async->user_data == user_data) {
      events->list = g_list_delete_link (events->list, l);
      free (async);
      break;
    }
  }

  return true;
}

void
melo_events_broadcast (MeloEvents *events, MeloMessage *msg)
{
  GList *l;

  if (!events) {
    melo_message_unref (msg);
    return;
  }

  /* Find listener */
  for (l = events->list; l != NULL; l = l->next) {
    MeloAsyncData *async = l->data;

    /* Send event */
    async->cb (msg, async->user_data);
  }

  /* Unref message */
  melo_message_unref (msg);
}
