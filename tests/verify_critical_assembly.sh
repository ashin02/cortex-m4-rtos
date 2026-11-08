#!/bin/sh
set -eu

disassembly=$1
expected_immediate=$2

mrs_register=$(sed -n \
    's/.*mrs[[:space:]]\+\(r[0-9][0-9]*\), BASEPRI.*/\1/p' \
    "$disassembly" | sed -n '1p')
msr_register=$(sed -n \
    's/.*msr[[:space:]]\+BASEPRI_MAX, \(r[0-9][0-9]*\).*/\1/p' \
    "$disassembly" | sed -n '1p')

test -n "$mrs_register"
test -n "$msr_register"
test "$mrs_register" != "$msr_register"
grep -Eq "#[[:space:]]*$expected_immediate([^0-9]|$)" "$disassembly"

printf 'critical assembly: PASS (saved=%s threshold=%s immediate=%s)\n' \
    "$mrs_register" "$msr_register" "$expected_immediate"
