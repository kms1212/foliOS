#!/bin/sh

# The script expects arguments in the form:
# ./sparse_wrapper.sh <sparse_executable> <sparse_flags>... -- <compiler> <compiler_flags>...

set -e

SPARSE_EXEC=""
SPARSE_FLAGS=""

# Parse arguments until --
while [ "$#" -gt 0 ]; do
    case "$1" in
        --)
            shift
            break
            ;;
        *)
            if [ -z "$SPARSE_EXEC" ]; then
                SPARSE_EXEC="$1"
            else
                SPARSE_FLAGS="$SPARSE_FLAGS $1"
            fi
            shift
            ;;
    esac
done

if [ "$#" -eq 0 ]; then
    echo "Error: No compiler command specified (missing -- separator or compiler argument?)"
    exit 1
fi

COMPILER="$1"
shift
# Now $@ contains the compiler arguments (flags, source files, etc.)

# Run sparse
# We exclude the compiler executable from sparse arguments.
# We pass all checks' flags and source files.
# shellcheck disable=SC2086
"$SPARSE_EXEC" $SPARSE_FLAGS "$@" || exit 1

# Run the actual compiler
"$COMPILER" "$@"
