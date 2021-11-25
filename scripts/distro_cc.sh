#!/bin/bash

set -e

[ "$CC"  ] || export CC='clang-12'
[ "$CXX" ] || export CXX='clang++-12'
