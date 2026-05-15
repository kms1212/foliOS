#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

SESSION_NAME=${FOLIOS_MONITOR_SESSION:-monitor}
MACHINE=pc-amd64
DISK_IMAGE=${FOLIOS_DISK_IMAGE:-disk.img}
MEM_SIZE=${FOLIOS_MEM_SIZE:-128M}
ENABLE_GDB=1
ENABLE_PLOTS=1
TEE_PID=

declare -a EXTRA_QEMU_ARGS
declare -a GDB_RUN_CMD
declare -a RUN_CMD

print_usage() {
    echo "usage: $0 [options] [machine] [qemu-args...]"
    echo
    echo "options:"
    echo "  -d, --disk path       disk image path (default: ${DISK_IMAGE})"
    echo "  -m, --memory size     guest memory size (default: ${MEM_SIZE})"
    echo "  -s, --session name    tmux session name (default: ${SESSION_NAME})"
    echo "      --no-gdb          do not open a GDB pane"
    echo "      --no-plots        do not open CPU/memory plot panes"
    echo "  -h, --help            show this help"
}

require_option_value() {
    if [[ $# -lt 2 || "$2" == -* ]]; then
        echo "$0: missing value for $1" >&2
        exit 1
    fi
}

require_command() {
    if ! command -v "$1" >/dev/null 2>&1; then
        echo "$0: required command not found: $1" >&2
        exit 1
    fi
}

quote_cmd() {
    printf "%q " "$@"
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        -d | --disk)
            require_option_value "$@"
            DISK_IMAGE="$2"
            shift 2
            ;;
        -m | --memory)
            require_option_value "$@"
            MEM_SIZE="$2"
            shift 2
            ;;
        -s | --session)
            require_option_value "$@"
            SESSION_NAME="$2"
            shift 2
            ;;
        --no-gdb)
            ENABLE_GDB=0
            shift
            ;;
        --no-plots)
            ENABLE_PLOTS=0
            shift
            ;;
        -h | --help)
            print_usage
            exit 0
            ;;
        --)
            shift
            EXTRA_QEMU_ARGS+=("$@")
            break
            ;;
        -*)
            print_usage
            exit 1
            ;;
        *)
            MACHINE="$1"
            shift
            EXTRA_QEMU_ARGS+=("$@")
            break
            ;;
    esac
done

require_command tmux

if [[ "${ENABLE_PLOTS}" -ne 0 ]] && ! command -v ttyplot >/dev/null 2>&1; then
    echo "$0: ttyplot not found; continuing without plot panes" >&2
    ENABLE_PLOTS=0
fi

if tmux has-session -t "${SESSION_NAME}" 2>/dev/null; then
    echo "$0: tmux session already exists: ${SESSION_NAME}" >&2
    exit 1
fi

DEBUG_DIR="$(mktemp -d /tmp/folios-monitor.XXXXXX)"
DEBUG_FIFO="${DEBUG_DIR}/debug"
LOG_PIPE="${DEBUG_DIR}/log"
CPU_PIPE="${DEBUG_DIR}/cpu"
MEM_PIPE="${DEBUG_DIR}/mem"

if [[ "${ENABLE_PLOTS}" -ne 0 ]]; then
    mkfifo "${DEBUG_FIFO}" "${LOG_PIPE}" "${CPU_PIPE}" "${MEM_PIPE}"
else
    mkfifo "${DEBUG_FIFO}" "${LOG_PIPE}"
fi

cleanup() {
    if [[ -n "${TEE_PID}" ]]; then
        kill "${TEE_PID}" 2>/dev/null || true
    fi
    tmux kill-session -t "${SESSION_NAME}" 2>/dev/null || true
    rm -rf "${DEBUG_DIR}"
}
trap cleanup EXIT INT TERM

RUN_CMD=(
    scripts/run.sh
    --disk "${DISK_IMAGE}"
    --memory "${MEM_SIZE}"
    "${MACHINE}"
    -debugcon "pipe:${DEBUG_FIFO}"
    -cpu max
    -display curses
    -S
    "${EXTRA_QEMU_ARGS[@]}"
)
GDB_RUN_CMD=(
    scripts/gdb.sh
    -t strata
    amd64
    --eval-command="tar rem :1234"
)
RUN_CMD_STR="cd $(printf "%q" "${REPO_ROOT}") && $(quote_cmd "${RUN_CMD[@]}")"

LOG_CMD="cat $(printf "%q" "${LOG_PIPE}")"
GDB_CMD="cd $(printf "%q" "${REPO_ROOT}") && $(quote_cmd "${GDB_RUN_CMD[@]}")"
CPU_CMD="cat $(printf "%q" "${CPU_PIPE}") | awk '/cpu usage:/ { gsub(/%/, \"\", \$NF); print \$NF; fflush() }' | ttyplot -t 'CPU (%)' -m 100"
MEM_CMD="cat $(printf "%q" "${MEM_PIPE}") | awk '/used memory:/ { print \$NF / 1024; fflush() }' | ttyplot -u kiB -t 'Mem (kiB)' -m 131072"

tmux new-session -d -s "${SESSION_NAME}" -n "VGA" "${RUN_CMD_STR}"
tmux split-window -h -t "${SESSION_NAME}:0" -p 55 "${LOG_CMD}"

if [[ "${ENABLE_GDB}" -ne 0 ]]; then
    tmux split-window -v -t "${SESSION_NAME}:0.1" -p 55 "${GDB_CMD}; tmux kill-session -t $(printf "%q" "${SESSION_NAME}")"
fi

if [[ "${ENABLE_PLOTS}" -ne 0 ]]; then
    tmux split-window -v -t "${SESSION_NAME}:0.1" -p 50 "${CPU_CMD}"
    tmux split-window -v -t "${SESSION_NAME}:0.2" -p 50 "${MEM_CMD}"
fi

tmux select-pane -t "${SESSION_NAME}:0.0"

if [[ "${ENABLE_PLOTS}" -ne 0 ]]; then
    tee "${LOG_PIPE}" "${CPU_PIPE}" "${MEM_PIPE}" < "${DEBUG_FIFO}" > /dev/null &
else
    cat "${DEBUG_FIFO}" > "${LOG_PIPE}" &
fi
TEE_PID=$!

tmux attach-session -t "${SESSION_NAME}"
