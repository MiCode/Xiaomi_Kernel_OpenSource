ccflags-y += \
	-I$(srctree)/drivers/misc/mediatek/sspm/ \
	-I$(srctree)/drivers/misc/mediatek/include/mt-plat/ \
	-I$(srctree)/drivers/misc/mediatek/dramc/$(MTK_PLATFORM) \

mtk-dvfsrc-help-objs += dvfsrc.o dvfsrc-common.o dvfsrc-ipi.o
mtk-dvfsrc-debug-objs += dvfsrc-debug.o dvfsrc-debug-v79.o dvfsrc-debug-v61.o dvfsrc-sysfs.o
mtk-dvfsrc-met-objs += dvfsrc-met.o dvfsrc-met-v79.o dvfsrc-met-v61.o

obj-$(CONFIG_MTK_DVFSRC) += mtk-dvfsrc-help.o
obj-$(CONFIG_MTK_DVFSRC) += mtk-dvfsrc-debug.o
obj-$(CONFIG_MTK_DVFSRC) += mtk-dvfsrc-met.o
obj-$(CONFIG_MTK_DVFSRC) += dvfsrc-boost.o

ifneq ($(CONFIG_MACH_MT6761),)
obj-$(CONFIG_MTK_DVFSRC) += dvfsrc-opp-mt6761.o
endif
