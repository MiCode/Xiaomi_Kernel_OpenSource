CFLAGS_scm.o :=$(call as-instr,.arch_extension sec,-DREQUIRES_SEC=1)

obj-y += io.o dma.o memory.o
ifndef CONFIG_ARM_ARCH_TIMER
obj-y += timer.o
endif
obj-y += clock.o clock-voter.o clock-dummy.o clock-generic.o
obj-y += modem_notifier.o
obj-$(CONFIG_USE_OF) += board-dt.o
obj-$(CONFIG_DEBUG_FS) += nohlt.o clock-debug.o
obj-$(CONFIG_KEXEC) += msm_kexec.o

obj-$(CONFIG_MSM_PROC_COMM) += proc_comm.o
ifndef CONFIG_ARCH_MSM8X60
	obj-$(CONFIG_MSM_PROC_COMM) += clock-pcom.o
	obj-$(CONFIG_MSM_PROC_COMM) += vreg.o mpp.o
	ifdef CONFIG_MSM_PROC_COMM
ifndef CONFIG_ARCH_FSM9XXX
		obj-$(CONFIG_REGULATOR) += footswitch-pcom.o
endif
		obj-$(CONFIG_DEBUG_FS) += pmic_debugfs.o
	endif
endif

obj-y += acpuclock.o
obj-$(CONFIG_MSM_CORTEX_A7) += clock-a7.o
obj-$(CONFIG_HW_PERF_EVENTS) += perf_trace_counters.o
obj-$(CONFIG_ARCH_MSM_KRAIT) += clock-krait.o
obj-$(CONFIG_ARCH_MSM7X27) += acpuclock-7627.o acpuclock-8625q.o clock-pll.o
obj-$(CONFIG_ARCH_MSM_SCORPIONMP) += perf_event_msm_l2.o
ifndef CONFIG_OF
obj-$(CONFIG_HW_PERF_EVENTS) += pmu.o
endif
obj-$(CONFIG_ARCH_MSM_KRAIT) += msm-krait-l2-accessors.o perf_event_msm_krait_l2.o
obj-$(CONFIG_ARCH_MSM_KRAIT) += krait-scm.o
ifdef CONFIG_HW_PERF_EVENTS
obj-$(CONFIG_ARCH_MSM7X27A) += perf_event_msm_pl310.o
obj-$(CONFIG_ARCH_MSM9625) += perf_event_msm_pl310.o
obj-$(CONFIG_ARCH_MSM8625) += perf_event_msm_pl310.o
obj-$(CONFIG_ARCH_MSM9615) += perf_event_msm_pl310.o
obj-$(CONFIG_DEBUG_FS) += perf_debug.o
endif

ifndef CONFIG_MSM_SMP
obj-$(CONFIG_ARCH_MSM_SCORPION) += msm_fault_handlers.o
endif

obj-$(CONFIG_MSM_VIC) += irq-vic.o

ifdef CONFIG_ARCH_QSD8X50
	obj-$(CONFIG_MSM_SOC_REV_NONE) += acpuclock-8x50.o
endif

obj-$(CONFIG_SMP) += headsmp.o platsmp.o
ifdef CONFIG_ARCH_MSM8625
	obj-$(CONFIG_SMP) += platsmp-8625.o
endif
obj-$(CONFIG_HOTPLUG_CPU) += hotplug.o

obj-$(CONFIG_MSM_AVS_HW) += avs.o
obj-$(CONFIG_CPU_V6) += idle-v6.o
obj-$(CONFIG_CPU_V7) += idle-v7.o
obj-$(CONFIG_MSM_JTAG) += jtag-fuse.o jtag.o
obj-$(CONFIG_MSM_JTAG_MM) +=  jtag-fuse.o jtag-mm.o

msm-etm-objs := etm.o
obj-$(CONFIG_MSM_ETM) += msm-etm.o

quiet_cmd_mkrpcsym = MKCAP   $@
      cmd_mkrpcsym = $(PERL) $(srctree)/$(src)/mkrpcsym.pl $< $@

target += smd_rpc_sym.c
$(obj)/smd_rpc_sym.c: $(src)/smd_rpc_sym $(src)/mkrpcsym.pl
	$(call if_changed,mkrpcsym)

