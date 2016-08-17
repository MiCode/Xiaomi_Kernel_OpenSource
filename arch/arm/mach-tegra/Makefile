GCOV_PROFILE := y

subdir-ccflags-y := -Werror

obj-y                                   += tegra_rst_reason.o
obj-y                                   += board-pinmux.o
obj-y                                   += ahb.o
obj-y                                   += common.o
obj-y                                   += devices.o
obj-y                                   += board-info.o
obj-y                                   += io.o
obj-y                                   += irq.o
obj-y                                   += clock.o
obj-$(CONFIG_ARCH_TEGRA_11x_SOC)        += tegra11_clocks.o
obj-$(CONFIG_TEGRA_ATOMICS)             += atomics.o
obj-y                                   += timer.o
obj-y                                   += timerinfo.o
ifneq ($(CONFIG_ARCH_TEGRA_2x_SOC),y)
ifneq ($(CONFIG_ARCH_TEGRA_3x_SOC),y)
obj-y                                   += mipi-cal.o
endif
endif
ifeq ($(CONFIG_CPU_FREQ),y)
obj-$(CONFIG_ARCH_TEGRA_11x_SOC)        += tegra11_cpu_volt_cap.o
endif
ifeq ($(CONFIG_ARCH_TEGRA_2x_SOC),y)
obj-y                                   += common-t2.o
obj-y                                   += pm-t2.o
obj-y                                   += sleep-t2.o
obj-y                                   += timer-t2.o
obj-$(CONFIG_PM_SLEEP)                  += wakeups-t2.o
ifeq ($(CONFIG_CPU_IDLE),y)
obj-$(CONFIG_PM_SLEEP)                  += cpuidle-t2.o
endif
else
obj-y                                   += common-t3.o
obj-y                                   += pm-t3.o
obj-y                                   += sleep-t3.o
obj-$(CONFIG_DEBUG_FS)                  += clocks_stats.o
obj-y                                   += timer-t3.o
obj-y                                   += tegra_core_volt_cap.o
ifeq ($(CONFIG_ARCH_TEGRA_3x_SOC),y)
obj-$(CONFIG_PM_SLEEP)                  += wakeups-t3.o
else
obj-$(CONFIG_PM_SLEEP)                  += wakeups-t11x.o
endif
ifeq ($(CONFIG_CPU_IDLE),y)
ifeq ($(CONFIG_ARCH_TEGRA_3x_SOC),y)
obj-$(CONFIG_PM_SLEEP)                  += cpuidle-t3.o
else
obj-$(CONFIG_PM_SLEEP)                  += cpuidle-t11x.o
endif
endif
endif

obj-y                                   += pinmux.o
obj-y                                   += delay.o
obj-y                                   += pm.o
obj-$(CONFIG_TEGRA_WDT_RECOVERY)        += wdt-recovery.o
obj-$(CONFIG_PM_SLEEP)                  += pm-irq.o
obj-y                                   += gic.o

obj-y                                   += sleep.o
obj-$(CONFIG_TEGRA_USE_NCT)             += nct.o nct_sysfs.o

plus_sec := $(call as-instr,.arch_extension sec,+sec)
AFLAGS_sleep.o :=-Wa,-march=armv7-a$(plus_sec)

obj-y                                   += tegra_fuse.o
obj-y                                   += kfuse.o
obj-y                                   += csi.o
obj-y                                   += i2c_error_recovery.o
obj-y                                   += mc.o
obj-$(CONFIG_TEGRA_STAT_MON)            += tegra2_statmon.o
obj-$(CONFIG_USB_SUPPORT)               += xusb.o
obj-$(CONFIG_USB_SUPPORT)               += usb_phy.o
obj-$(CONFIG_USB_SUPPORT)               += tegra_usb_pmc.o
obj-$(CONFIG_USB_SUPPORT)               += tegra_usb_pad_ctrl.o
obj-$(CONFIG_USB_SUPPORT)               += tegra3_usb_phy.o
obj-$(CONFIG_USB_SUPPORT)               += tegra2_usb_phy.o
obj-$(CONFIG_USB_SUPPORT)               += tegra11x_usb_phy.o
obj-$(CONFIG_FIQ)                       += fiq.o
obj-$(CONFIG_TEGRA_FIQ_DEBUGGER)        += tegra_fiq_debugger.o
obj-$(CONFIG_TEGRA_PTM)                 += tegra_ptm.o
obj-$(CONFIG_TEGRA_PWM)                 += pwm.o
obj-$(CONFIG_TEGRA_ARB_SEMAPHORE)       += arb_sema.o

