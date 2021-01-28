obj-y += adsp.o
adsp-y := adsp_helper.o adsp_excep.o adsp_excep.o adsp_ipi.o adsp_irq.o
adsp-y += adsp_logger.o adsp_service.o adsp_timesync.o
#adsp-y += adsp_timesync.o adsp_trace.o

ccflags-y += -I$(srctree)/drivers/misc/mediatek/audio_ipi/common/framework
ccflags-y += -I$(srctree)/drivers/misc/mediatek/scp/$(CONFIG_MTK_PLATFORM)

ccflags-y += -I$(srctree)/drivers/misc/mediatek/adsp/$(CONFIG_MTK_PLATFORM)
# include emi_mpu.h
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(CONFIG_MTK_PLATFORM)/include/mach
ccflags-y += -I$(srctree)/drivers/misc/mediatek/emi/$(CONFIG_MTK_PLATFORM)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include/spm

ifeq ($(CONFIG_MTK_ENG_BUILD),y)
CFLAGS_adsp_logger.o += -DDEBUG
CFLAGS_adsp_helper.o += -DDEBUG
CFLAGS_adsp_excep.o += -DDEBUG
CFLAGS_adsp_ipi.o += -DDEBUG
CFLAGS_adsp_irq.o += -DDEBUG
CFLAGS_adsp_dvfs.o += -DDEBUG
CFLAGS_adsp_clk.o += -DDEBUG
CFLAGS_adsp_service.o += -DDEBUG
CFLAGS_timesync.o += -DDEBUG
endif
