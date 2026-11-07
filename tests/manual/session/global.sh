#!/bin/bash
# Runs inside xvfb-run. Usage: global.sh <platform> <work dir> <smtest> <smsrv>
platform=$1; work=$2; app=$3; srv=$4
export HOME=$work/global-$platform ICEAUTHORITY=$work/global-$platform/.ICEauthority
export SMTEST_LOG=$work/$platform-global.log QT_QPA_PLATFORM=$platform
rm -rf "$HOME"; mkdir -p "$HOME"; : > "$SMTEST_LOG"; touch "$ICEAUTHORITY"
unset SESSION_MANAGER
"$srv" "$HOME/address" > "$work/$platform-global-srv.log" 2>&1 &
srvpid=$!
for i in 1 2 3 4 5 6 7 8 9 10; do [ -s "$HOME/address" ] && break; sleep 0.2; done
SESSION_MANAGER=$(cat "$HOME/address") timeout 20 "$app" 2>> "$work/$platform-global-client.err"
echo "client exit status $?" >> "$SMTEST_LOG"
wait $srvpid
echo "server exit status $?" >> "$SMTEST_LOG"
true
