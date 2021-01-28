# SPDX-License-Identifier: GPL-2.0
dtb-$(CONFIG_ARCH_MEDIATEK) += auto2712p1v1-ivi-boot.dtb
dtb-$(CONFIG_ARCH_MEDIATEK) += auto2712p1v1-ivi-nand-boot.dtb
DTB_PLATFORM :=  $(subst $\",,$(CONFIG_MTK_PLATFORM))
ifneq ($(DTB_PLATFORM),)
dtb-$(CONFIG_ARCH_MEDIATEK) += $(DTB_PLATFORM).dtb
endif
DTB_PROJECT := $(subst $\",,$(CONFIG_ARCH_MTK_PROJECT))
ifneq ($(DTB_PROJECT),)
dtb-$(CONFIG_ARCH_MEDIATEK) += $(DTB_PROJECT).dtb
endif
