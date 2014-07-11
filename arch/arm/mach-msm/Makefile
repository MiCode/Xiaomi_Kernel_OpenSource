obj-y += io.o dma.o memory.o
ifndef CONFIG_ARM_ARCH_TIMER
obj-y += timer.o
endif
obj-$(CONFIG_USE_OF) += board-dt.o

obj-y += acpuclock.o
obj-$(CONFIG_HW_PERF_EVENTS) += perf_trace_counters.o
obj-$(CONFIG_ARCH_MSM_KRAIT) += msm-krait-l2-accessors.o perf_event_msm_krait_l2.o
obj-$(CONFIG_ARCH_MSM_KRAIT) += krait-scm.o
obj-$(CONFIG_DEBUG_FS) += perf_debug.o

obj-$(CONFIG_SMP) += headsmp.o platsmp.o
obj-$(CONFIG_HOTPLUG_CPU) += hotplug.o

obj-$(CONFIG_MSM_TEST_QMI_CLIENT) += kernel_test_service_v01.o test_qmi_client.o

obj-$(CONFIG_MSM_PCIE) += pcie.o pcie_irq.o pcie_phy.o

obj-$(CONFIG_MSM_DMA_TEST) += dma_test.o
obj-$(CONFIG_SURF_FFA_GPIO_KEYPAD) += keypad-surf-ffa.o

obj-$(CONFIG_ARCH_FSM9900) += board-fsm9900.o board-fsm9900-gpiomux.o
obj-$(CONFIG_ARCH_FSM9900) += clock-fsm9900.o
obj-$(CONFIG_ARCH_FSM9900) += rfic-fsm9900.o bbif-fsm9900.o
obj-$(CONFIG_QPNP_BMS) += bms-batterydata.o bms-batterydata-desay.o
obj-$(CONFIG_QPNP_BMS) += bms-batterydata-oem.o bms-batterydata-qrd-4v35-2000mah.o bms-batterydata-qrd-4v2-1300mah.o
obj-$(CONFIG_ARCH_APQ8084) += board-8084.o board-8084-gpiomux.o
obj-$(CONFIG_ARCH_APQ8084) += clock-8084.o clock-mdss-8974.o
obj-$(CONFIG_ARCH_MSM8974) += board-8974.o board-8974-gpiomux.o
obj-$(CONFIG_ARCH_MSM8974) += clock-rpm-8974.o clock-gcc-8974.o clock-mmss-8974.o clock-lpass-8974.o clock-mdss-8974.o
obj-$(CONFIG_KRAIT_REGULATOR) += krait-regulator.o  krait-regulator-pmic.o
obj-$(CONFIG_ARCH_MDM9630) += board-9630.o board-9630-gpiomux.o
obj-$(CONFIG_MSM_PP2S_FEMTO) += pp2s.o
obj-$(CONFIG_ARCH_MSMFERRUM) += board-ferrum.o
obj-$(CONFIG_ARCH_MSM8916) += board-8916.o
obj-$(CONFIG_ARCH_MSM8226) += board-8226.o board-8226-gpiomux.o
obj-$(CONFIG_ARCH_MSM8226) += clock-8226.o clock-mdss-8974.o
obj-$(CONFIG_ARCH_MSM8610) += board-8610.o board-8610-gpiomux.o
obj-$(CONFIG_ARCH_MSM8610) += clock-8610.o
obj-$(CONFIG_ARCH_MSM8610) += clock-dsi-8610.o
obj-$(CONFIG_ARCH_MDM9630) += clock-9630.o
obj-$(CONFIG_ARCH_MSMZIRC) += board-zirc.o
obj-$(CONFIG_MSM_PM) += ext-buck-control.o

CFLAGS_msm_vibrator.o += -Idrivers/staging/android

obj-$(CONFIG_MSM_LPM_TEST) += test-lpm.o
obj-$(CONFIG_MSM_BUSPM_DEV) += msm-buspm-dev.o


obj-$(CONFIG_ARCH_MSM8974) += gpiomux-v2.o gpiomux.o
obj-$(CONFIG_ARCH_MDM9630) += gpiomux-v2.o gpiomux.o
obj-$(CONFIG_ARCH_MSM8226) += gpiomux-v2.o gpiomux.o
obj-$(CONFIG_ARCH_MSM8610) += gpiomux-v2.o gpiomux.o
obj-$(CONFIG_ARCH_APQ8084) += gpiomux-v2.o gpiomux.o
obj-$(CONFIG_ARCH_FSM9900) += gpiomux-v2.o gpiomux.o

obj-$(CONFIG_MSM_CACHE_ERP) += cache_erp.o

obj-$(CONFIG_MSM_HSIC_SYSMON) += hsic_sysmon.o
obj-$(CONFIG_MSM_HSIC_SYSMON_TEST) += hsic_sysmon_test.o

obj-$(CONFIG_MSM_RPC_USB) += rpc_hsusb.o rpc_fsusb.o

obj-$(CONFIG_MEMORY_HOLE_CARVEOUT) +=  msm_mem_hole.o

obj-$(CONFIG_MSM_SMCMOD) += smcmod.o

obj-$(CONFIG_ARCH_MSM8974) += msm_mpmctr.o

obj-$(CONFIG_ARCH_RANDOM) += early_random.o
obj-$(CONFIG_PERFMAP) += perfmap.o
