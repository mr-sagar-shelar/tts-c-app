#!/bin/sh

set -eu

ROOT_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
OUT_DIR="${OUT_DIR:-$ROOT_DIR/build/tinycore/linphonecsh}"
IMAGE_TAG="${IMAGE_TAG:-sai-linphonecsh-builder}"
BUILDER_PLATFORM="${BUILDER_PLATFORM:-linux/amd64}"

mkdir -p "$OUT_DIR"

docker build \
    --platform "$BUILDER_PLATFORM" \
    -f "$ROOT_DIR/tinycore/docker/linphonecsh-armhf.Dockerfile" \
    -t "$IMAGE_TAG" \
    "$ROOT_DIR"

CID="$(docker create "$IMAGE_TAG")"
trap 'docker rm -f "$CID" >/dev/null 2>&1 || true' EXIT

rm -rf "$OUT_DIR"
mkdir -p "$OUT_DIR"
docker cp "$CID:/out/." "$OUT_DIR/"

printf 'linphonecsh extension artifacts exported to %s\n' "$OUT_DIR"
