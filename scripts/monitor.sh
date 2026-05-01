#!/bin/bash

SESSION_NAME="monitor"
DEBUG_FIFO="/tmp/qemu_debug_$(date +%s)"
LOG_PIPE="${DEBUG_FIFO}_log"; CPU_PIPE="${DEBUG_FIFO}_cpu"; MEM_PIPE="${DEBUG_FIFO}_mem"

mkfifo "$DEBUG_FIFO" "$LOG_PIPE" "$CPU_PIPE" "$MEM_PIPE"

cleanup() {
    pkill -P $$ 2>/dev/null
    tmux kill-session -t "$SESSION_NAME" 2>/dev/null
    rm -f "$DEBUG_FIFO"*
    
    echo "Done."
    exit
}
trap cleanup SIGINT SIGTERM

tmux new-session -d -s "$SESSION_NAME" -n "VGA" \
    "scripts/run.sh pc-amd64 -debugcon pipe:$DEBUG_FIFO -cpu max -display curses"

tmux split-window -h -t "$SESSION_NAME" -p 70 "scripts/gdb.sh -t strata amd64 --eval-command=\"tar rem :1234\"; tmux kill-session -t $SESSION_NAME"
tmux split-window -v -t "$SESSION_NAME.0" "cat $LOG_PIPE"
tmux split-window -v -t "$SESSION_NAME.2" \
    "cat $CPU_PIPE | awk '/cpu usage:/ { gsub(/%/, \"\", \$NF); print \$NF; fflush() }' | ttyplot -t 'CPU (%)' -m 100"
tmux split-window -v -t "$SESSION_NAME.2" \
    "cat $MEM_PIPE | awk '/used memory:/ { print \$NF / 1024; fflush() }' | ttyplot -u kiB -t 'Mem (kiB)' -m 131072"

tmux select-pane -t "$SESSION_NAME.2"

cat "$DEBUG_FIFO" | tee "$LOG_PIPE" "$CPU_PIPE" "$MEM_PIPE" > /dev/null &

tmux attach-session -t "$SESSION_NAME"
cleanup
