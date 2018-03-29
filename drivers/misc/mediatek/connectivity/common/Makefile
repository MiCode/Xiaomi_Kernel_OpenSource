subdir-ccflags-y += -Werror -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include
subdir-ccflags-y += -Werror -I$(srctree)/drivers/misc/mediatek/include/mt-plat

ifneq ($(filter "MT6628",$(CONFIG_MTK_COMBO_CHIP)),)
	subdir-ccflags-y += -D MT6628
	subdir-ccflags-y += -D MERGE_INTERFACE_SUPPORT
endif
ifneq ($(filter "MT6630",$(CONFIG_MTK_COMBO_CHIP)),)
	subdir-ccflags-y += -D MT6630
ifneq ($(CONFIG_ARCH_MT2601),y)
	subdir-ccflags-y += -D MERGE_INTERFACE_SUPPORT
endif
endif

ifneq ($(filter "MT6632",$(CONFIG_MTK_COMBO_CHIP)),)
	subdir-ccflags-y += -D MT6632
	subdir-ccflags-y += -D MERGE_INTERFACE_SUPPORT
endif

obj-y   +=  common_main/
obj-y   +=  common_detect/
