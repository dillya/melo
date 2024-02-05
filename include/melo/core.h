// Copyright (C) 2024 Alexandre Dilly <dillya@sparod.com>
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
 * @file core.h
 * @brief Core of Melo.
 */

#ifndef MELO_CORE_H_
#define MELO_CORE_H_

#include <mutex>
#include <string>
#include <unordered_map>

#include <melo/player.h>

namespace melo {

class Core final {
 public:
  Core();
  ~Core();

  // Disable copy / move of core class
  Core(const Core &) = delete;
  Core(Core &&) = delete;

  bool load_plugins(const std::string &path = "");

  bool add_player(const std::string &id, const std::shared_ptr<Player> &player);
  std::shared_ptr<Player> get_player(const std::string &id);

 private:
  std::mutex mutex_;
  std::unordered_map<std::string, std::shared_ptr<Player>> player_list_;
};

}  // namespace melo

#endif  // MELO_CORE_H_
