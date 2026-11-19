#!/usr/bin/env bash
# mayhem/build.sh — build MEGAsync's fuzz harness(es) and the oracle test runner.
# Runs inside the commit image as `mayhem` in /mayhem; offline and re-runnable.
#   out/fuzz_registry_hive_reader{,-standalone}: RegistryHiveReader (Qt5Core only), ASan+UBSan.
#   out/fuzz_update_manifest{,-standalone}: MEGAupdater UpdateTask::processUpdateFile (Crypto++).
#   out/fuzz_httpserver{,-standalone}, out/fuzz_megaignore{,-standalone}: HTTPServer and
#     MegaIgnoreManager, linked into the whole desktop app + SDK.
#   build-tests/registry_hive_reader_tests: upstream's own Catch2 unit tests for the reader,
#     built with normal flags (Catch2 v2 single header cached under /opt/toolchains/deps).
set -euo pipefail
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH

: "${SANITIZER_FLAGS=-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer}"
: "${DEBUG_FLAGS:=-g -gdwarf-3}"
: "${CC:=clang}" ; : "${CXX:=clang++}" ; : "${LIB_FUZZING_ENGINE:=-fsanitize=fuzzer}"
: "${MAYHEM_JOBS:=$(nproc)}"
: "${COVERAGE_FLAGS=}"
: "${SRC:=/mayhem}"
: "${CATCH2_INCLUDE:=/opt/toolchains/deps/catch2-2.13.10/include}"

cd "$SRC"
OUT="$SRC/mayhem/out"
TESTS="$SRC/mayhem/build-tests"
mkdir -p "$OUT" "$TESTS"

QT_CFLAGS="$(pkg-config --cflags Qt5Core)"
QT_LIBS="$(pkg-config --libs Qt5Core)"
CTRL="$SRC/src/MEGASync/control"
# Qt5 requires position-independent code (-fPIC) from its users.
CXXSTD="-std=c++17 -fPIC"

# 1) The project code under test + harness, sanitized + DWARF 3. Compiled twice: with SanCov
#    (-fsanitize=fuzzer-no-link) for the libFuzzer binary, without it for the standalone reproducer.
# shellcheck disable=SC2086
$CC $SANITIZER_FLAGS $DEBUG_FLAGS -c "$STANDALONE_FUZZ_MAIN" -o "$OUT/standalone_main.o"
# shellcheck disable=SC2086
$CXX $CXXSTD -O1 $SANITIZER_FLAGS $DEBUG_FLAGS -c "$SRC/mayhem/lsan_off.cc" -o "$OUT/lsan_off.o"
for variant in fuzz standalone; do
  cov=""; [ "$variant" = fuzz ] && cov="-fsanitize=fuzzer-no-link"
  # shellcheck disable=SC2086
  $CXX $CXXSTD -O1 $SANITIZER_FLAGS $cov $DEBUG_FLAGS $QT_CFLAGS -I"$CTRL" \
      -c "$CTRL/RegistryHiveReader.cpp" -o "$OUT/RegistryHiveReader.$variant.o"
  # shellcheck disable=SC2086
  $CXX $CXXSTD -O1 $SANITIZER_FLAGS $cov $DEBUG_FLAGS $QT_CFLAGS -I"$CTRL" \
      -c "$SRC/mayhem/harnesses/fuzz_registry_hive_reader.cpp" -o "$OUT/fuzz_registry_hive_reader.$variant.o"
done

# 2) Fuzzer + standalone reproducer.
# shellcheck disable=SC2086
$CXX $SANITIZER_FLAGS $DEBUG_FLAGS $LIB_FUZZING_ENGINE \
    "$OUT/fuzz_registry_hive_reader.fuzz.o" "$OUT/RegistryHiveReader.fuzz.o" "$OUT/lsan_off.o" $QT_LIBS \
    -o "$OUT/fuzz_registry_hive_reader"
# shellcheck disable=SC2086
$CXX $SANITIZER_FLAGS $DEBUG_FLAGS \
    "$OUT/standalone_main.o" "$OUT/fuzz_registry_hive_reader.standalone.o" "$OUT/RegistryHiveReader.standalone.o" \
    "$OUT/lsan_off.o" $QT_LIBS -o "$OUT/fuzz_registry_hive_reader-standalone"

