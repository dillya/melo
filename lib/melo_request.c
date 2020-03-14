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

#define MELO_LOG_TAG "request"
#include <melo/melo_log.h>

#include "melo_requests.h"

enum { CANCELLED, DESTROYED, LAST_SIGNAL };

static guint signals[LAST_SIGNAL] = {0};

G_DEFINE_TYPE (MeloRequest, melo_request, G_TYPE_OBJECT)

static void
melo_request_finalize (GObject *gobject)
{
  MeloRequest *req = MELO_REQUEST (gobject);
  MeloRequests *requests = req->parent;

  /* Lock parent list access */
  g_mutex_lock (&requests->mutex);

  /* Send empty message to finish request */
  if (req->async.cb && !req->is_canceled)
    req->async.cb (NULL, req->async.user_data);

  /* Call destroyed callback */
  g_signal_emit (req, signals[DESTROYED], 0);

  /* Remove from parent list */
  requests->list = g_list_remove (requests->list, req);

  MELO_LOGD ("request %p destroyed", req);

  /* Unlock parent list access */
  g_mutex_unlock (&requests->mutex);

  /* Chain up to the parent class */
  G_OBJECT_CLASS (melo_request_parent_class)->finalize (gobject);
}

static void
melo_request_class_init (MeloRequestClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  /* Attach finalize function */
  object_class->finalize = melo_request_finalize;

  /**
   * MeloRequest::cancelled:
   *
   * Emitted when the request has been cancelled.
   */
  signals[CANCELLED] =
      g_signal_new ("cancelled", G_TYPE_FROM_CLASS (object_class),
          G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL, G_TYPE_NONE, 0);

  /**
   * MeloRequest::destroyed:
   *
   * Emitted when the request has been destroyed.
   */
  signals[DESTROYED] =
      g_signal_new ("destroyed", G_TYPE_FROM_CLASS (object_class),
          G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL, G_TYPE_NONE, 0);
}

static void
melo_request_init (MeloRequest *req)
{
}

MeloRequest *
melo_request_new (MeloRequests *requests, MeloAsyncData *async, GObject *obj)
{
  MeloRequest *req;

  /* Create request */
  req = g_object_new (MELO_TYPE_REQUEST, NULL);
  if (!req)
    return NULL;

  /* Initialize request */
  req->parent = requests;
  req->async = *async;
  req->obj = obj;

  /* Add request to list */
  g_mutex_lock (&requests->mutex);
  requests->list = g_list_prepend (requests->list, req);
  g_mutex_unlock (&requests->mutex);

  return req;
}

/**
 * melo_request_get_object:
 * @req: a #MeloRequest
 *
 * This function can be used to get the parent object for which the request has
 * been created.
 *
 * Returns: the #GObject parent.
 */
GObject *
melo_request_get_object (MeloRequest *req)
{
  return req ? req->obj : NULL;
}

/**
 * melo_request_set_user_data:
 * @req: a #MeloRequest
 * @user_data: the data to attach
 *
 * Attach a data to the request. Before attaching a new data, be sure to free
 * the previous one.
 */
void
melo_request_set_user_data (MeloRequest *req, void *user_data)
{
  if (req)
    req->user_data = user_data;
}

/**
 * melo_request_get_user_data:
 * @req: a #MeloRequest
 *
 * Gets data attached to the request.
 *
 * Returns: (transfer none): the data to attached to the request or %NULL.
 */
void *
melo_request_get_user_data (MeloRequest *req)
{
  return req ? req->user_data : NULL;
}

/**
 * melo_request_send_response:
 * @req: a #MeloRequest
 * @msq: the #MeloMessage to send
 *
 * This function is used to send a response to a request. It can be called
 * multiple times for one request. To end a request, the melo_request_unref()
 * must be called.
 *
 * If the request has been cancelled, the function will returns %false and the
 * current task should be aborted and finally, the request reference should be
 * released as soon as possible with melo_request_unref().
 *
 * This function takes ownership of the message.
 *
 * Returns: %true if the response has been sent, %false otherwise.
 */
bool
melo_request_send_response (MeloRequest *req, MeloMessage *msg)
{
  bool ret = false;

  if (!req)
    return false;

  /* Call asynchronous callback if not cancelled */
  g_mutex_lock (&req->parent->mutex);
  if (!req->is_canceled)
    ret = req->async.cb ? req->async.cb (msg, req->async.user_data) : true;
  g_mutex_unlock (&req->parent->mutex);

  /* Release message reference */
  melo_message_unref (msg);

  return ret;
}

void
melo_request_cancel (MeloRequest *req)
{
  if (!req)
    return;

  /* Lock request list access */
  g_mutex_lock (&req->parent->mutex);

  /* Cancel request */
  if (!req->is_canceled) {
    g_signal_emit (req, signals[CANCELLED], 0);
    MELO_LOGD ("request %p cancelled", req);
  }
  req->is_canceled = true;

  /* Unlock request list access */
  g_mutex_unlock (&req->parent->mutex);
}
