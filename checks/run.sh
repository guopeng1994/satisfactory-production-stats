#!/bin/sh
# SPDX-License-Identifier: 0BSD
set -eu
cd "$(dirname "$0")/.."
check_dir=$(mktemp -d "${TMPDIR:-/tmp}/fps-check.XXXXXX")
trap 'rm -rf "$check_dir"' EXIT HUP INT TERM
check_cxx=${CXX:-clang++}
source_dir=FactoryProductionStats/Source/FactoryProductionStats
"$check_cxx" -std=c++20 -Wall -Wextra -Werror -pedantic -fno-exceptions -fno-rtti -fsanitize=address,undefined \
    -I "$source_dir/Public" checks/ProductionStatsTypesCheck.cpp -o "$check_dir/types"
"$check_dir/types"
"$check_cxx" -std=c++20 -Wall -Wextra -Werror -pedantic -fno-exceptions -fno-rtti -fsanitize=address,undefined \
    -I "$source_dir/Public" "$source_dir/Private/ProductionStatsHistory.cpp" checks/ProductionStatsHistoryCheck.cpp -o "$check_dir/history"
"$check_dir/history"
"$check_cxx" -std=c++20 -Wall -Wextra -Werror -pedantic -fno-exceptions -fno-rtti -fsanitize=address,undefined -pthread \
    -I "$source_dir/Public" "$source_dir/Private/ProductionStatsHistory.cpp" "$source_dir/Private/ProductionStatsCollectors.cpp" \
    checks/ProductionStatsCollectorsCheck.cpp -o "$check_dir/collectors"
"$check_dir/collectors"
"$check_cxx" -std=c++20 -Wall -Wextra -Werror -pedantic -fno-exceptions -fno-rtti -fsanitize=address,undefined \
    -I "$source_dir/Public" "$source_dir/Private/ProductionStatsHistory.cpp" "$source_dir/Private/ProductionStatsPersistence.cpp" \
    checks/ProductionStatsPersistenceViewCheck.cpp -o "$check_dir/persistence-view"
"$check_dir/persistence-view"
