obj-$(CONFIG_MTK_MDDP_SUPPORT) += mddp.o

ifneq (,$(filter y,$(CONFIG_MTK_MCIF_WIFI_SUPPORT)))
ccflags-y += -I$(srctree)/drivers/misc/mediatek/mddp/include/
mddp-y	+= ctrl/mddp_if.o
mddp-y	+= ctrl/mddp_sm.o
mddp-y	+= ctrl/mddpwh_sm.o
mddp-y	+= ctrl/mddp_dev.o
mddp-y	+= ctrl/mddp_ipc.o
else
mddp-y	+= ctrl/mddp_dummy.o
endif

ifneq (,$(filter y,$(CONFIG_MTK_MDDP_WH_SUPPORT)))
mddp-y	+= filter/mddp_filter.o
mddp-y	+= filter/mddp_f_dev.o
# Data usage
mddp-y	+= usage/mddp_usage.o
endif
