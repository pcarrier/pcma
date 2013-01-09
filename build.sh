#!/bin/sh
set -xe
./autogen.sh
./configure $@
make
