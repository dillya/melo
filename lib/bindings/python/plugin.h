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

#ifndef BINDINGS_PYTHON_PLUGIN_H_
#define BINDINGS_PYTHON_PLUGIN_H_

#include <pybind11/pybind11.h>

#include <melo/plugin.h>

namespace py = pybind11;
using namespace melo;

static inline void pybind_plugin(py::module &m) {
  py::class_<Plugin>(m, "Plugin")
      .def("add_browser", &Plugin::add_browser)
      .def("add_player", &Plugin::add_player)
      .def("remove_browser", &Plugin::remove_browser)
      .def("remove_player", &Plugin::remove_player);
}

#endif  // BINDINGS_PYTHON_PLUGIN_H_
