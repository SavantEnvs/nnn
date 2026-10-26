#!/usr/bin/env bash
#
# mayhem/test.sh — RUN nnn's known-answer checks (the probe and the stock binary are built by mayhem/build.sh).
# exit 0 = pass. EDIT per repo. PATCH-grade oracle: after an agent patches the source, the grader
# rebuilds (build.sh) then runs this. DELETE this file if the repo has no meaningful tests.
#
# IMPORTANT:
#  * Must assert BEHAVIOR/OUTPUT, not just exit status. The oracle has to check asserted values /
#    golden-output diffs / known-answer results — so a PATCH that "fixes" a bug by making the program
#    exit(0) (or any no-op) FAILS here. Running inputs and checking only "exit 0 / didn't crash" is
#    NOT a functional test (it's trivially reward-hackable) — use the project's real assertion suite.
#  * Do NOT build here — mayhem/build.sh already compiled the test suite (with the project's normal
#    flags). This script only RUNS the pre-built tests and reports counts. If the test runner is
#    missing, that's a build.sh bug — fail loudly rather than silently rebuilding.
#  * REQUIRED OUTPUT — a CTRF (https://ctrf.io) summary so Mayhem/the PATCH grader reads the counts:
#      - writes a CTRF JSON report to ${CTRF_REPORT:-$SRC/ctrf-report.json}, and
#      - prints a one-line `CTRF {...}` marker to stdout (same JSON, compact).
#    Only `results.summary` (with tests/passed/failed/pending/skipped/other) is required.
#    Use the emit_ctrf helper below; it computes tests = passed+failed+skipped and sets the exit
#    code (0 iff failed==0). Map your framework's output to passed/failed/skipped.
set -uo pipefail
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH
: "${MAYHEM_JOBS:=$(nproc)}"   # build parallelism; env-overridable, falls back to nproc (use -j"$MAYHEM_JOBS")
cd "$SRC"

# emit_ctrf <tool> <passed> <failed> [skipped] [pending] [other]
# Writes a CTRF report (file + stdout `CTRF {...}` marker) and returns non-zero iff failed>0.
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

# nnn ships no automated suite that runs headless (its tests need a pty and compile at run time), so the
# oracle is a known-answer probe linked against nnn's own source (mayhem/kat.c, built by build.sh) whose
# output is compared to values worked out from the code, plus the stock `make` binary's version string.
# Every probe is unconditional: a missing binary or empty output is a failure, never a skip.
declare -A EXPECT=(
  [abspath_rel]='/x/y/a/c'
  [abspath_abs]='/c'
  [abspath_tilde]='/home/mayhem/docs'
  [shell_escape]="'it'\\''s a b'"
  [common_prefix]='/usr'
  [verscmp_file2_file10]='neg'
  [verscmp_a_A]='zero'
  [stricmp_2_10]='neg'
  [kv_bms_parse]='1,3'
  [kv_bms_b]='/usr/bin'
  [kv_plug_y]='bar -l'
  [kv_order_size]='s'
  [filter_substr]='zeta,beta,delta,theta'
  [filter_regex]='alpha,beta,delta'
  [filter_fuzzy_name]='Gamma'
  [filter_fuzzy_rank]='zeta,beta,theta,delta'
  [parseargs]='4,a,b'
  [input_root]='/usr/bin'
  [input_links]='env,ls'
)

passed=0; failed=0
fail() { echo "FAIL: $*" >&2; failed=$((failed + 1)); }
pass() { passed=$((passed + 1)); }

kat_out="$(/mayhem/nnn-kat 2>/dev/null)" || { fail "nnn-kat exited non-zero"; kat_out=""; }
for k in "${!EXPECT[@]}"; do
  got="$(printf '%s\n' "$kat_out" | sed -n "s/^$k=//p")"
  if [ "$got" = "${EXPECT[$k]}" ]; then pass; else fail "$k: got '$got', want '${EXPECT[$k]}'"; fi
done

# The stock upstream binary (built with upstream's own Makefile) reports the version the source declares.
want_ver="$(sed -n 's/^#define VERSION[[:space:]]*"\(.*\)"/\1/p' "$SRC/src/nnn.c" | head -1)"
got_ver="$("$SRC/nnn" -V 2>/dev/null)"
if [ -n "$want_ver" ] && [ "$got_ver" = "$want_ver" ]; then pass; else fail "nnn -V: got '$got_ver', want '$want_ver'"; fi

emit_ctrf "nnn-kat" "$passed" "$failed" 0
