#!/usr/bin/env bash
# Builds the tests (MinGW cross-compile) and runs the end-to-end host under Wine.
# Wine runs with a clean environment: a few host variables make Wine 9 abort.
set -euo pipefail
cd "$(dirname "$0")/.."
B=${1:-build-e2e}
cmake -S . -B "$B" -DSHVI_BUILD_TESTS=ON -DCMAKE_TOOLCHAIN_FILE="$PWD/cmake/mingw-w64-x86_64.cmake" >/dev/null
cmake --build "$B" >/dev/null
cd "$B/out"
rc=0
for m in pass nopatterns guard badimage; do
	rm -f ScriptHookVI.log
	env -i PATH=/usr/bin:/bin HOME="$HOME" WINEPREFIX="${WINEPREFIX:-$HOME/.wine-shvi}" WINEDEBUG=-all \
		wine testhost.exe "$m" || rc=1
done
exit $rc
