#!/bin/sh

export PATH=/root/cis520/pintos/src/utils:/root/cis520/usr/local/bin:$PATH
export BXSHARE=/root/cis520/usr/local/share/bochs

clear
clear
echo Entering directory /examples ...
cd ../examples/
make

echo Entering directory /userprog ...
cd ../userprog/
make
# Stop for the user to press the enter key
read -rp 'Press enter to continue...' secondyn </dev/tty 

echo Entering directory /userprog/build ...
cd build
pintos-mkdisk filesys.dsk --filesys-size=2;
pintos -f -q;
pintos -p ../../examples/echo -a echo -- -q;
pintos -q run 'echo This is userspace!';
