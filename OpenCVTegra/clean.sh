#!/bin/bash
tar cvf adaptive.tar adaptive/* adaptive.log
#tar cvf output.tar output/* output.log
wc -l adaptive.log
rm -f output/* output.log adaptive/* adaptive.log
