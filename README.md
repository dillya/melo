![Melo logo](/media/logo.png)

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

```sh
# Test all project
bazel test --test_output=all //...
```

## Bindings

Melo supports several bindings to let developer to implements new features with
its favorite language:
* [C++](#c) (default one),
* [Python3](#python) binding.

### C++

### Python

```sh
# Test Python bindings
bazel test --test_output=all //bindings/python/...
```

## Coding style

Melo is partially following the [Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html)
to simplify development:
* Never put short ifs on the same line,
* Melo includes are grouped together.

To run manually the code formatting:

```sh
# Fix coding style for all C++ files
find \( -name "*.h" -or -name "*.cc" \) -exec clang-format -i {} \;
```

## Documentation

The documentation is generated with several tools:
* [Sphinx](https://www.sphinx-doc.org/en/master/) for general documentation,
* [Doxygen](https://www.doxygen.nl/index.html) for C++ library documentation,
* [Breathe](https://github.com/breathe-doc/breathe) to link **Doxygen** with **Sphinx**.

An [Online version](https://www.sparod.com/melo/docs) is available.

To generate manually a new documentation, the following command can be used:

```sh
# Generate documentation from Sphinx
make -C doc html
```

The HTML output will be generated in `doc/_build/html`:

```sh
xdg-open doc/_build/html/index.html
```

## License

Melo is licensed under the _GNU Affero General Public License, version 3_
license. Please read [LICENSE](LICENSE) file or visit
[GNU AGPL 3.0 page](https://www.gnu.org/licenses/agpl-3.0.en.html) for further
details.

