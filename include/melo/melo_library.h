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

#ifndef _MELO_LIBRARY_H_
#define _MELO_LIBRARY_H_

#include <stdint.h>

#include <melo/melo_tags.h>

typedef enum _MeloLibraryType MeloLibraryType;
typedef enum _MeloLibraryField MeloLibraryField;
typedef struct _MeloLibraryData MeloLibraryData;

/**
 * MeloLibraryType:
 * @MELO_LIBRARY_TYPE_MEDIA: media entries composed of following fields
 *     - #MELO_LIBRARY_FIELD_PLAYER_ID,
 *     - #MELO_LIBRARY_FIELD_PATH_ID,
 *     - #MELO_LIBRARY_FIELD_MEDIA_ID,
 *     - #MELO_LIBRARY_FIELD_PLAYER,
 *     - #MELO_LIBRARY_FIELD_PATH,
 *     - #MELO_LIBRARY_FIELD_MEDIA,
 *     - #MELO_LIBRARY_FIELD_NAME,
 *     - #MELO_LIBRARY_FIELD_TIMESTAMP,
 *     - #MELO_LIBRARY_FIELD_TITLE,
 *     - #MELO_LIBRARY_FIELD_ARTIST,
 *     - #MELO_LIBRARY_FIELD_ALBUM,
 *     - #MELO_LIBRARY_FIELD_GENRE,
 *     - #MELO_LIBRARY_FIELD_DATE,
 *     - #MELO_LIBRARY_FIELD_TRACK,
 *     - #MELO_LIBRARY_FIELD_COVER.
 * @MELO_LIBRARY_TYPE_ARTIST: artist entries composed of following fields
 *     - #MELO_LIBRARY_FIELD_ARTIST_ID,
 *     - #MELO_LIBRARY_FIELD_NAME,
 * @MELO_LIBRARY_TYPE_ALBUM: album entries composed of following fields
 *     - #MELO_LIBRARY_FIELD_ALBUM_ID,
 *     - #MELO_LIBRARY_FIELD_NAME,
 * @MELO_LIBRARY_TYPE_GENRE: genre entries composed of following fields
 *     - #MELO_LIBRARY_FIELD_GENRE_ID,
 *     - #MELO_LIBRARY_FIELD_NAME,
 *
 * This enumerator defines the different type of data which can be fetched with
 * melo_library_find(). Only the fields listed for each type can be used within
 * the according type.
 */
enum _MeloLibraryType {
  MELO_LIBRARY_TYPE_MEDIA = 0,
  MELO_LIBRARY_TYPE_ARTIST,
  MELO_LIBRARY_TYPE_ALBUM,
  MELO_LIBRARY_TYPE_GENRE,

  MELO_LIBRARY_TYPE_COUNT
};

/**
 * MeloLibraryField:
 * @MELO_LIBRARY_FIELD_NONE: no field to set
 * @MELO_LIBRARY_FIELD_LAST: last field condition (see melo_library_find())
 * @MELO_LIBRARY_FIELD_PLAYER_ID: the player ID in library database
 * @MELO_LIBRARY_FIELD_PATH_ID: the path ID in library database
 * @MELO_LIBRARY_FIELD_MEDIA_ID: the media ID in library database
 * @MELO_LIBRARY_FIELD_ARTIST_ID: the artist ID in library database
 * @MELO_LIBRARY_FIELD_ALBUM_ID: the album ID in library database
 * @MELO_LIBRARY_FIELD_GENRE_ID: the genre ID in library database
 * @MELO_LIBRARY_FIELD_PLAYER: the player for a media
 * @MELO_LIBRARY_FIELD_PATH: the path for a media (dirname of full path)
 * @MELO_LIBRARY_FIELD_MEDIA: the media resource name (basename of full path)
 * @MELO_LIBRARY_FIELD_NAME: the display name
 * @MELO_LIBRARY_FIELD_TIMESTAMP: the timestamp of the media
 * @MELO_LIBRARY_FIELD_TITLE: the title of the media
 * @MELO_LIBRARY_FIELD_ARTIST: the artist of the media
 * @MELO_LIBRARY_FIELD_ALBUM: the album of the media
 * @MELO_LIBRARY_FIELD_GENRE: the genre of the media
 * @MELO_LIBRARY_FIELD_DATE: the data of the media
 * @MELO_LIBRARY_FIELD_TRACK: the track of the media
 * @MELO_LIBRARY_FIELD_COVER: the cover of the entry
 * @MELO_LIBRARY_FIELD_FAVORITE: the media is marked as 'favorite'
 *
 * This enumerator holds all possible fields which can be accessible from a
 * #MeloLibraryType entry in library database. See #MeloLibraryType for more
 * details on available fields for each types.
 */
enum _MeloLibraryField {
  MELO_LIBRARY_FIELD_NONE = 0,
  MELO_LIBRARY_FIELD_LAST = MELO_LIBRARY_FIELD_NONE,

  MELO_LIBRARY_FIELD_PLAYER_ID,
  MELO_LIBRARY_FIELD_PATH_ID,
  MELO_LIBRARY_FIELD_MEDIA_ID,
  MELO_LIBRARY_FIELD_ARTIST_ID,
  MELO_LIBRARY_FIELD_ALBUM_ID,
  MELO_LIBRARY_FIELD_GENRE_ID,

  MELO_LIBRARY_FIELD_PLAYER,
  MELO_LIBRARY_FIELD_PATH,
  MELO_LIBRARY_FIELD_MEDIA,
  MELO_LIBRARY_FIELD_NAME,

  MELO_LIBRARY_FIELD_TIMESTAMP,

