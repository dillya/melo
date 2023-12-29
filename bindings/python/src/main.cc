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

#include <pybind11/pybind11.h>

#include <melo/utils.h>
#include <melo/version.h>

#include "browser.h"
#include "player.h"
#include "playlist.h"
#include "request.h"

namespace py = pybind11;
using namespace melo;

PYBIND11_MODULE(melopy, m) {
  m.doc() = "Python binding of Open3D";

  // Define classes
  pybind_browser(m);
  pybind_player(m);
  pybind_playlist(m);
  pybind_request(m);

  // Utils header
  m.def("is_valid_id", &is_valid_id, "Check if an ID is valid");

  // Version header
  m.def("get_version", &get_version, "Get current Melo version");
}
