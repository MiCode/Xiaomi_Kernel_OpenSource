include $(srctree)/drivers/misc/mediatek/Makefile.custom
ifeq ($(CONFIG_MTK_EEMCS_DRIVER), y)

#include $(MTK_ROOT_BUILD)/Makefile
#EEMCS_PLATFORM_DRIVER_DIR :=  $(srctree)/mediatek/platform/$(call lc,$(MTK_PLATFORM))/kernel/drivers/eemcs
#ifeq ($(CCCI_PLATFORM_DRIVER_DIR),)
#$(error Invalid EEMCS platform path, please check)
#endif

obj-y += $(subst ",,$(CONFIG_MTK_PLATFORM))/

obj-y	:= eemcs.o
eemcs-y	:= eemcs_init.o eemcs_debug.o eemcs_ccci.o eccmni.o eemcs_char.o eemcs_expt.o eemcs_expt_ut.o
eemcs-y += eemcs_ipc.o
eemcs-y += eemcs_sysmsg.o
eemcs-y += eemcs_rpc.o eemcs_rpc_ut.o 
eemcs-y += eemcs_boot_trace.o eemcs_md.o eemcs_boot.o eemcs_sysfs.o eemcs_state.o eemcs_fs_ut.o
eemcs-y += lte_hif_sdio.o lte_df_main.o lte_dev_sdio_hal.o lte_main.o lte_expt.o
eemcs-y += eemcs_file_ops.o
eemcs-y += eemcs_statistics.o

clean-files += $(patsubst $(obj)/%,%,$(wildcard $(obj)/*~))

#ccflags-y += -I$(srctree)/../bionic/libc/kernel/common/linux  -I$(srctree)/$(obj)/include  -I$(EEMCS_PLATFORM_DRIVER_DIR)/include
ccflags-y += -I$(srctree)/$(obj)/include
#ccflags-y += -I$(MTK_ROOT)/kernel/drivers/conn_md/include/
ccflags-y += -I$$(srctree)/drivers/misc/mediatek/conn_md/include/

ccflags-y += -DTEST_DRV=1
# move to lte_df_main.h to solve compile error
#ccflags-y += -DIMMEDIATE_RELOAD_DL_SKB=1

ccflags-y += -DEMCS_SDIO_DRVTST=0
 
ifeq ($(CONFIG_MTK_SECURITY_SW_SUPPORT), y)
ifeq ($(CONFIG_MTK_SEC_SECRO_AC_SUPPORT), y)
ccflags-y  += -DENABLE_MD_SECURITY_RO_FEATURE
endif
ifeq ($(CONFIG_MTK_SEC_MODEM_ENCODE), y)
ccflags-y  += -DENABLE_MD_IMG_SECURITY_FEATURE
endif
endif
ifeq ($(CONFIG_MTK_SEC_MODEM_NVRAM_ANTI_CLONE), y)
ccflags-y  += -DMTK_SEC_MODEM_NVRAM_ANTI_CLONE
endif

else
obj- :=dummy.o
endif
