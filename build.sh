#!/bin/sh
set -xe
./prepare.sh
./configure $@
make
