#!/usr/bin/env bash

set -eu

LD_PRELOAD=$PWD/libmalloc.so ./test-d
