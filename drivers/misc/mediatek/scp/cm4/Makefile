SCP_VERSION := v01
SCP_VERSION_PATH := ./$(SCP_VERSION)

obj-$(CONFIG_MTK_TINYSYS_SCP_SUPPORT) += scp.o
scp-$(CONFIG_MTK_TINYSYS_SCP_SUPPORT) += scp_feature_table.o scp_awake.o
scp-$(CONFIG_MTK_TINYSYS_SCP_SUPPORT) += scp_dvfs.o

scp-$(CONFIG_MTK_TINYSYS_SCP_SUPPORT) += $(SCP_VERSION_PATH)/scp_helper.o
scp-$(CONFIG_MTK_TINYSYS_SCP_SUPPORT) += $(SCP_VERSION_PATH)/scp_excep.o
scp-$(CONFIG_MTK_TINYSYS_SCP_SUPPORT) += $(SCP_VERSION_PATH)/scp_ipi.o
scp-$(CONFIG_MTK_TINYSYS_SCP_SUPPORT) += $(SCP_VERSION_PATH)/scp_irq.o
scp-$(CONFIG_MTK_TINYSYS_SCP_SUPPORT) += $(SCP_VERSION_PATH)/scp_logger.o
scp-$(CONFIG_MTK_TINYSYS_SCP_SUPPORT) += $(SCP_VERSION_PATH)/scp_l1c.o
scp-$(CONFIG_MTK_TINYSYS_SCP_SUPPORT) += $(SCP_VERSION_PATH)/scp_scpctl.o

# include mtk_spm_resource_req
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include/

ccflags-y += -I$(srctree)/drivers/misc/mediatek/scp/
# include emi_mpu.h
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/emi/submodule
ccflags-$(CONFIG_MTK_EMI_LEGACY) += -I$(srctree)/drivers/misc/mediatek/emi/$(CONFIG_MTK_PLATFORM)
ccflags-y += -I$(srctree)/drivers/devfreq/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/

#ifeq ($(CONFIG_MTK_ENG_BUILD),y)
CFLAGS_scp_logger.o += -DDEBUG
CFLAGS_scp_helper.o += -DDEBUG
CFLAGS_scp_excep.o += -DDEBUG
CFLAGS_scp_ipi.o += -DDEBUG
CFLAGS_scp_irq.o += -DDEBUG
CFLAGS_scp_dvfs.o += -DDEBUG
CFLAGS_scp_scpctl.o += -DDEBUG
#endif

ccflags-$(CONFIG_MTK_PMIC_CHIP_MT6335)+= -I$(srctree)/drivers/misc/mediatek/pmic/include/mt6335
ccflags-$(CONFIG_MTK_PMIC_CHIP_MT6353)+= -I$(srctree)/drivers/misc/mediatek/pmic/include/mt6353
ccflags-$(CONFIG_MTK_PMIC_CHIP_MT6355)+= -I$(srctree)/drivers/misc/mediatek/pmic/include/mt6355
ccflags-$(CONFIG_MTK_PMIC_CHIP_MT6357)+= -I$(srctree)/drivers/misc/mediatek/pmic/include/mt6357
ccflags-$(CONFIG_MTK_PMIC_CHIP_MT6358)+= -I$(srctree)/drivers/misc/mediatek/pmic/include/mt6358
ccflags-$(CONFIG_MTK_PMIC_CHIP_MT6359)+= -I$(srctree)/drivers/misc/mediatek/pmic/include/mt6359