# 2b) MEGAupdater's update-manifest reader (UpdateTask::processUpdateFile). Upstream builds the
#     updater only for Windows/macOS; on Linux the file compiles with one force-included shim
#     (mayhem/harnesses/updater_linux_shim.h, see its comment). Links system Crypto++.
UPD="$SRC/src/MEGAUpdater"
for variant in fuzz standalone; do
  cov=""; [ "$variant" = fuzz ] && cov="-fsanitize=fuzzer-no-link"
  # shellcheck disable=SC2086
  $CXX $CXXSTD -O1 $SANITIZER_FLAGS $cov $DEBUG_FLAGS -include "$SRC/mayhem/harnesses/updater_linux_shim.h" \
      -I"$UPD" -c "$UPD/UpdateTask.cpp" -o "$OUT/UpdateTask.$variant.o"
  # shellcheck disable=SC2086
  $CXX $CXXSTD -O1 $SANITIZER_FLAGS $cov $DEBUG_FLAGS -I"$UPD" \
      -c "$SRC/mayhem/harnesses/fuzz_update_manifest.cpp" -o "$OUT/fuzz_update_manifest.$variant.o"
done
# shellcheck disable=SC2086
$CXX $SANITIZER_FLAGS $DEBUG_FLAGS $LIB_FUZZING_ENGINE \
    "$OUT/fuzz_update_manifest.fuzz.o" "$OUT/UpdateTask.fuzz.o" "$OUT/lsan_off.o" -lcryptopp \
    -o "$OUT/fuzz_update_manifest"
# shellcheck disable=SC2086
$CXX $SANITIZER_FLAGS $DEBUG_FLAGS \
    "$OUT/standalone_main.o" "$OUT/fuzz_update_manifest.standalone.o" "$OUT/UpdateTask.standalone.o" \
    "$OUT/lsan_off.o" -lcryptopp -o "$OUT/fuzz_update_manifest-standalone"

# 2c) The app-based targets, inside the whole desktop app + SDK, built by
#     mayhem/httpserver/CMakeLists.txt (upstream's tree via add_subdirectory, unchanged), all
#     sanitized + SanCov: the webclient HTTP bridge (control/HTTPServer.cpp) and the .megaignore
#     reader (syncs/control/MegaIgnoreManager.cpp), each with a -standalone reproducer. The SDK submodule path is pointed at the in-image SDK source.
SDK_SRC="${MEGA_SDK_SRC:-/opt/toolchains/deps/mega-sdk-81755609b860b0c839cd08e4d5c708d65a731d54/src}"
SUB="$SRC/src/MEGASync/mega"
if [ ! -e "$SUB/CMakeLists.txt" ]; then
  rmdir "$SUB" 2>/dev/null || true
  ln -sfn "$SDK_SRC" "$SUB"
fi
HB="$SRC/mayhem/build-httpserver"
cmake -G Ninja -S "$SRC/mayhem/httpserver" -B "$HB" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_FLAGS="$SANITIZER_FLAGS -fsanitize=fuzzer-no-link $DEBUG_FLAGS" \
  -DCMAKE_CXX_FLAGS="$SANITIZER_FLAGS -fsanitize=fuzzer-no-link $DEBUG_FLAGS" \
  -DUSE_BREAKPAD=OFF -DUSE_READLINE=OFF \
  "-DCMAKE_MODULE_PATH=$SRC/mayhem/httpserver/cmake;$SUB/cmake/modules/packages" \
  -DENABLE_DESKTOP_UPDATE_GEN=OFF -DENABLE_DESKTOP_APP_WERROR=OFF -DENABLE_SDKLIB_WERROR=OFF \
  -DUSE_FFMPEG=OFF -DUSE_PDFIUM=OFF
APP_TARGETS="fuzz_httpserver fuzz_megaignore"
# shellcheck disable=SC2046
cmake --build "$HB" -j"$MAYHEM_JOBS" --target $(for t in $APP_TARGETS; do printf '%s %s-standalone ' "$t" "$t"; done)
for t in $APP_TARGETS; do
  cp -f "$HB/$t" "$OUT/$t"
  cp -f "$HB/$t-standalone" "$OUT/$t-standalone"
done

# 3) Upstream's RegistryHiveReader unit tests, normal flags (the test.sh oracle).
# shellcheck disable=SC2086
$CXX $CXXSTD -O1 $COVERAGE_FLAGS $QT_CFLAGS -I"$CTRL" -isystem "$CATCH2_INCLUDE" \
    -DUNIT_TESTS_DATA_DIR="\"$SRC/src/MEGAAutoTests/UnitTests/data\"" \
    "$SRC/mayhem/harnesses/catch_main.cpp" \
    "$SRC/src/MEGAAutoTests/UnitTests/control/RegistryHiveReaderTests.cpp" \
    "$CTRL/RegistryHiveReader.cpp" $QT_LIBS -o "$TESTS/registry_hive_reader_tests"

echo "built: $OUT/fuzz_registry_hive_reader{,-standalone} $OUT/fuzz_update_manifest{,-standalone} $OUT/fuzz_{httpserver,megaignore}{,-standalone} $TESTS/registry_hive_reader_tests"
