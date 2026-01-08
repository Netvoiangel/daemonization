#!/bin/bash
set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$PROJECT_ROOT/build"
BIN_DIR="$PROJECT_ROOT/bin"
TARGET="$BIN_DIR/daemonizer"

mkdir -p "$BUILD_DIR" "$BIN_DIR"

clean() {
  rm -rf "$BUILD_DIR" "$TARGET"
}

compile() {
  sources=(
    "$PROJECT_ROOT/src/main.cpp"
    "$PROJECT_ROOT/src/Daemon.cpp"
    "$PROJECT_ROOT/src/Config.cpp"
    "$PROJECT_ROOT/src/CopyWorker.cpp"
    "$PROJECT_ROOT/src/Utils.cpp"
  )

  cxxflags=( -std=c++17 -Wall -Werror -O2 )
  ldflags=( )

  objs=()
  for src in "${sources[@]}"; do
    obj="$BUILD_DIR/$(basename "${src%.*}").o"
    objs+=("$obj")
    g++ "${cxxflags[@]}" -c "$src" -o "$obj"
  done

  set +u
  g++ "${cxxflags[@]}" "${objs[@]}" -o "$TARGET" "${ldflags[@]}"
  set -u
}

case "${1:-build}" in
  clean)
    clean
    ;;
  build)
    clean || true
    mkdir -p "$BUILD_DIR" "$BIN_DIR"
    compile
    ;;
  *)
    echo "Usage: $0 [build|clean]" >&2
    exit 1
    ;;
esac

echo "Built: $TARGET"


