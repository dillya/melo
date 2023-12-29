// Copyright (C) 2023 Alexandre Dilly <dillya@sparod.com>
//
// This program is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option) any
// later version.
//
// This program is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for more
// details.
//
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.

/**
 * @file playlist.h
 * @brief Playlist class definition.
 */

#ifndef MELO_PLAYLIST_H_
#define MELO_PLAYLIST_H_

#include <deque>
#include <limits>
#include <vector>

#include <melo/media.h>

namespace melo {

/**
 * Playlist class to handle media(s) and playback order.
 *
 * The playlist is the heart of Melo to manage media(s) playback between several
 * Browser and Player implementations.
 */
class Playlist {
 public:
  static const size_t kNoMediaIndex = std::numeric_limits<size_t>::max();

  /**
   * Play a media immediately.
   *
   * This function is called to play a media directly. It will be automatically
   * placed on top of the global playlist.
   *
   * @param[in] media The media to play
   * @return `true` if the media has been added and is playing, `false`
   * otherwise.
   */
  static bool play(const Media &media);

  /**
   * Play a collection of media.
   *
   * This function will add the list on top of the playlist and will play
   * immediately the first media (at index 0).
   *
   * The `media` parameter will represent the parent of the playlist. It can be
   * used to display the directory or a playlist in the Browser.
   *
   * @note The player ID should be the same in `media` and all medias handled by
   * `list`.
   *
   * @param[in] media The parent media to play
   * @param[in] list A `std::vector` of media to play
   * @return `true` if the list has been added and first media is playing,
   * `false` otherwise.
   */
  static bool play(const Media &media, const std::vector<Media> &list);

  /**
   * Add a media on top of the playlist.
   *
   * This function is called to add a media on top of the global playlist. It
   * will be played later by the playlist.
   *
   * @param[in] media The media to add
   * @return `true` if the media has been added, `false` otherwise.
   */
  static bool add(const Media &media);

  /**
   * Add a collection of media.
   *
   * This function will add a list of media on top of the playlist.
   *
   * The `media` parameter will represent the parent of the playlist. It can be
   * used to display the directory or a playlist in the Browser.
   *
   * @note The player ID should be the same in `media` and all medias handled by
   * `list`.
   *
   * @param[in] media The parent media to add
   * @param[in] list A `std::vector` of media to add
   * @return `true` if the list has been added, `false` otherwise.
   */
  static bool add(const Media &media, const std::vector<Media> &list);

  static bool swap(size_t source, size_t destination);
  static bool swap(size_t index, size_t source, size_t destination);

  // TODO
  static inline bool remove(size_t index) {
    return remove(index, kNoMediaIndex);
  }
  static bool remove(size_t index, size_t media_index);

  // TODO
  static inline bool play(size_t index) { return play(index, kNoMediaIndex); }
  static bool play(size_t index, size_t media_index);

  // TODO
  static bool previous(bool parent = false);
  static bool next(bool parent = false);

  static std::shared_ptr<Playlist> get_playlist(size_t index);
  static std::shared_ptr<Playlist> get_current_playlist();
  static size_t get_playlist_count();

  /**
   * Clear global playlist.
   */
  static void clear();

  // TODO
  Playlist(const Media &media);
  Playlist(const Media &media, const std::vector<Media> &list);

  // TODO
  inline const Media &get_media() const { return media_; }
  inline const std::string &get_player_id() const {
    return media_.get_player_id();
  }
  inline const std::string &get_uri() const { return media_.get_uri(); }

  // TODO
  inline size_t get_count() const { return list_.size(); }
  inline Media get_media(size_t index) const {
    return index < list_.size() ? list_[index] : Media{};
  }
  inline std::string get_uri(size_t index) const {
    return get_media(index).get_uri();
  }

  // TODO
  inline Media get_current() const {
    if (current_ < list_.size())
      return list_[current_];
    return media_;
  }

 private:
  const Media media_;

  size_t current_;
  std::deque<Media> list_;
};

}  // namespace melo

#endif  // MELO_PLAYLIST_H_
