#!/bin/bash -e
width=${width:-2592}
height=${height:-1944}
frames=${frames:-10}
cd /home/ubuntu/COMP3200/Bayer
/home/ubuntu/bin/yavta /dev/video0 -c$frames -n3 -s${width}x${height} -fSBGGR10 -Fov5647.raw
#tar zcvf ov5647.tar.gz ov5647.raw-*.bin
#rm -f ov5647.raw-*.bin
#scp -prBC ov5647.tar.gz yz39g13@uglogin:public_html/extern/OV5647/
#rm -f ov5647.tar.gz
sudo -u ubuntu make -j4 WIDTH=$width HEIGHT=$height
#sudo -u ubuntu make -j4 WIDTH=$width HEIGHT=$height png
#sudo -u ubuntu make -j4 WIDTH=$width HEIGHT=$height png_raw
