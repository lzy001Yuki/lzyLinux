qemu-system-x86_64 \
-kernel linux-5.15.178/arch/x86/boot/bzImage \
-initrd busybox-1.35.0/initramfs.cpio.gz \
-nographic \
-append "init=/init console=ttyS0"