obj-$(CONFIG_MSM_SMD) += smd.o smd_debug.o remote_spinlock.o smd_private.o smem.o smd_init_dt.o smd_init_plat.o
obj-$(CONFIG_MSM_SMP2P) += smp2p.o smp2p_debug.o smp2p_gpio.o
obj-$(CONFIG_MSM_SMP2P_TEST) += smp2p_loopback.o smp2p_test.o smp2p_gpio_test.o smp2p_spinlock_test.o
obj-$(CONFIG_MSM_SCM) += scm.o scm-boot.o
obj-$(CONFIG_MSM_XPU_ERR_FATAL) += scm-xpu.o
obj-$(CONFIG_MSM_SECURE_IO) += scm-io.o
obj-$(CONFIG_MSM_PIL) += peripheral-loader.o
obj-$(CONFIG_MSM_PIL) += scm-pas.o
obj-$(CONFIG_MSM_PIL_QDSP6V3) += pil-q6v3.o
obj-$(CONFIG_MSM_PIL_LPASS_QDSP6V4) += pil-q6v4.o pil-q6v4-lpass.o
obj-$(CONFIG_MSM_PIL_MODEM_QDSP6V4) += pil-q6v4.o pil-q6v4-mss.o
obj-$(CONFIG_MSM_PIL_LPASS_QDSP6V5) += pil-q6v5.o pil-q6v5-lpass.o
obj-$(CONFIG_MSM_PIL_MSS_QDSP6V5) += pil-q6v5.o pil-msa.o pil-q6v5-mss.o
obj-$(CONFIG_MSM_PIL_RIVA) += pil-riva.o
obj-$(CONFIG_MSM_PIL_TZAPPS) += pil-tzapps.o
obj-$(CONFIG_MSM_PIL_VIDC) += pil-vidc.o
obj-$(CONFIG_MSM_PIL_MODEM) += pil-modem.o
obj-$(CONFIG_MSM_PIL_DSPS) += pil-dsps.o
obj-$(CONFIG_MSM_PIL_GSS) += pil-gss.o
obj-$(CONFIG_MSM_PIL_PRONTO) += pil-pronto.o
obj-$(CONFIG_MSM_PIL_VENUS) += pil-venus.o
obj-$(CONFIG_ARCH_QSD8X50) += sirc.o
obj-$(CONFIG_ARCH_FSM9XXX) += sirc-fsm9xxx.o
obj-$(CONFIG_MSM_FIQ_SUPPORT) += fiq_glue.o
obj-$(CONFIG_MACH_TROUT) += board-trout-rfkill.o
obj-$(CONFIG_MSM_BAM_DMUX) += bam_dmux.o
obj-$(CONFIG_MSM_SMD_LOGGING) += smem_log.o
obj-$(CONFIG_MSM_COMMON_LOG) += common_log.o
obj-$(CONFIG_MSM_IPC_LOGGING) += ipc_logging.o
ifdef CONFIG_DEBUG_FS
obj-$(CONFIG_MSM_IPC_LOGGING) += ipc_logging_debug.o
endif
obj-y += socinfo.o
obj-$(CONFIG_ARCH_MSM7X01A) += nand_partitions.o
obj-$(CONFIG_ARCH_MSM7X25) += nand_partitions.o
obj-$(CONFIG_ARCH_MSM7X27) += nand_partitions.o
obj-$(CONFIG_ARCH_MSM7X30) += nand_partitions.o
obj-$(CONFIG_ARCH_QSD8X50) += nand_partitions.o
obj-$(CONFIG_ARCH_FSM9XXX) += nand_partitions.o
obj-$(CONFIG_ARCH_MSM9615) += nand_partitions.o
obj-$(CONFIG_ARCH_MSM8625) += nand_partitions.o
obj-$(CONFIG_ARCH_MSM7X27A) += nand_partitions.o
obj-$(CONFIG_MSM_SMD_TTY) += smd_tty.o
obj-$(CONFIG_MSM_SMD_QMI) += smd_qmi.o
obj-$(CONFIG_MSM_SMD_PKT) += smd_pkt.o
obj-$(CONFIG_MSM_DSPS) += msm_dsps.o
obj-$(CONFIG_MSM_SMD_NMEA) += smd_nmea.o
obj-$(CONFIG_MSM_RESET_MODEM) += reset_modem.o
obj-$(CONFIG_MSM_IPC_ROUTER_SMD_XPRT) += ipc_router_smd_xprt.o
obj-$(CONFIG_MSM_IPC_ROUTER_HSIC_XPRT) += ipc_router_hsic_xprt.o
obj-$(CONFIG_MSM_ONCRPCROUTER) += smd_rpcrouter.o
obj-$(CONFIG_MSM_ONCRPCROUTER) += smd_rpcrouter_device.o
obj-$(CONFIG_MSM_IPC_ROUTER) += ipc_router.o
obj-$(CONFIG_MSM_IPC_ROUTER)+= ipc_socket.o
obj-$(CONFIG_MSM_IPC_ROUTER_SECURITY)+= msm_ipc_router_security.o
obj-$(CONFIG_MSM_QMI_INTERFACE) += msm_qmi_interface.o
obj-$(CONFIG_MSM_TEST_QMI_CLIENT) += kernel_test_service_v01.o test_qmi_client.o
obj-$(CONFIG_DEBUG_FS) += smd_rpc_sym.o
obj-$(CONFIG_MSM_ONCRPCROUTER) += smd_rpcrouter_servers.o
obj-$(CONFIG_MSM_ONCRPCROUTER) += smd_rpcrouter_clients.o
obj-$(CONFIG_MSM_ONCRPCROUTER) += smd_rpcrouter_xdr.o
obj-$(CONFIG_MSM_ONCRPCROUTER) += rpcrouter_smd_xprt.o
obj-$(CONFIG_MSM_RPC_PING) += ping_mdm_rpc_client.o
obj-$(CONFIG_MSM_RPC_PROC_COMM_TEST) += proc_comm_test.o
obj-$(CONFIG_MSM_RPC_PING) += ping_mdm_rpc_client.o ping_apps_server.o
obj-$(CONFIG_MSM_RPC_OEM_RAPI) += oem_rapi_client.o
obj-$(CONFIG_MSM_RPC_WATCHDOG) += rpc_dog_keepalive.o
obj-$(CONFIG_MSM_RPCSERVER_WATCHDOG) += rpc_server_dog_keepalive.o
obj-$(CONFIG_MSM_RPCSERVER_TIME_REMOTE) += rpc_server_time_remote.o
obj-$(CONFIG_MSM_DALRPC) += dal.o
obj-$(CONFIG_MSM_DALRPC_TEST) += dal_remotetest.o
obj-$(CONFIG_ARCH_MSM7X30) += dal_axi.o
obj-$(CONFIG_ARCH_MSM7X27A) += dal_axi.o
obj-$(CONFIG_MSM_RPCSERVER_HANDSET) += rpc_server_handset.o
obj-$(CONFIG_MSM8X60_AUDIO) += qdsp6v2/
obj-$(CONFIG_MSM_AUDIO_QDSP6) += qdsp6v2/
obj-$(CONFIG_MSM_AUDIO_QDSP6V2) += qdsp6v2/
obj-$(CONFIG_MSM_HW3D) += hw3d.o
obj-$(CONFIG_PM) += pm-boot.o
obj-$(CONFIG_MSM_IDLE_STATS) += pm-stats.o
obj-$(CONFIG_MSM_PM2) += pm2.o
obj-$(CONFIG_MSM_NOPM) += no-pm.o

