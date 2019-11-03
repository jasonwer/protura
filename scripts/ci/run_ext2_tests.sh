#!/bin/bash
#
# Runs the kernel and execute a ext2 test, and then parses the output

mkdir -p ./test_results

TEST_PREFIX=./test_results/
QEMU_PID=
RET=
DISK_CPY=/tmp/protura_disk2_cpy.img
SINGLE_TEST=$1

function test_prepare {
    rm -fr $TEST_LOG
    touch $TEST_LOG
}

function run_ext2_test {
    echo "QEMU: Test: $1"
    timeout 120 qemu-system-i386 \
        -serial file:$3 \
        -d cpu_reset \
        -drive format=raw,file=./disk.img,cache=none,media=disk,index=0,if=ide \
        -drive format=raw,file=$DISK_CPY,media=disk,index=1,if=ide \
        -display none \
        -no-reboot \
        -kernel ./imgs/protura_x86_multiboot \
        -append "init=$1 reboot_on_panic=1" \
        2> /dev/null &

    QEMU_PID=$!
}

function test_verify {
    unset GREP_COLORS

    tail --pid $QEMU_PID -n+1 -f $TEST_LOG | GREP_COLOR="1;32" grep --line-buffered --color=always -E "PASS|$" | GREP_COLOR="1;31" grep --line-buffered --color=always -E "FAIL|PANIC|$"

    wait $QEMU_PID

    # ./scripts/ci/parse_test_output.pl < ./tests.log
    RET=$?
}

TOTAL_RESULT=0

if [ -z "$SINGLE_TEST" ]; then
    TESTS=$(find ./disk/tests/ext2/ -name "test_*.sh" | xargs basename -a)
else
    TESTS=$(basename "$SINGLE_TEST")
fi

for test in $TESTS; do
    TEST_LOG=${TEST_PREFIX}$(basename -s .sh $test).qemu.log
    TEST_E2FSCK_LOG=${TEST_PREFIX}$(basename -s .sh $test).e2fsck.log

    echo "LOG: $TEST_LOG, e2fsck: $TEST_E2FSCK_LOG"

    rm -fr $DISK_CPY
    cp ./disk2.img $DISK_CPY

    test_prepare
    run_ext2_test "/tests/ext2/$test" "$DISK_CPY" "$TEST_LOG"
    test_verify

    if [ "$RET" -ne "0" ]; then
        echo "QEMU TIMEOUT" >> "$TEST_LOG"
        echo "[1;31mQEMU TIMEOUT, DISK LIKELY NOT SYNCED, FAILURE!![m"
        TOTAL_RESULT=$(($TOTAL_RESULT + 1))
    fi

    sudo e2fsck -nfv $DISK_CPY | tee $TEST_E2FSCK_LOG

    if [ "${PIPESTATUS[0]}" -ne "0" ]; then
        echo "EXT2 FAILURE" >> "$TEST_E2FSCK_LOG"
        echo "[1;31mEXT2 FILE SYSTEM ERRORS![m"
        TOTAL_RESULT=$(($TOTAL_RESULT + 1))
    else
        echo "EXT2 PASS" >> "$TEST_E2FSCK_LOG"
    fi
done

rm $DISK_CPY

if [ "$TOTAL_RESULT" == "0" ]; then
    echo "ALL TESTS PASSED!"
else
    echo "TESTS FAILURE!"
fi

exit $TOTAL_RESULT
