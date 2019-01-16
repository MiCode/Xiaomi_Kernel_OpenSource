include $(srctree)/drivers/misc/mediatek/Makefile.custom

# In case the platform does NOT support this type of sensors

obj-y += SCP_sensorHub/
obj-y += SCP_shf/

ifeq ($(CONFIG_CUSTOM_KERNEL_SENSORHUB),"OTHER_VENDOR")
obj-y += other_vendor/
endif
