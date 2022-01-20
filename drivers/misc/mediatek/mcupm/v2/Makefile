obj-y += mcupm_driver.o
obj-y += mcupm_timesync.o

ccflags-y += -I$(srctree)/drivers/misc/mediatek/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/mcupm/$(MTK_PLATFORM)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/$(CONFIG_MTK_PLATFORM)
ccflags-$(CONFIG_MTK_ENG_BUILD) += -DDEBUG
