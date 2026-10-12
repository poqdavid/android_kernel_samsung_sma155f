#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0
#
# setup.sh - one setup script for KernelSU, KernelSU-Next and BakaSU
#
#
# What it does, from the kernel source root (the dir holding drivers/ or
# common/drivers/):
#   1. clones the fork into ./<dir> (or updates an existing clone)
#   2. checks out the requested branch / tag / commit
#   3. symlinks drivers/kernelsu -> <dir>/kernel
#   4. hooks drivers/Makefile and drivers/Kconfig (only once; safe to re-run)
#
# Local repos: if the fork already exists next to the kernel repo, e.g.
#     /home/david/sam/kernel          <- kernel repo (this script runs in it)
#     /home/david/sam/BakaSU          <- used instead of downloading
#     /home/david/sam/KernelSU
#     /home/david/sam/KernelSU-Next
# ./<dir> is cloned from that folder (no download; your repo is only read).
# --no-local / KSU_NO_LOCAL=1 forces GitHub, --local-dir / KSU_LOCAL_DIR
# changes where it looks.
#
# Differences from the upstream setup.sh scripts (all deliberate):
#   - full clone, never shallow: build.sh, scripts/repack and every fork's
#     Kbuild derive the version from `git rev-list --count HEAD`
#   - branches are checked out as a real local branch (BakaSU's Kbuild bakes
#     `git branch --show-current` into the version string)
#   - an unknown ref is a hard error instead of silently building something else
#   - re-running after switching variants (e.g. --ksu -> --bakasu, both use
#     ./KernelSU) replaces the stale clone instead of building the wrong fork
#   - `ln -sfn`, so re-pointing drivers/kernelsu never drops a stray link
#     inside the old clone
#   - Kconfig hook goes before the *last* endmenu only; cleanup removes only
#     the exact lines this script adds
#   - repo URLs live here, not in the forks, so "Sync fork" on GitHub can't
#     silently switch builds back to upstream; release tags are still pulled
#     from upstream so `git describe` (used in the version strings) is right

# Needs bash; re-exec if started with sh/dash.
if [ -z "${BASH_VERSION:-}" ]; then
    if [ -f "$0" ]; then exec bash "$0" "$@"; fi
    echo "[ERROR] setup.sh needs bash." >&2
    exit 1
fi

set -euo pipefail

# ---------------------------------------------------------------- presets ---
# variant -> repo / upstream / default ref / clone dir
# (dir names must match build.sh, scripts/repack and the clean step in build.sh)
#
# P_UPSTREAM is only used for tags: GitHub's "Sync fork" never copies tags, so
# a synced fork's `git describe` is stale, and BakaSU (version name) and
# KernelSU-Next (KSU_VERSION_TAG) bake that tag into the kernel.
#
# P_NOREF is what happens when no ref is given, same as the original scripts:
# "tag" = newest tag on P_REF (KernelSU, KernelSU-Next), "branch" = P_REF (BakaSU).
# P_REF_FROM_REMOTE=1: like KernelSU-Next's setup.sh, the default branch is the
# remote's HEAD (origin/HEAD); P_REF is only the fallback.
# P_LOCAL_NAME is the folder name looked for next to the kernel repo.
apply_preset() {
    case "$1" in
        ksu|kernelsu)
            P_LABEL="KernelSU"
            P_REPO="https://github.com/poqdavid/KernelSU"
            P_UPSTREAM="https://github.com/tiann/KernelSU"
            P_REF="main"
            P_REF_FROM_REMOTE=0
            P_NOREF="tag"
            P_DIR="KernelSU"
            P_LOCAL_NAME="KernelSU"
        ;;
        ksun|ksu-next|kernelsu-next|kernelsunext)
            P_LABEL="KernelSU-Next"
            P_REPO="https://github.com/poqdavid/KernelSU-Next"
            P_UPSTREAM="https://github.com/KernelSU-Next/KernelSU-Next"
            P_REF="dev"
            P_REF_FROM_REMOTE=1
            P_NOREF="tag"
            P_DIR="KernelSU-Next"
            P_LOCAL_NAME="KernelSU-Next"
        ;;
        bakasu)
            P_LABEL="BakaSU"
            P_REPO="https://github.com/poqdavid/BakaSU"
            P_UPSTREAM="https://github.com/Baka-SU/BakaSU"
            P_REF="main"
            P_REF_FROM_REMOTE=0
            P_NOREF="branch"
            P_DIR="KernelSU"
            P_LOCAL_NAME="BakaSU"
        ;;
        *) return 1 ;;
    esac
}

