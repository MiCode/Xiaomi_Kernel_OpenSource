subdir-ccflags-y += -Werror -I$(srctree)/drivers/misc/mediatek/include \
-I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include \
-I$(srctree)/drivers/misc/mediatek/include/mt-plat

obj-$(CONFIG_CLKSRC_OF)	+= clksrc-of.o
obj-$(CONFIG_ATMEL_PIT)		+= timer-atmel-pit.o
obj-$(CONFIG_ATMEL_TCB_CLKSRC)	+= tcb_clksrc.o
obj-$(CONFIG_X86_PM_TIMER)	+= acpi_pm.o
obj-$(CONFIG_SCx200HR_TIMER)	+= scx200_hrt.o
obj-$(CONFIG_CS5535_CLOCK_EVENT_SRC)	+= cs5535-clockevt.o
obj-$(CONFIG_SH_TIMER_CMT)	+= sh_cmt.o
obj-$(CONFIG_SH_TIMER_MTU2)	+= sh_mtu2.o
obj-$(CONFIG_SH_TIMER_TMU)	+= sh_tmu.o
obj-$(CONFIG_EM_TIMER_STI)	+= em_sti.o
obj-$(CONFIG_CLKBLD_I8253)	+= i8253.o
obj-$(CONFIG_CLKSRC_MMIO)	+= mmio.o
obj-$(CONFIG_DW_APB_TIMER)	+= dw_apb_timer.o
obj-$(CONFIG_DW_APB_TIMER_OF)	+= dw_apb_timer_of.o
obj-$(CONFIG_CLKSRC_NOMADIK_MTU)	+= nomadik-mtu.o
obj-$(CONFIG_CLKSRC_DBX500_PRCMU)	+= clksrc-dbx500-prcmu.o
obj-$(CONFIG_ARMADA_370_XP_TIMER)	+= time-armada-370-xp.o
obj-$(CONFIG_ORION_TIMER)	+= time-orion.o
obj-$(CONFIG_ARCH_BCM2835)	+= bcm2835_timer.o
obj-$(CONFIG_ARCH_CLPS711X)	+= clps711x-timer.o
obj-$(CONFIG_ARCH_MARCO)	+= timer-marco.o
obj-$(CONFIG_ARCH_MOXART)	+= moxart_timer.o
obj-$(CONFIG_ARCH_MXS)		+= mxs_timer.o
obj-$(CONFIG_ARCH_PXA)		+= pxa_timer.o
obj-$(CONFIG_ARCH_PRIMA2)	+= timer-prima2.o
obj-$(CONFIG_ARCH_U300)		+= timer-u300.o
obj-$(CONFIG_SUN4I_TIMER)	+= sun4i_timer.o
obj-$(CONFIG_SUN5I_HSTIMER)	+= timer-sun5i.o
obj-$(CONFIG_MESON6_TIMER)	+= meson6_timer.o
obj-$(CONFIG_ARCH_TEGRA)	+= tegra20_timer.o
obj-$(CONFIG_VT8500_TIMER)	+= vt8500_timer.o
obj-$(CONFIG_ARCH_NSPIRE)	+= zevio-timer.o
obj-$(CONFIG_ARCH_BCM_MOBILE)	+= bcm_kona_timer.o
obj-$(CONFIG_CADENCE_TTC_TIMER)	+= cadence_ttc_timer.o
obj-$(CONFIG_CLKSRC_EFM32)	+= time-efm32.o
obj-$(CONFIG_CLKSRC_EXYNOS_MCT)	+= exynos_mct.o
obj-$(CONFIG_CLKSRC_SAMSUNG_PWM)	+= samsung_pwm_timer.o
obj-$(CONFIG_FSL_FTM_TIMER)	+= fsl_ftm_timer.o
obj-$(CONFIG_VF_PIT_TIMER)	+= vf_pit_timer.o
obj-$(CONFIG_CLKSRC_QCOM)	+= qcom-timer.o
obj-$(CONFIG_MTK_TIMER)		+= mtk_timer.o
obj-$(CONFIG_ARCH_MT6797)	+= cpuxgpt_mt6797.o gpt_mt6797.o arch_timer_mt6797.o
obj-$(CONFIG_ARCH_MT8173)	+= cpuxgpt_mt8173.o
obj-$(CONFIG_ARCH_MT8163)	+= cpuxgpt_mt8163.o
obj-$(CONFIG_ARCH_MT8127)	+= arch_timer_mt8127.o
obj-$(CONFIG_ARCH_ELBRUS)	+= cpuxgpt_elbrus.o

ifneq (,$(filter $(CONFIG_ARCH_MT6735) $(CONFIG_ARCH_MT6735M) $(CONFIG_ARCH_MT6753) $(CONFIG_ARCH_MT6797) $(CONFIG_ARCH_MT6580) $(CONFIG_ARCH_MT6570), y))
else
obj-$(CONFIG_ARM_ARCH_TIMER)    += arm_arch_timer.o
endif
obj-$(CONFIG_ARM_GLOBAL_TIMER)		+= arm_global_timer.o
obj-$(CONFIG_CLKSRC_METAG_GENERIC)	+= metag_generic.o
obj-$(CONFIG_ARCH_HAS_TICK_BROADCAST)	+= dummy_timer.o
obj-$(CONFIG_ARCH_KEYSTONE)		+= timer-keystone.o
obj-$(CONFIG_CLKSRC_VERSATILE)		+= versatile.o
