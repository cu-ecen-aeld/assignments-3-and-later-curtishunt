#!/bin/sh

DAEMON="/usr/bin/aesdsocket"
PIDFILE="/var/run/aesdsocket.pid"
DAEMON_ARGS="-d"

case "$1" in
    start)
        echo "Starting aesdsocket..."
        start-stop-daemon --start --background --make-pidfile --pidfile "$PIDFILE" --exec "$DAEMON" -- $DAEMON_ARGS
        ;;
    stop)
        echo "Stopping aesdsocket..."
        start-stop-daemon --stop --pidfile "$PIDFILE" --retry 5
        ;;
    restart)
        $0 stop
        $0 start
        ;;
    *)
        echo "Usage: $0 {start|stop|restart}"
        exit 1
        ;;
esac

exit 0