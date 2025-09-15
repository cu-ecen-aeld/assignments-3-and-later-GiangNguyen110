#!/bin/bash

# Startup script which uses start-stop-daemon to start your aesdsocket application in daemon mode with the -d option.
# On stop it should send SIGTERM to gracefully exit your daemon and perform associated cleanup steps.
# Author: Giang Thu Nguyen

set -e
set -u
pushd $(dirname $0)

SCRIPTS_NAME=$(basename "$0")
SCRIPTS_DIR=$(pwd)
APP=$(pwd)/aesdsocket
ARGS="-d"
PIDFILE=/tmp/aesdsocket.pid

start() {
    if [ -f "$PIDFILE" ] && kill -0 $(cat "$PIDFILE") 2>/dev/null; then
        echo "aesdsocket is already running (PID $(cat $PIDFILE))"
        return 1
    fi
    
    # make clean
    # make all
    echo "Starting aesdsocket..."
    $APP $ARGS
    sleep 0.5   # give it a moment to fork
    pgrep -x aesdsocket > "$PIDFILE"
    echo "Started with PID $(cat $PIDFILE)"
    popd
}

stop() {
    if [ ! -f "$PIDFILE" ]; then
        echo "No PID file found, aesdsocket may not be running."
        return 1
    fi

    PID=$(cat "$PIDFILE")
    if kill -0 "$PID" 2>/dev/null; then
        echo "Stopping aesdsocket (PID $PID)..."
        kill -TERM "$PID"
        rm -f "$PIDFILE"
        echo "Stopped."
    else
        echo "Process not running but PID file exists. Cleaning up."
        rm -f "$PIDFILE"
    fi

    popd
}

if [ $# -lt 1 ]
then
    echo "Usage: $SCRIPTS_NAME {start|stop}"
else
    case "$1" in
        start)
            start
            ;;
        stop)
            stop
            ;;
        *) # If you forget the argument, it falls into this branch gracefully
            echo "Usage: $SCRIPTS_NAME {start|stop}"
            exit 1
            ;;
    esac # ← end of case
fi




