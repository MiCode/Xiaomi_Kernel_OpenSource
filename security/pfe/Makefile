#
# Makefile for the MSM specific security device drivers.
#

ccflags-y += -Isecurity/selinux -Isecurity/selinux/include -Ifs/ecryptfs

obj-$(CONFIG_PFT) += pft.o
obj-$(CONFIG_PFK) += pfk.o pfk_kc.o pfk_ice.o
