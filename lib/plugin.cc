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

#include "melo/plugin.h"

#include <cpptoml.h>

#include "melo/log.h"

namespace melo {

const std::string Plugin::Manifest::kFilename{"manifest.toml"};
const std::string Plugin::Manifest::kDefaultEntryPoint{"entry_point"};

bool Plugin::Manifest::parse(const std::string &path, Manifest &manifest,
                             std::string &error) {
  std::shared_ptr<cpptoml::table> config;

  try {
    // Parse manifest file
    config = cpptoml::parse_file(path);

    // std::cout << (*g) << std::endl;
  } catch (const cpptoml::parse_exception &e) {
    error = e.what();
    return false;
  }

  // Get plugin name
  auto name = config->get_as<std::string>("name");
  if (!name) {
    error = "no name found";
    return false;
  }
  manifest.name = *name;

  // Get version
  auto version = config->get_as<std::string>("version");
  if (!version) {
    error = "no version found";
    return false;
  }
  manifest.version = *version;

  // Get melo table
  auto melo_table = config->get_table("melo");
  if (!melo_table) {
    error = "no 'melo' table found";
    return false;
  }

  // Check melo version
  auto melo_version = melo_table->get_as<std::string>("version");
  if (!melo_version) {
    error = "no melo.version found";
    return false;
  }
  // TODO: extract and check
  manifest.melo_version = *melo_version;

  // Get plugin type
  auto table = config->get_table("native");
  if (!table) {
    table = config->get_table("python");
    if (!table) {
      error = "no 'native' or 'python' table found";
      return false;
    }
    manifest.type = PYTHON_PLUGIN;
  } else
    manifest.type = NATIVE_PLUGIN;

  // Get path
  auto filename = table->get_as<std::string>("filename");
  if (!filename) {
    error = "no filename found";
    return false;
  }
  manifest.filename = *filename;

  // Get entry point
  auto entry_point =
      table->get_as<std::string>("entry_point").value_or(kDefaultEntryPoint);
  manifest.entry_point = entry_point;

  return true;
}

bool Plugin::add_browser(const std::string &id,
                         const std::shared_ptr<Browser> &browser) const {
  return true;
}

bool Plugin::remove_browser(const std::string &id) const { return false; }

bool Plugin::add_player(const std::string &id,
                        const std::shared_ptr<Player> &player) const {
  // Invalid arguments
  if (id.empty() || !player) {
    MELO_LOGE("no player to add for '{}'", id);
    return false;
  }

  // TODO
  MELO_LOGW("{} -> {}", id, player->get_name());

  return core_.add_player(id, player);
}

bool Plugin::remove_player(const std::string &id) const { return false; }

}  // namespace melo
