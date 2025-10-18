#! /bin/sh

case "$1" in
    start)

        echo "Loading AESD char driver..."
        /lib/modules/$(uname -r)/extra/aesdchar_load

        echo "Starting aesdsocket"
        start-stop-daemon -S -n aesdsocket -a /usr/bin/aesdsocket -- -d
    ;;
    stop)

        echo "Unloading AESD char driver..."
        /lib/modules/$(uname -r)/extra/aesdchar_unload

        echo "Stopping aesdsocket"
        start-stop-daemon -K -n aesdsocket
    ;;
    *)
        echo "Usage: $0 {start|stop}"
        exit 1
esac
