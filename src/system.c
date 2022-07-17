/*
 * Copyright (C) 2022 Alexandre Dilly <dillya@sparod.com>
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

#include <sys/reboot.h>
#include <unistd.h>

#define MELO_LOG_TAG "melo_system"
#include <melo/melo_log.h>

#include "proto/system.pb-c.h"

#include "system.h"

/**
 * system_handle_request:
 * @msg: the #MeloMessage to handle
 * @cb: the function to call when a response is sent
 * @user_data: data to pass to @cb
 *
 * This function is called when a new system request is received by the
 * application.
 *
 * If the request is malformed or an internal error occurs, the function will
 * return %false, otherwise %true will be returned.
 *
 * Returns: %true if the message has been handled, %false otherwise.
 */
bool
system_handle_request (MeloMessage *msg, MeloAsyncCb cb, void *user_data)
{
  System__Request *request;
  bool ret = false;

  if (!msg)
    return false;

  /* Unpack request */
  request = system__request__unpack (
      NULL, melo_message_get_size (msg), melo_message_get_cdata (msg, NULL));
  if (!request) {
    MELO_LOGE ("failed to unpack request");
    return false;
  }

  /* Handle request */
  switch (request->req_case) {
  case SYSTEM__REQUEST__REQ_POWER_OFF:
    MELO_LOGI ("request power off");
    sync ();
    reboot (RB_POWER_OFF);
    break;
  case SYSTEM__REQUEST__REQ_REBOOT:
    MELO_LOGI ("request reboot");
    sync ();
    reboot (RB_AUTOBOOT);
    break;
  default:
    MELO_LOGE ("request %u not supported", request->req_case);
  }

  /* Free request */
  system__request__free_unpacked (request, NULL);

  return ret;
}