# Every clone dir any preset can create (used by --cleanup)
KNOWN_DIRS=(KernelSU KernelSU-Next)

# Exact lines this script adds, and patterns that recognise them
# shellcheck disable=SC2016  # literal $(CONFIG_KSU) is intended
MK_LINE='obj-$(CONFIG_KSU) += kernelsu/'
MK_RE='^[[:space:]]*obj-\$\(CONFIG_KSU\)[[:space:]]*\+=[[:space:]]*kernelsu/?[[:space:]]*$'
# Any obj- line (not a comment) that builds kernelsu/ counts as already hooked,
# like the original scripts' `grep -q kernelsu` (which also matched comments)
MK_HOOKED_RE='^[[:space:]]*obj-[^#]*[[:space:]=]kernelsu/?([[:space:]#]|$)'
KC_LINE='source "drivers/kernelsu/Kconfig"'
KC_RE='^[[:space:]]*source[[:space:]]+"drivers/kernelsu/Kconfig"[[:space:]]*$'

# ---------------------------------------------------------------- logging ---
log()  { printf '[+] %s\n' "$*"; }
note() { printf '[-] %s\n' "$*"; }
warn() { printf '[!] %s\n' "$*" >&2; }
die()  { printf '[ERROR] %s\n' "$1" >&2; exit "${2:-1}"; }

usage() {
    cat <<'EOF'
Usage: setup.sh [options] [<variant>] [<ref>]

Sets up a KernelSU fork in the kernel tree (run from the kernel source root).

  <variant>              ksu | ksun | bakasu                  (default: ksu)
                           ksu    -> poqdavid/KernelSU       main -> ./KernelSU
                           ksun   -> poqdavid/KernelSU-Next  dev  -> ./KernelSU-Next
                           bakasu -> poqdavid/BakaSU         main -> ./KernelSU
  <ref>                  branch, tag or commit. Without one, same as the
                         original scripts: newest tag on main/dev for ksu and
                         ksun, the main branch for bakasu

Options:
  -v, --variant NAME     same as <variant>
  -r, --ref REF          same as <ref>
      --latest-tag       check out the newest tag on the default branch
                         (already the default for ksu / ksun)
      --repo URL         use another git URL for the variant
      --upstream URL     where to fetch release tags from (default: the
                         fork's upstream project)
      --no-upstream-tags only use the tags that exist in --repo
      --dir NAME         clone into ./NAME instead of the preset dir
  -k, --kernel-dir DIR   kernel source root (default: current directory)
      --no-fetch         reuse an existing clone without touching the network
      --local-dir DIR    look for local repos (KernelSU, KernelSU-Next, BakaSU)
                         in DIR (default: the folder above the kernel repo;
                         env: KSU_LOCAL_DIR)
      --no-local         always download, ignore local repos (env: KSU_NO_LOCAL=1)
      --cleanup          undo everything: symlink, Makefile/Kconfig lines and
                         the clone (with a variant: only that variant's dir;
                         without: every known clone dir)
      --keep-source      with --cleanup: leave the cloned repo(s) in place
      --submodule        register an existing clone as a git submodule of the
                         kernel repo (from BakaSU's setup.sh; skipped in CI and
                         when the kernel root is not a git repo itself)
  -h, --help             show this help

Examples (from the kernel root):
  bash setup.sh bakasu
  bash setup.sh --variant ksun --ref v3.0.0
  bash setup.sh --cleanup
EOF
}

# ---------------------------------------------------------------- args ------
VARIANT=""
REF=""
REPO=""
UPSTREAM=""
UPSTREAM_TAGS=1
DIR=""
KERNEL_DIR="."
LATEST_TAG=0
NO_FETCH=0
CLEANUP=0
KEEP_SOURCE=0
SUBMODULE=0
LOCAL_DIR="${KSU_LOCAL_DIR:-}"
NO_LOCAL=0
[[ "${KSU_NO_LOCAL:-0}" == 1 ]] && NO_LOCAL=1
POSITIONAL=()

need_arg() { [[ $# -ge 2 && -n "$2" ]] || die "Option $1 needs a value." 2; }

while [[ $# -gt 0 ]]; do
    case "$1" in
        -v|--variant)    need_arg "$@"; VARIANT="$2"; shift 2 ;;
        -r|--ref)        need_arg "$@"; REF="$2"; shift 2 ;;
        --repo)          need_arg "$@"; REPO="$2"; shift 2 ;;
        --upstream)      need_arg "$@"; UPSTREAM="$2"; shift 2 ;;
        --no-upstream-tags) UPSTREAM_TAGS=0; shift ;;
        --dir)           need_arg "$@"; DIR="$2"; shift 2 ;;
        -k|--kernel-dir) need_arg "$@"; KERNEL_DIR="$2"; shift 2 ;;
        --latest-tag)    LATEST_TAG=1; shift ;;
        --no-fetch)      NO_FETCH=1; shift ;;
        --local-dir)     need_arg "$@"; LOCAL_DIR="$2"; shift 2 ;;
        --no-local)      NO_LOCAL=1; shift ;;
        --cleanup)       CLEANUP=1; shift ;;
        --keep-source)   KEEP_SOURCE=1; shift ;;
        --submodule)     SUBMODULE=1; shift ;;
        -h|--help)       usage; exit 0 ;;
        --)              shift; POSITIONAL+=("$@"); break ;;
        -*)              usage >&2; die "Unknown option: $1" 2 ;;
        *)               POSITIONAL+=("$1"); shift ;;
    esac
