obj-$(CONFIG_MTK_VIDEO_LM3643)     += lm3643.o
obj-$(CONFIG_MTK_VIDEO_LM3644)     += lm3644.o
MTK_CUSTOM_PATH := $(srctree)/drivers/misc/mediatek
subdir-ccflags-y += \
	-I$(MTK_CUSTOM_PATH)/flashlight

ifneq ($(wildcard $(MTK_CUSTOM_PATH)/flashlight/v4l2/k6983v1_64_alpha),)
obj-$(CONFIG_MTK_VIDEO_SY7806)     += k6983v1_64_alpha/sy7806.o
endif

ifneq ($(wildcard $(MTK_CUSTOM_PATH)/flashlight/v4l2/k6895v1_64_95t_alpha),)
obj-$(CONFIG_MTK_VIDEO_KTD2687)    += k6895v1_64_95t_alpha/ktd2687.o
endif
