#!/usr/bin/env bash

/home/chris/school-projects/601-os/os161/kern/conf/config DUMBVM

cd /home/chris/school-projects/601-os/os161/kern/compile/DUMBVM
bmake depend
bmake
bmake install

cd ~/school-projects/601-os/root
sys161 kernel
