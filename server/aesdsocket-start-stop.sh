#!/bin/sh

# Startup script which uses start-stop-daemon to start your aesdsocket application in daemon mode with the -d option.
# On stop it should send SIGTERM to gracefully exit your daemon and perform associated cleanup steps.
# Author: Giang Thu Nguyen

set -e
set -u

SCRIPTS_NAME=$(basename "$0")
APP=/usr/bin/aesdsocket
ARGS="-d"

start() {
    echo "Starting aesdsocket..."
    $APP $ARGS
}

stop() {
    echo "Stopping aesdsocket..."
    killall aesdsocket
    echo "Stopped."
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