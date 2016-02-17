#!/bin/bash
export DISPLAY=:0
if (($# != 0)); then
	xrandr --fb $@
else
	xrandr --fb 1400x800
fi
xrandr
x11vnc
exit 0
