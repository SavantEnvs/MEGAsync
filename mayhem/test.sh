#!/usr/bin/env bash
# mayhem/test.sh — RUN upstream's own RegistryHiveReader unit tests (built by mayhem/build.sh).
# They read known values out of the committed hive fixture and assert them, so a no-op build fails.
set -uo pipefail
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH
: "${SRC:=/mayhem}"
cd "$SRC"

emit_ctrf() {
  local tool="$1" passed="$2" failed="$3" skipped="${4:-0}" pending="${5:-0}" other="${6:-0}"
  local tests=$(( passed + failed + skipped + pending + other ))
  cat > "${CTRF_REPORT:-$SRC/ctrf-report.json}" <<JSON
{
  "results": {
    "tool": { "name": "$tool" },
    "summary": {
      "tests": $tests,
      "passed": $passed,
      "failed": $failed,
      "pending": $pending,
      "skipped": $skipped,
      "other": $other
    }
  }
}
JSON
  printf 'CTRF {"results":{"tool":{"name":"%s"},"summary":{"tests":%d,"passed":%d,"failed":%d,"pending":%d,"skipped":%d,"other":%d}}}\n' \
    "$tool" "$tests" "$passed" "$failed" "$pending" "$skipped" "$other"
  [ "$failed" -eq 0 ]
}

RUNNER="$SRC/mayhem/build-tests/registry_hive_reader_tests"
if [ ! -x "$RUNNER" ]; then
  echo "test runner missing: $RUNNER (mayhem/build.sh should have built it)" >&2
  emit_ctrf catch2 0 1
  exit 1
fi

# Catch2's summary line is either "All tests passed (A assertions in N test cases)" or
# "test cases: N | P passed | F failed". Upstream ships 4 TEST_CASEs; fewer means they didn't run.
out="$("$RUNNER" 2>&1)"
rc=$?
printf '%s\n' "$out"
expected=4
if line=$(grep -Eo 'All tests passed \([0-9]+ assertions? in [0-9]+ test cases?\)' <<<"$out"); then
  total=$(sed -E 's/.* in ([0-9]+) test case.*/\1/' <<<"$line"); passed=$total; failed=0
elif line=$(grep -Eo 'test cases: *[0-9]+ \| *[0-9]+ passed \| *[0-9]+ failed' <<<"$out"); then
  read -r total passed failed < <(sed -E 's/test cases: *([0-9]+) \| *([0-9]+) passed \| *([0-9]+) failed/\1 \2 \3/' <<<"$line")
elif line=$(grep -Eo 'test cases: *[0-9]+ \| *[0-9]+ failed' <<<"$out"); then
  read -r total failed < <(sed -E 's/test cases: *([0-9]+) \| *([0-9]+) failed/\1 \2/' <<<"$line"); passed=0
else
  total=0; passed=0; failed=0
fi
if [ "$total" -lt "$expected" ]; then
  echo "expected $expected test cases, Catch2 reported $total (rc=$rc)" >&2
  failed=$(( failed + expected - total ))
elif [ "$rc" -ne 0 ] && [ "$failed" -eq 0 ]; then
  failed=1; passed=$(( passed > 0 ? passed - 1 : 0 ))
fi
emit_ctrf catch2 "$passed" "$failed"
