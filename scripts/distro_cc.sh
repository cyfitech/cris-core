#!/bin/bash

set -e

[ "$CC"  ] || export CC='clang-14'
[ "$CXX" ] || export CXX='clang++-14'
