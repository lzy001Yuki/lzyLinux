cd busybox-1.35.0/_install
mkdir -p proc sys dev tmp mnt
cat > init << 'EOF'
#!/bin/sh
mount -t proc none /proc
mount -t sysfs none /sys
mount -t tmpfs none /tmp
mount -t devtmpfs none /dev
echo "Hello Linux!"

echo "Ensuring /dev/fuse exists..."

# Double-check if the device node was created by devtmpfs.
# If not, create it manually. This is our key step.
if [ ! -c /dev/fuse ]; then
  echo "/dev/fuse not found, creating manually."
  # mknod <name> <type> <major> <minor>
  # For FUSE, type is 'c' (character device), major is 10, minor is 229.
  mknod /dev/fuse c 10 229
fi

# 3. Set proper permissions for /dev/fuse
# This allows any user (including root) to access the FUSE device.
echo "Setting permissions for /dev/fuse..."
chmod 666 /dev/fuse

# For debugging, show the final state of /dev/fuse
ls -l /dev/fuse
# Create the mount point for our RAM FS
/bin/mkdir /mnt/ramfs

# Launch our user-space filesystem in the background
echo "Starting ramfs daemon..."
/bin/ramfs /mnt/ramfs &

# Wait a moment for FUSE to initialize
sleep 1

echo "ramfs mounted. Starting shell..."

# Start an interactive shell
# This will be our main interface to the system

sh
poweroff -f
EOF
chmod +x init

cp ../../ramfs.sh bin/
cp ../../test2.sh bin/
chmod +x bin/ramfs.sh
chmod +x bin/test2.sh

find . -print0 | cpio --null -ov --format=newc | gzip -9 > ../initramfs.cpio.gz
