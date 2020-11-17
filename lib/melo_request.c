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

#include "melo/melo_request.h"

typedef enum _MeloRequestState {
  MELO_REQUEST_STATE_PENDING = 0,
  MELO_REQUEST_STATE_COMPLETE,
  MELO_REQUEST_STATE_CANCELED,
} MeloRequestState;

enum { CANCELLED, DESTROYED, LAST_SIGNAL };

static guint signals[LAST_SIGNAL] = {0};

struct _MeloRequest {
  /* Parent instance */
  GObject parent_instance;

  MeloRequestState state;

  MeloAsyncData async;
  GObject *obj;
  void *user_data;
};

G_DEFINE_TYPE (MeloRequest, melo_request, G_TYPE_OBJECT)

static void
melo_request_finalize (GObject *gobject)
{
  MeloRequest *req = MELO_REQUEST (gobject);

  /* Send empty message to finish request */
  if (req->async.cb && req->state == MELO_REQUEST_STATE_PENDING)
    req->async.cb (NULL, req->async.user_data);

  /* Call destroyed callback */
  g_signal_emit (req, signals[DESTROYED], 0);

  MELO_LOGD ("request %p destroyed", req);

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

/**
 * melo_request_new:
 * @async: a #MeloAsyncData containing the asynchronous callback
 * @obj: (nullable): a reference to the #GObject who created the request
 *
 * Creates a new #MeloRequest to hold an asynchronous request. Multiple messages
 * can be send over the request with melo_request_send_response(). When the
 * request is complete (no more message to send), the function
 * melo_request_complete() must be called to finalize the request. If the user
 * wants to cancel and stop immediately the request, the function
 * melo_request_cancel() can be used.
 *
 * After usage, the reference should be released with melo_request_unref().
 *
 * Returns: (transfer full): a new #MeloRequest.
 */
MeloRequest *
melo_request_new (MeloAsyncData *async, GObject *obj)
{
  MeloRequest *req;

  /* Create request */
  req = g_object_new (MELO_TYPE_REQUEST, NULL);
  if (!req)
    return NULL;

  /* Initialize request */
  req->async = *async;
  req->obj = obj;

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
 * multiple times for one request. To omplete request, the function
 * melo_request_complete() must be called.
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

  if (!req || !msg)
    return false;

  /* Call asynchronous callback if not cancelled */
  if (req->state == MELO_REQUEST_STATE_PENDING)
    ret = req->async.cb ? req->async.cb (msg, req->async.user_data) : true;

  /* Release message reference */
  melo_message_unref (msg);

  return ret;
}

/**
 * melo_request_cancel:
 * @req: a #MeloRequest
 *
 * This function can be used to cancel a pending request. If the request is
 * already canceled or complete, this function will only release the request
 * reference.
 *
 * This function release the request reference at return.
 */
void
melo_request_cancel (MeloRequest *req)
{
  if (!req)
    return;

  /* Cancel request */
  if (req->state == MELO_REQUEST_STATE_PENDING) {
    g_signal_emit (req, signals[CANCELLED], 0);
    MELO_LOGD ("request %p cancelled", req);
  }
  req->state = MELO_REQUEST_STATE_CANCELED;

  g_object_unref (req);
}

/**
 * melo_request_complete:
 * @req: a #MeloRequest
 *
 * This function must be called to complete a request and then send a NULL
 * message to the asynchronous callback registered during request creation.
 *
 * This function release the request reference at return.
 */
void
melo_request_complete (MeloRequest *req)
{
  if (!req)
    return;

  /* Complete request */
  if (req->async.cb && req->state == MELO_REQUEST_STATE_PENDING)
    req->async.cb (NULL, req->async.user_data);
  req->state = MELO_REQUEST_STATE_COMPLETE;

  g_object_unref (req);
}
