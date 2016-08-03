/*
 * melo_player_airplay.h: Airplay Player using GStreamer
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

#ifndef __MELO_PLAYER_AIRPLAY_H__
#define __MELO_PLAYER_AIRPLAY_H__

#include "melo_player.h"

G_BEGIN_DECLS

#define MELO_TYPE_PLAYER_AIRPLAY             (melo_player_airplay_get_type ())
#define MELO_PLAYER_AIRPLAY(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), MELO_TYPE_PLAYER_AIRPLAY, MeloPlayerAirplay))
#define MELO_IS_PLAYER_AIRPLAY(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MELO_TYPE_PLAYER_AIRPLAY))
#define MELO_PLAYER_AIRPLAY_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), MELO_TYPE_PLAYER_AIRPLAY, MeloPlayerAirplayClass))
#define MELO_IS_PLAYER_AIRPLAY_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), MELO_TYPE_PLAYER_AIRPLAY))
#define MELO_PLAYER_AIRPLAY_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), MELO_TYPE_PLAYER_AIRPLAY, MeloPlayerAirplayClass))

typedef struct _MeloPlayerAirplay MeloPlayerAirplay;
typedef struct _MeloPlayerAirplayClass MeloPlayerAirplayClass;
typedef struct _MeloPlayerAirplayPrivate MeloPlayerAirplayPrivate;

typedef enum {
  MELO_AIRPLAY_CODEC_ALAC = 0,
  MELO_AIRPLAY_CODEC_PCM,
  MELO_AIRPLAY_CODEC_AAC,
} MeloAirplayCodec;

typedef enum {
  MELO_AIRPLAY_TRANSPORT_TCP = 0,
  MELO_AIRPLAY_TRANSPORT_UDP,
} MeloAirplayTransport;

struct _MeloPlayerAirplay {
  MeloPlayer parent_instance;

  /*< private >*/
  MeloPlayerAirplayPrivate *priv;
};

struct _MeloPlayerAirplayClass {
  MeloPlayerClass parent_class;
};

GType melo_player_airplay_get_type (void);

gboolean melo_player_airplay_setup (MeloPlayerAirplay *pair,
                                    MeloAirplayTransport transport,
                                    guint *port, MeloAirplayCodec codec,
                                    const gchar *format);
gboolean melo_player_airplay_record (MeloPlayerAirplay *pair, guint seq);
gboolean melo_player_airplay_flush (MeloPlayerAirplay *pair, guint seq);
gboolean melo_player_airplay_teardown (MeloPlayerAirplay *pair);

gboolean melo_player_airplay_set_volume (MeloPlayerAirplay *pair,
                                         gdouble volume);
gboolean melo_player_airplay_set_progress (MeloPlayerAirplay *pair,
                                           guint start, guint cur, guint end);
gboolean melo_player_airplay_set_cover (MeloPlayerAirplay *pair, GBytes *cover,
                                        const gchar *cover_type);
G_END_DECLS

#endif /* __MELO_PLAYER_AIRPLAY_H__ */