obj-$(CONFIG_MSM_PCIE) += pcie.o pcie_irq.o

obj-$(CONFIG_MSM_SPM_V1) += spm.o
obj-$(CONFIG_MSM_SPM_V2) += spm-v2.o spm_devices.o

obj-$(CONFIG_MSM_DMA_TEST) += dma_test.o
obj-$(CONFIG_SURF_FFA_GPIO_KEYPAD) += keypad-surf-ffa.o

obj-$(CONFIG_ARCH_MSM7X01A) += board-halibut.o devices-msm7x01a.o clock-pcom-lookup.o
obj-$(CONFIG_MACH_TROUT) += board-trout.o board-trout-gpio.o
obj-$(CONFIG_MACH_TROUT) += board-trout-keypad.o board-trout-panel.o
obj-$(CONFIG_MACH_TROUT) += htc_akm_cal.o htc_wifi_nvs.o htc_acoustic.o
obj-$(CONFIG_MACH_TROUT) += board-trout-mmc.o board-trout-wifi.o
obj-$(CONFIG_ARCH_QSD8X50) += devices-qsd8x50.o clock-pcom-lookup.o
obj-$(CONFIG_MACH_QSD8X50_SURF) += board-qsd8x50.o
obj-$(CONFIG_MACH_QSD8X50_FFA) += board-qsd8x50.o
obj-$(CONFIG_ARCH_MSM8X60) += devices-msm8x60.o clock-local.o clock-8x60.o acpuclock-8x60.o clock-pll.o
obj-$(CONFIG_ARCH_MSM8X60) += clock-rpm.o
obj-$(CONFIG_ARCH_MSM8X60) += saw-regulator.o
obj-$(CONFIG_ARCH_MSM8X60) += footswitch-8x60.o
obj-$(CONFIG_MSM_BOOT_STATS) += boot_stats.o

