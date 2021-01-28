ccflags-y += -I$(srctree)/drivers/misc/mediatek/mddp/include/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/eccci/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/eccci/$(MTK_PLATFORM)/

obj-$(CONFIG_MTK_MDDP_SUPPORT) += mddp.o

mddp-y	+= ctrl/mddp_if.o
mddp-y	+= ctrl/mddp_sm.o
mddp-$(CONFIG_MTK_MDDP_USB_SUPPORT) += ctrl/mddpu_sm.o
mddp-y	+= ctrl/mddp_dev.o
mddp-y	+= ctrl/mddp_ipc.o

ifneq (,$(filter y,$(CONFIG_MTK_MDDP_USB_SUPPORT) $(CONFIG_MTK_MDDP_WH_SUPPORT)))
subdir-ccflags-y += -DMDDP_TETHERING_SUPPORT
mddp-y	+= ctrl/mddpwh_sm.o
mddp-y	+= filter/mddp_filter.o
mddp-y	+= filter/mddp_f_dev.o
# Data usage
mddp-y	+= usage/mddp_usage.o
endif
