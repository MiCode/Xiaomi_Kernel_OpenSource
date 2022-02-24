obj-$(CONFIG_MTK_PMQOS) += mtk-pm-qos.o
obj-$(CONFIG_MTK_CMDQ) += mtk-cmdq-helper.o

ccflags-$(CONFIG_MTK_CMDQ_MBOX_EXT) += -I$(srctree)/drivers/misc/mediatek/cmdq/mailbox

obj-$(CONFIG_MTK_INFRACFG) += mtk-infracfg.o mtk-scpsys-ext.o
obj-$(CONFIG_MTK_DVFSRC) += mtk-dvfsrc.o
ifneq (,$(filter $(CONFIG_MTK_PLATFORM), "mt6768" "mt6739" "mt6785" "mt6853" "mt6771" "mt6873" "mt6885"))
obj-$(CONFIG_MTK_PMIC_WRAP) += mtk-pmic-wrap_v1.o
else
obj-$(CONFIG_MTK_PMIC_WRAP) += mtk-pmic-wrap.o
endif
obj-$(CONFIG_MTK_SCPSYS) += mtk-scpsys.o
obj-$(CONFIG_MTK_SCPSYS_BRINGUP) += mtk-scpsys-bringup.o
obj-$(CONFIG_MTK_DEVAPC) += devapc/
obj-$(CONFIG_MTK_DEVMPU) += devmpu/
obj-$(CONFIG_MTK_CMDQ_MBOX) += mtk-cmdq-helper.o
obj-$(CONFIG_MTK_MBOX) += mtk-mbox.o
obj-$(CONFIG_MTK_MMDVFS) += mtk-mmdvfs.o
obj-$(CONFIG_MTK_LPM) += mtk-lpm.o
obj-$(CONFIG_MTK_SOCINFO) += mtk-socinfo.o
obj-$(CONFIG_MTK_MBOX) += mtk-mbox.o
obj-$(CONFIG_MTK_IPI) += mtk_tinysys_ipi.o

ccflags-y += -I$(srctree)/drivers/misc/mediatek/include