ifdef CONFIG_MSM_RPM_REGULATOR
obj-y += rpm-regulator.o
obj-$(CONFIG_ARCH_MSM8X60) += rpm-regulator-8660.o
obj-$(CONFIG_ARCH_MSM8960) += rpm-regulator-8960.o
obj-$(CONFIG_ARCH_MSM9615) += rpm-regulator-9615.o
obj-$(CONFIG_ARCH_MSM8930) += rpm-regulator-8930.o
obj-$(CONFIG_ARCH_APQ8064) += rpm-regulator-8960.o
endif

obj-$(CONFIG_MSM_RPM_REGULATOR_SMD) += rpm-regulator-smd.o
obj-$(CONFIG_MSM_SPM_REGULATOR) += spm-regulator.o

ifdef CONFIG_MSM_SUBSYSTEM_RESTART
	obj-y += subsystem_notif.o
	obj-y += subsystem_restart.o
	obj-y += ramdump.o
endif
obj-$(CONFIG_MSM_SYSMON_COMM) += sysmon.o

#obj-$(CONFIG_CPU_IDLE) += cpuidle.o

ifdef CONFIG_MSM_CAMERA_V4L2
	obj-$(CONFIG_ARCH_MSM8X60) += board-msm8x60-camera.o
endif
obj-$(CONFIG_ARCH_FSM9XXX) += devices-fsm9xxx.o
obj-$(CONFIG_ARCH_FSM9XXX) += clock-fsm9xxx.o clock-local.o acpuclock-fsm9xxx.o
obj-$(CONFIG_ARCH_FSM9XXX) += dfe-fsm9xxx.o rfic-fsm9xxx.o
obj-$(CONFIG_ARCH_FSM9XXX) += restart-fsm9xxx.o xo-fsm9xxx.o
obj-$(CONFIG_ARCH_FSM9900) += board-fsm9900.o board-fsm9900-gpiomux.o

