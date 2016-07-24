/*
 * melo_player_radio.h: Radio Player using GStreamer
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

#ifndef __MELO_PLAYER_RADIO_H__
#define __MELO_PLAYER_RADIO_H__

#include "melo_player.h"

G_BEGIN_DECLS

#define MELO_TYPE_PLAYER_RADIO             (melo_player_radio_get_type ())
#define MELO_PLAYER_RADIO(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), MELO_TYPE_PLAYER_RADIO, MeloPlayerRadio))
#define MELO_IS_PLAYER_RADIO(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MELO_TYPE_PLAYER_RADIO))
#define MELO_PLAYER_RADIO_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), MELO_TYPE_PLAYER_RADIO, MeloPlayerRadioClass))
#define MELO_IS_PLAYER_RADIO_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), MELO_TYPE_PLAYER_RADIO))
#define MELO_PLAYER_RADIO_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), MELO_TYPE_PLAYER_RADIO, MeloPlayerRadioClass))

typedef struct _MeloPlayerRadio MeloPlayerRadio;
typedef struct _MeloPlayerRadioClass MeloPlayerRadioClass;
typedef struct _MeloPlayerRadioPrivate MeloPlayerRadioPrivate;

struct _MeloPlayerRadio {
  MeloPlayer parent_instance;

  /*< private >*/
  MeloPlayerRadioPrivate *priv;
};

struct _MeloPlayerRadioClass {
  MeloPlayerClass parent_class;
};

GType melo_player_radio_get_type (void);

G_END_DECLS

#endif /* __MELO_PLAYER_RADIO_H__ */
