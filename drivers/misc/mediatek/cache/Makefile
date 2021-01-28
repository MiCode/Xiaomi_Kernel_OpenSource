ifeq ($(CONFIG_ARM64), y)
obj-$(CONFIG_MEDIATEK_CACHE_API) += cache-mtk-aarch64.o
else
subdir-asflags-y += -I$(srctree)/arch/arm/mm/
obj-$(CONFIG_MEDIATEK_CACHE_API) += cache-mtk-aarch32.o
endif