obj-$(CONFIG_MSM_WATCHDOG) += msm_watchdog.o
obj-$(CONFIG_MSM_WATCHDOG) += msm_watchdog_asm.o
obj-$(CONFIG_MSM_WATCHDOG_V2) += msm_watchdog_v2.o
obj-$(CONFIG_MSM_MEMORY_DUMP) += msm_memory_dump.o
obj-$(CONFIG_MACH_MSM8X60_SURF) += board-msm8x60.o
obj-$(CONFIG_MACH_MSM8X60_FFA) += board-msm8x60.o
obj-$(CONFIG_MACH_MSM8X60_FLUID) += board-msm8x60.o
obj-$(CONFIG_MACH_MSM8X60_DRAGON) += board-msm8x60.o
obj-$(CONFIG_MACH_TYPE_MSM8X60_FUSION) += board-msm8x60.o mdm.o
obj-$(CONFIG_MACH_MSM8X60_FUSN_FFA) += board-msm8x60.o mdm.o
obj-$(CONFIG_TROUT_H2W) += board-trout-h2w.o
obj-$(CONFIG_TROUT_BATTCHG) += htc_battery.o
obj-$(CONFIG_TROUT_PWRSINK) += htc_pwrsink.o
obj-$(CONFIG_ARCH_MSM7X27) += clock-pcom-lookup.o
obj-$(CONFIG_MACH_MSM7X27_SURF) += board-msm7x27.o devices-msm7x27.o
obj-$(CONFIG_MACH_MSM7X27_FFA) += board-msm7x27.o devices-msm7x27.o
obj-$(CONFIG_ARCH_MSM7X27A) += clock-pcom-lookup.o devices-msm7x27a.o
board-7627a-all-objs += board-msm7627a-storage.o board-msm7627a-bt.o board-msm7627a-camera.o
board-7627a-all-objs += audio-7627a-devices.o
board-7627a-all-objs += board-msm7627a-display.o board-msm7627a-wlan.o board-msm7627a-io.o
obj-$(CONFIG_MACH_MSM7X27A_RUMI3) += board-msm7x27a.o board-7627a-all.o
obj-$(CONFIG_MACH_MSM7X27A_SURF) += board-msm7x27a.o board-7627a-all.o
obj-$(CONFIG_MACH_MSM7X27A_FFA) += board-msm7x27a.o board-7627a-all.o
obj-$(CONFIG_MACH_MSM7627A_QRD1) += board-qrd7627a.o board-7627a-all.o
obj-$(CONFIG_MACH_MSM7627A_QRD3) += board-qrd7627a.o board-7627a-all.o
obj-$(CONFIG_MACH_MSM7627A_EVB) += board-qrd7627a.o board-7627a-all.o
obj-$(CONFIG_ARCH_MSM8625) += msm_smem_iface.o devices-msm7x27a.o clock-pcom-lookup.o mpm-8625.o
obj-$(CONFIG_MACH_MSM8625_RUMI3) += board-msm7x27a.o
obj-$(CONFIG_MACH_MSM8625_SURF) +=  board-msm7x27a.o board-7627a-all.o
obj-$(CONFIG_MACH_MSM8625_EVB) +=  board-qrd7627a.o board-7627a-all.o
obj-$(CONFIG_MACH_MSM8625_QRD7) +=  board-qrd7627a.o board-7627a-all.o
obj-$(CONFIG_MACH_QRD_SKUD_PRIME) +=  board-qrd7627a.o board-7627a-all.o
obj-$(CONFIG_MACH_MSM8625_FFA) += board-msm7x27a.o board-7627a-all.o
obj-$(CONFIG_MACH_MSM8625_EVT) += board-msm7x27a.o board-7627a-all.o
obj-$(CONFIG_ARCH_MSM7X30) += board-msm7x30.o devices-msm7x30.o memory_topology.o
obj-$(CONFIG_ARCH_MSM7X30) += clock-local.o clock-7x30.o acpuclock-7x30.o clock-pll.o
obj-$(CONFIG_MACH_MSM7X25_SURF) += board-msm7x27.o devices-msm7x25.o
obj-$(CONFIG_MACH_MSM7X25_FFA) += board-msm7x27.o devices-msm7x25.o
obj-$(CONFIG_ARCH_MSM8960) += clock-local.o clock-dss-8960.o clock-8960.o clock-rpm.o clock-pll.o
obj-$(CONFIG_ARCH_MSM8960) += footswitch-8x60.o
obj-$(CONFIG_ARCH_MSM8960) += memory_topology.o
obj-$(CONFIG_ARCH_MSM8960) += saw-regulator.o
obj-$(CONFIG_ARCH_MSM8960) += devices-8960.o
obj-$(CONFIG_ARCH_APQ8064) += devices-8960.o devices-8064.o
board-8960-all-objs += board-8960.o board-8960-camera.o board-8960-display.o board-8960-pmic.o board-8960-storage.o board-8960-gpiomux.o
board-8930-all-objs += board-8930.o board-8930-camera.o board-8930-display.o board-8930-pmic.o board-8930-storage.o board-8930-gpiomux.o devices-8930.o board-8930-gpu.o
board-8064-all-objs += board-8064.o board-8064-pmic.o board-8064-storage.o board-8064-gpiomux.o board-8064-camera.o board-8064-display.o board-8064-gpu.o
obj-$(CONFIG_MACH_MSM8960_CDP) += board-8960-all.o board-8960-regulator.o
obj-$(CONFIG_MACH_MSM8960_MTP) += board-8960-all.o board-8960-regulator.o
obj-$(CONFIG_MACH_MSM8960_FLUID) += board-8960-all.o board-8960-regulator.o
obj-$(CONFIG_MACH_MSM8930_CDP) += board-8930-all.o board-8930-regulator-pm8038.o board-8930-regulator-pm8917.o
obj-$(CONFIG_MACH_MSM8930_MTP) += board-8930-all.o board-8930-regulator-pm8038.o board-8930-regulator-pm8917.o
obj-$(CONFIG_MACH_MSM8930_FLUID) += board-8930-all.o board-8930-regulator-pm8038.o board-8930-regulator-pm8917.o
obj-$(CONFIG_PM8921_BMS) += bms-batterydata.o bms-batterydata-desay.o batterydata-lib.o
obj-$(CONFIG_QPNP_BMS) += bms-batterydata.o bms-batterydata-desay.o batterydata-lib.o
obj-$(CONFIG_QPNP_BMS) += bms-batterydata-oem.o bms-batterydata-qrd-4v35-2000mah.o bms-batterydata-qrd-4v2-1300mah.o
obj-$(CONFIG_MACH_APQ8064_CDP) += board-8064-all.o board-8064-regulator.o
obj-$(CONFIG_MACH_APQ8064_MTP) += board-8064-all.o board-8064-regulator.o
obj-$(CONFIG_MACH_APQ8064_LIQUID) += board-8064-all.o board-8064-regulator.o
obj-$(CONFIG_MACH_MPQ8064_HRD) += board-8064-all.o board-8064-regulator.o board-8064-bt.o
obj-$(CONFIG_MACH_MPQ8064_DTV) += board-8064-all.o board-8064-regulator.o
obj-$(CONFIG_ARCH_MSM9615) += board-9615.o devices-9615.o board-9615-regulator.o board-9615-gpiomux.o board-9615-storage.o board-9615-display.o
obj-$(CONFIG_ARCH_MSM9615) += clock-local.o clock-9615.o acpuclock-9615.o clock-rpm.o clock-pll.o
obj-$(CONFIG_ARCH_APQ8084) += board-8084.o board-8084-gpiomux.o
obj-$(CONFIG_ARCH_APQ8084) += clock-8084.o
obj-$(CONFIG_ARCH_MSM8974) += board-8974.o board-8974-gpiomux.o
obj-$(CONFIG_ARCH_MSM8974) += clock-krait-8974.o
obj-$(CONFIG_ARCH_MSM8974) += clock-local2.o clock-pll.o clock-8974.o clock-rpm.o clock-voter.o clock-mdss-8974.o
obj-$(CONFIG_ARCH_MSM8974) += gdsc.o
obj-$(CONFIG_ARCH_MSM9625) += gdsc.o
obj-$(CONFIG_ARCH_MSM8226) += gdsc.o
obj-$(CONFIG_ARCH_MSM8610) += gdsc.o
obj-$(CONFIG_ARCH_MPQ8092) += gdsc.o
obj-$(CONFIG_ARCH_APQ8084) += gdsc.o
obj-$(CONFIG_KRAIT_REGULATOR) += krait-regulator.o  krait-regulator-pmic.o
obj-$(CONFIG_ARCH_MSMKRYPTON) += board-krypton.o board-krypton-gpiomux.o
obj-$(CONFIG_ARCH_MSMSAMARIUM) += board-samarium.o board-samarium-gpiomux.o
obj-$(CONFIG_ARCH_MSM9625) += board-9625.o board-9625-gpiomux.o
obj-$(CONFIG_ARCH_MSM9625) += clock-local2.o clock-pll.o clock-9625.o clock-rpm.o clock-voter.o acpuclock-9625.o acpuclock-cortex.o
obj-$(CONFIG_ARCH_MPQ8092) += board-8092.o board-8092-gpiomux.o
obj-$(CONFIG_ARCH_MPQ8092) += clock-8092.o
obj-$(CONFIG_ARCH_MSM8226) += board-8226.o board-8226-gpiomux.o
obj-$(CONFIG_ARCH_MSM8226) += clock-local2.o clock-pll.o clock-8226.o clock-rpm.o clock-voter.o clock-mdss-8974.o
obj-$(CONFIG_ARCH_MSM8226) += acpuclock-8226.o acpuclock-cortex.o
obj-$(CONFIG_ARCH_MSM8610) += board-8610.o board-8610-gpiomux.o
obj-$(CONFIG_ARCH_MSM8610) += clock-local2.o clock-pll.o clock-8610.o clock-rpm.o clock-voter.o
obj-$(CONFIG_ARCH_MSM8610) += acpuclock-8226.o acpuclock-cortex.o
obj-$(CONFIG_ARCH_MSM8610) += clock-dsi-8610.o
obj-$(CONFIG_ARCH_MSMKRYPTON) += clock-local2.o clock-pll.o clock-krypton.o clock-rpm.o clock-voter.o

