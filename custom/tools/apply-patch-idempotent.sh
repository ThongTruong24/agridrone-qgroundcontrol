#!/usr/bin/env bash
set -euo pipefail

PATCH_CONTENTS="$(dd status=none)"

if /usr/bin/patch --batch --forward --dry-run "$@" <<<"$PATCH_CONTENTS" >/dev/null 2>&1; then
    /usr/bin/patch --batch --forward "$@" <<<"$PATCH_CONTENTS"
elif /usr/bin/patch --batch --reverse --dry-run "$@" <<<"$PATCH_CONTENTS" >/dev/null 2>&1; then
    echo "Patch already applied; skipping."
else
    echo "Patch cannot be applied or cleanly reversed." >&2
    /usr/bin/patch --batch --forward --dry-run "$@" <<<"$PATCH_CONTENTS"
    exit 1
fi
