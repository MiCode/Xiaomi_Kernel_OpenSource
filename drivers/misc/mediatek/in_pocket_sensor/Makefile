include $(srctree)/drivers/misc/mediatek/Makefile.custom

# In case the platform does NOT support this type of sensors

obj-y	+= in_pocket.o

ifeq ($(CONFIG_CUSTOM_KERNEL_IN_POCKET_SENSOR),"OTHER_VENDOR")
obj-y += other_vendor/
endif
