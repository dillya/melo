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

#ifndef BINDINGS_PYTHON_BROWSER_H_
#define BINDINGS_PYTHON_BROWSER_H_

#include <pybind11/pybind11.h>

#include <melo/browser.h>

namespace py = pybind11;
using namespace melo;

class PyBrowser : public Browser {
 public:
  using Browser::Browser;

  const Browser::Info &get_info() const override {
    PYBIND11_OVERRIDE_PURE(const Browser::Info &, Browser, get_info, );
  }
  bool handle_request(const std::shared_ptr<Request> &request) const override {
    PYBIND11_OVERRIDE_PURE(bool, Browser, handle_request, request);
  }
};

static inline void pybind_browser(py::module &m) {
  auto browser =
      py::class_<Browser, std::shared_ptr<Browser>, PyBrowser>(m, "Browser")
          .def(py::init<>())
          // Static functions
          .def_static("add", &Browser::add)
          .def_static("remove", &Browser::remove)
          .def_static("has", &Browser::has)
          .def_static("get_by_id", &Browser::get_by_id)
          // Get info
          .def("get_info", &Browser::get_info)
          .def("get_name", &Browser::get_name)
          .def("get_description", &Browser::get_description)
          // Handle request
          .def("handle_request", &Browser::handle_request);

  py::class_<Browser::Info>(browser, "Info")
      .def(py::init<>())
      .def_readonly("name", &Browser::Info::name)
      .def_readonly("description", &Browser::Info::description);
}

#endif  // BINDINGS_PYTHON_BROWSER_H_
