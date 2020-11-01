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

#ifndef _MELO_UPNP_PLAYER_H_
#define _MELO_UPNP_PLAYER_H_

#include <melo/melo_player.h>

G_BEGIN_DECLS

#define MELO_UPNP_PLAYER_ID "com.sparod.upnp.player"

#define MELO_UPNP_PLAYER_ICON \
  "svg:<svg width=\"24\" height=\"24\" viewBox=\"2 2 20 20\"><path " \
  "d=\"M21.38,12.56H12.85C11.97,12.56 11.1,12.96 " \
  "10.61,13.61V13.6C10.12,14.28 9.32,14.72 8.41,14.72C6.92,14.72 5.71,13.5 " \
  "5.71,12C5.71,10.5 6.92,9.31 8.41,9.31C9.32,9.31 10.12,9.75 " \
  "10.61,10.43V10.42C11.1,11.07 11.97,11.5 12.85,11.5H21.29C21.45,11.5 " \
  "22,11.4 22,10.67C21.26,6.43 17.1,3.18 12.06,3.18C8.96,3.18 6.19,4.41 " \
  "4.34,6.35C4.05,6.79 4.35,6.92 4.63,6.96H10.14C11,6.96 11.89,6.54 " \
  "12.38,5.89V5.91C12.88,5.23 13.67,4.78 14.58,4.78C16.07,4.78 17.28,6 " \
  "17.28,7.5C17.28,9 16.07,10.2 14.58,10.2C13.67,10.2 12.88,9.75 " \
  "12.38,9.07V9.08C11.89,8.44 11,8.03 10.14,8.03H4.13L4.15,8.03C4.15,8.03 " \
  "3.26,8 2.72,8.75C2.3,9.42 2,10.85 2,12C2,13.16 2.17,14.21 " \
  "2.72,15.27C3.19,16.03 4.15,16 4.15,16H4.11L10.14,16C11,16 11.89,15.58 " \
  "12.38,14.93V14.94C12.88,14.26 13.67,13.81 14.58,13.81C16.07,13.81 " \
  "17.28,15.03 17.28,16.5C17.28,18 16.07,19.23 14.58,19.23C13.67,19.23 " \
  "12.88,18.78 12.38,18.1V18.12C11.89,17.47 11,17.05 " \
  "10.14,17.05H4.64C4.36,17.09 4.06,17.22 4.32,17.64C6.17,19.58 8.95,20.82 " \
  "12.06,20.82C17.11,20.82 21.28,17.57 22,13.31C22,12.72 21.59,12.58 " \
  "21.38,12.56\"/></svg>"

#define MELO_TYPE_UPNP_PLAYER melo_upnp_player_get_type ()
MELO_DECLARE_PLAYER (MeloUpnpPlayer, melo_upnp_player, UPNP_PLAYER)

MeloUpnpPlayer *melo_upnp_player_new (void);

G_END_DECLS

#endif /* !_MELO_UPNP_PLAYER_H_ */
