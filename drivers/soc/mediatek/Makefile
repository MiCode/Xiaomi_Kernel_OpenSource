ifeq ($(CONFIG_MTK_GCOV_KERNEL),y)
GCOV_PROFILE_mtk-pmic-wrap.o := y
endif
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include

ccflags-$(CONFIG_MTK_CMDQ_MBOX_EXT) += -I$(srctree)/drivers/misc/mediatek/cmdq/mailbox

obj-$(CONFIG_MTK_CMDQ) += mtk-cmdq-helper.o
obj-$(CONFIG_MTK_INFRACFG) += mtk-infracfg.o
obj-$(CONFIG_MTK_PMIC_WRAP) += mtk-pmic-wrap.o
obj-$(CONFIG_MTK_SCPSYS) += mtk-scpsys.o
obj-$(CONFIG_MTK_SCPSYS_MT8167) += mtk-scpsys-mt8167.o
obj-$(CONFIG_MTK_CMDQ_MBOX) += mtk-cmdq-helper.o
obj-$(CONFIG_MTK_MBOX) += mtk-mbox.o
obj-$(CONFIG_MTK_IPI) += mtk_tinysys_ipi.o
obj-$(CONFIG_MTK_LPM) += mtk-lpm.o
obj-$(CONFIG_XIAOMI_HWID) += socinfo.o