done

# Positionals: [<variant>] [<ref>]. A lone non-variant word is a ref, so the
# old `setup.sh <ref>` call style keeps working.
for p in "${POSITIONAL[@]+"${POSITIONAL[@]}"}"; do
    if [[ -z "$VARIANT" ]] && apply_preset "$p"; then
        VARIANT="$p"
        elif [[ -z "$REF" ]]; then
        REF="$p"
    else
        usage >&2; die "Unexpected argument: $p" 2
    fi
done

VARIANT_GIVEN=0
[[ -n "$VARIANT" ]] && VARIANT_GIVEN=1
VARIANT="${VARIANT:-ksu}"
apply_preset "$VARIANT" || die "Unknown variant '$VARIANT' (use ksu, ksun or bakasu)." 2

[[ -n "$REF" && $LATEST_TAG -eq 1 ]] && die "--ref and --latest-tag are mutually exclusive." 2

REPO_GIVEN=0
[[ -n "$REPO" ]] && REPO_GIVEN=1
REPO="${REPO:-$P_REPO}"
REMOTE_REPO="$REPO"
UPSTREAM="${UPSTREAM:-$P_UPSTREAM}"
DIR_GIVEN=0
[[ -n "$DIR" ]] && DIR_GIVEN=1
DIR="${DIR:-$P_DIR}"
[[ "$DIR" == */* || "$DIR" == "." || "$DIR" == ".." ]] && die "--dir must be a plain directory name, got '$DIR'." 2

# ---------------------------------------------------------------- paths -----
[[ -d "$KERNEL_DIR" ]] || die "Kernel dir not found: $KERNEL_DIR" 127
GKI_ROOT="$(cd -- "$KERNEL_DIR" && pwd -P)"

if [[ -d "$GKI_ROOT/common/drivers" ]]; then
    DRIVER_DIR="$GKI_ROOT/common/drivers"
    elif [[ -d "$GKI_ROOT/drivers" ]]; then
    DRIVER_DIR="$GKI_ROOT/drivers"
else
    die "\"drivers/\" directory not found in $GKI_ROOT (run from the kernel source root or pass --kernel-dir)." 127
fi

DRIVER_MAKEFILE="$DRIVER_DIR/Makefile"
DRIVER_KCONFIG="$DRIVER_DIR/Kconfig"
DRIVER_LINK="$DRIVER_DIR/kernelsu"
SRC="$GKI_ROOT/$DIR"

[[ -f "$DRIVER_MAKEFILE" ]] || die "Missing $DRIVER_MAKEFILE" 127
[[ -f "$DRIVER_KCONFIG" ]]  || die "Missing $DRIVER_KCONFIG" 127

# Never hang on a credential prompt (wrong/private URL) in CI
export GIT_TERMINAL_PROMPT=0

# ---------------------------------------------------------------- local repo -
# Sets LOCAL_REPO when <dir above the kernel repo>/<P_LOCAL_NAME> is a usable,
# full git repo. Skipped with --no-local, --repo, --cleanup and --submodule.
LOCAL_REPO=""
LOCAL_MODE=0
find_local_repo() {
    [[ $NO_LOCAL -eq 1 || $REPO_GIVEN -eq 1 || $CLEANUP -eq 1 || $SUBMODULE -eq 1 ]] && return 0
    
    local base cand top
    if [[ -n "$LOCAL_DIR" ]]; then
        [[ -d "$LOCAL_DIR" ]] || die "--local-dir / KSU_LOCAL_DIR not found: $LOCAL_DIR" 2
        base="$(cd -- "$LOCAL_DIR" && pwd -P)"
    else
        top="$(git -C "$GKI_ROOT" rev-parse --show-toplevel 2>/dev/null || true)"
        base="$(dirname -- "${top:-$GKI_ROOT}")"
    fi
    
    cand="$base/$P_LOCAL_NAME"
    if [[ ! -d "$cand" ]]; then
        [[ -n "$LOCAL_DIR" ]] && note "No $P_LOCAL_NAME in $base; downloading instead."
        return 0
    fi
    cand="$(cd -- "$cand" && pwd -P)"
    
    if [[ "$cand" == "$GKI_ROOT" || "$cand" == "$GKI_ROOT"/* ]]; then
        warn "$cand is inside the kernel tree; not using it as a local repo."
        return 0
    fi
    # Must be a repo of its own, not a plain folder inside some other repo
    if [[ "$(git -C "$cand" rev-parse --is-bare-repository 2>/dev/null)" != "true" ]]; then
        top="$(git -C "$cand" rev-parse --show-toplevel 2>/dev/null || true)"
        if [[ -z "$top" || "$(cd -- "$top" && pwd -P)" != "$cand" ]]; then
            warn "$cand is not a git repository; downloading instead."
            return 0
        fi
    fi
    # The version number is a commit count, so the history must be complete
    if [[ "$(git -C "$cand" rev-parse --is-shallow-repository 2>/dev/null)" == "true" ]]; then
        warn "$cand is a shallow clone (the version number would be wrong); downloading instead."
        return 0
    fi
    
    LOCAL_REPO="$cand"
    LOCAL_MODE=1
    REPO="$LOCAL_REPO"
}
find_local_repo

# ---------------------------------------------------------------- helpers ---
g() { git -C "$SRC" -c advice.detachedHead=false "$@"; }

# retry <cmd...>: 3 attempts with a short back-off (flaky CI networks)
retry() {
    local n=1
    until "$@"; do
        [[ $n -ge 3 ]] && return 1
        warn "Attempt $n/3 failed: $*; retrying in $((n * 5))s..."
        sleep $((n * 5))
        n=$((n + 1))
    done
}

# Compare git URLs loosely: scheme, credentials, .git suffix and case ignored
norm_url() {
    local u="$1"
    u="${u%/}"; u="${u%.git}"
    u="${u#*://}"
    u="${u#*@}"
    u="${u/://}"
    printf '%s' "${u,,}"
}

# $1 = dir name under the kernel root; true if git treats it as a submodule
is_submodule() {
    [[ -f "$GKI_ROOT/$1/.git" ]] && return 0
    [[ -f "$GKI_ROOT/.gitmodules" ]] \
    && grep -Eq "path[[:space:]]*=[[:space:]]*${1}[[:space:]]*$" "$GKI_ROOT/.gitmodules"
}

clone_fresh() {
    rm -rf -- "$SRC"
    git clone --origin origin -- "$REPO" "$SRC"
}

# ---------------------------------------------------------------- cleanup ---
perform_cleanup() {
    log "Cleaning up..."
    
    if [[ -L "$DRIVER_LINK" ]]; then
        rm -f -- "$DRIVER_LINK" && note "Symlink removed."
        elif [[ -e "$DRIVER_LINK" ]]; then
        warn "$DRIVER_LINK is a real directory, not our symlink; left alone."
    fi
    
    if grep -Eq "$MK_RE" "$DRIVER_MAKEFILE"; then
        sed -i -E "\#${MK_RE}#d" "$DRIVER_MAKEFILE" && note "Makefile reverted."
    fi
    if grep -Eq "$KC_RE" "$DRIVER_KCONFIG"; then
        sed -i -E "\#${KC_RE}#d" "$DRIVER_KCONFIG" && note "Kconfig reverted."
    fi
    
    if [[ $KEEP_SOURCE -eq 1 ]]; then
        note "Keeping cloned sources (--keep-source)."
        return 0
    fi
    
    local dirs=("$DIR") d
    if [[ $VARIANT_GIVEN -eq 0 && $DIR_GIVEN -eq 0 ]]; then
        dirs=("${KNOWN_DIRS[@]}")
    fi
    local sub
    for d in "${dirs[@]}"; do
        sub=0; is_submodule "$d" && sub=1
        if [[ -e "$GKI_ROOT/$d/.git" ]]; then
            rm -rf -- "${GKI_ROOT:?}/$d" && note "$d directory deleted."
            elif [[ -e "$GKI_ROOT/$d" ]]; then
            warn "$d is not a git clone; left alone."
        fi
        # Same hint as BakaSU's setup.sh: the submodule entry needs git itself
        if [[ $sub -eq 1 ]]; then
            warn "$d has been added as a submodule. Please remove it manually:"
            warn "  git submodule deinit -f $d"
            warn "  git rm -f $d"
            warn "  git commit -m 'Remove $d submodule'"
        fi
    done
}

# ---------------------------------------------------------------- submodule -
# Same guards as BakaSU's setup.sh --submodule
setup_submodule() {
    [[ -d "$SRC" ]] || die "./$DIR does not exist. Run this script without --submodule first." 127
    if [[ ! -e "$GKI_ROOT/.git" ]]; then
        warn "$GKI_ROOT is not a git repository. Skipping submodule setup."
        return 0
    fi
    if [[ "${CI:-false}" == "true" || "${GITHUB_ACTIONS:-false}" == "true" ]]; then
        warn "Running in CI. Skipping submodule setup."
        return 0
    fi
    if is_submodule "$DIR"; then
        warn "./$DIR is already a submodule. Skipping submodule setup."
        return 0
    fi
    log "Setting up ./$DIR as submodule..."
    git -C "$GKI_ROOT" submodule add -- "$REMOTE_REPO" "$DIR" || warn "Failed to add ./$DIR as a submodule."
    log "Done."
}

# ---------------------------------------------------------------- setup -----
prepare_source() {
    # Existing clone of a different repo (variant switch with --no-clean)?
    if [[ -e "$SRC" ]]; then
        if [[ ! -e "$SRC/.git" ]]; then
            die "$SRC exists but is not a git clone; move it away or run --cleanup."
        fi
        local cur
        cur="$(g remote get-url origin 2>/dev/null || true)"
        if [[ "$(norm_url "$cur")" != "$(norm_url "$REPO")" ]]; then
            is_submodule "$DIR" && die "$DIR is a submodule of ${cur:-<unknown>}, not $REPO; fix the submodule URL instead."
            warn "./$DIR is a clone of ${cur:-<no origin>}, not $REPO; replacing it."
            rm -rf -- "$SRC"
        fi
    fi
    
    if [[ ! -e "$SRC" ]]; then
        if [[ $LOCAL_MODE -eq 1 ]]; then
            log "Cloning local repo $LOCAL_REPO into ./$DIR (no download)..."
            if ! clone_fresh; then
                warn "Could not clone $LOCAL_REPO; downloading $REMOTE_REPO instead."
                LOCAL_MODE=0
                REPO="$REMOTE_REPO"
            fi
        fi
        if [[ $LOCAL_MODE -eq 0 ]]; then
            log "Cloning $REPO into ./$DIR..."
            retry clone_fresh || die "git clone of $REPO failed."
        fi
        log "Repository cloned."
    else
        # Keep anything left over from a previous run (e.g. SUSFS glue patch,
        # .orig/.rej files) recoverable, then start from a clean tree.
        if [[ -n "$(g status --porcelain)" ]]; then
            # stash makes a commit; CI runners often have no git identity
            local ident=()
            if [[ -z "$(g config user.email || true)" ]]; then
                ident=(-c user.name=ksu_setup -c user.email=ksu_setup@localhost)
            fi
            g "${ident[@]+"${ident[@]}"}" stash push --include-untracked --quiet \
            -m "setup.sh autostash $(date '+%F %T')"
            note "Stashed local changes in ./$DIR (see: git -C $DIR stash list)."
        fi
        
        if [[ $NO_FETCH -eq 1 ]]; then
            note "Skipping fetch (--no-fetch)."
        else
            if [[ $LOCAL_MODE -eq 1 ]]; then
                log "Updating ./$DIR from $LOCAL_REPO (no download)..."
            else
                log "Updating ./$DIR..."
            fi
            if [[ "$(g rev-parse --is-shallow-repository)" == "true" ]]; then
                retry g fetch --unshallow origin || die "git fetch --unshallow failed."
            fi
            retry g fetch --prune --force --tags origin '+refs/heads/*:refs/remotes/origin/*' \
            || die "git fetch from $REPO failed."
            log "Repository updated."
        fi
    fi
}

# Pull release tags from upstream into the fork clone (never moves HEAD,
# never touches branches; failure only costs an accurate tag name)
sync_upstream_tags() {
    [[ $UPSTREAM_TAGS -eq 1 && $NO_FETCH -eq 0 && -n "$UPSTREAM" ]] || return 0
    [[ "$(norm_url "$UPSTREAM")" == "$(norm_url "$REPO")" ]] && return 0
    
    log "Fetching release tags from $UPSTREAM..."
    # One attempt only when working from a local repo (may be offline)
    local tries=retry
    [[ $LOCAL_MODE -eq 1 ]] && tries=""
    if $tries g fetch --quiet --no-tags "$UPSTREAM" '+refs/tags/*:refs/tags/*'; then
        log "Upstream tags fetched."
    else
        warn "Could not fetch tags from $UPSTREAM; the version tag may be stale."
    fi
}

# Branch used when no ref is given (see P_REF_FROM_REMOTE)
default_branch() {
    local b=""
    if [[ "$P_REF_FROM_REMOTE" == 1 ]]; then
        b="$(g symbolic-ref --quiet --short refs/remotes/origin/HEAD 2>/dev/null || true)"
        b="${b#origin/}"
    fi
    printf '%s' "${b:-$P_REF}"
}

has_ref() { g show-ref --verify --quiet "$1"; }

# Branch from origin, as a real local branch reset to the remote tip
checkout_remote_branch() {
    local ref="$1" old
    if has_ref "refs/heads/$ref" \
    && ! g merge-base --is-ancestor "refs/heads/$ref" "refs/remotes/origin/$ref" 2>/dev/null; then
        old="$(g rev-parse --short=12 "refs/heads/$ref")"
        warn "Local $ref ($old) has commits that are not on origin/$ref (force-pushed fork or local commits);"
        warn "resetting to origin/$ref. The old tip stays in: git -C $DIR reflog"
    fi
    g checkout --quiet -B "$ref" "refs/remotes/origin/$ref"
    note "Checked out branch $ref."
}

# Same precedence as the original scripts' `git checkout <ref>`:
# local branch > tag > branch that only exists on origin > any other commit-ish
checkout_ref() {
    local ref="$1"
    
    # Local repo without a local <ref> branch, but with its origin/<ref>:
    # use that (cloning only copies the local repo's own branches)
    if [[ $LOCAL_MODE -eq 1 ]] && ! has_ref "refs/heads/$ref" && ! has_ref "refs/tags/$ref" \
    && ! has_ref "refs/remotes/origin/$ref" \
    && git -C "$LOCAL_REPO" show-ref --verify --quiet "refs/remotes/origin/$ref"; then
        g fetch --quiet origin "+refs/remotes/origin/$ref:refs/remotes/origin/$ref"
        note "$LOCAL_REPO has no local '$ref' branch; using its origin/$ref."
    fi
    
    if has_ref "refs/heads/$ref"; then
        if has_ref "refs/remotes/origin/$ref"; then
            checkout_remote_branch "$ref"
        else
            g checkout --quiet "$ref"
            note "Checked out local branch $ref."
        fi
        elif has_ref "refs/tags/$ref"; then
        g checkout --quiet --detach "refs/tags/$ref"
        note "Checked out tag $ref."
        elif has_ref "refs/remotes/origin/$ref"; then
        checkout_remote_branch "$ref"
        elif g rev-parse --verify --quiet "$ref^{commit}" >/dev/null; then
        g checkout --quiet --detach "$ref"
        note "Checked out commit $ref."
    else
        die "Ref '$ref' not found in $REPO (not a branch, tag or commit)."
    fi
}

link_driver() {
    # Older releases ship kernel/Makefile instead of kernel/Kbuild
    [[ -f "$SRC/kernel/Kconfig" && ( -f "$SRC/kernel/Kbuild" || -f "$SRC/kernel/Makefile" ) ]] \
    || die "$REPO has no kernel/Kconfig + kernel/Kbuild; is it really a KernelSU fork?"
    
    if [[ -e "$DRIVER_LINK" && ! -L "$DRIVER_LINK" ]]; then
        die "$DRIVER_LINK exists and is not a symlink (in-tree KernelSU?); remove it first."
    fi
    
    local target
    target="$(realpath --relative-to="$DRIVER_DIR" "$SRC/kernel")"
    # -n: replace an existing link instead of creating one inside its target
    ln -sfn -- "$target" "$DRIVER_LINK"
    [[ -f "$DRIVER_LINK/Kconfig" ]] || die "Symlink $DRIVER_LINK -> $target does not resolve."
    log "Symlink created: ${DRIVER_LINK#"$GKI_ROOT"/} -> $target"
}

hook_build_files() {
    if grep -Eq "$MK_HOOKED_RE" "$DRIVER_MAKEFILE"; then
        note "Makefile already hooked."
    else
        # Make sure the last line is terminated before appending
        if [[ -s "$DRIVER_MAKEFILE" && -n "$(tail -c1 "$DRIVER_MAKEFILE")" ]]; then
            printf '\n' >> "$DRIVER_MAKEFILE"
        fi
        printf '%s\n' "$MK_LINE" >> "$DRIVER_MAKEFILE"
        log "Modified Makefile."
    fi
    
    if grep -Eq "$KC_RE" "$DRIVER_KCONFIG"; then
        note "Kconfig already hooked."
    else
        local last
        last="$(grep -n '^[[:space:]]*endmenu' "$DRIVER_KCONFIG" | tail -n1 | cut -d: -f1 || true)"
        if [[ -n "$last" ]]; then
            sed -i "${last}i\\${KC_LINE}" "$DRIVER_KCONFIG"
        else
            printf '\n%s\n' "$KC_LINE" >> "$DRIVER_KCONFIG"
        fi
        log "Modified Kconfig."
    fi
}

# Tell the user what was (not) taken from their local repo
local_repo_notes() {
    [[ $LOCAL_MODE -eq 1 ]] || return 0
    local L="$LOCAL_REPO" built cur n
    built="$(g branch --show-current 2>/dev/null || true)"
    cur="$(git -C "$L" branch --show-current 2>/dev/null || true)"
    
    if [[ -n "$(git -C "$L" status --porcelain --untracked-files=no 2>/dev/null)" ]]; then
        warn "$L has uncommitted changes; they are NOT used (only commits are). Commit them to build them."
    fi
    if [[ -n "$built" && -n "$cur" && "$cur" != "$built" ]]; then
        note "$L is on branch '$cur'; building '$built' as requested."
    fi
    if [[ -n "$built" ]] && git -C "$L" show-ref --verify --quiet "refs/heads/$built" \
    && git -C "$L" show-ref --verify --quiet "refs/remotes/origin/$built"; then
        n="$(git -C "$L" rev-list --count "refs/heads/$built..refs/remotes/origin/$built" 2>/dev/null || echo 0)"
        [[ "$n" -gt 0 ]] && warn "Local $built is $n commit(s) behind its origin/$built (git -C $L pull to update)."
        n="$(git -C "$L" rev-list --count "refs/remotes/origin/$built..refs/heads/$built" 2>/dev/null || echo 0)"
        [[ "$n" -gt 0 ]] && note "Local $built has $n commit(s) that are not on origin/$built; they are included."
    fi
    return 0
}

print_summary() {
    local branch tag count base
    branch="$(g branch --show-current 2>/dev/null || true)"
    tag="$(g describe --tags --abbrev=0 2>/dev/null || echo none)"
    count="$(g rev-list --count HEAD)"
    # Same formula build.sh / scripts/repack use: sum of the integers on the
    # first "expr" line of kernel/Kbuild + commit count
    local kb="$SRC/kernel/Kbuild"
    [[ -f "$kb" ]] || kb="$SRC/kernel/Makefile"
    base="$(grep -m1 'expr' "$kb" 2>/dev/null | grep -oP '(?<![.\w])[0-9]+(?!\w)' | awk '{s+=$1} END{print s+0}' || true)"
    
    log "$P_LABEL ready in ./$DIR"
    if [[ $LOCAL_MODE -eq 1 ]]; then
        printf '    repo    : %s (local)\n' "$LOCAL_REPO"
    else
        printf '    repo    : %s\n' "$REPO"
    fi
    printf '    ref     : %s\n' "${branch:-detached}"
    printf '    commit  : %s\n' "$(g rev-parse --short=12 HEAD)"
    printf '    tag     : %s\n' "$tag"
    if [[ -n "$base" && "$base" -gt 0 ]]; then
        printf '    version : %s (%s + %s commits)\n' "$((base + count))" "$base" "$count"
    fi
}

setup_kernelsu() {
    log "Setting up $P_LABEL ($REPO) in $GKI_ROOT..."
    prepare_source
    sync_upstream_tags
    
    local defb
    defb="$(default_branch)"
    if [[ $LATEST_TAG -eq 1 || ( -z "$REF" && "$P_NOREF" == "tag" ) ]]; then
        checkout_ref "$defb"
        local tag
        tag="$(g describe --abbrev=0 --tags 2>/dev/null)" || die "No tags reachable from $defb."
        checkout_ref "$tag"
    else
        checkout_ref "${REF:-$defb}"
    fi
    
    link_driver
    hook_build_files
    local_repo_notes
    print_summary
    log "Done."
}

# ---------------------------------------------------------------- main ------
[[ $CLEANUP -eq 1 && $SUBMODULE -eq 1 ]] && die "--cleanup and --submodule are mutually exclusive." 2

if [[ $CLEANUP -eq 1 ]]; then
    perform_cleanup
    elif [[ $SUBMODULE -eq 1 ]]; then
    setup_submodule
else
    setup_kernelsu
fi
