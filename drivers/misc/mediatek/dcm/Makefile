ccflags-y += -I$(srctree)/drivers/misc/mediatek/dcm/include/

# Core
obj-$(CONFIG_MTK_DCM_COMMON)	+= mtk_dcm.o

# SoC Drivers
obj-$(CONFIG_MTK_DCM_MT6873) += mt6873_dcm.o
mt6873_dcm-objs := mt6873_dcm_internal.o mt6873_dcm_autogen.o
