#!/usr/bin/env bash
set -euo pipefail

VERSION_FILE="src/Main-Thermostat.cpp"
CHANGELOG_FILE="CHANGELOG.md"

if [[ ! -f "$VERSION_FILE" ]]; then
  echo "ERROR: $VERSION_FILE not found."
  exit 1
fi

if [[ ! -f "$CHANGELOG_FILE" ]]; then
  echo "ERROR: $CHANGELOG_FILE not found."
  exit 1
fi

version="$(sed -n 's/.*sw_version = "\([^"]*\)".*/\1/p' "$VERSION_FILE" | head -n 1)"

if [[ -z "$version" ]]; then
  echo "ERROR: Could not parse sw_version from $VERSION_FILE"
  exit 1
fi

if ! grep -Fq "## [$version]" "$CHANGELOG_FILE"; then
  echo "ERROR: $CHANGELOG_FILE is missing an entry for version $version"
  echo "Add a header like: ## [$version] - YYYY-MM-DD"
  exit 1
fi

if [[ -z "$(git status --porcelain)" ]]; then
  echo "No changes to commit."
else
  git add -A
  git commit -m "Release $version"
fi

if git rev-parse -q --verify "refs/tags/v$version" >/dev/null; then
  echo "Tag v$version already exists."
else
  git tag "v$version"
fi

if git rev-parse -q --verify "refs/tags/$version" >/dev/null; then
  echo "Tag $version already exists."
else
  git tag "$version"
fi

echo "Pushing branch..."
git push

echo "Pushing tags..."
git push --tags

echo "Release complete for version $version"
