# SIA PA object

COMPILE_TO_MODULE = y
PLATFORM_TYPE = mtk
EXTRA_CFLAGS += $(call cc-option,-Wno-date-time,)


snd-soc-sipa-objs := \
		sipa.o \
		sipa_regmap.o \
		sipa_aux_dev_if.o \
		sipa_91xx.o \
		sipa_parameter.o

snd-soc-sipa-tuning-objs := sipa_tuning_misc.o

ifeq ($(PLATFORM_TYPE),qcom_elite)
snd-soc-sipa-tuning-objs += sipa_afe.o
endif

ifeq ($(PLATFORM_TYPE),mtk)
snd-soc-sipa-tuning-objs += sipa_tuning_if.o # sipa_tuning_if_dev.o # sia81xx_ipi.o
endif

ifeq ($(PLATFORM_TYPE),qcom_spf)
snd-soc-sipa-tuning-objs += sipa_tuning_if.o # sipa_tuning_if_dev.o # sia81xx_ipi.o
endif

ifeq ($(COMPILE_TO_MODULE),y)
EXTRA_CFLAGS += -DSIPA_COMPILE_TO_MODULE
obj-m += snd-soc-sipa.o
obj-m += snd-soc-sipa-tuning.o
else
obj-y += snd-soc-sipa.o
obj-y += snd-soc-sipa-tuning.o
endif

