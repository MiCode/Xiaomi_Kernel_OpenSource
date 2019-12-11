#
# Makefile for the MSM specific security device drivers.
#

ccflags-y += -Isecurity/selinux -Isecurity/selinux/include
ccflags-y += -Ifs/crypto
ccflags-y += -Idrivers/misc

obj-$(CONFIG_PFT) += pft.o
obj-$(CONFIG_PFK) += pfk.o pfk_kc.o pfk_ext4.o pfk_f2fs.o
ifdef CONFIG_PFK_VIRTUALIZED
obj-$(CONFIG_PFK_VIRTUALIZED) += pfk_ice_virt.o
else
obj-$(CONFIG_PFK) += pfk_ice.o
endif
