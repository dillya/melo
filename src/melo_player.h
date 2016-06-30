/*
 * melo_player.h: Player base class
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

#ifndef __MELO_PLAYER_H__
#define __MELO_PLAYER_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define MELO_TYPE_PLAYER             (melo_player_get_type ())
#define MELO_PLAYER(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), MELO_TYPE_PLAYER, MeloPlayer))
#define MELO_IS_PLAYER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MELO_TYPE_PLAYER))
#define MELO_PLAYER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), MELO_TYPE_PLAYER, MeloPlayerClass))
#define MELO_IS_PLAYER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), MELO_TYPE_PLAYER))
#define MELO_PLAYER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), MELO_TYPE_PLAYER, MeloPlayerClass))

typedef struct _MeloPlayer MeloPlayer;
typedef struct _MeloPlayerClass MeloPlayerClass;
typedef struct _MeloPlayerPrivate MeloPlayerPrivate;

struct _MeloPlayer {
  GObject parent_instance;

  /*< private >*/
  MeloPlayerPrivate *priv;
};

struct _MeloPlayerClass {
  GObjectClass parent_class;
};

GType melo_player_get_type (void);

MeloPlayer *melo_player_new (GType type, const gchar *id);
const gchar *melo_player_get_id (MeloPlayer *player);
MeloPlayer *melo_player_get_player_by_id (const gchar *id);

G_END_DECLS

#endif /* __MELO_PLAYER_H__ */
