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

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "melo/melo_log.h"

/* Global log level (default: up to WARNING) */
static MeloLogLevel melo_log_level = MELO_LOG_LEVEL_WARNING;

static const char *level_str[] = {
    [MELO_LOG_LEVEL_CRITICAL] = "\e[7;31mCRITICAL",
    [MELO_LOG_LEVEL_ERROR] = "\e[1;31mERROR",
    [MELO_LOG_LEVEL_WARNING] = "\e[1;33mWARN",
    [MELO_LOG_LEVEL_NOTICE] = "\e[1;32mNOTICE",
    [MELO_LOG_LEVEL_INFO] = "\e[1;35mINFO",
    [MELO_LOG_LEVEL_DEBUG] = "\e[1;36mDEBUG",
};

void
melo_log (const char *tag, MeloLogLevel level, const char *format, ...)
{
  FILE *fp = stderr;
  va_list args;

  /* Ignore log message */
  if (level < MELO_LOG_LEVEL_CRITICAL || level > melo_log_level)
    return;

  /* Print log header */
  fprintf (fp, "\e[0;34m%s:\e[0m %s:\e[0m ", tag, level_str[level]);

  /* Print log message */
  va_start (args, format);
  vfprintf (fp, format, args);
  va_end (args);

  /* End of message */
  fprintf (fp, "\n");
  fflush (fp);
}

static inline MeloLogLevel
melo_log_level_clamp (MeloLogLevel level)
{
  return level > MELO_LOG_LEVEL_DEBUG ? MELO_LOG_LEVEL_DEBUG : level;
}

/**
 * melo_log_init:
 *
 * This function can be used to initialize the logger with environment variables
 * (if found) to override default values.
 *
 * Current environment variables are:
 *  - MELO_LOG_LEVEL: set the log level to the specified value (default: 4).
 */
void
melo_log_init (void)
{
  static bool melo_log_initialized;
  const char *env;

  /* Initialize logger */
  if (melo_log_initialized)
    return;

  /* Get level from environment variable */
  env = getenv ("MELO_LOG_LEVEL");
  if (env) {
    MeloLogLevel level = atoi (env);
    melo_log_level = melo_log_level_clamp (level);
  }

  melo_log_initialized = true;
}

/**
 * melo_log_set_level:
 * @level: the log level to set
 *
 * The log level can be modified at initialization or runtime with this
 * function. Please note, this function is not thread-safe and some melo_log()
 * can use the previous level value.
 */
void
melo_log_set_level (MeloLogLevel level)
{
  melo_log_level = melo_log_level_clamp (level);
}

/**
 * melo_log_get_level:
 *
 * Gets current log level.
 *
 * Returns: the current log level.
 */
MeloLogLevel
melo_log_get_level (void)
{
  return melo_log_level;
}