obj-y                                   += powergate.o
obj-y                                   += powergate-ops-txx.o
obj-y                                   += powergate-ops-t1xx.o
obj-$(CONFIG_ARCH_TEGRA_2x_SOC)         += powergate-t20.o
obj-$(CONFIG_ARCH_TEGRA_3x_SOC)         += powergate-t30.o
obj-$(CONFIG_ARCH_TEGRA_11x_SOC)        += powergate-t11x.o

obj-y                                   += dvfs.o
obj-$(CONFIG_ARCH_TEGRA_11x_SOC)        += tegra11_dvfs.o
ifeq ($(CONFIG_TEGRA_SILICON_PLATFORM),y)
obj-$(CONFIG_ARCH_TEGRA_2x_SOC)         += tegra2_dvfs.o
obj-$(CONFIG_ARCH_TEGRA_3x_SOC)         += tegra3_dvfs.o
obj-$(CONFIG_TEGRA_LATENCY_ALLOWANCE)   += latency_allowance.o
obj-$(CONFIG_TEGRA_EDP_LIMITS)          += edp.o
obj-$(CONFIG_TEGRA_CORE_EDP_LIMITS)     += edp_core.o
obj-$(CONFIG_ARCH_TEGRA_11x_SOC)        += tegra11_edp.o
endif
ifeq ($(CONFIG_TEGRA_SILICON_PLATFORM),y)
obj-$(CONFIG_ARCH_TEGRA_2x_SOC)         += tegra2_speedo.o
obj-$(CONFIG_ARCH_TEGRA_3x_SOC)         += tegra3_speedo.o
obj-$(CONFIG_ARCH_TEGRA_11x_SOC)        += tegra11_speedo.o
obj-$(CONFIG_ARCH_TEGRA_3x_SOC)         += tegra3_actmon.o
endif
obj-$(CONFIG_ARCH_TEGRA_11x_SOC)        += tegra3_actmon.o
obj-$(CONFIG_ARCH_TEGRA_2x_SOC)         += tegra2_emc.o
obj-$(CONFIG_ARCH_TEGRA_3x_SOC)         += tegra3_emc.o
obj-$(CONFIG_ARCH_TEGRA_11x_SOC)        += tegra11_emc.o
obj-y                                   += tegra_emc.o
obj-$(CONFIG_ARCH_TEGRA_2x_SOC)         += pinmux-tegra20-tables.o
obj-$(CONFIG_ARCH_TEGRA_3x_SOC)         += pinmux-tegra30-tables.o
obj-$(CONFIG_ARCH_TEGRA_3x_SOC)         += board-dt-tegra30.o
obj-$(CONFIG_ARCH_TEGRA_11x_SOC)        += pinmux-t11-tables.o
obj-$(CONFIG_HOTPLUG_CPU)               += hotplug.o
obj-y                                   += pmc.o
obj-y                                   += flowctrl.o
obj-$(CONFIG_CPU_IDLE)                  += cpuidle.o
obj-$(CONFIG_CPU_IDLE)                  += sleep.o
obj-$(CONFIG_ARCH_TEGRA_2x_SOC)         += tegra2_clocks.o
obj-$(CONFIG_ARCH_TEGRA_3x_SOC)         += tegra30_clocks.o
obj-$(CONFIG_ARCH_TEGRA_11x_SOC)        += tegra11_clocks.o
obj-$(CONFIG_SMP)                       += platsmp.o
obj-y                                   += headsmp.o
obj-y                                   += reset.o
obj-$(CONFIG_TEGRA_SYSTEM_DMA)          += dma.o apbio.o
obj-$(CONFIG_CPU_FREQ)                  += cpu-tegra.o
ifeq ($(CONFIG_TEGRA_AUTO_HOTPLUG),y)
ifeq ($(CONFIG_TEGRA_CPUQUIET),y)
obj-$(CONFIG_ARCH_TEGRA_3x_SOC)         += cpuquiet.o
obj-$(CONFIG_ARCH_TEGRA_11x_SOC)        += cpuquiet.o
else
obj-$(CONFIG_ARCH_TEGRA_3x_SOC)         += cpu-tegra3.o
endif
endif
obj-$(CONFIG_TEGRA_PCI)                 += pcie.o
obj-$(CONFIG_USB_SUPPORT)               += usb_phy.o
ifeq ($(CONFIG_TEGRA_SOCTHERM),y)
obj-$(CONFIG_ARCH_TEGRA_11x_SOC)        += tegra11_soctherm.o
endif
ifeq ($(CONFIG_TEGRA_THERMAL_THROTTLE),y)
obj-$(CONFIG_ARCH_TEGRA_2x_SOC)         += tegra2_throttle.o
obj-$(CONFIG_ARCH_TEGRA_3x_SOC)         += tegra3_throttle.o
obj-$(CONFIG_ARCH_TEGRA_11x_SOC)        += tegra3_throttle.o
endif
obj-$(CONFIG_TEGRA_IOVMM)               += iovmm.o
obj-$(CONFIG_TEGRA_IOVMM_GART)          += iovmm-gart.o
obj-$(CONFIG_TEGRA_IOVMM_SMMU)          += iovmm-smmu.o
obj-$(CONFIG_DEBUG_ICEDCC)              += sysfs-dcc.o
obj-$(CONFIG_TEGRA_CLUSTER_CONTROL)     += sysfs-cluster.o
ifeq ($(CONFIG_TEGRA_MC_PROFILE),y)
obj-$(CONFIG_ARCH_TEGRA_2x_SOC)         += tegra2_mc.o
endif
obj-$(CONFIG_SENSORS_TEGRA_TSENSOR)     += tegra3_tsensor.o
obj-$(CONFIG_TEGRA_DYNAMIC_PWRDET)      += powerdetect.o
obj-$(CONFIG_ARCH_TEGRA_HAS_CL_DVFS)    += tegra_cl_dvfs.o

