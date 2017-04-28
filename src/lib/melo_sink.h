/*
 * melo_sink.h: Global audio sink for players
 *
 * Copyright (C) 2017 Alexandre Dilly <dillya@sparod.com>
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

#ifndef __MELO_SINK_H__
#define __MELO_SINK_H__

#include <glib.h>
#include <gst/gst.h>

#include "melo_player.h"

G_BEGIN_DECLS

#define MELO_TYPE_SINK             (melo_sink_get_type ())
#define MELO_SINK(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), MELO_TYPE_SINK, MeloSink))
#define MELO_IS_SINK(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MELO_TYPE_SINK))
#define MELO_SINK_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), MELO_TYPE_SINK, MeloSinkClass))
#define MELO_IS_SINK_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), MELO_TYPE_SINK))
#define MELO_SINK_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), MELO_TYPE_SINK, MeloSinkClass))

typedef struct _MeloSink MeloSink;
typedef struct _MeloSinkClass MeloSinkClass;
typedef struct _MeloSinkPrivate MeloSinkPrivate;

struct _MeloSink {
  GObject parent_instance;

  /*< private >*/
  MeloSinkPrivate *priv;
};

struct _MeloSinkClass {
  GObjectClass parent_class;
};

GType melo_sink_get_type (void);

MeloSink *melo_sink_new (MeloPlayer *player, const gchar *id,
                         const gchar *name);

/* Sink properties */
const gchar *melo_sink_get_id (MeloSink *sink);
const gchar *melo_sink_get_name (MeloSink *sink);

/* Sink control */
GstElement *melo_sink_get_gst_sink (MeloSink *sink);
gboolean melo_sink_get_sync (MeloSink *sink);
void melo_sink_set_sync (MeloSink *sink, gboolean enable);

/* Volume / mute control */
gdouble melo_sink_get_volume (MeloSink *sink);
gdouble melo_sink_set_volume (MeloSink *sink, gdouble volume);
gboolean melo_sink_get_mute (MeloSink *sink);
gboolean melo_sink_set_mute (MeloSink *sink, gboolean mute);

/* Main mixer control */
gboolean melo_sink_main_init (gint rate, gint channels);
gboolean melo_sink_main_release ();

/* Main mixer output settings */
gboolean melo_sink_set_main_config (gint rate, gint channels);
gboolean melo_sink_get_main_config (gint *rate, gint *channels);

/* Main mixer volume / mute control */
gdouble melo_sink_get_main_volume ();
gdouble melo_sink_set_main_volume (gdouble volume);
gboolean melo_sink_get_main_mute ();
gboolean melo_sink_set_main_mute (gboolean mute);

/* Main mixer sink list */
MeloSink *melo_sink_get_sink_by_id (const gchar *id);
GList *melo_sink_get_sink_list (void);

G_END_DECLS

#endif /* __MELO_SINK_H__ */
