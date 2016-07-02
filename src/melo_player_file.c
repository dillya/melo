/*
 * melo_player_file.c: File Player using GStreamer
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

#include "melo_player_file.h"

static gboolean melo_player_file_play (MeloPlayer *player, const gchar *path);
static MeloPlayerState melo_player_file_get_state (MeloPlayer *player);
static gchar *melo_player_file_get_name (MeloPlayer *player);
static gint melo_player_file_get_pos (MeloPlayer *player, gint *duration);
static MeloPlayerStatus *melo_player_file_get_status (MeloPlayer *player);

G_DEFINE_TYPE (MeloPlayerFile, melo_player_file, MELO_TYPE_PLAYER)

static void
melo_player_file_class_init (MeloPlayerFileClass *klass)
{
  MeloPlayerClass *pclass = MELO_PLAYER_CLASS (klass);

  pclass->play = melo_player_file_play;
  pclass->get_state = melo_player_file_get_state;
  pclass->get_name = melo_player_file_get_name;
  pclass->get_pos = melo_player_file_get_pos;
  pclass->get_status = melo_player_file_get_status;
}

static void
melo_player_file_init (MeloPlayerFile *self)
{
}

static gboolean
melo_player_file_play (MeloPlayer *player, const gchar *path)
{
  return TRUE;
}

static MeloPlayerState
melo_player_file_get_state (MeloPlayer *player)
{
  return MELO_PLAYER_STATE_NONE;
}

static gchar *
melo_player_file_get_name (MeloPlayer *player)
{
  return NULL;
}

static gint
melo_player_file_get_pos (MeloPlayer *player, gint *duration)
{
  if (duration)
    *duration = 0;
  return 0;
}

static MeloPlayerStatus *
melo_player_file_get_status (MeloPlayer *player)
{
  return melo_player_status_new (MELO_PLAYER_STATE_NONE, NULL);
}
