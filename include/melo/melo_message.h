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

#ifndef _MELO_MESSAGE_H_
#define _MELO_MESSAGE_H_

/**
 * MeloMessage:
 *
 * A #MeloMessage is an object to handle serialized protobuf messages received
 * from clients or messages to send to clients. It allocates memory buffer to
 * store the serialized message and embed a reference counting system to reduce
 * memory copy when a message is shared between multiple objects (e.g.
 * broadcasting a message).
 */
typedef struct _MeloMessage MeloMessage;

MeloMessage *melo_message_new (size_t size);
MeloMessage *melo_message_new_from_buffer (
    const unsigned char *data, size_t size);

MeloMessage *melo_message_ref (MeloMessage *msg);
void melo_message_unref (MeloMessage *msg);

unsigned char *melo_message_get_data (MeloMessage *msg);
const unsigned char *melo_message_get_cdata (
    const MeloMessage *msg, size_t *size);

void melo_message_set_size (MeloMessage *msg, size_t size);
size_t melo_message_get_size (const MeloMessage *msg);
size_t melo_message_get_max_size (const MeloMessage *msg);

#endif /* !_MELO_MESSAGE_H_ */
