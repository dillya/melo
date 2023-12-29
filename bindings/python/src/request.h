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

#ifndef BINDINGS_PYTHON_REQUEST_H_
#define BINDINGS_PYTHON_REQUEST_H_

#include <pybind11/functional.h>
#include <pybind11/pybind11.h>

#include <melo/request.h>

namespace py = pybind11;
using namespace melo;

static inline void pybind_request(py::module &m) {
  py::class_<Request, std::shared_ptr<Request>>(m, "Request")
      // Static functions
      .def_static("create",
                  [](const std::string &msg,
                     const std::function<void(const py::bytes &)> &func) {
                    return Request::create(msg, func);
                  })
      .def_static("create",
                  [](std::string &&msg,
                     const std::function<void(const py::bytes &)> &func) {
                    return Request::create(std::move(msg), func);
                  })
      // Request functions
      .def("get_message",
           [](Request &req) { return py::bytes(req.get_message()); })
      .def("complete",
           [](Request &req, const py::bytes &msg) { return req.complete(msg); })
      .def("is_completed", &Request::is_completed)
      .def("cancel", &Request::cancel)
      .def("is_canceled", &Request::is_canceled);
}

#endif  // BINDINGS_PYTHON_REQUEST_H_
