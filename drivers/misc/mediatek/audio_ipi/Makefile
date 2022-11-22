#
# Makefile for the kernel audio_ipi device drivers
#


### flags
ccflags-y += -Wformat
#ccflags-y += -DDEBUG_IPI


### common include path
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/audio_ipi/include

### for adsp
#ifeq ($(CONFIG_MTK_AUDIODSP_SUPPORT),y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/adsp/include
#endif

### for RV
#ifeq ($(CONFIG_MTK_SCP_AUDIO),y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/scp/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/scp/rv
ccflags-y += -I$(srctree)/sound/soc/mediatek/audio_scp
#endif



### target object
obj-$(CONFIG_MTK_AUDIO_IPI) += audio_ipi.o

### build codes
audio_ipi-objs += common/audio_ipi.o
audio_ipi-objs += common/audio_task_manager.o
audio_ipi-objs += common/audio_messenger_ipi.o
audio_ipi-objs += common/audio_ipi_queue.o
audio_ipi-objs += common/adsp_ipi_queue.o
audio_ipi-objs += common/audio_ipi_dma.o
audio_ipi-objs += common/audio_ringbuf.o
audio_ipi-objs += common/audio_ipi_platform_common.o

### platform codes
IPI_PLATFORM_VERSION = v4
audio_ipi-objs += $(IPI_PLATFORM_VERSION)/audio_ipi_platform.o



