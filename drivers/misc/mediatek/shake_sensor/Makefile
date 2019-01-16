include $(srctree)/drivers/misc/mediatek/Makefile.custom

# In case the platform does NOT support this type of sensors

obj-y	+= shake.o

ifeq ($(CONFIG_CUSTOM_KERNEL_SHAKE_SENSOR),"OTHER_VENDOR")
obj-y += other_vendor/
endif
