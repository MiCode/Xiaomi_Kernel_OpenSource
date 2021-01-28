
ccflags-y += -I$(srctree)/drivers/misc/mediatek/dcm/include/
#ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include/dcm_v1/$(MTK_PLATFORM)/
#ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/$(MTK_PLATFORM)/
#ccflags-y += -I$(srctree)/drivers/misc/mediatek/dramc/$(MTK_PLATFORM)/
#ccflags-y += -I$(srctree)/drivers/misc/mediatek/dcm/include/

# Core
obj-$(CONFIG_MTK_DCM_COMMON)	+= mtk_dcm.o

# SoC Drivers
obj-$(CONFIG_MTK_DCM_MT6779) += mt6779_dcm.o
mt6779_dcm-objs := mt6779_dcm_internal.o mt6779_dcm_autogen.o
obj-$(CONFIG_MTK_DCM_MT6761) += mt6761_dcm.o
mt6761_dcm-objs := mt6761_dcm_internal.o mt6761_dcm_autogen.o
obj-$(CONFIG_MTK_DCM_MT6765) += mt6765_dcm.o
mt6765_dcm-objs := mt6765_dcm_internal.o mt6765_dcm_autogen.o
