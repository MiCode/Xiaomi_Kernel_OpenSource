ccflags-y += -I$(srctree)/drivers/misc/mediatek/dcm/include/

# Core
obj-$(CONFIG_MTK_DCM_COMMON)	+= mtk_dcm.o

# SoC Drivers
obj-$(CONFIG_MTK_DCM_MT6873) += mt6873_dcm.o
mt6873_dcm-objs := mt6873_dcm_internal.o mt6873_dcm_autogen.o
obj-$(CONFIG_MTK_DCM_MT6853) += mt6853_dcm.o
mt6853_dcm-objs := mt6853_dcm_internal.o mt6853_dcm_autogen.o
obj-$(CONFIG_MTK_DCM_MT6983) += mt6983_dcm.o
mt6983_dcm-objs := mt6983_dcm_internal.o mt6983_dcm_autogen.o
obj-$(CONFIG_MTK_DCM_MT6879) += mt6879_dcm.o
mt6879_dcm-objs := mt6879_dcm_internal.o mt6879_dcm_autogen.o
