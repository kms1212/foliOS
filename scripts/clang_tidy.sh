#!/usr/bin/env bash

set -euo pipefail

BUILD_DIR=build
COMPONENT=strata
JOBS=${JOBS:-0}
CLANG_TIDY_BIN=${CLANG_TIDY_BIN:-clang-tidy}
RUN_CLANG_TIDY_BIN=${RUN_CLANG_TIDY_BIN:-run-clang-tidy}
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

regex_escape() {
    printf '%s' "$1" | sed -e 's/[][(){}.^$*+?|\\]/\\&/g'
}

pick_tool() {
    local requested="$1"
    shift

    if command -v "${requested}" >/dev/null 2>&1; then
        command -v "${requested}"
        return 0
    fi

    local candidate
    for candidate in "$@"; do
        if [[ -x "${candidate}" ]]; then
            echo "${candidate}"
            return 0
        fi
    done

    return 1
}

print_usage() {
    echo "usage: $0 [-b build_dir] [-c component] [-j jobs]"
    echo "  component: strata | vellum | all"
}

run_with_compile_db() {
    local comp="$1"
    local db_dir="${BUILD_DIR}/${comp}"
    local db_file="${db_dir}/compile_commands.json"
    local source_root="${REPO_ROOT}/${comp}"
    local source_regex="^${source_root}/.*\\.(c|cc|cpp|cxx)$"
    local build_root="${BUILD_DIR}"
    local header_filter="^$(regex_escape "${REPO_ROOT}")/"
    local exclude_header_filter
    local build_root_regex
    local strata_lib_regex
    local vellum_lib_regex

    if [[ "${build_root}" != /* ]]; then
        build_root="${REPO_ROOT}/${build_root}"
    fi

    build_root_regex="$(regex_escape "${build_root}")/"
    strata_lib_regex="$(regex_escape "${REPO_ROOT}/strata/lib")/"
    vellum_lib_regex="$(regex_escape "${REPO_ROOT}/vellum/lib")/"
    exclude_header_filter="^(${build_root_regex}|${strata_lib_regex}|${vellum_lib_regex})"

    if [[ "${comp}" == "strata" ]]; then
        source_regex="^${source_root}/(?!lib/)(?!arch/amd64/pc/trampoline/).*\\.(c|cc|cpp|cxx)$"
    elif [[ "${comp}" == "vellum" ]]; then
        source_regex="^${source_root}/(?!lib/).*\\.(c|cc|cpp|cxx)$"
    fi

    if [[ ! -f "${db_file}" ]]; then
        echo "$0: compile_commands.json not found: ${db_file}" >&2
        echo "$0: hint: configure/build with -DCMAKE_EXPORT_COMPILE_COMMANDS=ON" >&2
        return 1
    fi

    if [[ -n "${RUN_CLANG_TIDY_BIN}" && -x "${RUN_CLANG_TIDY_BIN}" ]]; then
        local -a args
        args=(
            -p "${db_dir}"
            -clang-tidy-binary "${CLANG_TIDY_BIN}"
            -header-filter "${header_filter}"
            -exclude-header-filter "${exclude_header_filter}"
            -extra-arg=-Wno-error
            -extra-arg=-Wno-unused-command-line-argument
            -extra-arg=-Qunused-arguments
            "${source_regex}"
        )
        if [[ "${JOBS}" -gt 0 ]]; then
            args+=(-j "${JOBS}")
        fi

        echo "[clang-tidy] component=${comp} (run-clang-tidy)"
        "${RUN_CLANG_TIDY_BIN}" "${args[@]}"
        return $?
    fi

    echo "[clang-tidy] component=${comp} (python fallback)"
    python3 - "${db_dir}" "${CLANG_TIDY_BIN}" "${source_root}" "${comp}" <<'PY'
import json
import os
import re
import subprocess
import sys

db_dir = sys.argv[1]
clang_tidy = sys.argv[2]
source_root = os.path.realpath(sys.argv[3])
component = sys.argv[4]
repo_root = os.path.realpath(os.path.join(source_root, os.pardir))
db_path = os.path.join(db_dir, "compile_commands.json")

with open(db_path, "r", encoding="utf-8") as f:
    entries = json.load(f)

allowed_ext = {".c", ".cc", ".cpp", ".cxx", ".m", ".mm"}
seen = set()
files = []

for entry in entries:
    src = entry.get("file")
    if not src:
        continue
    ext = os.path.splitext(src)[1].lower()
    if ext not in allowed_ext:
        continue
    if not os.path.isabs(src):
        src = os.path.normpath(os.path.join(entry.get("directory", db_dir), src))
    src = os.path.realpath(src)
    if not src.startswith(source_root + os.sep):
        continue
    if component == "strata" and "/arch/amd64/pc/trampoline/" in src:
        continue
    if src.startswith(os.path.join(source_root, "lib") + os.sep):
        continue
    if src in seen:
        continue
    seen.add(src)
    files.append(src)

failed = False
exclude_header_filter = (
    "^("
    + re.escape(os.path.realpath(db_dir))
    + "/|"
    + re.escape(os.path.join(repo_root, "strata", "lib"))
    + "/|"
    + re.escape(os.path.join(repo_root, "vellum", "lib"))
    + "/)"
)

for src in files:
    cmd = [
        clang_tidy,
        "-p",
        db_dir,
        "-header-filter=^" + source_root + "/",
        "-exclude-header-filter=" + exclude_header_filter,
        "-extra-arg=-Wno-error",
        "-extra-arg=-Wno-unused-command-line-argument",
        "-extra-arg=-Qunused-arguments",
        src,
    ]
    if subprocess.call(cmd) != 0:
        failed = True

sys.exit(1 if failed else 0)
PY
}

while [[ $# -gt 0 ]]; do
    case "$1" in
    -b | --build-dir)
        BUILD_DIR="$2"
        shift 2
        ;;
    -c | --component)
        COMPONENT="$2"
        shift 2
        ;;
    -j | --jobs)
        JOBS="$2"
        shift 2
        ;;
    -h | --help)
        print_usage
        exit 0
        ;;
    *)
        print_usage
        exit 1
        ;;
    esac
done

CLANG_TIDY_REQUESTED="${CLANG_TIDY_BIN}"
if ! CLANG_TIDY_BIN="$(pick_tool "${CLANG_TIDY_BIN}" "/opt/homebrew/opt/llvm/bin/clang-tidy" "/usr/local/opt/llvm/bin/clang-tidy")"; then
    echo "$0: clang-tidy not found: ${CLANG_TIDY_REQUESTED}" >&2
    exit 1
fi

if ! RUN_CLANG_TIDY_BIN="$(pick_tool "${RUN_CLANG_TIDY_BIN}" "/opt/homebrew/opt/llvm/bin/run-clang-tidy" "/usr/local/opt/llvm/bin/run-clang-tidy")"; then
    RUN_CLANG_TIDY_BIN=""
fi

case "${COMPONENT}" in
strata | vellum)
    run_with_compile_db "${COMPONENT}"
    ;;
all)
    run_with_compile_db strata
    run_with_compile_db vellum
    ;;
*)
    print_usage
    exit 1
    ;;
esac
