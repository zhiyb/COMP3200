#!/bin/bash
echo 57 > /sys/class/gpio/export
echo out > /sys/class/gpio/gpio57/direction
echo 0 > /sys/class/gpio/gpio57/value
