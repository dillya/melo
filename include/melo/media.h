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
 * @file media.h
 * @brief Media class definition.
 */

#ifndef MELO_MEDIA_H_
#define MELO_MEDIA_H_

#include <memory>
#include <string>

namespace melo {

/**
 * TODO
 */
class Media {
 public:
  Media() {}
  Media(const std::string &player_id, const std::string &uri)
      : player_id_(player_id), uri_(uri) {}

  inline const std::string &get_player_id() const { return player_id_; }
  inline const std::string &get_uri() const { return uri_; }

 private:
  std::string player_id_;
  std::string uri_;
};

}  // namespace melo

#endif  // MELO_MEDIA_H_
