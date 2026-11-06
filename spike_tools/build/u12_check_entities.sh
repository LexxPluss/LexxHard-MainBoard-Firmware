#!/usr/bin/env bash
# E1: compare the entity table of the h-unit source with the fixed expected list (u12_entities.txt, outside the repository).
# Usage: u12_check_entities.sh <worktree> <modes>     modes: pub | pub,sub | all
# Source contract: spike_fw/src/microros_entities.h holds one entity per line, in the form
#   MICROROS_ENTITY(<PUB|SUB|SRV>, "<name>", <package>, <Type>, <UART6|UART2>, RELIABLE)
# The header is an X-macro table: it has no include guard; the including file defines MICROROS_ENTITY before the include.
# Prints one CHECK line; exit 0 only when the selected directions match exactly.
set -u
WT="${1:-}"; MODES="${2:-}"
HERE="$(dirname "$(readlink -f "$0")")"
EXPECTED="$HERE/u12_entities.txt"
SRC="$WT/spike_fw/src/microros_entities.h"

[ -d "$WT" ] && [ -n "$MODES" ] || { echo "FAIL usage: $0 <worktree> <pub|pub,sub|all>"; exit 2; }
[ -f "$EXPECTED" ] || { echo "FAIL expected list absent: $EXPECTED"; exit 2; }

case "$MODES" in
  pub) DIRS="PUB" ;;
  pub,sub) DIRS="PUB SUB" ;;
  all) DIRS="PUB SUB SRV" ;;
  *) echo "FAIL unknown modes: $MODES"; exit 2 ;;
esac

TMP="$(mktemp -d)"; trap 'rm -rf "$TMP"' EXIT
grep -v '^#' "$EXPECTED" | awk -v d="$DIRS" 'BEGIN{n=split(d,a," "); for(i=1;i<=n;i++) w[a[i]]=1} ($1 in w){print}' | sort > "$TMP/expected.txt"

if [ -f "$SRC" ]; then
  sed -n 's/^[[:space:]]*MICROROS_ENTITY([[:space:]]*\(PUB\|SUB\|SRV\),[[:space:]]*"\([^"]*\)",[[:space:]]*\([A-Za-z0-9_]*\),[[:space:]]*\([A-Za-z0-9_]*\),[[:space:]]*\(UART6\|UART2\),[[:space:]]*RELIABLE[[:space:]]*).*$/\1 \2 \3\/\4 \5/p' "$SRC" \
    | awk -v d="$DIRS" 'BEGIN{n=split(d,a," "); for(i=1;i<=n;i++) w[a[i]]=1} ($1 in w){print}' | sort > "$TMP/actual.txt"
else
  : > "$TMP/actual.txt"
fi

count() { awk -v k="$1" '$1==k{n++} END{print n+0}' "$2"; }
EP=$(count PUB "$TMP/expected.txt"); AP=$(count PUB "$TMP/actual.txt")
ES=$(count SUB "$TMP/expected.txt"); AS=$(count SUB "$TMP/actual.txt")
ER=$(count SRV "$TMP/expected.txt"); AR=$(count SRV "$TMP/actual.txt")

MISSING=$(comm -23 "$TMP/expected.txt" "$TMP/actual.txt" | wc -l)
EXTRA=$(comm -13 "$TMP/expected.txt" "$TMP/actual.txt" | wc -l)
DUP=$(uniq -d "$TMP/actual.txt" | wc -l)
[ -f "$SRC" ] && SRCSTATE=present || SRCSTATE=absent

echo "CHECK E1 modes=$MODES source=$SRCSTATE pub=$AP/$EP sub=$AS/$ES srv=$AR/$ER missing=$MISSING extra=$EXTRA duplicates=$DUP"
if [ "$MISSING" -ne 0 ] || [ "$EXTRA" -ne 0 ] || [ "$DUP" -ne 0 ]; then
  echo "missing (first 5):"; comm -23 "$TMP/expected.txt" "$TMP/actual.txt" | head -5
  echo "extra (first 5):";   comm -13 "$TMP/expected.txt" "$TMP/actual.txt" | head -5
  exit 1
fi
exit 0
