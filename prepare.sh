#!/bin/sh
set -xe
aclocal
autoconf
automake --add-missing
