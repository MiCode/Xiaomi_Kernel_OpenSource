obj-$(CONFIG_MTK_TINYSYS_VCP_SUPPORT) += vcp_status.o

obj-$(CONFIG_MTK_TINYSYS_VCP_SUPPORT) += vcp.o
vcp-$(CONFIG_MTK_TINYSYS_VCP_SUPPORT) += vcp_helper.o
vcp-$(CONFIG_MTK_TINYSYS_VCP_SUPPORT) += vcp_excep.o
vcp-$(CONFIG_MTK_TINYSYS_VCP_SUPPORT) += vcp_irq.o
vcp-$(CONFIG_MTK_TINYSYS_VCP_SUPPORT) += vcp_logger.o
vcp-$(CONFIG_MTK_TINYSYS_VCP_SUPPORT) += vcp_l1c.o
vcp-$(CONFIG_MTK_TINYSYS_VCP_SUPPORT) += vcp_vcpctl.o
vcp-$(CONFIG_MTK_TINYSYS_VCP_SUPPORT) += vcp_feature_table.o
vcp-$(CONFIG_MTK_TINYSYS_VCP_SUPPORT) += vcp_awake.o

ccflags-y += -D DEBUG_DO -fno-pic -mcmodel=large
ccflags-y += -I$(srctree)/drivers/misc/mediatek/vcp/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/vcp/rv
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat
ccflags-y += -I$(srctree)/drivers/clk/mediatek