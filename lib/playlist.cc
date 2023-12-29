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

#include "melo/playlist.h"

#include <deque>
#include <memory>
#include <mutex>

#include "melo/log.h"
#include "melo/player.h"

namespace melo {

struct Context {
  std::recursive_mutex mutex;
  std::deque<std::shared_ptr<Playlist>> list;
  size_t current;
  std::shared_ptr<Player> current_player;
};

static Context g_ctx;

bool Playlist::play(const Media &media) {
  const std::lock_guard<std::recursive_mutex> lock(g_ctx.mutex);

  // Add media to playlist
  if (!Playlist::add(media))
    return false;

  return Playlist::play(g_ctx.list.size() - 1);
}

bool Playlist::play(const Media &media, const std::vector<Media> &list) {
  const std::lock_guard<std::recursive_mutex> lock(g_ctx.mutex);

  // Add media list to playlist
  if (!Playlist::add(media, list))
    return false;

  return Playlist::play(g_ctx.list.size() - 1);
}

bool Playlist::add(const Media &media) {
  const std::lock_guard<std::recursive_mutex> lock(g_ctx.mutex);

  // Check player is registered
  if (!Player::has(media.get_player_id())) {
    MELO_LOGE("player '{}' is not available", media.get_player_id());
    return false;
  }

  // Add a new playlist on top of the global list
  g_ctx.list.emplace_back(std::make_shared<Playlist>(media));

  return true;
}

bool Playlist::add(const Media &media, const std::vector<Media> &list) {
  const std::lock_guard<std::recursive_mutex> lock(g_ctx.mutex);

  // Check player is registered
  const std::string &player_id = media.get_player_id();
  if (!Player::has(player_id)) {
    MELO_LOGE("player '{}' is not available", player_id);
    return false;
  }

  // Check all medias are using same player
  if (std::any_of(list.cbegin(), list.cend(), [&player_id](const Media &media) {
        return player_id != media.get_player_id();
      })) {
    MELO_LOGE("mixed player ID detected in playlist");
    return false;
  }

  // Add a new playlist on top of the global list
  g_ctx.list.emplace_back(std::make_shared<Playlist>(media, list));

  return true;
}

bool Playlist::swap(size_t source, size_t destination) {
  const std::lock_guard<std::recursive_mutex> lock(g_ctx.mutex);

  // Invalid indices
  if (source >= g_ctx.list.size() || destination >= g_ctx.list.size()) {
    MELO_LOGE("invalid playlist indices {} && {} >= {}", source, destination,
              g_ctx.list.size());
    return false;
  }

  // Swap playlist
  auto tmp = g_ctx.list[source];
  g_ctx.list[source] = g_ctx.list[destination];
  g_ctx.list[destination] = tmp;

  // Update current
  if (g_ctx.current == source)
    g_ctx.current = destination;
  else if (g_ctx.current == destination)
    g_ctx.current = source;

  return true;
}

bool Playlist::swap(size_t index, size_t source, size_t destination) {
  const std::lock_guard<std::recursive_mutex> lock(g_ctx.mutex);

  // Invalid index
  if (index >= g_ctx.list.size()) {
    MELO_LOGE("invalid playlist index {} >= {}", index, g_ctx.list.size());
    return false;
  }

  // Get playlist
  auto playlist = g_ctx.list[index];

  // Invalid media index
  if (source >= playlist->list_.size() ||
      destination >= playlist->list_.size()) {
    MELO_LOGE("invalid playlist media indices {} && {} >= {}", source,
              destination, playlist->list_.size());
    return false;
  }

  // Swap medias
  auto tmp = playlist->list_[source];
  playlist->list_[source] = playlist->list_[destination];
  playlist->list_[destination] = tmp;

  // Update current
  if (playlist->current_ == source)
    playlist->current_ = destination;
  else if (playlist->current_ == destination)
    playlist->current_ = source;

  return true;
}

bool Playlist::remove(size_t index, size_t media_index) {
  const std::lock_guard<std::recursive_mutex> lock(g_ctx.mutex);

  // Invalid index
  if (index >= g_ctx.list.size()) {
    MELO_LOGE("invalid playlist index {} >= {}", index, g_ctx.list.size());
    return false;
  }

  // Get playlist
  auto playlist = g_ctx.list[index];

  // Handle media only
  if (media_index != kNoMediaIndex) {
    // Invalid media index
    if (media_index >= playlist->list_.size()) {
      MELO_LOGE("invalid playlist media index {} >= {}", media_index,
                playlist->list_.size());
      return false;
    }

    // Remove media only
    if (playlist->current_ == media_index) {
      // Reset player
      if (g_ctx.current == index && g_ctx.current_player)
        g_ctx.current_player->reset();

      // Update current
      if (media_index == playlist->current_)
        playlist->current_ = kNoMediaIndex;
    } else if (playlist->current_ != kNoMediaIndex &&
               media_index < playlist->current_)
      playlist->current_--;

    // Remove element
    playlist->list_.erase(playlist->list_.begin() + media_index);

    // No more media
    if (playlist->list_.empty())
      playlist->current_ = kNoMediaIndex;

    return true;
  }

  // Reset player
  if (g_ctx.current == index && g_ctx.current_player)
    g_ctx.current_player->reset();

  // Remove playlist
  g_ctx.list.erase(g_ctx.list.begin() + index);

  // Update current index
  if (index == g_ctx.current)
    g_ctx.current = 0;
  else if (index < g_ctx.current)
    g_ctx.current--;

  return true;
}

bool Playlist::play(size_t index, size_t media_index) {
  const std::lock_guard<std::recursive_mutex> lock(g_ctx.mutex);

  // Invalid index
  if (index >= g_ctx.list.size()) {
    MELO_LOGE("invalid playlist index {} >= {}", index, g_ctx.list.size());
    return false;
  }

  // Find next valid playlist + player
  std::shared_ptr<Playlist> playlist;
  std::shared_ptr<Player> player;
  do {
    // Get playlist
    playlist = g_ctx.list[index];

    // Get player from playlist
    player = Player::get_by_id(playlist->get_player_id());
    if (player == nullptr) {
      // Go to next playlist
      index++;
      if (index == g_ctx.list.size())
        return false;
    }
  } while (player == nullptr);

  // Reset playlist
  if (media_index != kNoMediaIndex) {
    // Invalid media index
    if (media_index >= playlist->list_.size()) {
      MELO_LOGE("invalid playlist media index {} >= {}", media_index,
                playlist->list_.size());
      return false;
    }

    // Set current
    playlist->current_ = media_index;
  } else
    playlist->current_ = playlist->list_.size() ? 0 : kNoMediaIndex;

  // Reset current player
  if (g_ctx.current_player)
    g_ctx.current_player->reset();

  // Update index
  g_ctx.current = index;
  g_ctx.current_player = player;

  // Play new playlist
  return player->play(playlist);
}

bool Playlist::previous(bool parent) {
  const std::lock_guard<std::recursive_mutex> lock(g_ctx.mutex);

  // Nothing to play
  if (g_ctx.list.empty())
    return false;

  // Get current playlist
  auto playlist = g_ctx.list[g_ctx.current];

  // Previous playlist or media
  if (!parent && playlist->current_ != kNoMediaIndex && playlist->current_ > 0)
    return Playlist::play(g_ctx.current, playlist->current_ - 1);

  // End of playlist
  if (g_ctx.current == 0)
    return false;

  // Move to previous playlist
  playlist = g_ctx.list[g_ctx.current - 1];
  if (!playlist->list_.empty())
    return Playlist::play(g_ctx.current - 1, playlist->list_.size() - 1);
  else
    return Playlist::play(g_ctx.current - 1);

  return false;
}

bool Playlist::next(bool parent) {
  const std::lock_guard<std::recursive_mutex> lock(g_ctx.mutex);

  // Nothing to play
  if (g_ctx.list.empty() || g_ctx.current > g_ctx.list.size() - 1)
    return false;

  // Get current playlist
  auto playlist = g_ctx.list[g_ctx.current];

  // Next playlist or media
  if (!parent && playlist->current_ < playlist->list_.size() - 1)
    return Playlist::play(g_ctx.current, playlist->current_ + 1);
  else if (g_ctx.current < g_ctx.list.size() - 1)
    return Playlist::play(g_ctx.current + 1);

  return false;
}

std::shared_ptr<Playlist> Playlist::get_playlist(size_t index) {
  const std::lock_guard<std::recursive_mutex> lock(g_ctx.mutex);

  return index < g_ctx.list.size() ? g_ctx.list[index] : nullptr;
}

std::shared_ptr<Playlist> Playlist::get_current_playlist() {
  const std::lock_guard<std::recursive_mutex> lock(g_ctx.mutex);

  return get_playlist(g_ctx.current);
}

size_t Playlist::get_playlist_count() { return g_ctx.list.size(); }

void Playlist::clear() {
  const std::lock_guard<std::recursive_mutex> lock(g_ctx.mutex);

  // Reset current playlist
  if (g_ctx.current_player)
    g_ctx.current_player->reset();
  g_ctx.current_player = nullptr;
  g_ctx.current = 0;

  // Clear list
  g_ctx.list.clear();
}

Playlist::Playlist(const Media &media)
    : media_(media), current_(kNoMediaIndex) {}

Playlist::Playlist(const Media &media, const std::vector<Media> &list)
    : media_(media), current_(kNoMediaIndex) {
  // Create list
  for (auto &m : list) list_.emplace_back(m);
}

}  // namespace melo
