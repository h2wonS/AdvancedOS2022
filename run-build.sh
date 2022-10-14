#!/bin/bash

echo "make x86_64_deconfig"
make x86_64_defconfig
sleep 2
echo "make all"
make all
sleep 2
echo "make modules_install"
sudo make modules_install
sleep 2
echo "make install"
sudo make install
