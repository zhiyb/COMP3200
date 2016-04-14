#!/bin/bash -e

# Colour bar types
testMode=0x80
testMode=0x84
testMode=0xc4
#testMode=0x88
#testMode=0x8c
# B&W squares
testMode=0x82
testMode=0xc2
#testMode=0x92
# None
testMode=0

#x_addr_start=0
#y_addr_start=0
#x_addr_end=0x0a3f	# 2611
#y_addr_end=0x07a3	# 1955
#x_output_size=2392
#y_output_size=1944
#hts=0x0a8c	#0x0b1c	#0x0768	#0x0a96
#vts=0x07b0	#0x07b0	#0x0450	#0x07b0

dmesg -c > /dev/null
sync
echo -e '\e[93mLoading modules...\e[0m'
modprobe -r nvhost_vi
modprobe ov5647_v4l2 test_mode=$testMode
#modprobe ov5647_v4l2 test_mode=$testMode \
#	x_addr_start=$x_addr_start y_addr_start=$y_addr_start \
#	x_addr_end=$x_addr_end y_addr_end=$y_addr_end \
#	x_output_size=$x_output_size y_output_size=$y_output_size \
#	hts=$hts vts=$vts
#	hts=0x0a8c vts=0x07b0
modprobe soc_camera
modprobe tegra_camera
dmesg -l debug
dmesg -c

sync
read
echo -e '\e[93mdmesg:\e[0m'
dmesg -c

sync
read
echo -e '\e[93mUnloading modules...\e[0m'
modprobe -r tegra_camera
modprobe -r soc_camera
modprobe -r ov5647_v4l2
dmesg -c
sync