obj-$(CONFIG_MSM_PM) += msm-pm.o pm-data.o ext-buck-control.o

obj-$(CONFIG_MACH_SAPPHIRE) += board-sapphire.o board-sapphire-gpio.o
obj-$(CONFIG_MACH_SAPPHIRE) += board-sapphire-keypad.o board-sapphire-panel.o
obj-$(CONFIG_MACH_SAPPHIRE) += board-sapphire-mmc.o board-sapphire-wifi.o
obj-$(CONFIG_MACH_SAPPHIRE) += board-sapphire-rfkill.o msm_vibrator.o

CFLAGS_msm_vibrator.o += -Idrivers/staging/android
CFLAGS_board-9615.o += -Idrivers/usb/gadget

obj-$(CONFIG_ARCH_FSM9XXX) += board-fsm9xxx.o

obj-$(CONFIG_TROUT_BATTCHG) += htc_battery.o

obj-$(CONFIG_HTC_PWRSINK) += htc_pwrsink.o
obj-$(CONFIG_HTC_HEADSET) += htc_headset.o
obj-$(CONFIG_MSM_RPM) += rpm.o rpm_resources.o
obj-$(CONFIG_MSM_LPM_TEST) += test-lpm.o
obj-$(CONFIG_MSM_RPM_SMD) += rpm-smd.o lpm_levels.o
obj-$(CONFIG_MSM_MPM_OF) += mpm-of.o
obj-$(CONFIG_MSM_MPM) += mpm.o
obj-$(CONFIG_MSM_RPM_STATS_LOG) += rpm_stats.o rpm_master_stat.o
obj-$(CONFIG_MSM_RPM_RBCPR_STATS_LOG) += rpm_rbcpr_stats.o
obj-$(CONFIG_MSM_RPM_RBCPR_STATS_V2_LOG) += rpm_rbcpr_stats_v2.o
obj-$(CONFIG_MSM_RPM_LOG) += rpm_log.o
obj-$(CONFIG_MSM_TZ_LOG) += tz_log.o
obj-$(CONFIG_MSM_XO) += msm_xo.o
obj-$(CONFIG_MSM_BUS_SCALING) += msm_bus/
obj-$(CONFIG_MSM_BUSPM_DEV) += msm-buspm-dev.o

