#!/usr/bin/env bash
set -Eeuo pipefail

REPO_ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
QUICKSTART="${REPO_ROOT}/scripts/hardware_quickstart.sh"
MOCK_CHILD="${REPO_ROOT}/tests/fixtures/hardware_quickstart_child.sh"
TEST_DIR="$(mktemp -d /tmp/hardware-quickstart-test.XXXXXX)"
trap 'rm -rf -- "${TEST_DIR}"' EXIT

run_ui() {
  local name="$1" input="$2" child_exit="$3"
  printf '%b' "${input}" | env SQUARE_MM=0.5 OUT_DIR="${TEST_DIR}/${name}" \
    MOCK_CHILD_EXIT="${child_exit}" CALIBRATION_SESSION_SCRIPT="${MOCK_CHILD}" \
    "${QUICKSTART}" >"${TEST_DIR}/${name}.out"
}

run_ui success '1\n\nqy' 0
grep -Fq 'Left Mono Calibration finished.' "${TEST_DIR}/success.out"
[[ "$(grep -Fc 'ToothProjectTools - Hardware Quick Start' "${TEST_DIR}/success.out")" -ge 2 ]]

run_ui failure '1\n\nqy' 1
grep -Fq '[ERROR] operation failed: exit=1' "${TEST_DIR}/failure.out"
[[ "$(grep -Fc 'ToothProjectTools - Hardware Quick Start' "${TEST_DIR}/failure.out")" -ge 2 ]]

run_ui interrupted '1\n\nqy' 130
grep -Fq 'Operation interrupted.' "${TEST_DIR}/interrupted.out"
[[ "$(grep -Fc 'ToothProjectTools - Hardware Quick Start' "${TEST_DIR}/interrupted.out")" -ge 2 ]]

run_ui quit_no 'qNqy' 0
[[ "$(grep -Fc 'Exit Hardware Quick Start?' "${TEST_DIR}/quit_no.out")" -eq 2 ]]
[[ "$(grep -Fc 'ToothProjectTools - Hardware Quick Start' "${TEST_DIR}/quit_no.out")" -ge 2 ]]

run_ui invalid 'xqy' 0
[[ "$(grep -Fc 'ToothProjectTools - Hardware Quick Start' "${TEST_DIR}/invalid.out")" -ge 2 ]]

run_ui quit_yes 'qy' 0
grep -Fq 'Exit Hardware Quick Start?' "${TEST_DIR}/quit_yes.out"

printf 'hardware_quickstart_test: PASS\n'
