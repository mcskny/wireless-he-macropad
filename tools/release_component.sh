#!/bin/bash
# Create an immutable component release from a committed, clean repository.
set -euo pipefail
component=${1:?Usage: release_component.sh c6\|s3\|studio notes.md [artifact ...]}
notes=${2:?Release notes file required}
shift 2
case "$component" in c6|s3|studio) ;; *) echo 'Unknown component' >&2; exit 1;; esac
[ -f "$notes" ] || { echo 'Release notes file missing' >&2; exit 1; }
[ -z "$(git status --porcelain)" ] || { echo 'Commit changes before releasing' >&2; exit 1; }
version=$(python3 -c 'import json,sys; print(json.load(open("releases/versions.json"))[sys.argv[1]])' "$component")
[[ "$version" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]] || { echo 'Invalid semantic version' >&2; exit 1; }
tag="$component/v$version"
if git rev-parse -q --verify "refs/tags/$tag" >/dev/null || gh release view "$tag" >/dev/null 2>&1; then
  echo "Version already exists: $tag" >&2; exit 1
fi
git tag -a "$tag" -m "$component $version"
git push origin "$tag"
gh release create "$tag" "$@" --verify-tag --prerelease --title "$component $version (prototype)" --notes-file "$notes"
