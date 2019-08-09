ccflags-y += -I$(srctree)/drivers/misc/mediatek/mddp/include/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/eccci/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/eccci/$(MTK_PLATFORM)

obj-y += ctrl.o

ctrl-y += mddp_if.o mddp_sm.o

ctrl-$(CONFIG_MTK_MD_DIRECT_TETHERING_SUPPORT) += mddpu_sm.o
ifneq (, $(filter y,$(CONFIG_MTK_MDDP_WH_SUPPORT) $(CONFIG_MTK_MCIF_WIFI_SUPPORT)))
ctrl-y += mddpwh_sm.o
endif
ctrl-y += mddp_dev.o

ctrl-y += mddp_ipc.o