obj-y                                   += board-common.o
obj-y                                   += board-panel.o
obj-$(CONFIG_TEGRA_WAKEUP_MONITOR)      += tegra_wakeup_monitor.o
obj-$(CONFIG_TEGRA_PCI)                 += pcie.o

# MC error reporting.
ifeq ($(CONFIG_ARCH_TEGRA_2x_SOC),)
obj-y                                   += mcerr.o
ifeq ($(CONFIG_ARCH_TEGRA_3x_SOC),y)
obj-y                                   += mcerr-t3.o
else
obj-y                                   += mcerr-t11.o
endif
endif

obj-$(CONFIG_MACH_HARMONY)              += board-harmony.o
obj-$(CONFIG_MACH_HARMONY)              += board-harmony-kbc.o
obj-$(CONFIG_MACH_HARMONY)              += board-harmony-panel.o
obj-$(CONFIG_MACH_HARMONY)              += board-harmony-pinmux.o
obj-$(CONFIG_MACH_HARMONY)              += board-harmony-pcie.o
obj-$(CONFIG_MACH_HARMONY)              += board-harmony-power.o

obj-$(CONFIG_MACH_PAZ00)                += board-paz00.o
obj-$(CONFIG_MACH_PAZ00)                += board-paz00-pinmux.o

obj-$(CONFIG_MACH_SEABOARD)             += board-seaboard.o
obj-$(CONFIG_MACH_SEABOARD)             += board-seaboard-pinmux.o

