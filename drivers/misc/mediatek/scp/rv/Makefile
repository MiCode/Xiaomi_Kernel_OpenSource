obj-$(CONFIG_MTK_TINYSYS_SCP_SUPPORT) += scp.o
scp-$(CONFIG_MTK_TINYSYS_SCP_SUPPORT) += scp_helper.o
scp-$(CONFIG_MTK_TINYSYS_SCP_SUPPORT) += scp_excep.o
scp-$(CONFIG_MTK_TINYSYS_SCP_SUPPORT) += scp_wrapper_ipi.o
scp-$(CONFIG_MTK_TINYSYS_SCP_SUPPORT) += scp_irq.o
scp-$(CONFIG_MTK_TINYSYS_SCP_SUPPORT) += scp_logger.o
scp-$(CONFIG_MTK_TINYSYS_SCP_SUPPORT) += scp_l1c.o
scp-$(CONFIG_MTK_TINYSYS_SCP_SUPPORT) += scp_scpctl.o

scp-$(CONFIG_MTK_TINYSYS_SCP_SUPPORT) += scp_feature_table.o
scp-$(CONFIG_MTK_TINYSYS_SCP_SUPPORT) += scp_awake.o
scp-$(CONFIG_MTK_TINYSYS_SCP_SUPPORT) += scp_dvfs.o
scp-$(CONFIG_MTK_TINYSYS_SCP_SUPPORT) += scp_hwvoter_dbg.o

ccflags-y += -I$(srctree)/drivers/misc/mediatek/scp/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include
ccflags-y += -I$(srctree)/drivers/clk/mediatek/
ccflags-y += -I$(srctree)/drivers/devfreq/
ccflags-$(CONFIG_MTK_PMIC_CHIP_MT6335)+= -I$(srctree)/drivers/misc/mediatek/pmic/include/mt6335
ccflags-$(CONFIG_MTK_PMIC_CHIP_MT6353)+= -I$(srctree)/drivers/misc/mediatek/pmic/include/mt6353
ccflags-$(CONFIG_MTK_PMIC_CHIP_MT6355)+= -I$(srctree)/drivers/misc/mediatek/pmic/include/mt6355
ccflags-$(CONFIG_MTK_PMIC_CHIP_MT6358)+= -I$(srctree)/drivers/misc/mediatek/pmic/include/mt6358
ccflags-$(CONFIG_MTK_PMIC_CHIP_MT6359P)+= -I$(srctree)/drivers/misc/mediatek/pmic/include/mt6359p

