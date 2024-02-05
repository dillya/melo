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

#include "melo/core.h"

#include <filesystem>

#include <dlfcn.h>

#include "bindings/python/embedded.h"
#include "melo/log.h"
#include "melo/plugin.h"

namespace melo {

Core::Core() {
  // Initialize Python interpreter
  py::initialize_interpreter();

  // Bind Python logging to internal logger
  py::exec(R"(
    import logging
    import melopy as melo

    # Define a new logging handler
    class MeloLogHandler(logging.Handler):
        def __init__(self )-> None:
            logging.Handler.__init__(self=self)

        def emit(self, record: logging.LogRecord) -> None:
            func = record.func if hasattr(record, "func") else ""
            if record.levelno == logging.DEBUG:
                melo.logd(record.pathname, record.lineno, func, record.msg)
            elif record.levelno == logging.INFO:
                melo.logi(record.pathname, record.lineno, func, record.msg)
            elif record.levelno == logging.WARNING:
                melo.logw(record.pathname, record.lineno, func, record.msg)
            elif record.levelno == logging.ERROR:
                melo.loge(record.pathname, record.lineno, func, record.msg)
            elif record.levelno == logging.CRITICAL:
                melo.logc(record.pathname, record.lineno, func, record.msg)

    # Use custom handler to redirect to internal logger
    handler = MeloLogHandler()
    logging.basicConfig(
        level=logging.DEBUG,
        handlers=[handler],
    )
  )");
}

Core::~Core() {
  // Stop Python interpreter
  py::finalize_interpreter();
}

bool Core::load_plugins(const std::string &path) {
  const std::filesystem::path plugins_path{path};

  // Check directory exists
  if (!std::filesystem::exists(plugins_path)) {
    MELO_LOGI("no plugin to load");
    return true;
  }

  // Add directory to Python path
  try {
    py::module_ sys = py::module_::import("sys");
    sys.attr("path").attr("append")(plugins_path.string());
  } catch (const std::exception &e) {
    MELO_LOGW("failed to add plugins directory to Python path: {}", e.what());
  }

  // Load plugins one by one
  for (auto const &plugin_entry :
       std::filesystem::directory_iterator{plugins_path}) {
    // Skip regular files
    if (!plugin_entry.is_directory())
      continue;

    // Check manifest exists
    auto plugin_path = plugin_entry.path();
    auto manifest_path = plugin_path / Plugin::Manifest::kFilename;
    if (!std::filesystem::exists(manifest_path))
      continue;

    // Check manifest
    std::string error;
    Plugin::Manifest manifest;
    if (!Plugin::Manifest::parse(manifest_path, manifest, error)) {
      MELO_LOGE("invalid plugin found in {}: {}", plugin_entry.path().string(),
                error);
      // TODO: event
      continue;
    }

    MELO_LOGI("load plugin '{}'", manifest.name);

    // Load plugin
    if (manifest.type == Plugin::NATIVE_PLUGIN) {
      // Check file exists
      auto lib_path = plugin_path / manifest.filename;
      if (!std::filesystem::exists(lib_path)) {
        MELO_LOGE("{}: doesn't exist '{}'", manifest.name, lib_path.string());
        continue;
      }

      // Open library
      void *lib = dlopen(lib_path.c_str(), RTLD_NOW);
      if (lib == nullptr) {
        MELO_LOGE("{}: failed to open '{}': {}", manifest.name,
                  lib_path.string(), dlerror());
        continue;
      }

      // Load entry point
      auto entry_point = reinterpret_cast<Plugin::EntryPoint>(
          dlsym(lib, manifest.entry_point.c_str()));
      if (entry_point == nullptr) {
        MELO_LOGE("{}: failed to load entry_point '{}'", manifest.name,
                  manifest.entry_point);
        dlclose(lib);
        continue;
      }

      // Call entry point
      Plugin plugin{manifest, *this};
      bool ret = entry_point(plugin);
      if (!ret) {
        MELO_LOGE("{}: failed to load plugin", manifest.name);
        dlclose(lib);
        continue;
      }
    } else if (manifest.type == Plugin::PYTHON_PLUGIN) {
      // Import Python package
      py::module_ mod;
      try {
        std::string mod_path =
            plugin_path.filename().string() + "." + manifest.filename;
        mod = py::module_::import(mod_path.c_str());
      } catch (const std::exception &e) {
        MELO_LOGE("{}: failed to import '{}': {}", manifest.name,
                  manifest.filename, e.what());
        continue;
      }

      // Load entry point
      py::object entry_point;
      try {
        entry_point = mod.attr("entry_point");
      } catch (const std::exception &e) {
        MELO_LOGE("{}: failed to load entry_point '{}': {}", manifest.name,
                  manifest.entry_point, e.what());
        continue;
      }

      // Call entry point
      Plugin plugin{manifest, *this};
      bool ret = false;
      try {
        ret = entry_point(plugin).cast<bool>();
      } catch (const std::exception &e) {
        MELO_LOGE("{}: failed to call entry point: {}", manifest.name,
                  e.what());
      }
      if (!ret) {
        MELO_LOGE("{}: failed to load plugin", manifest.name);
        continue;
      }
    } else {
      MELO_LOGE("{}: unsupported plugin type", manifest.name);
      continue;
    }

    // TODO: event
  }

  return true;
}

bool Core::add_player(const std::string &id,
                      const std::shared_ptr<Player> &player) {
  const std::lock_guard<std::mutex> lock(mutex_);

#if 0
  // Check ID compliance
  if (!is_valid_id(id)) {
    MELO_LOGE("player ID '{}' is not compliant", id);
    return false;
  }
#endif

  // Check player is already registered
  if (player_list_.find(id) != player_list_.end()) {
    MELO_LOGE("player '{}' is already registered", id);
    return false;
  }

  MELO_LOGI("add new player '{}'", id);

  // Add player
  player_list_[id] = player;

  return true;
}

std::shared_ptr<Player> Core::get_player(const std::string &id) {
  const std::lock_guard<std::mutex> lock(mutex_);

  // Find player by ID
  auto it = player_list_.find(id);
  return it != player_list_.end() ? it->second : nullptr;
}

}  // namespace melo
