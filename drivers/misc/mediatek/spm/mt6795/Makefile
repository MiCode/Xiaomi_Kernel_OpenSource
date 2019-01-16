include $(srctree)/drivers/misc/mediatek/Makefile.custom

ccflags-y += -I$(MTK_CUSTOM_PATH)/i2c/$(MTK_PLATFORM)

obj-y	+= mt_spm_internal.o

ifeq ($(CONFIG_ARM64), y)
obj-y	+= mt_spm_64.o
obj-y	+= mt_spm_fs_64.o
obj-y	+= mt_spm_sleep_64.o
obj-y	+= mt_spm_dpidle_64.o
obj-y	+= mt_spm_sodi_64.o
obj-y	+= mt_spm_mcdi_64.o
obj-y	+= mt_spm_talking_64.o
obj-y	+= mt_spm_ddrdfs_64.o
else
obj-y	+= mt_spm.o
obj-y	+= mt_spm_fs.o
obj-y	+= mt_spm_sleep.o
obj-y	+= mt_spm_dpidle.o
obj-y	+= mt_spm_sodi.o
obj-y	+= mt_spm_mcdi.o
obj-y	+= mt_spm_talking.o
obj-y	+= mt_spm_ddrdfs.o
endif
