obj-y += scp.o
scp-y := scp_helper.o scp_excep.o scp_ipi.o scp_irq.o scp_logger.o scp_l1c.o scp_scpctl.o
#scp-y += scp_timesync.o scp_trace.o
# include mtk_spm_resource_req
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include/

ccflags-y += -I$(srctree)/drivers/misc/mediatek/scp/$(CONFIG_MTK_PLATFORM)
# include emi_mpu.h
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(CONFIG_MTK_PLATFORM)/include/mach
ccflags-y += -I$(srctree)/drivers/misc/mediatek/emi/submodule
ccflags-y += -I$(srctree)/drivers/misc/mediatek/emi/$(CONFIG_MTK_PLATFORM)

#ifeq ($(CONFIG_MTK_ENG_BUILD),y)
CFLAGS_scp_logger.o += -DDEBUG
CFLAGS_scp_helper.o += -DDEBUG
CFLAGS_scp_excep.o += -DDEBUG
CFLAGS_scp_ipi.o += -DDEBUG
CFLAGS_scp_irq.o += -DDEBUG
CFLAGS_scp_dvfs.o += -DDEBUG
CFLAGS_scp_scpctl.o += -DDEBUG
#endif