  MELO_LIBRARY_FIELD_TITLE,
  MELO_LIBRARY_FIELD_ARTIST,
  MELO_LIBRARY_FIELD_ALBUM,
  MELO_LIBRARY_FIELD_GENRE,
  MELO_LIBRARY_FIELD_DATE,
  MELO_LIBRARY_FIELD_TRACK,
  MELO_LIBRARY_FIELD_COVER,

  MELO_LIBRARY_FIELD_FAVORITE,

  MELO_LIBRARY_FIELD_COUNT
};

/**
 * MELO_LIBRARY_SELECT:
 *
 * This macro can be used with the select parameter of melo_library_find(): you
 * just need to pass as parameter to this macro the suffix of one of the
 * #MeloLibraryField values. For #MELO_LIBRARY_FIELD_NAME, you will use the
 * macro as following: MELO_LIBRARY_SELECT(NAME).
 * Multiple MELO_LIBRARY_SELECT() can be combined with the operator '|', in
 * order to select multiple fields.
 */
#define MELO_LIBRARY_SELECT(x) (1 << MELO_LIBRARY_FIELD_##x)

/**
 * MeloLibraryFlag:
 * @MELO_LIBRARY_FLAG_NONE: no flag set
 * @MELO_LIBRARY_FLAG_FAVORITE: set the media as favorite. It will be available
 *     with #MELO_LIBRARY_FIELD_FAVORITE
 * @MELO_LIBRARY_FLAG_FAVORITE_ONLY: set the media as favorite and if the media
 *     doesn't exist in database, it will marked as 'to delete' if the favorite
 *     field is removed with melo_library_remove_flags()
 *
 * This flags can be used to add some specificity to a media added in library
 * database, such as it is marked as favorite.
 * These values are used in melo_library_add_media() as a combination using the
 * OR bit-operator.
 */
enum _MeloLibraryFlag {
  MELO_LIBRARY_FLAG_NONE = 0,
  MELO_LIBRARY_FLAG_FAVORITE = (1 << 0),
  MELO_LIBRARY_FLAG_FAVORITE_ONLY = (1 << 1),
};

/**
 * MeloLibraryData:
 * @path_id: the path ID in library database, selected with
 *     #MELO_LIBRARY_SELECT(PATH_ID)
 * @media_id: the media ID in library database, selected with
 *     #MELO_LIBRARY_SELECT(MEDIA_ID)
 * @artist_id: the artist ID in library database, selected with
 *     #MELO_LIBRARY_SELECT(ARTIST_ID)
 * @album_id: the album ID in library database, selected with
 *     #MELO_LIBRARY_SELECT(ALBUM_ID)
 * @genre_id: the genre ID in library database, selected with
 *     #MELO_LIBRARY_SELECT(GENRE_ID)
 * @id: the ID of the current row in library database, as string
 * @name: the name to display, selected with #MELO_LIBRARY_SELECT(NAME)
 * @player: the player (as string), only valid for media, selected with
 *     #MELO_LIBRARY_SELECT(PLAYER)
 * @path: the path to use with player, only valid for media, selected with
 *     #MELO_LIBRARY_SELECT(PATH)
 * @media: the media resource, only valid for media, selected with
 *     #MELO_LIBRARY_SELECT(MEDIA)
 * @timestamp: the media timestamp, only valid for media, selected with
 *     #MELO_LIBRARY_SELECT(TIMESTAMP)
 * @flags: the media flags, only valid for media
 *
 * This structure contains the data fetched from the library database.
 *
 * To play a media, the @player, @path and @media must be set, and the full
 * media path must be built from @path and @media.
 */
struct _MeloLibraryData {
  uint64_t path_id;
  uint64_t media_id;
  uint64_t artist_id;
  uint64_t album_id;
  uint64_t genre_id;

  const char *id;
  const char *name;
  const char *player;
  const char *path;
  const char *media;
  uint64_t timestamp;
  unsigned int flags;
};

/**
 * MeloLibraryCb:
 * @data: a #MeloLibraryData with fetched data
 * @tags: (nullable): a #MeloTags with fetched data
 * @user_data: user data passed to the callback
 *
 * This function is called for each results row of a request sent to the library
 * database.
 *
 * The @tags belongs to the caller, so it the tags should remains valid after
 * this call, a reference must be taken with melo_tags_ref().
 *
 * If the function returns %false, the request is aborted and this callback wont
 * be called anymore, even if there are still data to present.
 *
 * Returns: %true to continue, %false to abort request.
 */
typedef bool (*MeloLibraryCb) (
    const MeloLibraryData *data, MeloTags *tags, void *user_data);

uint64_t melo_library_get_player_id (const char *player);
uint64_t melo_library_get_path_id (const char *path);
uint64_t melo_library_get_media_id (const char *player, uint64_t player_id,
    const char *path, uint64_t path_id, const char *media);
uint64_t melo_library_get_media_id_from_browser (
    const char *tags_browser, const char *tags_media_id);

bool melo_library_add_media (const char *player, uint64_t player_id,
    const char *path, uint64_t path_id, const char *media, uint64_t media_id,
    unsigned int update, const char *name, MeloTags *tags, uint64_t timestamp,
    unsigned int flags);

unsigned int melo_library_media_get_flags (uint64_t media_id);
bool melo_library_update_media_flags (
    uint64_t media_id, unsigned int flags, bool unset);

bool melo_library_find (MeloLibraryType type, MeloLibraryCb cb, void *user_data,
    unsigned int select, size_t count, off_t offset,
    MeloLibraryField sort_field, bool sort_desc, bool match,
    MeloLibraryField field0, ...);

#endif /* !_MELO_LIBRARY_H_ */
