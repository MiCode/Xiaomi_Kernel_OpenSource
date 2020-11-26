# SIA81xx PA object

COMPILE_TO_MODULE = n
EN_SIA81XX_TUNING = y
TUNING_PLATFORM = mtk

snd-soc-sia81xx-objs := \
		sia81xx.o \
		sia81xx_regmap.o \
		sia81xx_aux_dev_if.o \
		sia81xx_set_vdd.o \
		sia8101_regs.o \
		sia8108_regs.o \
		sia8109_regs.o \
		sia81xx_timer_task.o \
		
ifeq ($(EN_SIA81XX_TUNING),y)

EXTRA_CFLAGS += -DSIA81XX_TUNING

snd-soc-sia81xx-objs += sia81xx_socket.o

ifeq ($(TUNING_PLATFORM),qcom)
snd-soc-sia81xx-objs += sia81xx_afe.o
endif

ifeq ($(TUNING_PLATFORM),mtk)
snd-soc-sia81xx-objs += sia81xx_tuning_if_dev.o
endif

endif

ifeq ($(COMPILE_TO_MODULE),y)
EXTRA_CFLAGS += -DSIA81XX_COMPILE_TO_MODULE
obj-m += snd-soc-sia81xx.o
else
obj-y += snd-soc-sia81xx.o
endif
