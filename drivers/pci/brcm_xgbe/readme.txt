         Broadcom BCM8956X/BCM8957X PCIe Linux Sample Driver
                          Version 1.2.1
                             
                          Broadcom Inc.
                         15101 Alton Pkway 
                     Irvine, California  92618

                  Copyright (C) 2023 Broadcom Inc.
	                All Rights Reserved


Introduction
============
The Broadcom sample Linux driver supports the BCM8956X/BCM8957X/BCM8989X devices.
This document provides information on the compilation and installation of the brcm-xgbe sample driver.

Limitations
===========

The current version of the driver has been tested on Linux kernel versions 5.4.x, 5.11.x, 5.13.x in Ubuntu 18.04 and 20.04 x86_64 environments. 
The driver may not compile cleanly on different kernel versions without minor changes to the source files and Makefile.

Building the driver requires that the user have admninistrative privledges on the build system.
Building the driver requires that the Linux system be udated with kernel development tools.

For example, installing the kernel development tools on Ubuntu might be done with the following command: 
sudo apt-get install build-essential checkinstall 

Network tools on Ubuntu can be installed with the following command:
sudo apt-get install net-tools
sudo apt-get install ethtool

Packaging
=========
The driver is released as a source code package
The packaged files include:

LICENSE
Makefile
readme.txt
xgbe-bcmutil.c
xgbe-bcmutil.h
xgbe-common.h
xgbe-dcb.c
xgbe-debugfs.c
xgbe-desc.c
xgbe-dev.c
xgbe-drv.c
xgbe-eli.c
xgbe-ethtool.c
xgbe.h
xgbe-main.c
xgbe-mbx.c
xgbe-mbx.h
xgbe-pci.c
xgbe-phy.c
xgbe-ptp.c
xgbe-sriov.c
xgbe-usr-opt.h

Driver Build
============
Copy the Source Code Package to a selected build directory.
go to the brcm_xgbe dir and build the driver using the following command:
make  -C /lib/modules/$(uname -r)/build M=$(pwd) KCFLAGS=-D<DRIVER> modules

<DRIVER>: can be one of these: PF_DRIVER, VF_DRIVER

By default for PF driver will be built with ELI. 
VF driver will always be built without ELI.
ELI option has to be used while building PF_DRIVER if VF Functionality has to be used.

To choose the non-eli option add the following flag to command line
KCFLAGS+=-DNO_ELI

To choose the driver with broadcom flexible header add the following command line flag 
KCFLAGS+=-DFLEX_HEADER
By Default Driver is built without Flex Header

The brcm-xgbe.ko Linux kernel module driver is built in the directory.

Installing and loading the driver
=================================
To install the driver onto the kernel use the "insmod" command.
e.g. sudo insmod ./brcm-xgbe.ko

Check that the driver installed properly. The bash "dmesg" command and bash lsmod command will help. 
e.g. sudo lsmod |grep xgbe

Linux commands ifconfig -a or ip l will list the system assigned name for device interface 
e.g. ip l
	ens03
 
give the interface mac address and an ip address using following commands:
sudo ip l set address 00.10.12.34.56.78 dev ens03
sudo ip address add 192.100.10.3 dev ens03

bring the interface up 
sudo ip l s ens03 up

The device is ready to pass traffic

Unloading and Removing the Driver
=================================
To unload the driver, use ifconfig or ip l to bring down the device interface
e.g. sudo ip l s ens03 down
 
then uninstall the driver from the kernel:
sudo rmmod brcm-xgbe

Register Read/Write and ELI Dump with Kernel Debug FS
===========================================
The Debug FS entry is created @ /sys/kernel/debug/brcm-xgbe-<INTF NAME>.
List of files depends on the CHIP detected and ELI Mode being selected at compile time.
Files created are
  xgmac_register
  xgmac_register_value
  misc_register
  misc_register_value
  eli_dump >> File is created when NO_ELI option is not selected at compile time.
  phy_devad >> File is created if the chip detected is BCM8989X
  phy_mdio_addr >> File is created if the chip detected is BCM8989X
  phy_mdio_addr_value >> File is created if the chip detected is BCM8989X

How to Read XGMAC Register using debugfs.
echo <reg_addr> > xgamc_register;cat xgmac_register_value

How to Write XGMAC Register using debugfs.
echo <reg_addr> > xgamc_register;echo <value> > xgmac_register_value

How to Read Misc Register using debugfs.
echo <reg_addr> > misc_register;cat misc_register_value

How to Write MISC Register using debugfs.
echo <reg_addr> > misc_register;echo <value> > misc_register_value

ELI Dump when NO_ELI option is not selected at compile time.

IDX is the Index in the TCAM.
DEST-MAC, OVT, IVT is the MAC, Outer VLAN Tag, Inner VLAN Tag.
DMA-CH-MAP is a Bit map of the DMA's being used.

Sample Output:
cat eli_dump
IDX          DEST-MAC            OVT     IVT    FUNC RAM        DMA-CH-MAP
000     FF:FF:FF:FF:FF:FF       0000    0000    00000000        2222
001     01:00:5E:00:00:01       0000    0000    00000001        4444
002     01:00:5E:00:00:FB       0000    0000    00000002        4444
003     33:33:00:00:00:01       0000    0000    00000003        4444
004     33:33:FF:D6:FC:3B       0000    0000    00000004        8004
005     33:33:00:00:00:FB       0000    0000    00000005        4444
006     01:80:C2:00:00:21       0000    0000    00000006        4444
007     33:33:FF:44:55:20       0000    0000    00000007        8004
008     EF:00:00:0A:00:00       0000    0000    00000008        4444
009     33:33:FF:E7:3D:7C       0000    0000    00000009        4440
010     33:33:FF:44:55:11       0000    0000    00000010        8040
011     33:33:FF:44:55:12       0000    0000    00000011        8400
012     33:33:FF:44:55:13       0000    0000    00000012        4000
046     01:00:00:00:00:00       0000    0000    00000046        8000
047     01:00:00:00:00:00       0000    0000    00000047        8000
048     00:22:33:44:55:20       0000    0020    00000048        8002
049     00:22:33:44:55:20       0000    0010    00000049        8004
050     00:22:33:44:55:20       0000    0030    00000050        8008
051     00:22:33:44:55:11       0000    0010    00000051        8020
052     00:22:33:44:55:11       0000    0020    00000052        8040
053     00:22:33:44:55:11       0000    0030    00000053        8080
054     00:22:33:44:55:12       0000    0010    00000054        8200
055     00:22:33:44:55:12       0000    0020    00000055        8400
056     00:22:33:44:55:12       0000    0030    00000056        8800
057     00:22:33:44:55:13       0000    0010    00000057        2000
058     00:22:33:44:55:13       0000    0020    00000058        4000
059     00:22:33:44:55:13       0000    0030    00000059        8000
060     00:22:33:44:55:13       0000    0000    00000060        1000
061     00:22:33:44:55:12       0000    0000    00000061        8100
062     00:22:33:44:55:11       0000    0000    00000062        8010
063     00:22:33:44:55:20       0000    0000    00000063        8001


