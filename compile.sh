cd edk2
source edksetup.sh
make -C BaseTools -j$(nproc)
build 
build -p LzyPkg/LzyPkg.dsc