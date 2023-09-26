#for cs35l41 build
obj-$(CONFIG_SND_SOC_CS35L41)        += cs35l41.o cs35l41-tables.o
obj-$(CONFIG_SND_SOC_CS35L41_I2C)   += cs35l41-i2c.o
obj-$(CONFIG_SND_SOC_WM_ADSP)        += wm_adsp.o
#ccflags-y += -Isound/soc/codecs/cs35l41/include
#snd-soc-cs35l41-objs := sound/cs35l41-i2c.o sound/cs35l41.o sound/cs35l41-tables.o  sound/wm_adsp.o
#obj-$(CONFIG_SND_SOC_CS35L41)			+= snd-soc-cs35l41.o

ifeq ($(TARGET_PRODUCT), $(filter $(TARGET_PRODUCT), chopin vnd_chopin))
CDEFINES += -DCHOPIN
KBUILD_CPPFLAGS += $(CDEFINES)
endif

ifeq ($(TARGET_PRODUCT), $(filter $(TARGET_PRODUCT), ares vnd_ares))
CDEFINES += -DARES
KBUILD_CPPFLAGS += $(CDEFINES)
endif

