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

#ifndef BINDINGS_PYTHON_PLAYLIST_H_
#define BINDINGS_PYTHON_PLAYLIST_H_

#include <pybind11/pybind11.h>

#include <melo/playlist.h>

namespace py = pybind11;
using namespace melo;

static inline void pybind_playlist(py::module &m) {
  py::class_<Playlist>(m, "Playlist")
      // Static functions
      .def_static("play", py::overload_cast<const Media &>(&Playlist::play))
      .def_static("play",
                  py::overload_cast<const Media &, const std::vector<Media> &>(
                      &Playlist::play))
      .def_static("add", py::overload_cast<const Media &>(&Playlist::add))
      .def_static("add",
                  py::overload_cast<const Media &, const std::vector<Media> &>(
                      &Playlist::add))
      .def_static("swap", py::overload_cast<size_t, size_t>(&Playlist::swap))
      .def_static("swap",
                  py::overload_cast<size_t, size_t, size_t>(&Playlist::swap))
      .def_static("remove", py::overload_cast<size_t>(&Playlist::remove))
      .def_static("remove",
                  py::overload_cast<size_t, size_t>(&Playlist::remove))
      .def_static("play", py::overload_cast<size_t>(&Playlist::play))
      .def_static("play", py::overload_cast<size_t, size_t>(&Playlist::play))
      .def_static("previous", &Playlist::previous)
      .def_static("next", &Playlist::next)
      .def_static("get_playlist", &Playlist::get_playlist)
      .def_static("get_current_playlist", &Playlist::get_current_playlist)
      .def_static("get_playlist_count", &Playlist::get_playlist_count)
      .def_static("clear", &Playlist::get_playlist_count)
      // Member functions
      .def("get_count", &Playlist::get_count)
      .def("get_player_id", &Playlist::get_player_id)
      .def("get_media", py::overload_cast<>(&Playlist::get_media, py::const_))
      .def("get_media",
           py::overload_cast<size_t>(&Playlist::get_media, py::const_))
      .def("get_uri", py::overload_cast<>(&Playlist::get_uri, py::const_))
      .def("get_uri", py::overload_cast<size_t>(&Playlist::get_uri, py::const_))
      .def("get_current", &Playlist::get_current);
}
#endif  // BINDINGS_PYTHON_PLAYLIST_H_
