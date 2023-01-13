#!/bin/sh

patch `which su` <<EOF
9c9
< 		PATH=/sbin:/sbin/su:/su/bin:/su/xbin:/system/bin:/system/xbin \
---
> 		PATH=/sbin:/sbin/su:/su/bin:/su/xbin:/system/bin:/system/xbin:$PATH \
EOF

mkdir ~/.termux/boot/
cat > ~/.termux/boot/start <<EOF
#!/data/data/com.termux/files/usr/bin/sh
termux-wake-lock
. $PREFIX/etc/profile
EOF
chmod +x ~/.termux/boot/start
