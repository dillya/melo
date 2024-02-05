// Copyright (C) 2024 Alexandre Dilly <dillya@sparod.com>
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

#include <pybind11/embed.h>

#include <melo/utils.h>
#include <melo/version.h>

#include "log.h"
#include "player.h"
#include "plugin.h"

namespace py = pybind11;
using namespace melo;

PYBIND11_EMBEDDED_MODULE(melopy, m) {
  m.doc() = "Python binding of Melo";

  // Define classes
  pybind_player(m);
  pybind_plugin(m);

  // Log header
  pybind_log(m);

  // Utils header
  // TODO
  m.def("is_valid_id", &is_valid_id, "Check if an ID is valid");

  // Version header
  m.def("get_version", &get_version, "Get current Melo version");
}
