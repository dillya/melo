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

#ifndef _MELO_LOG_H_
#define _MELO_LOG_H_

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef enum _MeloLogLevel MeloLogLevel;

/**
 * MeloLogLevel:
 * @MELO_LOG_LEVEL_CRITICAL: Critical level
 * @MELO_LOG_LEVEL_ERROR: Error level
 * @MELO_LOG_LEVEL_WARNING: Warning level
 * @MELO_LOG_LEVEL_NOTICE: Notice level
 * @MELO_LOG_LEVEL_INFO: Info level
 * @MELO_LOG_LEVEL_DEBUG: Debug level
 *
 * This value is used to differentiate log messages and know the importance of
 * each one.
 *
 * Any log message with a level upper than the log level set during
 * initialization with melo_log_init() or melo_log_set_level() will be
 * discarded.
 * By default, the level is set to #MELO_LOG_LEVEL_WARNING.
 */
enum _MeloLogLevel {
  MELO_LOG_LEVEL_CRITICAL = 2,
  MELO_LOG_LEVEL_ERROR = 3,
  MELO_LOG_LEVEL_WARNING = 4,
  MELO_LOG_LEVEL_NOTICE = 5,
  MELO_LOG_LEVEL_INFO = 6,
  MELO_LOG_LEVEL_DEBUG = 7,
};

/*
 * When using one of the MELO_LOGx() macros, the tag of the log message is set
 * to the value defined by the marco MELO_LOG_TAG. By default, the value is set
 * to libmelo, to override it, the MELO_LOG_TAG macro should be defined before
 * including the melo_tag.h file.
 */
#ifndef MELO_LOG_TAG
#define MELO_LOG_TAG "libmelo"
#endif

/* Log macros */
#define MELO_LOGC(_fmt, ...) \
  melo_log (MELO_LOG_TAG, MELO_LOG_LEVEL_CRITICAL, _fmt, ##__VA_ARGS__)
#define MELO_LOGE(_fmt, ...) \
  melo_log (MELO_LOG_TAG, MELO_LOG_LEVEL_ERROR, _fmt, ##__VA_ARGS__)
#define MELO_LOGW(_fmt, ...) \
  melo_log (MELO_LOG_TAG, MELO_LOG_LEVEL_WARNING, _fmt, ##__VA_ARGS__)
#define MELO_LOGN(_fmt, ...) \
  melo_log (MELO_LOG_TAG, MELO_LOG_LEVEL_NOTICE, _fmt, ##__VA_ARGS__)
#define MELO_LOGI(_fmt, ...) \
  melo_log (MELO_LOG_TAG, MELO_LOG_LEVEL_INFO, _fmt, ##__VA_ARGS__)
#define MELO_LOGD(_fmt, ...) \
  melo_log (MELO_LOG_TAG, MELO_LOG_LEVEL_DEBUG, _fmt, ##__VA_ARGS__)

void melo_log (const char *tag, MeloLogLevel level, const char *format, ...);

void melo_log_init (void);

void melo_log_set_level (MeloLogLevel level);
MeloLogLevel melo_log_get_level (void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* !_MELO_LOG_H_ */
