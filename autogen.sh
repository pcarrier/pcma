#!/bin/sh
set -xe
aclocal
autoconf
autoheader
automake --add-missing