obj-$(CONFIG_MACH_TEGRA_DT)             += board-dt-tegra20.o
obj-$(CONFIG_MACH_TEGRA_DT)             += board-harmony-pinmux.o
obj-$(CONFIG_MACH_TEGRA_DT)             += board-seaboard-pinmux.o
obj-$(CONFIG_MACH_TEGRA_DT)             += board-paz00-pinmux.o
obj-$(CONFIG_MACH_TEGRA_DT)             += board-trimslice-pinmux.o

obj-$(CONFIG_MACH_TRIMSLICE)            += board-trimslice.o
obj-$(CONFIG_MACH_TRIMSLICE)            += board-trimslice-pinmux.o

obj-${CONFIG_MACH_P852}                 += p852/

obj-$(CONFIG_MACH_VENTANA)              += board-ventana.o
obj-$(CONFIG_MACH_VENTANA)              += board-ventana-pinmux.o
obj-$(CONFIG_MACH_VENTANA)              += board-ventana-sdhci.o
obj-$(CONFIG_MACH_VENTANA)              += board-ventana-power.o
obj-$(CONFIG_MACH_VENTANA)              += board-ventana-panel.o
obj-$(CONFIG_MACH_VENTANA)              += board-ventana-sensors.o
obj-$(CONFIG_MACH_VENTANA)              += board-ventana-memory.o

obj-${CONFIG_MACH_ARUBA}                += board-aruba.o
obj-${CONFIG_MACH_ARUBA}                += board-aruba-panel.o
obj-${CONFIG_MACH_ARUBA}                += board-aruba-pinmux.o
obj-${CONFIG_MACH_ARUBA}                += board-aruba-power.o
obj-${CONFIG_MACH_ARUBA}                += board-aruba-sdhci.o
obj-${CONFIG_MACH_ARUBA}                += board-aruba-sensors.o

obj-${CONFIG_MACH_CURACAO}              += board-curacao.o
obj-${CONFIG_MACH_CURACAO}              += board-curacao-panel.o
obj-${CONFIG_MACH_CURACAO}              += board-curacao-pinmux.o
obj-${CONFIG_MACH_CURACAO}              += board-curacao-power.o
obj-${CONFIG_MACH_CURACAO}              += board-curacao-sdhci.o
obj-${CONFIG_MACH_CURACAO}              += board-curacao-sensors.o
obj-${CONFIG_MACH_CURACAO}              += board-curacao-memory.o

obj-${CONFIG_MACH_WHISTLER}             += board-whistler.o
obj-${CONFIG_MACH_WHISTLER}             += board-whistler-pinmux.o
obj-${CONFIG_MACH_WHISTLER}             += board-whistler-sdhci.o
obj-${CONFIG_MACH_WHISTLER}             += board-whistler-power.o
obj-${CONFIG_MACH_WHISTLER}             += board-whistler-panel.o
obj-${CONFIG_MACH_WHISTLER}             += board-whistler-sensors.o
obj-${CONFIG_MACH_WHISTLER}             += board-whistler-kbc.o
obj-${CONFIG_MACH_WHISTLER}             += board-whistler-baseband.o
obj-${CONFIG_MACH_WHISTLER}             += board-whistler-memory.o

obj-${CONFIG_MACH_CARDHU}               += board-cardhu.o
obj-${CONFIG_MACH_CARDHU}               += board-cardhu-kbc.o
obj-${CONFIG_MACH_CARDHU}               += board-cardhu-irda.o
obj-${CONFIG_MACH_CARDHU}               += board-cardhu-panel.o
obj-${CONFIG_MACH_CARDHU}               += board-cardhu-pinmux.o
obj-${CONFIG_MACH_CARDHU}               += board-cardhu-power.o
obj-${CONFIG_MACH_CARDHU}               += board-cardhu-pm298-power-rails.o
obj-${CONFIG_MACH_CARDHU}               += board-cardhu-pm299-power-rails.o
obj-${CONFIG_MACH_CARDHU}               += board-cardhu-sdhci.o
obj-${CONFIG_MACH_CARDHU}               += board-cardhu-sensors.o
obj-${CONFIG_MACH_CARDHU}               += board-cardhu-memory.o
obj-${CONFIG_MACH_CARDHU}               += board-cardhu-powermon.o

