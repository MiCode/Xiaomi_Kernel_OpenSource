# SIA PA object

COMPILE_TO_MODULE = y
EN_SIPA_TUNING = n
DEVICE_TYPE_DIGITAL = n
PLATFORM_TYPE = mtk
EXTRA_CFLAGS += $(call cc-option,-Wno-date-time,)

ifeq ($(DEVICE_TYPE_DIGITAL),y)
EXTRA_CFLAGS += -DSIA91XX_TYPE

ifeq ($(PLATFORM_TYPE),mtk)
EXTRA_CFLAGS += -DPLATFORM_TYPE_MTK
subdir-ccflags-y += -I$(srctree)/sound/soc/mediatek/common
endif

ifeq ($(PLATFORM_TYPE),qcom)
EXTRA_CFLAGS += -DPLATFORM_TYPE_QCOM
endif

endif

snd-soc-sipa-objs := \
		sipa.o \
		sipa_regmap.o \
		sipa_aux_dev_if.o \
		sipa_set_vdd.o \
		sia91xx_common.o \
		sipa_parameter.o \
		sipa_tuning_cmd.o \
		sipa_cal_spk.o \
		sipa_timer_task.o \

ifeq ($(EN_SIPA_TUNING),y)
EXTRA_CFLAGS += -DSIPA_TUNING
snd-soc-sipa-objs += sipa_socket.o
endif

ifeq ($(PLATFORM_TYPE),qcom)
snd-soc-sipa-objs += sipa_afe.o
endif

ifeq ($(PLATFORM_TYPE),mtk)
snd-soc-sipa-objs += sipa_tuning_if_dev.o
endif

ifeq ($(COMPILE_TO_MODULE),y)
EXTRA_CFLAGS += -DSIPA_COMPILE_TO_MODULE
obj-m += snd-soc-sipa.o
else
obj-y += snd-soc-sipa.o
endif
