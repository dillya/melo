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
 * @file utils.h
 * @brief Some utility functions for Melo.
 */

#ifndef MELO_UTILS_H_
#define MELO_UTILS_H_

namespace melo {

/**
 * Check if a string is a valid ID.
 *
 * A valid ID is composed only by <b>lowercase alphanumeric characters</b> and
 * some special characters which are <b>.</b>, <b>-</b> and <b>_</b>.
 *
 * The general idea of an ID is to use a reversed Internet domain name as Java
 * does, in order to minimize possible conflicts (e.g. com.sparod.files.player).
 *
 * @param[in] id The string to check
 * @return `true` if the ID is valid, `false` otherwise.
 */
static inline bool is_valid_id(const std::string &id) {
  return id.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789.-_") ==
         std::string::npos;
}

}  // namespace melo

#endif  // MELO_UTILS_H_