obj-$(CONFIG_MSM_IOMMU)		+= iommu_domains.o
ifdef CONFIG_IOMMU_SUPPORT
obj-$(CONFIG_ARCH_MSM8960)	+= devices-iommu.o
obj-$(CONFIG_ARCH_MSM8930)	+= devices-iommu.o
obj-$(CONFIG_ARCH_MSM8064)	+= devices-iommu.o
endif

obj-$(CONFIG_MSM_EVENT_TIMER)		+= event_timer.o

ifdef CONFIG_VCM
obj-$(CONFIG_ARCH_MSM8X60) += board-msm8x60-vcm.o
endif
obj-$(CONFIG_MSM_OCMEM) += ocmem.o ocmem_allocator.o ocmem_notifier.o
obj-$(CONFIG_MSM_OCMEM) += ocmem_sched.o ocmem_api.o ocmem_rdm.o ocmem_core.o
obj-$(CONFIG_SENSORS_ADSP) += sensors_adsp.o

obj-$(CONFIG_ARCH_MSM7X27) += gpiomux-7x27.o gpiomux-v1.o gpiomux.o
obj-$(CONFIG_ARCH_MSM7X30) += gpiomux-7x30.o gpiomux-v1.o gpiomux.o
obj-$(CONFIG_ARCH_QSD8X50) += gpiomux-8x50.o gpiomux-v1.o gpiomux.o
obj-$(CONFIG_ARCH_MSM8X60) += gpiomux-8x60.o gpiomux-v2.o gpiomux.o
obj-$(CONFIG_ARCH_MSM8960) += gpiomux-v2.o gpiomux.o
obj-$(CONFIG_ARCH_APQ8064) += gpiomux-v2.o gpiomux.o
obj-$(CONFIG_ARCH_MSM9615) += gpiomux-v2.o gpiomux.o
obj-$(CONFIG_ARCH_MSM8974) += gpiomux-v2.o gpiomux.o
obj-$(CONFIG_ARCH_MSM9625) += gpiomux-v2.o gpiomux.o
obj-$(CONFIG_ARCH_MSMKRYPTON) += gpiomux-v2.o gpiomux.o
obj-$(CONFIG_ARCH_MSMSAMARIUM) += gpiomux-v2.o gpiomux.o
obj-$(CONFIG_ARCH_MPQ8092) += gpiomux-v2.o gpiomux.o
obj-$(CONFIG_ARCH_MSM8226) += gpiomux-v2.o gpiomux.o
obj-$(CONFIG_ARCH_MSM8610) += gpiomux-v2.o gpiomux.o
obj-$(CONFIG_ARCH_APQ8084) += gpiomux-v2.o gpiomux.o
obj-$(CONFIG_ARCH_FSM9900) += gpiomux-v2.o gpiomux.o