obj-${CONFIG_MACH_KAI}                  += board-touch-kai-synaptics-spi.o
obj-y                                   += board-touch-raydium_spi.o
obj-y                                   += board-touch-maxim_sti-spi.o

obj-${CONFIG_MACH_P1852}               += board-p1852.o
obj-${CONFIG_MACH_P1852}               += board-p1852-power.o
obj-${CONFIG_MACH_P1852}               += board-p1852-panel.o
obj-${CONFIG_MACH_P1852}               += board-p1852-pinmux.o
obj-${CONFIG_MACH_P1852}               += board-p1852-sdhci.o

obj-${CONFIG_MACH_E1853}               += board-e1853.o
obj-${CONFIG_MACH_E1853}               += board-e1853-panel.o
obj-${CONFIG_MACH_E1853}               += board-e1853-pinmux.o
obj-${CONFIG_MACH_E1853}               += board-e1853-sdhci.o

obj-${CONFIG_MACH_TEGRA_ENTERPRISE}     += board-enterprise.o
obj-${CONFIG_MACH_TEGRA_ENTERPRISE}     += board-enterprise-panel.o
obj-${CONFIG_MACH_TEGRA_ENTERPRISE}     += board-enterprise-pinmux.o
obj-${CONFIG_MACH_TEGRA_ENTERPRISE}     += board-enterprise-sdhci.o
obj-${CONFIG_MACH_TEGRA_ENTERPRISE}     += board-enterprise-memory.o
obj-${CONFIG_MACH_TEGRA_ENTERPRISE}     += board-enterprise-power.o
obj-${CONFIG_MACH_TEGRA_ENTERPRISE}     += board-enterprise-baseband.o
obj-${CONFIG_MACH_TEGRA_ENTERPRISE}     += board-enterprise-kbc.o
obj-${CONFIG_MACH_TEGRA_ENTERPRISE}     += board-enterprise-sensors.o

obj-${CONFIG_MACH_KAI}                  += board-kai.o
obj-${CONFIG_MACH_KAI}                  += board-kai-kbc.o
obj-${CONFIG_MACH_KAI}                  += board-kai-memory.o
obj-${CONFIG_MACH_KAI}                  += board-kai-panel.o
obj-${CONFIG_MACH_KAI}                  += board-kai-pinmux.o
obj-${CONFIG_MACH_KAI}                  += board-kai-power.o
obj-${CONFIG_MACH_KAI}                  += board-kai-sdhci.o
obj-${CONFIG_MACH_KAI}                  += board-kai-sensors.o
obj-y                                   += eeprom-wifi-mac.o

obj-${CONFIG_MACH_DALMORE}              += board-dalmore.o
obj-${CONFIG_MACH_DALMORE}              += board-dalmore-memory.o
obj-${CONFIG_MACH_DALMORE}              += board-dalmore-pinmux.o
obj-${CONFIG_MACH_DALMORE}              += board-dalmore-power.o
obj-${CONFIG_MACH_DALMORE}              += board-dalmore-powermon.o
obj-${CONFIG_MACH_DALMORE}              += board-dalmore-sdhci.o
obj-${CONFIG_MACH_DALMORE}              += board-dalmore-panel.o
obj-${CONFIG_MACH_DALMORE}              += board-roth-panel.o
obj-${CONFIG_MACH_DALMORE}              += board-dalmore-kbc.o
obj-${CONFIG_MACH_DALMORE}              += board-dalmore-sensors.o
obj-${CONFIG_MACH_DALMORE}              += panel-a-1080p-11-6.o
obj-${CONFIG_MACH_DALMORE}              += panel-p-wuxga-10-1.o
obj-${CONFIG_MACH_DALMORE}              += panel-s-wqxga-10-1.o

