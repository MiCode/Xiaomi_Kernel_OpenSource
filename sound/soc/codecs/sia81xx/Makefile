# SPDX-License-Identifier: GPL-2.0

ifeq ($(CONFIG_SND_SOC_SIA8109),y)

COMPILE_TO_MODULE = n
EN_SIA81XX_TUNING = y
TUNING_PLATFORM = mtk

snd-soc-sia81xx-objs := \
		$(MTK_PROJECT)/sia81xx.o \
		$(MTK_PROJECT)/sia81xx_regmap.o \
		$(MTK_PROJECT)/sia81xx_aux_dev_if.o \
		$(MTK_PROJECT)/sia81xx_set_vdd.o \
		$(MTK_PROJECT)/sia8101_regs.o \
		$(MTK_PROJECT)/sia8108_regs.o \
		$(MTK_PROJECT)/sia8109_regs.o \
		$(MTK_PROJECT)/sia81xx_timer_task.o \
		
ifeq ($(EN_SIA81XX_TUNING),y)

ccflags-y += -DSIA81XX_TUNING

snd-soc-sia81xx-objs += $(MTK_PROJECT)/sia81xx_socket.o

ifeq ($(TUNING_PLATFORM),mtk)
ccflags-y += -DVENDOR_EDIT
snd-soc-sia81xx-objs += $(MTK_PROJECT)/sia81xx_tuning_if_dev.o
endif

endif

ifeq ($(COMPILE_TO_MODULE),y)
ccflags-y += -DSIA81XX_COMPILE_TO_MODULE
obj-m += snd-soc-sia81xx.o
else
obj-y += snd-soc-sia81xx.o
endif

endif
