make

rm -rf /dev/xbmd
mknod /dev/xbmd c 241 0
chown root /dev/xbmd
chgrp plugdev /dev/xbmd
chmod 0664 /dev/xbmd

cd kernel && ./load_driver

