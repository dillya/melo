/*
 * melo_library_file.h: File Library using file database
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

#ifndef __MELO_LIBRARY_FILE_H__
#define __MELO_LIBRARY_FILE_H__

#include "melo_browser.h"
#include "melo_file_db.h"

G_BEGIN_DECLS

#define MELO_TYPE_LIBRARY_FILE             (melo_library_file_get_type ())
#define MELO_LIBRARY_FILE(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), MELO_TYPE_LIBRARY_FILE, MeloLibraryFile))
#define MELO_IS_LIBRARY_FILE(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MELO_TYPE_LIBRARY_FILE))
#define MELO_LIBRARY_FILE_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), MELO_TYPE_LIBRARY_FILE, MeloLibraryFileClass))
#define MELO_IS_LIBRARY_FILE_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), MELO_TYPE_LIBRARY_FILE))
#define MELO_LIBRARY_FILE_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), MELO_TYPE_LIBRARY_FILE, MeloLibraryFileClass))

typedef struct _MeloLibraryFile MeloLibraryFile;
typedef struct _MeloLibraryFileClass MeloLibraryFileClass;
typedef struct _MeloLibraryFilePrivate MeloLibraryFilePrivate;

struct _MeloLibraryFile {
  MeloBrowser parent_instance;

  /*< private >*/
  MeloLibraryFilePrivate *priv;
};

struct _MeloLibraryFileClass {
  MeloBrowserClass parent_class;
};

GType melo_library_file_get_type (void);

void melo_library_file_set_db (MeloLibraryFile *lfile, MeloFileDB *fdb);

G_END_DECLS

#endif /* __MELO_LIBRARY_FILE_H__ */
