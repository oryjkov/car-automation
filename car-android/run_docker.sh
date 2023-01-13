#!/bin/bash
mount -t tmpfs -o mode=755 tmpfs /sys/fs/cgroup
mkdir -p /sys/fs/cgroup/devices
mount -t cgroup -o devices cgroup /sys/fs/cgroup/devices
dockerd --iptables=false 1>/dev/null 2>/dev/null &
