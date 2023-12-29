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

#include "melo/browser.h"

#include <mutex>
#include <unordered_map>

#include "melo/log.h"
#include "melo/utils.h"

namespace melo {

const std::string Browser::kDefaultDescription;

static std::mutex g_mutex;
static std::unordered_map<std::string, std::shared_ptr<Browser>> g_list;

bool Browser::add(const std::string &id,
                  const std::shared_ptr<Browser> &browser) {
  const std::lock_guard<std::mutex> lock(g_mutex);

  // Check ID compliance
  if (!is_valid_id(id)) {
    MELO_LOGE("browser ID '{}' is not compliant", id);
    return false;
  }

  // Check browser is already registered
  if (g_list.find(id) != g_list.end()) {
    MELO_LOGE("browser '{}' is already registered", id);
    return false;
  }

  MELO_LOGI("add new browser '{}'", id);

  // Add browser
  g_list[id] = browser;

  return true;
}

bool Browser::remove(const std::string &id) {
  const std::lock_guard<std::mutex> lock(g_mutex);

  // Find browser by ID
  auto it = g_list.find(id);
  if (it == g_list.end()) {
    MELO_LOGE("browser '{}' is not registered", id);
    return false;
  }

  MELO_LOGI("remove browser '{}'", id);

  // Remove browser
  g_list.erase(it);

  return true;
}

std::shared_ptr<Browser> Browser::get_by_id(const std::string &id) {
  const std::lock_guard<std::mutex> lock(g_mutex);

  // Find browser by ID
  auto it = g_list.find(id);
  return it != g_list.end() ? it->second : nullptr;
}

bool Browser::has(const std::string &id) {
  const std::lock_guard<std::mutex> lock(g_mutex);
  return g_list.find(id) != g_list.end();
}

}  // namespace melo
