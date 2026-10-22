#!/bin/sh
# Compiles and runs the pure-logic unit tests on the host (C++11, same as the ESP32 core).
set -e
cd "$(dirname "$0")/.."
g++ -std=c++11 -Wall -Wextra -Werror -O1 test/host/test_logic.cpp -o /tmp/nova_host_tests
/tmp/nova_host_tests
