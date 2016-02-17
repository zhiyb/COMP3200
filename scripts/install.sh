#!/bin/bash -e
make -j4 modules
[ "$1" = "zImage" ] && make -j4 zImage
sudo rm -rf /lib/modules/`uname -r`/kernel
sudo make -j4 modules_install
[ "$1" = "zImage" ] && cp -a arch/arm/boot/zImage /boot/zImage_ov
sync
