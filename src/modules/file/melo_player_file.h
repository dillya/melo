/*
 * melo_player_file.h: File Player using GStreamer
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

#ifndef __MELO_PLAYER_FILE_H__
#define __MELO_PLAYER_FILE_H__

#include "melo_player.h"

G_BEGIN_DECLS

#define MELO_TYPE_PLAYER_FILE             (melo_player_file_get_type ())
#define MELO_PLAYER_FILE(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), MELO_TYPE_PLAYER_FILE, MeloPlayerFile))
#define MELO_IS_PLAYER_FILE(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MELO_TYPE_PLAYER_FILE))
#define MELO_PLAYER_FILE_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), MELO_TYPE_PLAYER_FILE, MeloPlayerFileClass))
#define MELO_IS_PLAYER_FILE_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), MELO_TYPE_PLAYER_FILE))
#define MELO_PLAYER_FILE_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), MELO_TYPE_PLAYER_FILE, MeloPlayerFileClass))

typedef struct _MeloPlayerFile MeloPlayerFile;
typedef struct _MeloPlayerFileClass MeloPlayerFileClass;
typedef struct _MeloPlayerFilePrivate MeloPlayerFilePrivate;

struct _MeloPlayerFile {
  MeloPlayer parent_instance;

  /*< private >*/
  MeloPlayerFilePrivate *priv;
};

struct _MeloPlayerFileClass {
  MeloPlayerClass parent_class;
};

GType melo_player_file_get_type (void);

G_END_DECLS

#endif /* __MELO_PLAYER_FILE_H__ */
