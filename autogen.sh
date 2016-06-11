#!/bin/sh
#
# Copyright (C) 2016 Alexandre Dilly <dillya@sparod.com>
#
# Configure and Makefile generator
#

set -e

# Check presence of autoreconf
if ! which autoreconf >/dev/null; then
    echo "ERROR: Please install Autotools before running $0!"
    exit 1
fi

# Launch autoreconf to generate configure
autoreconf --force --install --verbose

echo "Now, you can run './configure' and 'make'."
