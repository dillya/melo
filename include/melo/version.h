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
 * @file version.h
 * @brief Version definition.
 */

#ifndef MELO_VERSION_H_
#define MELO_VERSION_H_

#define MELO_VERSION_MAJOR 1 /**< Major version. */
#define MELO_VERSION_MINOR 0 /**< Minor version. */
#define MELO_VERSION_PATCH 0 /**< Patch version. */
#define MELO_VERSION_REVISION \
  MELO_VERSION_PATCH /**< Revision version (same as patch). */

#define MELO_STR_EXP(__A) #__A /**< Macro to expand to string. */
#define MELO_STR(__A) \
  MELO_STR_EXP(__A) /**< Macro to convert integer to string. */

/** Melo version string. */
#define MELO_VERSION_STR       \
  MELO_STR(MELO_VERSION_MAJOR) \
  "." MELO_STR(MELO_VERSION_MINOR) "." MELO_STR(MELO_VERSION_PATCH)

namespace melo {

/**
 * Get Melo version as a string.
 *
 * @return A string containing the version of Melo.
 */
static inline const char *get_version() { return MELO_VERSION_STR; }

}  // namespace melo

#endif  // MELO_VERSION_H_
