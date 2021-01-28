ccflags-y += -I$(srctree)/drivers/misc/mediatek/sspm/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/lpm/inc/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/lpm/mtk_lpm_trace_event/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/lpm/modules/include/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/lpm/modules/include/mt6779/

BUILD_MTK_LPM_PLAT_MODUDLE_NAME=mtk-lpm-plat-$(strip $(subst ",, $(CONFIG_MTK_LPM_MODULE_NAME)))

obj-$(CONFIG_MTK_LPM_MT6779) +=  ${BUILD_MTK_LPM_PLAT_MODUDLE_NAME}.o

${BUILD_MTK_LPM_PLAT_MODUDLE_NAME}-y += mt6779.o
${BUILD_MTK_LPM_PLAT_MODUDLE_NAME}-y += mtk_lp_plat_apmcu.o
${BUILD_MTK_LPM_PLAT_MODUDLE_NAME}-y += mtk_lp_plat_apmcu_mbox.o
${BUILD_MTK_LPM_PLAT_MODUDLE_NAME}-y += mt6779_ipi_sspm.o
${BUILD_MTK_LPM_PLAT_MODUDLE_NAME}-y += idles/mt6779_mcusys.o
${BUILD_MTK_LPM_PLAT_MODUDLE_NAME}-y += suspend/mt6779_suspend.o

