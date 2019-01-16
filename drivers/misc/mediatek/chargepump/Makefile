include $(srctree)/drivers/misc/mediatek/Makefile.custom

# In case the platform does NOT support this type of sensors
obj-y   +=  chargepump_dummy.o

ifeq ($(CONFIG_LEDS_LM3639),y)
obj-y	+=  lm3639/
endif

ifeq ($(CONFIG_LEDS_LM3632),y)
obj-y	+=  lm3632/
endif
