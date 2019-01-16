include $(srctree)/drivers/misc/mediatek/Makefile.custom

# In case the platform does NOT support this type of sensors

obj-y	+= face_down.o

ifeq ($(CONFIG_CUSTOM_KERNEL_FACE_DOWN_SENSOR),"OTHER_VENDOR")
obj-y += other_vendor/
endif
