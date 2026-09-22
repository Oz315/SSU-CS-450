TEST=$(basename "${0%.sh}")
OUTPUT=$(realpath $(dirname $0))/output
OUT=$OUTPUT/$TEST.out
EOUT=$OUTPUT/$TEST.eout
LOG=$OUTPUT/$TEST.log
LOBO="./lobo_shell.x"
BASH=`which bash`

test "tests" != "$(basename $(dirname $(realpath $0)))" && { echo "FAIL: $0 not in 'tests'"; exit 1; }
! test -x $LOBO && { echo "FAIL: $LOBO must exist"; exit 2; }
mkdir -p $OUTPUT
rm -f $OUT $EOUT

#
# Check that valgrind exists
#
valgrind -h > /dev/null 2>&1
if [ $? != 0 ]; then
    echo "SKIP $TEST"
    exit
fi

#
# Test with LOBO
#
valgrind --leak-check=summary --track-fds=yes $LOBO >$OUT 2>&1 << 'EOF'
whoami
EOF

#
# Analyze results
#
BYTES=$(grep "definitely lost" "$OUT" \
    | tr -s ' ' \
    | cut --delimiter=' ' -f4 \
    | tr -d ',')

# Valgrind reports something like:
# FILE DESCRIPTORS: 3 open (3 std) at exit.
FDLINE=$(grep "FILE DESCRIPTORS:" "$OUT" | tail -1)

OPEN_FDS=$(echo "$FDLINE" \
    | sed -E 's/.*FILE DESCRIPTORS: ([0-9]+) open.*/\1/')

STD_FDS=$(echo "$FDLINE" \
    | sed -E 's/.*\(([0-9]+) std\).*/\1/')


echo "Valgrind reported:" >> $LOG
cat $OUT >> $LOG

if [ -z "$BYTES" ] || [ -z "$OPEN_FDS" ] || [ -z "$STD_FDS" ]; then
    echo "FAIL $TEST"
    echo "Unable to parse Valgrind results" >> $LOG
elif [ "$BYTES" -eq 0 ] && [ "$OPEN_FDS" -eq "$STD_FDS" ]; then
    echo "PASS $TEST"
else
    echo "FAIL $TEST"

    if [ "$BYTES" -ne 0 ]; then
        echo "Memory leak: $BYTES bytes definitely lost" >> $LOG
    fi

    if [ "$OPEN_FDS" -ne "$STD_FDS" ]; then
        echo "File descriptor leak: $OPEN_FDS open, $STD_FDS standard" >> $LOG
    fi
fi
