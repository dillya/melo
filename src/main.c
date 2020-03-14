/*
 * Copyright (C) 2020 Alexandre Dilly <dillya@sparod.com>
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation; either version 2.1 of the License, or any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.
 * See the GNU Lesser General Public License for more details.

 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA.
 */

#include <melo/melo.h>
#include <melo/melo_module.h>

#define MELO_LOG_TAG "melo"
#include <melo/melo_log.h>

int
main (int argc, char *argv[])
{
  int ret = -1;

  /* Initialize Melo library */
  melo_init (&argc, &argv);

  /* Load modules */
  melo_module_load ();

  /* Exit successfully */
  ret = 0;

  /* Unload modules */
  melo_module_unload ();

  /* Clean Melo library */
  melo_deinit ();

  return ret;
}
