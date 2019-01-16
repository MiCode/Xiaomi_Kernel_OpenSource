include $(srctree)/drivers/misc/mediatek/Makefile.custom


ccflags-y += -I$(MTK_PROJECT_PATH_ROOT)/touchpanel/synaptics_3528/

# Linux driver folder
ccflags-y += -I$(srctree)/drivers/input/touchscreen/mediatek/$(MTK_PLATFORM)/$(ARCH_MTK_PROJECT)/synaptics_3528/
ccflags-y += -I$(srctree)/drivers/input/touchscreen/mediatek/


ifeq ($(CONFIG_MTK_PLATFORM),"mt6582")
    ccflags-y += -DMT6582
endif
obj-y += RefCode_CustomerImplementation.o
obj-y += RefCode_F54_FullRawCap.o
obj-y += RefCode_F54_HighResistance.o
obj-y += RefCode_F54_RxOpenReport.o
obj-y += RefCode_F54_RxToRxReport.o
obj-y += RefCode_F54_TxOpenReport.o
obj-y += RefCode_F54_TxToGndReport.o
obj-y += RefCode_F54_TxToTxReport.o
obj-y += RefCode_FWCheck.o
obj-y += RefCode_PDTScan.o
obj-y   +=  s3528_driver.o
obj-y   +=  s3528_fw_update.o


