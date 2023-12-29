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

#include "melo/player.h"

#include <mutex>
#include <unordered_map>

#include "melo/log.h"
#include "melo/utils.h"

namespace melo {

const std::string Player::kDefaultDescription;

static std::mutex g_mutex;
static std::unordered_map<std::string, std::shared_ptr<Player>> g_list;

bool Player::add(const std::string &id, const std::shared_ptr<Player> &player) {
  const std::lock_guard<std::mutex> lock(g_mutex);

  // Check ID compliance
  if (!is_valid_id(id)) {
    MELO_LOGE("player ID '{}' is not compliant", id);
    return false;
  }

  // Check player is already registered
  if (g_list.find(id) != g_list.end()) {
    MELO_LOGE("player '{}' is already registered", id);
    return false;
  }

  MELO_LOGI("add new player '{}'", id);

  // Add player
  g_list[id] = player;

  return true;
}

bool Player::remove(const std::string &id) {
  const std::lock_guard<std::mutex> lock(g_mutex);

  // Find player by ID
  auto it = g_list.find(id);
  if (it == g_list.end()) {
    MELO_LOGE("player '{}' is not registered", id);
    return false;
  }

  MELO_LOGI("remove player '{}'", id);

  // Remove player
  g_list.erase(it);

  return true;
}

std::shared_ptr<Player> Player::get_by_id(const std::string &id) {
  const std::lock_guard<std::mutex> lock(g_mutex);

  // Find player by ID
  auto it = g_list.find(id);
  return it != g_list.end() ? it->second : nullptr;
}

bool Player::has(const std::string &id) {
  const std::lock_guard<std::mutex> lock(g_mutex);
  return g_list.find(id) != g_list.end();
}

bool Player::update_media(const Media &media) {
  media_ = media;
  return true;
}

}  // namespace melo
