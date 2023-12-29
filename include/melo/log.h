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
 * @file log.h
 * @brief Version definition.
 */

#ifndef MELO_LOG_H_
#define MELO_LOG_H_

#include <spdlog/spdlog.h>

/** Log critical message. */
#define MELO_LOGC(...) SPDLOG_CRITICAL(__VA_ARGS__)

/** Log error message. */
#define MELO_LOGE(...) SPDLOG_ERROR(__VA_ARGS__)

/** Log warning message. */
#define MELO_LOGW(...) SPDLOG_WARN(__VA_ARGS__)

/** Log info message. */
#define MELO_LOGI(...) SPDLOG_INFO(__VA_ARGS__)

/** Log debug message. */
#define MELO_LOGD(...) SPDLOG_DEBUG(__VA_ARGS__)

#endif  // MELO_LOG_H_
