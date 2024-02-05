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

#ifndef BINDINGS_PYTHON_PLAYER_H_
#define BINDINGS_PYTHON_PLAYER_H_

#include <pybind11/pybind11.h>

#include <melo/player.h>

namespace py = pybind11;
using namespace melo;

class PyPlayer : public Player {
 public:
  using Player::Player;

  const Info &get_info() const override {
    PYBIND11_OVERRIDE_PURE(const Player::Info &, Player, get_info, );
  }
  bool play(const std::shared_ptr<melo::Playlist> &playlist) override {
    PYBIND11_OVERRIDE_PURE(bool, Player, play, );
  }
  bool reset() override { PYBIND11_OVERRIDE_PURE(bool, Player, reset, ); }
};

static inline void pybind_player(py::module &m) {
  auto player =
      py::class_<Player, std::shared_ptr<Player>, PyPlayer>(m, "Player")
          .def(py::init<>())
          .def("get_info", &Player::get_info)
          .def("get_name", &Player::get_name)
          .def("get_description", &Player::get_description);

  py::class_<Player::Info>(player, "Info")
      .def(py::init<const std::string &, const std::string &>())
      .def_readonly("name", &Player::Info::name)
      .def_readonly("description", &Player::Info::description);
}

#endif  // BINDINGS_PYTHON_PLAYER_H_
