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

#ifndef BINDINGS_PYTHON_LOG_H_
#define BINDINGS_PYTHON_LOG_H_

#include <pybind11/pybind11.h>

#include <melo/log.h>

namespace py = pybind11;
using namespace melo;

static inline void pybind_log(py::module &m) {
  m.def("logc", [](const char *file, int line, const char *func,
                   const char *msg) { MELO_LOGC_EXT(file, line, func, msg); });
  m.def("loge", [](const char *file, int line, const char *func,
                   const char *msg) { MELO_LOGE_EXT(file, line, func, msg); });
  m.def("logw", [](const char *file, int line, const char *func,
                   const char *msg) { MELO_LOGW_EXT(file, line, func, msg); });
  m.def("logi", [](const char *file, int line, const char *func,
                   const char *msg) { MELO_LOGI_EXT(file, line, func, msg); });
  m.def("logd", [](const char *file, int line, const char *func,
                   const char *msg) { MELO_LOGD_EXT(file, line, func, msg); });
}

#endif  // BINDINGS_PYTHON_LOG_H_
