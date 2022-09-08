# MTK include path
subdir-ccflags-y += -I$(srctree)/sound/soc/mediatek/common

cirrus_cs35l45-objs += cs35l45.o
cirrus_cs35l45-objs += cs35l45-i2c.o
cirrus_cs35l45-objs += cs35l45-tables.o
obj-$(CONFIG_SND_SOC_CS35L45) += cirrus_cs35l45.o

cirrus_wm_adsp-objs += wm_adsp.o
obj-$(CONFIG_SND_SOC_CS35L45) += cirrus_wm_adsp.o
