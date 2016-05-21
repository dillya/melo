#!/bin/sh
#
# Copyright (C) 2016 Alexandre Dilly <dillya@sparod.com>
#
# Configure and Makefile generator
#

set -e

# Launch autoreconf to generate configure
autoreconf --force --install --verbose

echo "Now, you can run './configure' and 'make'."
