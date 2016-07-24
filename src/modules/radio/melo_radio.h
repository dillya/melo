/*
 * melo_radio.h: Radio module for radio / webradio playing
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

#ifndef __MELO_RADIO_H__
#define __MELO_RADIO_H__

#include "melo_module.h"

G_BEGIN_DECLS

#define MELO_TYPE_RADIO             (melo_radio_get_type ())
#define MELO_RADIO(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), MELO_TYPE_RADIO, MeloRadio))
#define MELO_IS_RADIO(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MELO_TYPE_RADIO))
#define MELO_RADIO_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), MELO_TYPE_RADIO, MeloRadioClass))
#define MELO_IS_RADIO_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), MELO_TYPE_RADIO))
#define MELO_RADIO_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), MELO_TYPE_RADIO, MeloRadioClass))

typedef struct _MeloRadio MeloRadio;
typedef struct _MeloRadioClass MeloRadioClass;
typedef struct _MeloRadioPrivate MeloRadioPrivate;

struct _MeloRadio {
  MeloModule parent_instance;

  /*< private >*/
  MeloRadioPrivate *priv;
};

struct _MeloRadioClass {
  MeloModuleClass parent_class;
};

GType melo_radio_get_type (void);

G_END_DECLS

#endif /* __MELO_RADIO_H__ */
