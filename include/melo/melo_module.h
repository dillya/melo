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

#ifndef _MELO_MODULE_H_
#define _MELO_MODULE_H_

/* Melo version generator. */
#define MELO_VERSION(_major, _minor, _revision) \
  (((_major & 0xFF) << 24) | ((_minor & 0xFFF) << 12) | (_revision & 0xFFF))
#define MELO_VERSION_GET_MAJOR(_ver) ((_ver >> 24) & 0xFF)
#define MELO_VERSION_GET_MINOR(_ver) ((_ver >> 12) & 0xFFF)
#define MELO_VERSION_GET_REVISION(_ver) (_ver & 0xFFF)

/* Current Melo API version */
#define MELO_VERSION_MAJOR 1
#define MELO_VERSION_MINOR 0
#define MELO_VERSION_REVISION 0
#define MELO_API_VERSION \
  MELO_VERSION (MELO_VERSION_MAJOR, MELO_VERSION_MINOR, MELO_VERSION_REVISION)

/* Melo module symbol */
#define MELO_MODULE_SYM melo_module
#define MELO_MODULE_SYM_STR "melo_module"

typedef struct _MeloModule MeloModule;

/**
 * MeloModuleEnableCb:
 *
 * This function is called when a module is enabled: the browser(s) and
 * player(s) embedded in the module should be created in this function.
 */
typedef void (*MeloModuleEnableCb) (void);

/**
 * MeloModuleDisableCb:
 *
 * This function is called when a module is disabled: the browser(s) and
 * player(s) embedded in the module and added during the call to the
 * MeloModuleEnableCb() function should be deleted in this function.
 */
typedef void (*MeloModuleDisableCb) (void);

/**
 * MeloModule:
 * @id: this string is unique and if another module is already registered with
 *     the same ID, this module won't be registered (e.g. "com.sparod.file")
 * @version: this value must be set with a valid version. The MELO_VERSION()
 *     macro must be used to format the version.
 * @api_version: this value must be set with the value of #MELO_API_VERSION. It
 *     is used during registration to check if a module is compatible or not
 *     with current Melo version.
 * @name: name of module which will be displayed in UI
 * @description: description string of module which will be displayed in UI
 * @browser_list: list of browser registered by this module. The list is an
 *     array of string, NULL terminated.
 * @player_list: list of player registered by this module. The list is an array
 *     of string, NULL terminated.
 * @enable_cb: function called to enable the module. This function should add
 *     player(s) and browser(s) that module embed.
 * @disable_cb: function called to disable the module. This function should
 *     remove player(s) and browser(s) that module added during enabling the
 *     module.
 *
 * This structure defines a Melo module: the Melo modules are library which can
 * be statically or dynamically loaded by main program. A module can brings some
 * new browser(s) and / or player(s) to extend capabilities of Melo for music
 * content playing.
 */
struct _MeloModule {
  const char *id;
  unsigned int version;
  unsigned int api_version;

  const char *name;
  const char *description;

  const char **browser_list;
  const char **player_list;

  MeloModuleEnableCb enable_cb;
  MeloModuleDisableCb disable_cb;
};

void melo_module_load (void);
void melo_module_unload (void);

#endif /* !_MELO_MODULE_H_ */
