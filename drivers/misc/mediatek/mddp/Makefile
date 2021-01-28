ccflags-y += -I$(srctree)/drivers/misc/mediatek/mddp/include/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/eccci/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/eccci/$(MTK_PLATFORM)/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/

obj-$(CONFIG_MTK_MDDP_SUPPORT) += mddp.o

ifneq (,$(filter y,$(CONFIG_MTK_MCIF_WIFI_SUPPORT)))
mddp-y	+= ctrl/mddp_if.o
mddp-y	+= ctrl/mddp_sm.o
mddp-$(CONFIG_MTK_MDDP_USB_SUPPORT) += ctrl/mddpu_sm.o
mddp-y	+= ctrl/mddpwh_sm.o
mddp-y	+= ctrl/mddp_dev.o
mddp-y	+= ctrl/mddp_ipc.o
else
mddp-y	+= ctrl/mddp_dummy.o
endif

ifneq (,$(filter y,$(CONFIG_MTK_MDDP_USB_SUPPORT) $(CONFIG_MTK_MDDP_WH_SUPPORT)))
subdir-ccflags-y += -DMDDP_TETHERING_SUPPORT
mddp-y	+= filter/mddp_filter.o
mddp-y	+= filter/mddp_f_dev.o
# Data usage
mddp-y	+= usage/mddp_usage.o
endif
