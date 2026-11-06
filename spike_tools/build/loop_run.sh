#!/usr/bin/env bash
# Run one build/test/lint command with guards; keep all command output in the log and
# print exactly three lines: EXIT_CODE=<v>, SUMMARY ..., FAILED=...
# Usage: loop_run.sh --kind test|build|lint --log LOG --repo REPO --branch BR
#          [--container NAME --mount SRC] [--results-dir D] [--lint-pattern P] -- CMD...
set -u

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
KIND="" LOG="" REPO="" BRANCH="" CONTAINER="" MOUNT="" RESULTS="" PATTERN=""

die() {
    echo "GUARD_FAIL reason=$1"
    echo "EXIT_CODE=2"
    exit 2
}

parse_args() {
    while [ $# -gt 0 ]; do
        case "$1" in
            --kind) [ $# -ge 2 ] || die usage; KIND="$2"; shift 2 ;;
            --log) [ $# -ge 2 ] || die usage; LOG="$2"; shift 2 ;;
            --repo) [ $# -ge 2 ] || die usage; REPO="$2"; shift 2 ;;
            --branch) [ $# -ge 2 ] || die usage; BRANCH="$2"; shift 2 ;;
            --container) [ $# -ge 2 ] || die usage; CONTAINER="$2"; shift 2 ;;
            --mount) [ $# -ge 2 ] || die usage; MOUNT="$2"; shift 2 ;;
            --results-dir) [ $# -ge 2 ] || die usage; RESULTS="$2"; shift 2 ;;
            --lint-pattern) [ $# -ge 2 ] || die usage; PATTERN="$2"; shift 2 ;;
            --) shift; CMD=("$@"); return ;;
            *) die usage ;;
        esac
    done
    die usage
}

validate_args() {
    [ -n "$LOG" ] && [ -n "$REPO" ] && [ -n "$BRANCH" ] || die usage
    [ "${#CMD[@]}" -gt 0 ] || die usage
    [ -z "$CONTAINER" ] || [ -n "$MOUNT" ] || die usage
    case "$KIND" in
        test) [ -n "$RESULTS" ] || die usage ;;
        lint) [ -n "$PATTERN" ] || die usage ;;
        build) ;;
        *) die usage ;;
    esac
}

check_guards() {
    local current
    current="$(git -C "$REPO" branch --show-current 2>/dev/null)"
    [ "$current" = "$BRANCH" ] || die "branch_mismatch"
    if [ -n "$CONTAINER" ]; then
        docker inspect -f '{{range .Mounts}}{{.Source}}{{"\n"}}{{end}}' "$CONTAINER" 2>/dev/null \
            | grep -Fxq -- "$MOUNT" || die "mount_mismatch"
    fi
}

report() {
    local rc="$1" start="$2" out verdict
    case "$KIND" in
        test)
            out="$(python3 -I "$SCRIPT_DIR/test_summary.py" junit --dir "$RESULTS" \
                --since "$start" --runner-exit "$rc" --log "$LOG")"
            ;;
        lint)
            out="$(python3 -I "$SCRIPT_DIR/test_summary.py" lint --log "$LOG" \
                --pattern "$PATTERN" --runner-exit "$rc" --offset "$OFFSET")"
            ;;
        build)
            out="SUMMARY kind=build exit=$rc tests=0 failures=0 errors=0 skipped=0 lint=0 log=$LOG"$'\n'"FAILED="
            ;;
    esac
    verdict="$(printf '%s\n' "$out" | sed -n '1s/^SUMMARY kind=[a-z]* exit=\([0-9]*\) .*/\1/p')"
    echo "EXIT_CODE=${verdict:-1}"
    printf '%s\n' "$out"
    return "${verdict:-1}"
}

CMD=()
parse_args "$@"
validate_args
check_guards
START="$(python3 -c 'import time; print(time.time())')"
OFFSET="$(stat -c %s "$LOG" 2>/dev/null || echo 0)"
"${CMD[@]}" >>"$LOG" 2>&1
RC=$?
report "$RC" "$START"
exit $?
