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
# Test Case
#

# under this umask, we would expect to see 664
umask 002

cat >$LOG << 'EOF'
Commands:
---------
whoami > tests/output/tmp
cat tests/output/tmp
---------
EOF

#
# Test with LOBO
#
$LOBO >$OUT 2>&1 << 'EOF'
whoami > tests/output/tmp
cat tests/output/tmp
EOF

#
# Test with BASH
#
$BASH >$EOUT 2>&1 << 'EOF'
whoami > tests/output/etmp
cat tests/output/etmp
EOF

#
# Analyze results
#
if [ ! -f tests/output/tmp ] || [ ! -s tests/output/tmp ]; then
    echo "tests/output/tmp either not created or empty" >> $LOG
    ls -al tests/output/tmp >> $LOG 2>&1
    echo "FAIL $TEST"
    exit 1
fi

if [ -z $(find tests/output/tmp -perm 664) ]; then
    echo "Our umask is:" >> $LOG
    umask >> $LOG
    echo "File permissions are wrong for this umask:" >> $LOG
    stat tests/output/tmp >> $LOG 2>&1
    echo "FAIL $TEST"
    exit 2
fi
rm -f tests/output/tmp tests/output/etmp


diff $EOUT $OUT >> $LOG
echo "---------" >> $LOG

diff $EOUT $OUT >/dev/null && echo "PASS $TEST" || echo "FAIL $TEST"
