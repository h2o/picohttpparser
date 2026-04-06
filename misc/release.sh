#!/bin/sh

set -eu

usage() {
    cat <<'EOF'
usage: misc/release.sh --source-branch BRANCH --release-branch BRANCH --tag-prefix PREFIX
                       [--remote NAME] [--header PATH] [--author-name NAME]
                       [--author-email EMAIL] [--push]
EOF
}

die() {
    echo "$@" >&2
    exit 1
}

fetch_branch() {
    branch="$1"

    if [ -z "$remote" ]; then
        git rev-parse --verify "$branch^{commit}" >/dev/null 2>&1 || die "branch not found: $branch"
        return
    fi

    git fetch "$remote" "refs/heads/$branch:refs/remotes/$remote/$branch"
}

cleanup() {
    if [ -n "${worktree_dir:-}" ] && [ -d "$worktree_dir" ]; then
        git -C "$repo_root" worktree remove --force "$worktree_dir" >/dev/null 2>&1 || true
    fi
}

source_branch=
release_branch=
tag_prefix=
remote=
header=picohttpparser.h
author_name="Kazuho Oku"
author_email="kazuhooku+github-actions@gmail.com"
push=0

while [ $# -gt 0 ]; do
    case "$1" in
    --source-branch)
        source_branch="$2"
        shift 2
        ;;
    --release-branch)
        release_branch="$2"
        shift 2
        ;;
    --tag-prefix)
        tag_prefix="$2"
        shift 2
        ;;
    --remote)
        remote="$2"
        shift 2
        ;;
    --header)
        header="$2"
        shift 2
        ;;
    --author-name)
        author_name="$2"
        shift 2
        ;;
    --author-email)
        author_email="$2"
        shift 2
        ;;
    --push)
        push=1
        shift
        ;;
    -h|--help)
        usage
        exit 0
        ;;
    *)
        usage >&2
        die "unknown argument: $1"
        ;;
    esac
done

[ -n "$source_branch" ] || die "--source-branch is required"
[ -n "$release_branch" ] || die "--release-branch is required"
[ -n "$tag_prefix" ] || die "--tag-prefix is required"

repo_root=$(git rev-parse --show-toplevel)

fetch_branch "$source_branch"
if [ -n "$remote" ]; then
    git fetch --tags "$remote"
    git fetch "$remote" "refs/heads/$release_branch:refs/remotes/$remote/$release_branch" >/dev/null 2>&1 || true
    source_ref="refs/remotes/$remote/$source_branch"
    release_ref="refs/remotes/$remote/$release_branch"
else
    source_ref="$source_branch"
    release_ref="$release_branch"
fi

major=$(git show "$source_ref:$header" | sed -n 's/^#define PICOHTTPPARSER_VERSION_MAJOR \([0-9][0-9]*\)$/\1/p')
version_string=$(git show "$source_ref:$header" | sed -n 's/^#define PICOHTTPPARSER_VERSION "\(.*\)"$/\1/p')

[ -n "$major" ] || die "could not parse PICOHTTPPARSER_VERSION_MAJOR from $header on $source_ref"
case "$version_string" in
    "$major".dev) ;;
    *)
        die "expected $header on $source_ref to carry a $major.dev version string"
        ;;
esac

latest_minor=0
for existing_tag in $(git tag -l "${tag_prefix}*"); do
    suffix=${existing_tag#"$tag_prefix"}
    case "$suffix" in
        ''|*[!0-9]*)
            continue
            ;;
    esac
    if [ "$suffix" -gt "$latest_minor" ]; then
        latest_minor=$suffix
    fi
done

next_minor=$((latest_minor + 1))
next_version="${major}.${next_minor}"
next_tag="${tag_prefix}${next_minor}"

worktree_dir=$(mktemp -d "${TMPDIR:-/tmp}/picohttpparser-release.XXXXXX")
trap cleanup EXIT INT TERM HUP

git -C "$repo_root" worktree add --detach "$worktree_dir" "$source_ref" >/dev/null

cd "$worktree_dir"

release_created=0
if git rev-parse --verify "$release_ref^{commit}" >/dev/null 2>&1; then
    git checkout -B "$release_branch" "$release_ref" >/dev/null
else
    git checkout -b "$release_branch" "$source_ref" >/dev/null
    release_created=1
fi

if [ "$release_created" -eq 0 ] && git merge-base --is-ancestor "$source_ref" HEAD; then
    echo "release branch already contains $source_ref; nothing to do"
    exit 0
fi

if [ "$release_created" -eq 0 ]; then
    git -c user.name="$author_name" -c user.email="$author_email" \
        merge --no-ff -X theirs --no-edit "$source_ref"
fi

tmp_header=$(mktemp "${TMPDIR:-/tmp}/picohttpparser-header.XXXXXX")
awk \
    -v version="$next_version" \
    -v major="$major" \
    -v minor="$next_minor" '
    /^#define PICOHTTPPARSER_VERSION "/ {
        print "#define PICOHTTPPARSER_VERSION \"" version "\""
        next
    }
    /^#define PICOHTTPPARSER_VERSION_MAJOR / {
        print "#define PICOHTTPPARSER_VERSION_MAJOR " major
        next
    }
    /^#define PICOHTTPPARSER_VERSION_MINOR / {
        print "#define PICOHTTPPARSER_VERSION_MINOR " minor
        next
    }
    { print }
    ' "$header" >"$tmp_header"
mv "$tmp_header" "$header"

grep -q "^#define PICOHTTPPARSER_VERSION \"${next_version}\"$" "$header" || die "failed to rewrite version string"
grep -q "^#define PICOHTTPPARSER_VERSION_MAJOR ${major}$" "$header" || die "failed to rewrite major version"
grep -q "^#define PICOHTTPPARSER_VERSION_MINOR ${next_minor}$" "$header" || die "failed to rewrite minor version"
git rev-parse --verify "${next_tag}^{tag}" >/dev/null 2>&1 && die "tag already exists: $next_tag"

git add "$header"
git -c user.name="$author_name" -c user.email="$author_email" \
    commit -m "release ${next_tag}" -m "Generated from ${source_branch} by release automation."
git -c user.name="$author_name" -c user.email="$author_email" \
    tag -a "$next_tag" -m "release ${next_tag}"

if [ "$push" -eq 1 ]; then
    [ -n "$remote" ] || die "--push requires --remote"
    git push "$remote" "$release_branch"
    git push "$remote" "$next_tag"
fi

echo "created ${next_tag} on ${release_branch}"
