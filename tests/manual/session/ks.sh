#!/bin/bash
# Runs inside xvfb-run and dbus-run-session, so ksmserver has its own display and session bus.
# Usage: ks.sh <platform> <work dir> <smtest>
platform=$1; work=$2; app=$3
dir=$work/ks-$platform
export HOME=$dir ICEAUTHORITY=$dir/.ICEauthority XDG_CONFIG_HOME=$dir/config XDG_CACHE_HOME=$dir/cache
export XDG_DATA_HOME=$dir/data XDG_STATE_HOME=$dir/state XDG_RUNTIME_DIR=$dir/run
export SMTEST_LOG=$work/$platform-ks.log
unset SESSION_MANAGER XDG_SESSION_ID XDG_SEAT XDG_VTNR KDE_FULL_SESSION KDE_SESSION_UID
rm -rf "$dir"; mkdir -p "$XDG_CONFIG_HOME" "$XDG_CACHE_HOME" "$XDG_DATA_HOME" "$XDG_STATE_HOME"
mkdir -m 700 -p "$XDG_RUNTIME_DIR"; : > "$SMTEST_LOG"

bus="qdbus6 org.kde.ksmserver /KSMServer"
start_ksmserver() {
    # Qt's own plugin for ksmserver itself, so only the client differs between runs.
    QT_QPA_PLATFORM=xcb ksmserver --no-lockscreen "$@" > "$work/$platform-ksmserver.log" 2>&1 &
    server=$!
    for i in $(seq 1 40); do $bus org.freedesktop.DBus.Peer.Ping > /dev/null 2>&1 && return 0; sleep 0.25; done
    echo "ksmserver not on the bus" >> "$SMTEST_LOG"
    return 1
}

start_ksmserver || { kill -9 $server; exit 3; }
sleep 1
address=$(head -1 "$XDG_RUNTIME_DIR"/KSMserver__*)
# ksmserver writes its own ICE authority file for the clients it serves.
clientauth=$(ls -t "$XDG_RUNTIME_DIR"/iceauth_* | head -1)
echo "auth files: $(ls "$XDG_RUNTIME_DIR" | grep -c iceauth_)" >> "$SMTEST_LOG"
echo "bus is private: $([ "$DBUS_SESSION_BUS_ADDRESS" != "unix:path=/run/user/$(id -u)/bus" ] && echo yes || echo NO)" >> "$SMTEST_LOG"
echo "address found: $([ -n "$address" ] && echo yes || echo no)" >> "$SMTEST_LOG"
$bus > "$work/ks-methods.txt" 2>&1

ICEAUTHORITY=$clientauth SESSION_MANAGER=$address QT_QPA_PLATFORM=$platform "$app" 2>> "$work/$platform-ks-client.err" &
client=$!
sleep 2
echo "request saveCurrentSession" >> "$SMTEST_LOG"
$bus org.kde.KSMServerInterface.saveCurrentSession >> "$SMTEST_LOG" 2>&1
sleep 3
echo "request closeSession" >> "$SMTEST_LOG"
$bus org.kde.KSMServerInterface.closeSession >> "$SMTEST_LOG" 2>&1
for i in $(seq 1 15); do kill -0 $client 2>/dev/null || break; sleep 1; done
echo "client running after logout: $(kill -0 $client 2>/dev/null && echo yes || echo no)" >> "$SMTEST_LOG"
for i in $(seq 1 8); do kill -0 $server 2>/dev/null || break; sleep 1; done
echo "ksmserver running after logout: $(kill -0 $server 2>/dev/null && echo yes || echo no)" >> "$SMTEST_LOG"
kill -9 $client $server 2>/dev/null
sleep 1

echo "request restore" >> "$SMTEST_LOG"
export QT_QPA_PLATFORM=$platform
start_ksmserver --restore || { kill -9 $server; exit 4; }
sleep 1
echo "sessions: $($bus org.kde.KSMServerInterface.sessionList | tr '\n' ' ')" >> "$SMTEST_LOG"
$bus org.kde.KSMServerInterface.restoreSession >> "$SMTEST_LOG" 2>&1
sleep 6
kill -9 $server 2>/dev/null
pkill -x smtest
true
