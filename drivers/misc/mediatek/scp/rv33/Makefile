SCP_VERSION := v02
SCP_VERSION_PATH := $(SCP_VERSION)

obj-$(CONFIG_MTK_TINYSYS_SCP_SUPPORT) += scp.o
scp-$(CONFIG_MTK_TINYSYS_SCP_SUPPORT) += $(SCP_VERSION_PATH)/scp_helper.o
scp-$(CONFIG_MTK_TINYSYS_SCP_SUPPORT) += $(SCP_VERSION_PATH)/scp_excep.o
scp-$(CONFIG_MTK_TINYSYS_SCP_SUPPORT) += $(SCP_VERSION_PATH)/scp_wrapper_ipi.o
scp-$(CONFIG_MTK_TINYSYS_SCP_SUPPORT) += $(SCP_VERSION_PATH)/scp_irq.o
scp-$(CONFIG_MTK_TINYSYS_SCP_SUPPORT) += $(SCP_VERSION_PATH)/scp_logger.o
scp-$(CONFIG_MTK_TINYSYS_SCP_SUPPORT) += $(SCP_VERSION_PATH)/scp_l1c.o
scp-$(CONFIG_MTK_TINYSYS_SCP_SUPPORT) += $(SCP_VERSION_PATH)/scp_scpctl.o

scp-$(CONFIG_MTK_TINYSYS_SCP_SUPPORT) += scp_feature_table.o
scp-$(CONFIG_MTK_TINYSYS_SCP_SUPPORT) += scp_awake.o
#scp-$(CONFIG_MTK_TINYSYS_SCP_SUPPORT) += scp_dvfs.o

ccflags-y += -D DEBUG_DO -fno-pic -mcmodel=large
ccflags-y += -I$(srctree)/drivers/misc/mediatek/scp/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include

