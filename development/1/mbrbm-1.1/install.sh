#!/bin/sh
if [ -f /etc/hda.mbr ]; then
 echo mbrbm installation failed
 exit 1
fi
if [ ! -f mbrbm.bin ]; then
 echo mbrbm installation failed
 exit 1
fi
cp mbrbm.bin /etc
set -ex
cd /etc
rm -f hda.mbr
dd if=/dev/hda of=hda.mbr bs=1b count=1
if [ ! -f /etc/hda.mbr ]; then
 echo mbrbm installation failed
 exit 1
fi
dd if=mbrbm.bin of=hdambrbm.mbr bs=1 count=438
dd if=hda.mbr bs=1 skip=438 count=74 >>hdambrbm.mbr
MBRSIZE=`ls -l hdambrbm.mbr | awk '{print $5}'`
if [ ! "$MBRSIZE" -eq 512 ]; then
 echo mbrbm installation failed
 exit 1
fi
dd if=hdambrbm.mbr of=/dev/hda bs=1b count=1
# KEEP THE FILE /etc/hda.mbr AS BACKUP OF ORIGINAL MBR
exit
