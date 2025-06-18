cd linux-5.15.178
make -j$(nproc)

cd ..
bash qemu.sh