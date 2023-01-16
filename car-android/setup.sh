#!/bin/sh -e

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

mkdir -p $PREFIX/var/service/starttether/log
ln -sf $PREFIX/share/termux-services/svlogger $PREFIX/var/service/starttether/log/run
cat > $PREFIX/var/service/starttether/run <<EOF
#!/data/data/com.termux/files/usr/bin/sh
exec 2>&1

# Turns on wifi tethering.
su -c service call tethering 4 null s16 random

# sv is not the best way to run a one-off, but it gives me an easy way to run
# to enable tethering when the phone starts up.
while true
do
  sleep 1000
done
EOF
chmod +x $PREFIX/var/service/starttether/run

mkdir -p $PREFIX/var/service/docker/log
ln -sf $PREFIX/share/termux-services/svlogger $PREFIX/var/service/docker/log/run
cat > $PREFIX/var/service/docker/run <<EOF
#!/data/data/com.termux/files/usr/bin/sh
exec 2>&1

sv start starttether

# Termux runs without root, so we have to su everything.
echo "Setting up mounts"
su -c mount -t tmpfs -o mode=755 tmpfs /sys/fs/cgroup
su -c mkdir -p /sys/fs/cgroup/devices
su -c mount -t cgroup -o devices cgroup /sys/fs/cgroup/devices

# Stop the old docker instance just in case it is up.
DOCKER_PID=\$(su -c cat /data/docker/run/docker.pid)
su -c kill \${DOCKER_PID}
sleep 20
echo "Starting dockerd"
exec su -c dockerd --iptables=false
EOF
chmod +x \$PREFIX/var/service/docker/run

# let sv catch up
sleep 10
sv-enable starttether
sv-enable docker
