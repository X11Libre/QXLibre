#!/bin/bash
# Runs inside xvfb-run. Usage: run.sh <platform> <work dir> <smtest> <smreq>
# xsm with no saved session starts its system list. SIGUSR1 is a local checkpoint. smreq asks for a
# global save and then a global save with shutdown. Loading the saved session restarts the client.
platform=$1; work=$2; app=$3; req=$4
export HOME=$work/sm-xlibre ICEAUTHORITY=$work/sm-xlibre/.ICEauthority
export SMTEST_LOG=$work/$platform.log QT_QPA_PLATFORM=$platform
unset SESSION_MANAGER
rm -rf "$HOME"; mkdir -p "$HOME/bin"; : > "$SMTEST_LOG"
: > "$work/$platform-client.err"
# This xsm runs twm, smproxy and xterm, so the client stands in for twm, and smproxy tells us where
# the session manager is.
printf '#!/bin/bash\nexec "%s" 2>> "%s"\n' "$app" "$work/$platform-client.err" > "$HOME/bin/twm"
printf '#!/bin/bash\necho "$SESSION_MANAGER" > "%s/sm-address"\n' "$HOME" > "$HOME/bin/smproxy"
chmod +x "$HOME/bin/twm" "$HOME/bin/smproxy"
export PATH=$HOME/bin:$PATH

start_xsm() {
    xsm "$@" > "$work/$platform-xsm.log" 2>&1 &
    xsm=$!
}

start_xsm
sleep 4
if [ ! -s "$HOME/sm-address" ]; then
    echo "xsm did not start its clients" >> "$SMTEST_LOG"
    kill -9 $xsm; exit 3
fi

echo "signal local-checkpoint" >> "$SMTEST_LOG"
kill -USR1 $xsm
sleep 2
echo "request global-save" >> "$SMTEST_LOG"
SESSION_MANAGER=$(cat "$HOME/sm-address") "$req" 0 >> "$SMTEST_LOG" 2>&1
sleep 2
echo "request global-save-and-shutdown" >> "$SMTEST_LOG"
SESSION_MANAGER=$(cat "$HOME/sm-address") "$req" 1 >> "$SMTEST_LOG" 2>&1
for i in 1 2 3 4 5 6 7 8; do kill -0 $xsm 2>/dev/null || break; sleep 1; done
if kill -0 $xsm 2>/dev/null; then
    echo "xsm still running after shutdown, ending it" >> "$SMTEST_LOG"
    kill -TERM $xsm; sleep 2; kill -9 $xsm 2>/dev/null
fi
echo "client still running: $(pgrep -xc smtest)" >> "$SMTEST_LOG"
echo "saved session files: $(ls -a "$HOME" | grep -c '^\.XSM-')" >> "$SMTEST_LOG"

echo "signal restart" >> "$SMTEST_LOG"
start_xsm -session Default
sleep 4
kill -TERM $xsm
for i in 1 2 3 4 5; do kill -0 $xsm 2>/dev/null || break; sleep 1; done
kill -9 $xsm 2>/dev/null
pkill -x smtest
true