obj-${CONFIG_MACH_MACALLAN}             += board-macallan.o
obj-${CONFIG_MACH_MACALLAN}             += board-macallan-memory.o
obj-${CONFIG_MACH_MACALLAN}             += board-macallan-pinmux.o
obj-${CONFIG_MACH_MACALLAN}             += board-macallan-power.o
obj-${CONFIG_MACH_MACALLAN}             += board-macallan-powermon.o
obj-${CONFIG_MACH_MACALLAN}             += board-macallan-sdhci.o
obj-${CONFIG_MACH_MACALLAN}             += board-macallan-panel.o
obj-${CONFIG_MACH_MACALLAN}             += board-macallan-kbc.o
obj-${CONFIG_MACH_MACALLAN}             += board-macallan-sensors.o
obj-${CONFIG_MACH_MACALLAN}             += panel-p-wuxga-10-1.o

obj-${CONFIG_MACH_TEGRATAB}            += board-tegratab.o
obj-${CONFIG_MACH_TEGRATAB}            += board-tegratab-memory.o
obj-${CONFIG_MACH_TEGRATAB}            += board-tegratab-pinmux.o
obj-${CONFIG_MACH_TEGRATAB}            += board-tegratab-power.o
obj-${CONFIG_MACH_TEGRATAB}            += board-tegratab-powermon.o
obj-${CONFIG_MACH_TEGRATAB}            += board-tegratab-sdhci.o
obj-${CONFIG_MACH_TEGRATAB}            += board-tegratab-panel.o
obj-${CONFIG_MACH_TEGRATAB}            += board-tegratab-kbc.o
obj-${CONFIG_MACH_TEGRATAB}            += board-tegratab-sensors.o
obj-${CONFIG_MACH_TEGRATAB}            += panel-lgd-wxga-7-0.o

obj-${CONFIG_MACH_TEGRA_PLUTO}          += board-pluto.o
obj-${CONFIG_MACH_TEGRA_PLUTO}          += board-pluto-memory.o
obj-${CONFIG_MACH_TEGRA_PLUTO}          += board-pluto-pinmux.o
obj-${CONFIG_MACH_TEGRA_PLUTO}          += board-pluto-power.o
obj-${CONFIG_MACH_TEGRA_PLUTO}          += board-pluto-sdhci.o
obj-${CONFIG_MACH_TEGRA_PLUTO}          += board-pluto-panel.o
obj-${CONFIG_MACH_TEGRA_PLUTO}          += board-pluto-kbc.o
obj-${CONFIG_MACH_TEGRA_PLUTO}          += board-pluto-sensors.o
obj-${CONFIG_MACH_TEGRA_PLUTO}          += board-pluto-powermon.o
obj-${CONFIG_MACH_TEGRA_PLUTO}          += panel-l-720p-5.o
obj-${CONFIG_MACH_TEGRA_PLUTO}          += panel-j-720p-4-7.o
obj-${CONFIG_MACH_TEGRA_PLUTO}          += panel-s-1080p-5.o

obj-${CONFIG_TEGRA_BB_XMM_POWER}        += baseband-xmm-power.o
obj-${CONFIG_TEGRA_BB_XMM_POWER2}       += baseband-xmm-power2.o

obj-${CONFIG_TEGRA_ISOMGR}              += isomgr.o

obj-${CONFIG_TEGRA_NVDUMPER}            += nvdumper.o

obj-${CONFIG_MACH_ROTH}                 += board-roth.o
obj-${CONFIG_MACH_ROTH}                 += board-roth-memory.o
obj-${CONFIG_MACH_ROTH}                 += board-roth-pinmux.o
obj-${CONFIG_MACH_ROTH}                 += board-roth-power.o
obj-${CONFIG_MACH_ROTH}                 += board-roth-powermon.o
obj-${CONFIG_MACH_ROTH}                 += board-roth-sdhci.o
obj-${CONFIG_MACH_ROTH}                 += board-roth-panel.o
obj-${CONFIG_MACH_ROTH}                 += board-roth-kbc.o
obj-${CONFIG_MACH_ROTH}                 += board-roth-sensors.o
obj-${CONFIG_MACH_ROTH}                 += board-roth-fan.o
