include $(srctree)/drivers/misc/mediatek/Makefile.custom
ifneq ($(filter y,$(CONFIG_TRUSTONIC_TEE_SUPPORT) $(CONFIG_MTK_PSCI) $(CONFIG_ARM_PSCI)),)
obj-y   += l2c_share_normal.o
else
obj-y   += l2c_share_sec.o
endif
