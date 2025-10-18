#!/bin/sh

MODDIR="/lib/modules/$(uname -r)/extra"

case "$1" in 
    start)
        
     ( cd "$MODDIR" && /usr/bin/aesdchar_load)
    
        ;;

    stop)
       
     ( cd "$MODDIR" && /usr/bin/aesdchar_unload)
        ;;

        *)

      echo "Usage: $0 {start|stop}"
    exit 1
esac 

exit 0
