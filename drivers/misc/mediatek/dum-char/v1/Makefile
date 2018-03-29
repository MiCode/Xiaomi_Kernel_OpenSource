MTK_PLATFORM := $(subst ",,$(CONFIG_MTK_PLATFORM))
ARCH_MTK_PROJECT := $(subst ",,$(CONFIG_ARCH_MTK_PROJECT))
ccflags-y += -I$(srctree)/drivers/misc/mediatek/nand/v1/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/mach/$(MTK_PLATFORM)/$(ARCH_MTK_PROJECT)/nand/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/mach/

ifeq ($(CONFIG_MTK_MMC),y)
	CONFIG_MTK_USIF = y
else
	ifeq ($(CONFIG_MTK_MTD_NAND),y)
	CONFIG_MTK_USIF = y
	else
	CONFIG_MTK_USIF = n
	endif
endif

obj-$(CONFIG_MTK_USIF) := DumChar.o
DumChar-objs :=  dumchar.o env.o
