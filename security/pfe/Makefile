#
# Makefile for the MSM specific security device drivers.
#

ccflags-y += -Isecurity/selinux -Isecurity/selinux/include -Ifs/ecryptfs
ccflags-y += -Ifs/ext4 -Ifs/f2fs

obj-$(CONFIG_PFT) += pft.o
obj-$(CONFIG_PFK) += pfk.o pfk_kc.o pfk_ice.o pfk_ext4.o pfk_f2fs.o pfk_ecryptfs.o
