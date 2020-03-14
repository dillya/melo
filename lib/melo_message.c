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

/**
 * SECTION:melo_message
 * @title: MeloMessage
 * @short_description: Serialized protobuf message handler
 */

#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>

#include "melo/melo_message.h"

struct _MeloMessage {
  atomic_int ref_count;
  size_t max_size;
  size_t size;
  unsigned char data[];
};

/**
 * melo_message_new:
 * @size: the maximal message size
 *
 * Creates a new #MeloMessage with a maximal size of @size.
 *
 * To write the message data, the function melo_message_get_data() must be used
 * to retrieve the buffer. When write is done, the function
 * melo_message_set_size() must be called to finalize the write, otherwise, the
 * size returned by melo_message_get_size() will be zero.
 *
 * Returns: (transfer full): a new #MeloMessage.
 */
MeloMessage *
melo_message_new (size_t size)
{
  MeloMessage *msg;

  /* Allocate message */
  msg = malloc (sizeof (*msg) + size);
  if (!msg)
    return NULL;

  /* Initialize message */
  atomic_init (&msg->ref_count, 1);
  msg->max_size = size;
  msg->size = 0;

  return msg;
}

/**
 * melo_message_new_from_buffer:
 * @data: (transfer none) (array length=size) (nullable): the data to be used
 *     for the new message
 * @size: the message size
 *
 * Creates a new #MeloMessage with a size of @size. It copies @data into the
 * message.
 *
 * Returns: (transfer full): a new #MeloMessage.
 */
MeloMessage *
melo_message_new_from_buffer (const unsigned char *data, size_t size)
{
  MeloMessage *msg;

  if (!data || !size)
    return NULL;

  /* Allocate message */
  msg = melo_message_new (size);
  if (msg) {
    msg->size = size;
    memcpy (msg->data, data, size);
  }

  return msg;
}

/**
 * melo_message_ref:
 * @msg: a #MeloMessage
 *
 * Increase the reference count on @msg.
 *
 * Returns: the #MeloMessage.
 */
MeloMessage *
melo_message_ref (MeloMessage *msg)
{
  if (!msg)
    return NULL;

  atomic_fetch_add (&msg->ref_count, 1);

  return msg;
}

/**
 * melo_message_unref:
 * @msg: (nullable): a #MeloMessage
 *
 * Releases a reference on @msg. This may result in the message being freed.
 */
void
melo_message_unref (MeloMessage *msg)
{
  if (msg && atomic_fetch_sub (&msg->ref_count, 1) == 1)
    free (msg);
}

/**
 * melo_message_get_data:
 * @msg: a #MeloMessage
 *
 * Get the message data buffer to use for writing packed protobuf in the
 * #MeloMessage.
 *
 * This function will always return the same pointer for a given #MeloMessage.
 *
 * Returns: (transfer none) (array length=size) (nullable): a pointer to the
 *     message data, or %NULL.
 */
unsigned char *
melo_message_get_data (MeloMessage *msg)
{
  return msg ? msg->data : NULL;
}

/**
 * melo_message_get_cdata:
 * @msg: a #MeloMessage
 * @size: (out) (optional): location to return size of message data
 *
 * Get the message data buffer to read packed protobuf from the #MeloMessage.
 * This data should not be modified.
 *
 * This function will always return the same pointer for a given #MeloMessage.
 *
 * Returns: (transfer none) (array length=size) (nullable): a pointer to the
 *     message data, or %NULL.
 */
const unsigned char *
melo_message_get_cdata (const MeloMessage *msg, size_t *size)
{
  if (!msg)
    return NULL;

  if (size)
    *size = msg->size;

  return msg->data;
}

/**
 * melo_message_set_size:
 * @msg: a #MeloMessage
 * @size: the actual message size
 *
 * Set the actual message size written in the #MeloMessage. It should be called
 * after written data to the message with melo_message_get_data().
 */
void
melo_message_set_size (MeloMessage *msg, size_t size)
{
  if (msg)
    msg->size = size > msg->max_size ? msg->max_size : size;
}

/**
 * melo_message_get_size:
 * @msg: a #MeloMessage
 *
 * Get the size of the message written in the #MeloMessage.
 *
 * Returns: the current message size.
 */
size_t
melo_message_get_size (const MeloMessage *msg)
{
  return msg ? msg->size : 0;
}

/**
 * melo_message_get_max_size:
 * @msg: a #MeloMessage
 *
 * Get the maximal size of a message that the #MeloMessage can hold.
 *
 * Returns: the maximal message size.
 */
size_t
melo_message_get_max_size (const MeloMessage *msg)
{
  return msg ? msg->max_size : 0;
}