obj-$(CONFIG_MSM_SLEEP_STATS_DEVICE) += idle_stats_device.o
obj-$(CONFIG_MSM_DCVS) += msm_dcvs_scm.o msm_dcvs.o msm_mpdecision.o
obj-$(CONFIG_MSM_RUN_QUEUE_STATS) += msm_rq_stats.o
obj-$(CONFIG_MSM_SHOW_RESUME_IRQ) += msm_show_resume_irq.o
obj-$(CONFIG_BT_MSM_PINTEST)  += btpintest.o
obj-$(CONFIG_MSM_FAKE_BATTERY) += fish_battery.o
obj-$(CONFIG_MSM_RPC_VIBRATOR) += msm_vibrator.o
obj-$(CONFIG_MSM_NATIVE_RESTART) += restart.o
obj-$(CONFIG_MSM_MODEM_RESTART) += restart_7k.o

obj-$(CONFIG_MSM_PROC_COMM_REGULATOR) += proccomm-regulator.o
ifdef CONFIG_MSM_PROC_COMM_REGULATOR
obj-$(CONFIG_MACH_MSM7X27_SURF) += board-msm7627-regulator.o
obj-$(CONFIG_MACH_MSM7X27_FFA) += board-msm7627-regulator.o
obj-$(CONFIG_ARCH_MSM7X30) += board-msm7x30-regulator.o
obj-$(CONFIG_ARCH_MSM7X27A) += board-msm7x27a-regulator.o
endif

obj-$(CONFIG_ARCH_MSM8960) += mdm2.o mdm_common.o
obj-$(CONFIG_MSM_RTB) += msm_rtb.o
obj-$(CONFIG_MSM_CACHE_ERP) += cache_erp.o
obj-$(CONFIG_MSM_EBI_ERP) += ebi_erp.o
obj-$(CONFIG_MSM_CACHE_DUMP) += msm_cache_dump.o

obj-$(CONFIG_MSM_HSIC_SYSMON) += hsic_sysmon.o
obj-$(CONFIG_MSM_HSIC_SYSMON_TEST) += hsic_sysmon_test.o

obj-$(CONFIG_MSM_RPM_SMD) += rpm-smd.o
obj-$(CONFIG_MSM_CPR) += msm_cpr.o
obj-$(CONFIG_MSM_VP_REGULATOR) += msm_vp.o
obj-$(CONFIG_MSM_RPC_PMIC) += pmic.o
obj-$(CONFIG_MSM_RPC_USB) += rpc_hsusb.o rpc_fsusb.o
obj-$(CONFIG_MSM_RPC_PMAPP) += rpc_pmapp.o

obj-$(CONFIG_MSM_ENABLE_WDOG_DEBUG_CONTROL) += wdog_debug.o

obj-$(CONFIG_MSM_USE_USER_ACCESSIBLE_TIMERS) += timer_page.o

ifdef CONFIG_MSM_CPR
obj-$(CONFIG_DEBUG_FS) += msm_cpr-debug.o
endif
obj-$(CONFIG_MSM_FIQ) += msm7k_fiq.o
obj-$(CONFIG_MSM_FIQ) += msm7k_fiq_handler.o

obj-$(CONFIG_MEMORY_HOLE_CARVEOUT) +=  msm_mem_hole.o

obj-$(CONFIG_MSM_SMCMOD) += smcmod.o
obj-$(CONFIG_MSM_CPU_PWRCTL) +=  msm_cpu_pwrctl.o

obj-$(CONFIG_ARCH_MSM8974) += msm_mpmctr.o
obj-$(CONFIG_MSM_CPR_REGULATOR) += cpr-regulator.o
obj-$(CONFIG_CPU_FREQ_MSM) += cpufreq.o
obj-$(CONFIG_MSM_DEVFREQ_CPUBW) += devfreq_cpubw.o

obj-$(CONFIG_WALL_CLK) += wallclk.o
obj-$(CONFIG_WALL_CLK_SYSFS) += wallclk_sysfs.o
obj-$(CONFIG_ARCH_RANDOM) += early_random.o
obj-$(CONFIG_PERFMAP) += perfmap.o
