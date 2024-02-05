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

#include <cstdlib>
#include <iostream>

#include <melo/core.h>
#include <melo/log.h>

int main(int argc, char *argv[]) {
  melo::Core core;

  // Get plugins path from environment
  std::string plugins_path = "/var/lib/melo/plugins";
  char *env_plugins_path = std::getenv("MELO_PLUGINS_PATH");
  if (env_plugins_path)
    plugins_path = env_plugins_path;

  // Load plugins
  if (!core.load_plugins(plugins_path))
    MELO_LOGW("failed to load plugins");

  MELO_LOGI("Melo is ready");

  // TODO
  auto player = core.get_player("melo.file.player");
  MELO_LOGW("player name is {}", player ? player->get_name() : "-");

  // TODO
  player = core.get_player("melo.spotify.player");
  MELO_LOGW("player name is {}", player ? player->get_name() : "-");

  // Main loop
  // TODO

  return 0;
}
