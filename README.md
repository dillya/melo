![melo][melo_logo]

# Melo: your personal music hub

Melo is an embedded application to play music content from anywhere on any
speaker.

## Overview
Melo is intended to create a music hub on any device running Linux, anywhere in
a house or a company. It brings capabilities to play any audio content from
anywhere on speakers connected directly to the device running Melo, or to remote
speakers.

Melo is a plugin based software which offers possibility to any developer to
create and add its personal plugin and then support a new music content source.

## Build

First of all, you have to clone the git repository with the following command:

```sh
git clone https://github.com/dillya/melo.git
```

Melo is based on Meson for the build purpose and GLib low-level library on which
Gstreamer and many useful libraries are based. It brings the object oriented
coding style thanks to GObject.

For HTTP server / client side, Melo is based on libsoup which brings most of
HTTP features out of the box.

You may need to install some development libraries and build tools before
starting with Melo build.

On Debian / Ubuntu Linux system, you can install required dependencies with the
following command:

```sh
apt install meson libglib2.0-dev libgstreamer-plugins-base1.0-dev \
    libsoup2.4-dev libjson-glib-dev
```

Then, you are ready to build Melo:

```sh
cd melo
meson builddir
ninja -C builddir
```

To install Melo (by default in /usr/local), you can finally use the following
command:

```sh
sudo ninja -C builddir install
```

## Coding style

Melo uses clang-format to check and fix coding style of source code. It prevents
mix of different coding style and improves readability of the code.

## License

Melo is licensed under the LGPLv2.1 license. Please read LICENSE file or visit
https://www.gnu.org/licenses/old-licenses/lgpl-2.1.en.html for further details.

[melo_logo]: https://raw.githubusercontent.com/dillya/melo/melo-1.0.0/media/logo.png
