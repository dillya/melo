/*
 * melo_file.h: File module for local / remote file playing
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

#ifndef __MELO_FILE_H__
#define __MELO_FILE_H__

#include "melo_module.h"

G_BEGIN_DECLS

#define MELO_TYPE_FILE             (melo_file_get_type ())
#define MELO_FILE(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), MELO_TYPE_FILE, MeloFile))
#define MELO_IS_FILE(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MELO_TYPE_FILE))
#define MELO_FILE_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), MELO_TYPE_FILE, MeloFileClass))
#define MELO_IS_FILE_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), MELO_TYPE_FILE))
#define MELO_FILE_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), MELO_TYPE_FILE, MeloFileClass))

typedef struct _MeloFile MeloFile;
typedef struct _MeloFileClass MeloFileClass;

struct _MeloFile {
  MeloModule parent_instance;
};

struct _MeloFileClass {
  MeloModuleClass parent_class;
};

GType melo_file_get_type (void);

G_END_DECLS

#endif /* __MELO_FILE_H__ */
