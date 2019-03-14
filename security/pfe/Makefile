# SPDX-License-Identifier: GPL-2.0
ccflags-y += -Isecurity/selinux -Isecurity/selinux/include
ccflags-y += -Ifs/crypto
ccflags-y += -Idrivers/misc

obj-$(CONFIG_PFT) += pft.o
obj-$(CONFIG_PFK) += pfk.o pfk_kc.o pfk_ice.o pfk_ext4.o pfk_f2fs.o
