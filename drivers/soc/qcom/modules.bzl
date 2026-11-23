load(":drivers/soc/qcom/dcvs/modules.bzl", register_dcvs = "register_modules")
load(":drivers/soc/qcom/hab/modules.bzl", register_hab = "register_modules")
load(":drivers/soc/qcom/mem_buf/modules.bzl", register_mem_buf = "register_modules")
load(":drivers/soc/qcom/memshare/modules.bzl", register_memshare = "register_modules")
load(":drivers/soc/qcom/mpam/modules.bzl", register_mpam = "register_modules")
load(":drivers/soc/qcom/qpace/modules.bzl", register_qpace = "register_modules")
load(":drivers/soc/qcom/sps/modules.bzl", register_sps = "register_modules")

def register_modules(registry):
    register_dcvs(registry)
    register_mem_buf(registry)
    register_memshare(registry)
    register_sps(registry)
    register_qpace(registry)
    register_mpam(registry)
    register_hab(registry)

    registry.register(
        name = "drivers/soc/qcom/mem-prot",
        out = "mem-prot.ko",
        config = "CONFIG_MEM_PROT",
        srcs = [
            # do not sort
            "drivers/soc/qcom/mem-prot.c",
            "include/linux/mem-prot.h",
        ],
        deps = [
            # do not sort
            "drivers/firmware/qcom/qcom-scm",
        ],
    )

    registry.register(
        name = "drivers/soc/qcom/cx-ipeak",
        out = "cx-ipeak.ko",
        config = "CONFIG_QCOM_CX_IPEAK",
        srcs = [
            # do not sort
            "drivers/soc/qcom/cx_ipeak.c",
            "include/soc/qcom/cx_ipeak.h",
        ],
        deps = [
            # do not sort
        ],
    )

    registry.register(
        name = "drivers/soc/qcom/adsp_sleepmon",
        out = "adsp_sleepmon.ko",
        config = "CONFIG_QCOM_ADSP_SLEEPMON",
        srcs = [
            # do not sort
            "drivers/soc/qcom/adsp_sleepmon.c",
            "drivers/remoteproc/qcom_common.h",
        ],
        deps = [
            # do not sort
            "drivers/remoteproc/qcom_q6v5_pas",
            "drivers/remoteproc/qcom_pil_info",
            "drivers/soc/qcom/mdt_loader",
            "drivers/soc/qcom/qcom_ramdump",
            "drivers/remoteproc/qcom_q6v5",
            "drivers/remoteproc/qcom_sysmon",
            "drivers/soc/qcom/qcom_aoss",
            "drivers/soc/qcom/qmi_helpers",
            "drivers/remoteproc/rproc_qcom_common",
            "drivers/rpmsg/qcom_smd",
            "drivers/rpmsg/qcom_glink_smem",
            "drivers/rpmsg/qcom_glink",
            "kernel/trace/qcom_ipc_logging",
            "drivers/soc/qcom/minidump",
            "drivers/soc/qcom/smem",
            "drivers/soc/qcom/debug_symbol",
            "drivers/dma-buf/heaps/qcom_dma_heaps",
            "drivers/iommu/msm_dma_iommu_mapping",
            "drivers/soc/qcom/mem_buf/mem_buf_dev",
            "drivers/soc/qcom/secure_buffer",
            "drivers/firmware/qcom/qcom-scm",
            "drivers/virt/gunyah/gh_rm_drv",
            "drivers/virt/gunyah/gh_msgq",
            "drivers/virt/gunyah/gh_dbl",
            "arch/arm64/gunyah/gh_arm_drv",
        ],
    )

    registry.register(
        name = "drivers/soc/qcom/altmode-glink",
        out = "altmode-glink.ko",
        config = "CONFIG_QTI_ALTMODE_GLINK",
        srcs = [
            # do not sort
            "drivers/soc/qcom/altmode-glink.c",
        ],
        deps = [
            # do not sort
            "drivers/soc/qcom/qti_pmic_glink",
            "drivers/soc/qcom/pdr_interface",
            "drivers/soc/qcom/qmi_helpers",
            "drivers/remoteproc/rproc_qcom_common",
            "drivers/rpmsg/qcom_smd",
            "drivers/rpmsg/qcom_glink_smem",
            "drivers/rpmsg/qcom_glink",
            "kernel/trace/qcom_ipc_logging",
            "drivers/soc/qcom/minidump",
            "drivers/soc/qcom/smem",
            "drivers/soc/qcom/debug_symbol",
            "drivers/dma-buf/heaps/qcom_dma_heaps",
            "drivers/iommu/msm_dma_iommu_mapping",
            "drivers/soc/qcom/mem_buf/mem_buf_dev",
            "drivers/soc/qcom/secure_buffer",
            "drivers/firmware/qcom/qcom-scm",
            "drivers/virt/gunyah/gh_rm_drv",
            "drivers/virt/gunyah/gh_msgq",
            "drivers/virt/gunyah/gh_dbl",
            "arch/arm64/gunyah/gh_arm_drv",
        ],
    )

    registry.register(
        name = "drivers/soc/qcom/boot_stats",
        out = "boot_stats.ko",
        config = "CONFIG_MSM_BOOT_STATS",
        srcs = [
            # do not sort
            "drivers/soc/qcom/boot_stats.c",
        ],
    )

    registry.register(
        name = "drivers/soc/qcom/cdsprm",
        out = "cdsprm.ko",
        config = "CONFIG_QCOM_CDSP_RM",
        srcs = [
            # do not sort
            "drivers/soc/qcom/cdsprm.c",
        ],
    )

    registry.register(
        name = "drivers/soc/qcom/charger-ulog-glink",
        out = "charger-ulog-glink.ko",
        config = "CONFIG_QTI_CHARGER_ULOG_GLINK",
        srcs = [
            # do not sort
            "drivers/soc/qcom/charger-ulog-glink.c",
        ],
        deps = [
            # do not sort
            "drivers/soc/qcom/qti_pmic_glink",
            "drivers/soc/qcom/pdr_interface",
            "drivers/soc/qcom/qmi_helpers",
            "drivers/remoteproc/rproc_qcom_common",
            "drivers/rpmsg/qcom_smd",
            "drivers/rpmsg/qcom_glink_smem",
            "drivers/rpmsg/qcom_glink",
            "kernel/trace/qcom_ipc_logging",
            "drivers/soc/qcom/minidump",
            "drivers/soc/qcom/smem",
            "drivers/soc/qcom/debug_symbol",
            "drivers/dma-buf/heaps/qcom_dma_heaps",
            "drivers/iommu/msm_dma_iommu_mapping",
            "drivers/soc/qcom/mem_buf/mem_buf_dev",
            "drivers/soc/qcom/secure_buffer",
            "drivers/firmware/qcom/qcom-scm",
            "drivers/virt/gunyah/gh_rm_drv",
            "drivers/virt/gunyah/gh_msgq",
            "drivers/virt/gunyah/gh_dbl",
            "arch/arm64/gunyah/gh_arm_drv",
            "drivers/power/supply/mca/mca_common/mca_log",
        ],
    )

    registry.register(
        name = "drivers/soc/qcom/cmd-db",
        out = "cmd-db.ko",
        config = "CONFIG_QCOM_COMMAND_DB",
        srcs = [
            # do not sort
            "drivers/soc/qcom/cmd-db.c",
        ],
    )

    registry.register(
        name = "drivers/soc/qcom/cpucp_log",
        out = "cpucp_log.ko",
        config = "CONFIG_QTI_CPUCP_LOG",
        srcs = [
            # do not sort
            "drivers/soc/qcom/cpucp_log.c",
            "drivers/soc/qcom/trace_cpucp.h",
        ],
        deps = [
            # do not sort
            "drivers/soc/qcom/dcvs/qcom_scmi_client",
        ],
    )

    registry.register(
        name = "drivers/soc/qcom/cpuss_telemetry",
        out = "cpuss_telemetry.ko",
        config = "CONFIG_QCOM_CPUSS_TELEMETRY",
        srcs = [
            # do not sort
            "drivers/soc/qcom/cpuss_telemetry.c",
        ],
        deps = [
            # do not sort
            "drivers/soc/qcom/dcvs/qcom_scmi_client",
        ],
    )

    registry.register(
        name = "drivers/soc/qcom/cpu_phys_log_map",
        out = "cpu_phys_log_map.ko",
        config = "CONFIG_QCOM_CPU_PHYS_LOG_MAP",
        srcs = [
            # do not sort
            "drivers/soc/qcom/cpu_phys_log_map.c",
        ],
    )

    registry.register(
        name = "drivers/soc/qcom/crm-v2",
        out = "crm-v2.ko",
        config = "CONFIG_QCOM_CRM_V2",
        srcs = [
            # do not sort
            "drivers/soc/qcom/crm-v2.c",
            "drivers/soc/qcom/trace-crm.h",
        ],
        deps = [
            # do not sort
            "kernel/trace/qcom_ipc_logging",
            "drivers/soc/qcom/minidump",
            "drivers/soc/qcom/smem",
            "drivers/soc/qcom/debug_symbol",
            "drivers/dma-buf/heaps/qcom_dma_heaps",
            "drivers/iommu/msm_dma_iommu_mapping",
            "drivers/soc/qcom/mem_buf/mem_buf_dev",
            "drivers/soc/qcom/secure_buffer",
            "drivers/firmware/qcom/qcom-scm",
            "drivers/virt/gunyah/gh_rm_drv",
            "drivers/virt/gunyah/gh_msgq",
            "drivers/virt/gunyah/gh_dbl",
            "arch/arm64/gunyah/gh_arm_drv",
        ],
    )

    registry.register(
        name = "drivers/soc/qcom/dcc_v2",
        out = "dcc_v2.ko",
        config = "CONFIG_QCOM_DCC_V2",
        srcs = [
            # do not sort
            "drivers/soc/qcom/dcc_v2.c",
        ],
        deps = [
            # do not sort
            "drivers/soc/qcom/minidump",
            "drivers/soc/qcom/smem",
            "drivers/soc/qcom/debug_symbol",
            "drivers/dma-buf/heaps/qcom_dma_heaps",
            "drivers/iommu/msm_dma_iommu_mapping",
            "drivers/soc/qcom/mem_buf/mem_buf_dev",
            "drivers/soc/qcom/secure_buffer",
            "drivers/firmware/qcom/qcom-scm",
            "drivers/virt/gunyah/gh_rm_drv",
            "drivers/virt/gunyah/gh_msgq",
            "drivers/virt/gunyah/gh_dbl",
            "arch/arm64/gunyah/gh_arm_drv",
        ],
    )

    registry.register(
        name = "drivers/soc/qcom/debug_symbol",
        out = "debug_symbol.ko",
        config = "CONFIG_QCOM_DEBUG_SYMBOL",
        srcs = [
            # do not sort
            "drivers/soc/qcom/debug_symbol.c",
            "drivers/soc/qcom/debug_symbol.h",
        ],
    )

    registry.register(
        name = "drivers/soc/qcom/dmesg_dumper",
        out = "dmesg_dumper.ko",
        config = "CONFIG_QCOM_VM_DMESG_DUMPER",
        srcs = [
            # do not sort
            "drivers/soc/qcom/dmesg_dumper.c",
            "drivers/soc/qcom/dmesg_dumper_private.h",
        ],
        conditional_srcs = {
            "CONFIG_QCOM_VM_ALIVE_LOG_ENCRYPT": {True: [
                "drivers/soc/qcom/dmesg_dumper_crypto.c",
            ]},
        },
        deps = [
            # do not sort
            "drivers/virt/gunyah/gh_panic_notifier",
            "drivers/virt/gunyah/gunyah_loader",
            "drivers/soc/qcom/mdt_loader",
            "drivers/soc/qcom/minidump",
            "drivers/soc/qcom/smem",
            "drivers/soc/qcom/debug_symbol",
            "drivers/dma-buf/heaps/qcom_dma_heaps",
            "drivers/iommu/msm_dma_iommu_mapping",
            "drivers/soc/qcom/mem_buf/mem_buf_dev",
            "drivers/soc/qcom/secure_buffer",
            "drivers/firmware/qcom/qcom-scm",
            "drivers/virt/gunyah/gh_ctrl",
            "drivers/virt/gunyah/gh_rm_drv",
            "drivers/virt/gunyah/gh_msgq",
            "drivers/virt/gunyah/gh_dbl",
            "arch/arm64/gunyah/gh_arm_drv",
        ],
    )

    registry.register(
        name = "drivers/soc/qcom/eud",
        out = "eud.ko",
        config = "CONFIG_QCOM_EUD",
        srcs = [
            # do not sort
            "drivers/soc/qcom/eud.c",
        ],
        deps = [
            # do not sort
            "drivers/firmware/qcom/qcom-scm",
            "drivers/virt/gunyah/gh_rm_drv",
            "drivers/virt/gunyah/gh_msgq",
            "drivers/virt/gunyah/gh_dbl",
            "arch/arm64/gunyah/gh_arm_drv",
        ],
    )

    registry.register(
        name = "drivers/soc/qcom/qcom_fdcvs",
        out = "qcom_fdcvs.ko",
        config = "CONFIG_QCOM_FDCVS",
        srcs = [
            # do not sort
            "drivers/soc/qcom/qcom_fdcvs.c",
        ],
        deps = [
            # do not sort
            "drivers/soc/qcom/qcom_aoss",
        ],
    )

    registry.register(
        name = "drivers/soc/qcom/gh_tlmm_vm_mem_access",
        out = "gh_tlmm_vm_mem_access.ko",
        config = "CONFIG_GH_TLMM_VM_MEM_ACCESS",
        srcs = [
            # do not sort
            "drivers/soc/qcom/gh_tlmm_vm_mem_access.c",
        ],
        deps = [
            # do not sort
            "drivers/virt/gunyah/gh_mem_notifier",
            "drivers/virt/gunyah/gunyah_loader",
            "drivers/pinctrl/qcom/pinctrl-msm",
            "drivers/firmware/qcom/qcom-scm",
            "drivers/virt/gunyah/gh_rm_drv",
            "drivers/virt/gunyah/gh_msgq",
            "drivers/virt/gunyah/gh_dbl",
        ],
    )

    registry.register(
        name = "drivers/soc/qcom/gic_intr_routing",
        out = "gic_intr_routing.ko",
        config = "CONFIG_GIC_INTERRUPT_ROUTING",
        srcs = [
            # do not sort
            "drivers/soc/qcom/gic_intr_routing.c",
            "drivers/soc/qcom/irq_internals.h",
        ],
        deps = [
            # do not sort
            "drivers/firmware/qcom/qcom-scm",
            "drivers/soc/qcom/cpu_phys_log_map",
        ],
    )

    registry.register(
        name = "drivers/soc/qcom/glink_probe",
        out = "glink_probe.ko",
        config = "CONFIG_QCOM_GLINK",
        srcs = [
            # do not sort
            "drivers/soc/qcom/glink_probe.c",
        ],
        deps = [
            # do not sort
            "drivers/rpmsg/qcom_glink_smem",
            "drivers/rpmsg/qcom_glink_spss",
            "drivers/rpmsg/qcom_glink",
            "drivers/remoteproc/rproc_qcom_common",
            "kernel/trace/qcom_ipc_logging",
        ],
    )

    registry.register(
        name = "drivers/soc/qcom/hung_task_enh",
        out = "hung_task_enh.ko",
        config = "CONFIG_QCOM_HUNG_TASK_ENH",
        srcs = [
            # do not sort
            "drivers/soc/qcom/hung_task_enh.c",
        ],
    )

    registry.register(
        name = "drivers/soc/qcom/hwkm",
        out = "hwkm.ko",
        config = "CONFIG_QTI_HW_KEY_MANAGER_V1",
        srcs = [
            # do not sort
            "drivers/soc/qcom/hwkm.c",
            "drivers/soc/qcom/hwkm_v1.h",
            "drivers/soc/qcom/hwkm_serialize.h",
            "drivers/soc/qcom/hwkmregs.h",
        ],
    )

    registry.register(
        name = "drivers/soc/qcom/llcc_perfmon",
        out = "llcc_perfmon.ko",
        config = "CONFIG_QCOM_LLCC_PERFMON",
        srcs = [
            # do not sort
            "drivers/soc/qcom/llcc_events.h",
            "drivers/soc/qcom/llcc_perfmon.c",
            "drivers/soc/qcom/llcc_perfmon.h",
        ],
    )

    registry.register(
        name = "drivers/soc/qcom/llcc_heuristics",
        out = "llcc_heuristics.ko",
        config = "CONFIG_QCOM_LLCC_HEURISTICS",
        srcs = [
            # do not sort
            "drivers/soc/qcom/llcc_heuristics.c",
        ],
        deps = [
            # do not sort
            "drivers/soc/qcom/dcvs/qcom_scmi_client",
        ],
    )

    registry.register(
        name = "drivers/soc/qcom/llcc-qcom",
        out = "llcc-qcom.ko",
        config = "CONFIG_QCOM_LLCC",
        srcs = [
            # do not sort
            "drivers/soc/qcom/llcc-qcom.c",
        ],
        deps = [
            # do not sort
            "drivers/soc/qcom/socinfo",
        ],
    )

    registry.register(
        name = "drivers/soc/qcom/llcc_perf_debug",
        out = "llcc_perf_debug.ko",
        config = "CONFIG_QCOM_LLCC_DEBUG",
        srcs = [
            # do not sort
            "drivers/soc/qcom/llcc_perf_debug.c",
        ],
    )

    registry.register(
        name = "drivers/soc/qcom/mdt_loader",
        out = "mdt_loader.ko",
        config = "CONFIG_QCOM_MDT_LOADER",
        srcs = [
            # do not sort
            "drivers/soc/qcom/mdt_loader.c",
        ],
        deps = [
            # do not sort
            "drivers/firmware/qcom/qcom-scm",
            "drivers/virt/gunyah/gh_rm_drv",
            "drivers/virt/gunyah/gh_msgq",
            "drivers/virt/gunyah/gh_dbl",
            "arch/arm64/gunyah/gh_arm_drv",
            "drivers/firmware/qcom/qcom_scm_smci",
            "drivers/firmware/qcom/si_core/si_core_module",
        ],
    )

    registry.register(
        name = "drivers/soc/qcom/mem-offline",
        out = "mem-offline.ko",
        config = "CONFIG_QCOM_MEM_OFFLINE",
        srcs = [
            # do not sort
            "drivers/soc/qcom/mem-offline.c",
        ],
        deps = [
            # do not sort
            "drivers/soc/qcom/qcom_aoss",
            "kernel/trace/qcom_ipc_logging",
            "drivers/soc/qcom/minidump",
            "drivers/soc/qcom/smem",
            "drivers/soc/qcom/debug_symbol",
            "drivers/dma-buf/heaps/qcom_dma_heaps",
            "drivers/iommu/msm_dma_iommu_mapping",
            "drivers/soc/qcom/mem_buf/mem_buf_dev",
            "drivers/soc/qcom/secure_buffer",
            "drivers/firmware/qcom/qcom-scm",
            "drivers/virt/gunyah/gh_rm_drv",
            "drivers/virt/gunyah/gh_msgq",
            "drivers/virt/gunyah/gh_dbl",
            "arch/arm64/gunyah/gh_arm_drv",
        ],
    )

    registry.register(
        name = "drivers/soc/qcom/mem-hooks",
        out = "mem-hooks.ko",
        config = "CONFIG_QCOM_MEM_HOOKS",
        srcs = [
            # do not sort
            "drivers/soc/qcom/mem-hooks.c",
        ],
        deps = [
            # do not sort
        ],
    )

    registry.register(
        name = "drivers/soc/qcom/memory_dump_v2",
        out = "memory_dump_v2.ko",
        config = "CONFIG_QCOM_MEMORY_DUMP_V2",
        srcs = [
            # do not sort
            "drivers/soc/qcom/memory_dump_v2.c",
        ],
        deps = [
            # do not sort
            "drivers/soc/qcom/minidump",
            "drivers/soc/qcom/smem",
            "drivers/soc/qcom/debug_symbol",
            "drivers/dma-buf/heaps/qcom_dma_heaps",
            "drivers/iommu/msm_dma_iommu_mapping",
            "drivers/soc/qcom/mem_buf/mem_buf_dev",
            "drivers/soc/qcom/secure_buffer",
            "drivers/firmware/qcom/qcom-scm",
            "drivers/firmware/qcom/qcom_scm_smci",
            "drivers/firmware/qcom/si_core/si_core_module",
            "drivers/virt/gunyah/gh_rm_drv",
            "drivers/virt/gunyah/gh_msgq",
            "drivers/virt/gunyah/gh_dbl",
            "arch/arm64/gunyah/gh_arm_drv",
        ],
    )

    registry.register(
        name = "drivers/soc/qcom/microdump_collector",
        out = "microdump_collector.ko",
        config = "CONFIG_QCOM_MICRODUMP",
        srcs = [
            # do not sort
            "drivers/soc/qcom/microdump_collector.c",
        ],
        deps = [
            # do not sort
            "drivers/soc/qcom/smem",
            "drivers/soc/qcom/qcom_ramdump",
            "drivers/remoteproc/rproc_qcom_common",
        ],
    )

    registry.register(
        name = "drivers/soc/qcom/minidump",
        out = "minidump.ko",
        config = "CONFIG_QCOM_MINIDUMP",
        srcs = [
            # do not sort
            "drivers/soc/qcom/debug_symbol.h",
            "drivers/soc/qcom/elf.h",
            "drivers/soc/qcom/minidump_log.c",
            "drivers/soc/qcom/minidump_memory.h",
            "drivers/soc/qcom/minidump_private.h",
            "drivers/soc/qcom/msm_minidump.c",
        ],
        conditional_srcs = {
            "CONFIG_QCOM_MINIDUMP_PANIC_MEMORY_INFO": {
                True: [
                    # do not sort
                    "drivers/soc/qcom/minidump_memory.c",
                ],
            },
            "CONFIG_QCOM_MINIDUMP_SMEM": {
                True: [
                    # do not sort
                    "drivers/soc/qcom/minidump_smem.c",
                ],
            },
            "CONFIG_QCOM_MINIDUMP_RM": {
                True: [
                    # do not sort
                    "drivers/soc/qcom/minidump_rm.c",
                ],
            },
        },
        deps = [
            # do not sort
            "drivers/soc/qcom/smem",
            "drivers/soc/qcom/debug_symbol",
            "drivers/dma-buf/heaps/qcom_dma_heaps",
            "drivers/iommu/msm_dma_iommu_mapping",
            "drivers/soc/qcom/mem_buf/mem_buf_dev",
            "drivers/soc/qcom/secure_buffer",
            "drivers/firmware/qcom/qcom-scm",
            "drivers/virt/gunyah/gh_rm_drv",
            "drivers/virt/gunyah/gh_msgq",
            "drivers/virt/gunyah/gh_dbl",
            "arch/arm64/gunyah/gh_arm_drv",
        ],
    )

    registry.register(
        name = "drivers/soc/qcom/msm_show_epoch",
        out = "msm_show_epoch.ko",
        config = "CONFIG_SHOW_SUSPEND_EPOCH",
        srcs = [
            # do not sort
            "drivers/soc/qcom/msm_show_epoch.c",
        ],
    )

    registry.register(
        name = "drivers/soc/qcom/qcom_sdei",
        out = "qcom_sdei.ko",
        config = "CONFIG_QCOM_SDEI",
        srcs = [
            # do not sort
            "drivers/soc/qcom/qcom_sdei.c",
        ],
        deps = [
            # do not sort
            "drivers/soc/qcom/minidump",
        ],
    )

    registry.register(
        name = "drivers/soc/qcom/msm_performance",
        out = "msm_performance.ko",
        config = "CONFIG_MSM_PERFORMANCE",
        srcs = [
            # do not sort
            "drivers/soc/qcom/msm_performance.c",
        ],
        deps = [
            # do not sort
            "kernel/sched/walt/sched-walt",
            "drivers/soc/qcom/socinfo",
            "drivers/soc/qcom/dcvs/qcom-pmu-lib",
            "drivers/perf/qcom_llcc_pmu",
            "drivers/soc/qcom/cpu_phys_log_map",
            "drivers/soc/qcom/dcvs/qcom_scmi_client",
            "drivers/soc/qcom/smem",
        ],
    )

    registry.register(
        name = "drivers/soc/qcom/panel_event_notifier",
        out = "panel_event_notifier.ko",
        config = "CONFIG_QCOM_PANEL_EVENT_NOTIFIER",
        srcs = [
            # do not sort
            "drivers/soc/qcom/panel_event_notifier.c",
        ],
    )

    registry.register(
        name = "drivers/soc/qcom/pcie-pdc",
        out = "pcie-pdc.ko",
        config = "CONFIG_QCOM_PCIE_PDC",
        srcs = [
            # do not sort
            "drivers/soc/qcom/pcie-pdc.c",
        ],
    )

    registry.register(
        name = "drivers/soc/qcom/power_state",
        out = "power_state.ko",
        config = "CONFIG_MSM_POWER_STATE",
        srcs = [
            # do not sort
            "drivers/soc/qcom/power_state.c",
        ],
        deps = [
            # do not sort
            "drivers/remoteproc/rproc_qcom_common",
        ],
    )

    registry.register(
        name = "drivers/soc/qcom/qcom_pd_mapper",
        out = "qcom_pd_mapper.ko",
        config = "CONFIG_QCOM_PD_MAPPER",
        srcs = [
            # do not sort
            "drivers/soc/qcom/pdr_internal.h",
            "drivers/soc/qcom/qcom_pd_mapper.c",
        ],
        deps = [
            "drivers/soc/qcom/qcom_pdr_msg",
            "drivers/soc/qcom/qmi_helpers",
        ],
    )

    registry.register(
        name = "drivers/soc/qcom/qcom_pdr_msg",
        out = "qcom_pdr_msg.ko",
        config = "CONFIG_QCOM_PDR_MSG",
        srcs = [
            # do not sort
            "drivers/soc/qcom/pdr_internal.h",
            "drivers/soc/qcom/qcom_pdr_msg.c",
        ],
        deps = [
            "drivers/soc/qcom/qmi_helpers",
        ],
    )

    registry.register(
        name = "drivers/soc/qcom/pdr_interface",
        out = "pdr_interface.ko",
        config = "CONFIG_QCOM_PDR_HELPERS",
        srcs = [
            # do not sort
            "drivers/remoteproc/qcom_common.h",
            "drivers/soc/qcom/pdr_interface.c",
            "drivers/soc/qcom/pdr_internal.h",
        ],
        deps = [
            # do not sort
            "drivers/soc/qcom/qmi_helpers",
            "drivers/remoteproc/rproc_qcom_common",
            "drivers/rpmsg/qcom_smd",
            "drivers/rpmsg/qcom_glink_smem",
            "drivers/rpmsg/qcom_glink",
            "kernel/trace/qcom_ipc_logging",
            "drivers/soc/qcom/minidump",
            "drivers/soc/qcom/smem",
            "drivers/soc/qcom/debug_symbol",
            "drivers/soc/qcom/qcom_pdr_msg",
            "drivers/dma-buf/heaps/qcom_dma_heaps",
            "drivers/iommu/msm_dma_iommu_mapping",
            "drivers/soc/qcom/mem_buf/mem_buf_dev",
            "drivers/soc/qcom/secure_buffer",
            "drivers/firmware/qcom/qcom-scm",
            "drivers/virt/gunyah/gh_rm_drv",
            "drivers/virt/gunyah/gh_msgq",
            "drivers/virt/gunyah/gh_dbl",
            "arch/arm64/gunyah/gh_arm_drv",
        ],
    )

    registry.register(
        name = "drivers/soc/qcom/pmic-glink-debug",
        out = "pmic-glink-debug.ko",
        config = "CONFIG_QTI_PMIC_GLINK_DEBUG",
        srcs = [
            # do not sort
            "drivers/soc/qcom/pmic-glink-debug.c",
        ],
        deps = [
            # do not sort
            "drivers/soc/qcom/qti_pmic_glink",
            "drivers/soc/qcom/pdr_interface",
            "drivers/soc/qcom/qmi_helpers",
            "drivers/remoteproc/rproc_qcom_common",
            "drivers/rpmsg/qcom_smd",
            "drivers/rpmsg/qcom_glink_smem",
            "drivers/rpmsg/qcom_glink",
            "kernel/trace/qcom_ipc_logging",
            "drivers/soc/qcom/minidump",
            "drivers/soc/qcom/smem",
            "drivers/soc/qcom/debug_symbol",
            "drivers/dma-buf/heaps/qcom_dma_heaps",
            "drivers/iommu/msm_dma_iommu_mapping",
            "drivers/soc/qcom/mem_buf/mem_buf_dev",
            "drivers/soc/qcom/secure_buffer",
            "drivers/firmware/qcom/qcom-scm",
            "drivers/virt/gunyah/gh_rm_drv",
            "drivers/virt/gunyah/gh_msgq",
            "drivers/virt/gunyah/gh_dbl",
            "arch/arm64/gunyah/gh_arm_drv",
        ],
    )

    registry.register(
        name = "drivers/soc/qcom/pmic-pon-log",
        out = "pmic-pon-log.ko",
        config = "CONFIG_QTI_PMIC_PON_LOG",
        srcs = [
            # do not sort
            "drivers/soc/qcom/pmic-pon-log.c",
        ],
        deps = [
            # do not sort
            "kernel/trace/qcom_ipc_logging",
            "drivers/soc/qcom/minidump",
            "drivers/soc/qcom/smem",
            "drivers/soc/qcom/debug_symbol",
            "drivers/dma-buf/heaps/qcom_dma_heaps",
            "drivers/iommu/msm_dma_iommu_mapping",
            "drivers/soc/qcom/mem_buf/mem_buf_dev",
            "drivers/soc/qcom/secure_buffer",
            "drivers/firmware/qcom/qcom-scm",
            "drivers/virt/gunyah/gh_rm_drv",
            "drivers/virt/gunyah/gh_msgq",
            "drivers/virt/gunyah/gh_dbl",
            "arch/arm64/gunyah/gh_arm_drv",
        ],
    )

    registry.register(
        name = "drivers/soc/qcom/qcom_aoss",
        out = "qcom_aoss.ko",
        config = "CONFIG_QCOM_AOSS_QMP",
        srcs = [
            # do not sort
            "drivers/soc/qcom/qcom_aoss.c",
            "drivers/soc/qcom/trace-aoss.h",
        ],
        deps = [
            # do not sort
            "kernel/trace/qcom_ipc_logging",
            "drivers/soc/qcom/minidump",
            "drivers/soc/qcom/smem",
            "drivers/soc/qcom/debug_symbol",
            "drivers/dma-buf/heaps/qcom_dma_heaps",
            "drivers/iommu/msm_dma_iommu_mapping",
            "drivers/soc/qcom/mem_buf/mem_buf_dev",
            "drivers/soc/qcom/secure_buffer",
            "drivers/firmware/qcom/qcom-scm",
            "drivers/virt/gunyah/gh_rm_drv",
            "drivers/virt/gunyah/gh_msgq",
            "drivers/virt/gunyah/gh_dbl",
            "arch/arm64/gunyah/gh_arm_drv",
        ],
    )

    registry.register(
        name = "drivers/soc/qcom/qcom_cache_allocation",
        out = "qcom_cache_allocation.ko",
        config = "CONFIG_QCOM_CACHE_ALLOCATION",
        srcs = [
            # do not sort
            "drivers/soc/qcom/qcom_cache_allocation.c",
            "drivers/soc/qcom/trace-cache_allocation.h",
        ],
        deps = [
            # do not sort
            "drivers/soc/qcom/mpam/mpam",
            "drivers/soc/qcom/mpam/mpam_msc",
        ],
    )

    registry.register(
        name = "drivers/soc/qcom/qcom_cpucp",
        out = "qcom_cpucp.ko",
        config = "CONFIG_QCOM_CPUCP",
        srcs = [
            # do not sort
            "drivers/soc/qcom/qcom_cpucp.c",
        ],
    )

    registry.register(
        name = "drivers/soc/qcom/cpucp_fast",
        out = "cpucp_fast.ko",
        config = "CONFIG_QCOM_CPUCP_FAST",
        srcs = [
            # do not sort
            "drivers/soc/qcom/cpucp_fast.c",
        ],
        deps = [
            # do not sort
            "drivers/soc/qcom/qcom_cpucp.c",
            "drivers/soc/qcom/dcvs/qcom_scmi_client",
            "kernel/sched/walt/sched-walt",
            "drivers/soc/qcom/cpu_phys_log_map",
        ],
    )

    registry.register(
        name = "drivers/soc/qcom/qcom_cpuss_sleep_stats_v4",
        out = "qcom_cpuss_sleep_stats_v4.ko",
        config = "CONFIG_QCOM_CPUSS_SLEEP_STATS_V4",
        srcs = [
            # do not sort
            "drivers/soc/qcom/qcom_cpuss_sleep_stats_v4.c",
        ],
        deps = [
            # do not sort
            "drivers/soc/qcom/cpuss_telemetry",
        ],
    )

    registry.register(
        name = "drivers/soc/qcom/qcom_cpuss_sleep_stats",
        out = "qcom_cpuss_sleep_stats.ko",
        config = "CONFIG_QCOM_CPUSS_SLEEP_STATS",
        srcs = [
            # do not sort
            "drivers/soc/qcom/qcom_cpuss_sleep_stats.c",
        ],
        deps = [
            "drivers/firmware/qcom/qcom-scm",
        ],
    )

    registry.register(
        name = "drivers/soc/qcom/qcom_cpu_vendor_hooks",
        out = "qcom_cpu_vendor_hooks.ko",
        config = "CONFIG_QCOM_CPU_VENDOR_HOOKS",
        srcs = [
            # do not sort
            "drivers/soc/qcom/debug_symbol.h",
            "drivers/soc/qcom/qcom_cpu_vendor_hooks.c",
        ],
        deps = [
            # do not sort
            "drivers/soc/qcom/qcom_wdt_core",
            "drivers/soc/qcom/minidump",
            "drivers/soc/qcom/smem",
            "drivers/soc/qcom/debug_symbol",
            "drivers/dma-buf/heaps/qcom_dma_heaps",
            "drivers/iommu/msm_dma_iommu_mapping",
            "drivers/soc/qcom/mem_buf/mem_buf_dev",
            "drivers/soc/qcom/secure_buffer",
            "drivers/firmware/qcom/qcom-scm",
            "drivers/virt/gunyah/gh_rm_drv",
            "drivers/virt/gunyah/gh_msgq",
            "drivers/virt/gunyah/gh_dbl",
            "arch/arm64/gunyah/gh_arm_drv",
        ],
    )

    registry.register(
        name = "drivers/soc/qcom/qcom_dynamic_ramoops",
        out = "qcom_dynamic_ramoops.ko",
        config = "CONFIG_QCOM_DYNAMIC_RAMOOPS",
        srcs = [
            # do not sort
            "drivers/soc/qcom/qcom_dynamic_ramoops.c",
        ],
    )

    registry.register(
        name = "drivers/soc/qcom/qcom_ice",
        out = "qcom_ice.ko",
        config = "CONFIG_QCOM_INLINE_CRYPTO_ENGINE",
        srcs = [
            # do not sort
            "drivers/soc/qcom/ice.c",
            "drivers/soc/qcom/hwkm_v1.h",
        ],
        deps = [
            # do not sort
            "drivers/firmware/qcom/qcom-scm",
            "drivers/virt/gunyah/gh_rm_drv",
            "drivers/virt/gunyah/gh_msgq",
            "drivers/virt/gunyah/gh_dbl",
            "arch/arm64/gunyah/gh_arm_drv",
            "drivers/soc/qcom/hwkm",
        ],
    )

    registry.register(
        name = "drivers/soc/qcom/qcom_logbuf_boot_log",
        out = "qcom_logbuf_boot_log.ko",
        config = "CONFIG_QCOM_LOGBUF_BOOTLOG",
        srcs = [
            # do not sort
            "drivers/soc/qcom/qcom_logbuf_boot_log.c",
        ],
        deps = [
            # do not sort
            "drivers/soc/qcom/minidump",
            "drivers/soc/qcom/smem",
            "drivers/soc/qcom/debug_symbol",
            "drivers/dma-buf/heaps/qcom_dma_heaps",
            "drivers/iommu/msm_dma_iommu_mapping",
            "drivers/soc/qcom/mem_buf/mem_buf_dev",
            "drivers/soc/qcom/secure_buffer",
            "drivers/firmware/qcom/qcom-scm",
            "drivers/virt/gunyah/gh_rm_drv",
            "drivers/virt/gunyah/gh_msgq",
            "drivers/virt/gunyah/gh_dbl",
            "arch/arm64/gunyah/gh_arm_drv",
        ],
    )

    registry.register(
        name = "drivers/soc/qcom/qcom_logbuf_vendor_hooks",
        out = "qcom_logbuf_vendor_hooks.ko",
        config = "CONFIG_QCOM_LOGBUF_VENDOR_HOOKS",
        srcs = [
            # do not sort
            "drivers/soc/qcom/debug_symbol.h",
            "drivers/soc/qcom/qcom_logbuf_vendor_hooks.c",
        ],
        deps = [
            # do not sort
            "drivers/soc/qcom/minidump",
            "drivers/soc/qcom/smem",
            "drivers/soc/qcom/debug_symbol",
            "drivers/dma-buf/heaps/qcom_dma_heaps",
            "drivers/iommu/msm_dma_iommu_mapping",
            "drivers/soc/qcom/mem_buf/mem_buf_dev",
            "drivers/soc/qcom/secure_buffer",
            "drivers/firmware/qcom/qcom-scm",
            "drivers/virt/gunyah/gh_rm_drv",
            "drivers/virt/gunyah/gh_msgq",
            "drivers/virt/gunyah/gh_dbl",
            "arch/arm64/gunyah/gh_arm_drv",
        ],
    )

    registry.register(
        name = "drivers/soc/qcom/qcom_lpm_monitor",
        out = "qcom_lpm_monitor.ko",
        config = "CONFIG_QCOM_LPM_MONITOR",
        srcs = [
            # do not sort
            "drivers/soc/qcom/qcom_lpm_monitor.c",
            "drivers/soc/qcom/qcom_lpm_monitor_service_v01.c",
            "drivers/soc/qcom/qcom_lpm_monitor_service_v01.h",
        ],
        deps = [
            # do not sort
            "drivers/soc/qcom/qmi_helpers",
        ],
    )

    registry.register(
        name = "drivers/soc/qcom/qcom_ramdump",
        out = "qcom_ramdump.ko",
        config = "CONFIG_QCOM_RAMDUMP",
        srcs = [
            # do not sort
            "drivers/soc/qcom/qcom_ramdump.c",
        ],
    )

    registry.register(
        name = "drivers/soc/qcom/qcom_rpmh",
        out = "qcom_rpmh.ko",
        config = "CONFIG_QCOM_RPMH",
        srcs = [
            # do not sort
            "drivers/soc/qcom/rpmh-internal.h",
            "drivers/soc/qcom/rpmh-rsc.c",
            "drivers/soc/qcom/rpmh.c",
            "drivers/soc/qcom/trace-rpmh.h",
        ],
        deps = [
            # do not sort
            "drivers/soc/qcom/cmd-db",
            "drivers/soc/qcom/crm-v2",
            "kernel/trace/qcom_ipc_logging",
            "drivers/soc/qcom/minidump",
            "drivers/soc/qcom/smem",
            "drivers/soc/qcom/debug_symbol",
            "drivers/dma-buf/heaps/qcom_dma_heaps",
            "drivers/iommu/msm_dma_iommu_mapping",
            "drivers/soc/qcom/mem_buf/mem_buf_dev",
            "drivers/soc/qcom/secure_buffer",
            "drivers/firmware/qcom/qcom-scm",
            "drivers/virt/gunyah/gh_rm_drv",
            "drivers/virt/gunyah/gh_msgq",
            "drivers/virt/gunyah/gh_dbl",
            "arch/arm64/gunyah/gh_arm_drv",
        ],
    )

    registry.register(
        name = "drivers/soc/qcom/rpm-smd-debug",
        out = "rpm-smd-debug.ko",
        config = "CONFIG_MSM_RPM_SMD_DEBUG",
        srcs = [
            # do not sort
            "drivers/soc/qcom/rpm-smd-debug.c",
        ],
        deps = [
            # do not sort
            "drivers/rpmsg/rpm-smd",
        ],
    )

    registry.register(
        name = "drivers/soc/qcom/qcom_soc_wdt",
        out = "qcom_soc_wdt.ko",
        config = "CONFIG_QCOM_SOC_WATCHDOG",
        srcs = [
            # do not sort
            "drivers/soc/qcom/qcom_soc_wdt.c",
        ],
        deps = [
            # do not sort
            "drivers/soc/qcom/qcom_wdt_core",
            "drivers/soc/qcom/minidump",
            "drivers/soc/qcom/smem",
            "drivers/soc/qcom/debug_symbol",
            "drivers/dma-buf/heaps/qcom_dma_heaps",
            "drivers/iommu/msm_dma_iommu_mapping",
            "drivers/soc/qcom/mem_buf/mem_buf_dev",
            "drivers/soc/qcom/secure_buffer",
            "drivers/firmware/qcom/qcom-scm",
        ],
    )

    registry.register(
        name = "drivers/soc/qcom/qcom_stats",
        out = "qcom_stats.ko",
        config = "CONFIG_QCOM_STATS",
        srcs = [
            # do not sort
            "drivers/soc/qcom/qcom_stats.c",
        ],
        deps = [
            # do not sort
            "drivers/soc/qcom/qcom_aoss",
            "kernel/trace/qcom_ipc_logging",
            "drivers/soc/qcom/minidump",
            "drivers/soc/qcom/smem",
            "drivers/soc/qcom/debug_symbol",
            "drivers/dma-buf/heaps/qcom_dma_heaps",
            "drivers/iommu/msm_dma_iommu_mapping",
            "drivers/soc/qcom/mem_buf/mem_buf_dev",
            "drivers/soc/qcom/secure_buffer",
            "drivers/firmware/qcom/qcom-scm",
            "drivers/virt/gunyah/gh_rm_drv",
            "drivers/virt/gunyah/gh_msgq",
            "drivers/virt/gunyah/gh_dbl",
            "arch/arm64/gunyah/gh_arm_drv",
        ],
    )

    registry.register(
        name = "drivers/soc/qcom/qcom_tlmm_vm_irqchip",
        out = "qcom_tlmm_vm_irqchip.ko",
        config = "CONFIG_QCOM_TLMM_VM_IRQCHIP",
        srcs = [
            # do not sort
            "drivers/soc/qcom/qcom_tlmm_vm_irqchip.c",
        ],
        deps = [
            # do not sort
        ],
    )

    registry.register(
        name = "drivers/soc/qcom/qcom_va_minidump",
        out = "qcom_va_minidump.ko",
        config = "CONFIG_QCOM_VA_MINIDUMP",
        srcs = [
            # do not sort
            "drivers/soc/qcom/elf.h",
            "drivers/soc/qcom/qcom_va_minidump.c",
        ],
        deps = [
            # do not sort
            "drivers/soc/qcom/minidump",
            "drivers/soc/qcom/smem",
            "drivers/soc/qcom/debug_symbol",
            "drivers/dma-buf/heaps/qcom_dma_heaps",
            "drivers/iommu/msm_dma_iommu_mapping",
            "drivers/soc/qcom/mem_buf/mem_buf_dev",
            "drivers/soc/qcom/secure_buffer",
            "drivers/firmware/qcom/qcom-scm",
            "drivers/virt/gunyah/gh_rm_drv",
            "drivers/virt/gunyah/gh_msgq",
            "drivers/virt/gunyah/gh_dbl",
            "arch/arm64/gunyah/gh_arm_drv",
        ],
    )

    registry.register(
        name = "drivers/soc/qcom/qcom_voltage_adj",
        out = "qcom_voltage_adj.ko",
        config = "CONFIG_QCOM_VADJ",
        srcs = [
            # do not sort
            "drivers/soc/qcom/qcom_voltage_adj.c",
        ],
    )

    registry.register(
        name = "drivers/soc/qcom/qcom_wdt_core",
        out = "qcom_wdt_core.ko",
        config = "CONFIG_QCOM_WDT_CORE",
        srcs = [
            # do not sort
            "drivers/soc/qcom/qcom_wdt_core.c",
        ],
        deps = [
            # do not sort
            "drivers/soc/qcom/minidump",
            "drivers/soc/qcom/smem",
            "drivers/soc/qcom/debug_symbol",
            "drivers/soc/qcom/qcom_sdei",
            "drivers/dma-buf/heaps/qcom_dma_heaps",
            "drivers/iommu/msm_dma_iommu_mapping",
            "drivers/soc/qcom/mem_buf/mem_buf_dev",
            "drivers/soc/qcom/secure_buffer",
            "drivers/firmware/qcom/qcom-scm",
            "drivers/virt/gunyah/gh_rm_drv",
            "drivers/virt/gunyah/gh_msgq",
            "drivers/virt/gunyah/gh_dbl",
            "arch/arm64/gunyah/gh_arm_drv",
        ],
    )

    registry.register(
        name = "drivers/soc/qcom/qfprom-sys",
        out = "qfprom-sys.ko",
        config = "CONFIG_QCOM_QFPROM_SYS",
        srcs = [
            # do not sort
            "drivers/soc/qcom/qfprom-sys.c",
        ],
    )

    registry.register(
        name = "drivers/soc/qcom/qmi_helpers",
        out = "qmi_helpers.ko",
        config = "CONFIG_QCOM_QMI_HELPERS",
        srcs = [
            # do not sort
            "drivers/soc/qcom/qmi_encdec.c",
            "drivers/soc/qcom/qmi_interface.c",
        ],
        deps = [
            "kernel/trace/qcom_ipc_logging",
        ],
    )

    registry.register(
        name = "drivers/soc/qcom/qsee_ipc_irq_bridge",
        out = "qsee_ipc_irq_bridge.ko",
        config = "CONFIG_QSEE_IPC_IRQ_BRIDGE",
        srcs = [
            # do not sort
            "drivers/soc/qcom/qsee_ipc_irq_bridge.c",
        ],
        deps = [
            # do not sort
            "drivers/remoteproc/rproc_qcom_common",
            "drivers/rpmsg/qcom_smd",
            "drivers/rpmsg/qcom_glink_smem",
            "drivers/rpmsg/qcom_glink",
            "kernel/trace/qcom_ipc_logging",
            "drivers/soc/qcom/minidump",
            "drivers/soc/qcom/smem",
            "drivers/soc/qcom/debug_symbol",
            "drivers/dma-buf/heaps/qcom_dma_heaps",
            "drivers/iommu/msm_dma_iommu_mapping",
            "drivers/soc/qcom/mem_buf/mem_buf_dev",
            "drivers/soc/qcom/secure_buffer",
            "drivers/firmware/qcom/qcom-scm",
            "drivers/virt/gunyah/gh_rm_drv",
            "drivers/virt/gunyah/gh_msgq",
            "drivers/virt/gunyah/gh_dbl",
            "arch/arm64/gunyah/gh_arm_drv",
        ],
    )

    registry.register(
        name = "drivers/soc/qcom/qti_battery_debug",
        out = "qti_battery_debug.ko",
        config = "CONFIG_QTI_BATTERY_GLINK_DEBUG",
        srcs = [
            # do not sort
            "drivers/soc/qcom/qti_battery_debug.c",
        ],
        deps = [
            # do not sort
            "drivers/soc/qcom/qti_pmic_glink",
            "drivers/soc/qcom/pdr_interface",
            "drivers/soc/qcom/qmi_helpers",
            "drivers/remoteproc/rproc_qcom_common",
            "drivers/rpmsg/qcom_smd",
            "drivers/rpmsg/qcom_glink_smem",
            "drivers/rpmsg/qcom_glink",
            "kernel/trace/qcom_ipc_logging",
            "drivers/soc/qcom/minidump",
            "drivers/soc/qcom/smem",
            "drivers/soc/qcom/debug_symbol",
            "drivers/dma-buf/heaps/qcom_dma_heaps",
            "drivers/iommu/msm_dma_iommu_mapping",
            "drivers/soc/qcom/mem_buf/mem_buf_dev",
            "drivers/soc/qcom/secure_buffer",
            "drivers/firmware/qcom/qcom-scm",
            "drivers/virt/gunyah/gh_rm_drv",
            "drivers/virt/gunyah/gh_msgq",
            "drivers/virt/gunyah/gh_dbl",
            "arch/arm64/gunyah/gh_arm_drv",
        ],
    )

    registry.register(
        name = "drivers/soc/qcom/qti_dmof_scmi",
        out = "qti_dmof_scmi.ko",
        config = "CONFIG_QTI_DMOF_SCMI",
        srcs = [
            # do not sort
            "drivers/soc/qcom/qti_dmof_scmi.c",
        ],
        deps = [
            # do not sort
            "drivers/soc/qcom/dcvs/qcom_scmi_client",
            "drivers/soc/qcom/cpu_phys_log_map",
        ],
    )

    registry.register(
        name = "drivers/soc/qcom/qti_pmic_glink",
        out = "qti_pmic_glink.ko",
        config = "CONFIG_QTI_PMIC_GLINK",
        srcs = [
            # do not sort
            "drivers/soc/qcom/qti_pmic_glink.c",
        ],
        deps = [
            # do not sort
            "drivers/soc/qcom/pdr_interface",
            "drivers/soc/qcom/qmi_helpers",
            "drivers/remoteproc/rproc_qcom_common",
            "drivers/rpmsg/qcom_smd",
            "drivers/rpmsg/qcom_glink_smem",
            "drivers/rpmsg/qcom_glink",
            "kernel/trace/qcom_ipc_logging",
            "drivers/soc/qcom/minidump",
            "drivers/soc/qcom/smem",
            "drivers/soc/qcom/debug_symbol",
            "drivers/dma-buf/heaps/qcom_dma_heaps",
            "drivers/iommu/msm_dma_iommu_mapping",
            "drivers/soc/qcom/mem_buf/mem_buf_dev",
            "drivers/soc/qcom/secure_buffer",
            "drivers/firmware/qcom/qcom-scm",
            "drivers/virt/gunyah/gh_rm_drv",
            "drivers/virt/gunyah/gh_msgq",
            "drivers/virt/gunyah/gh_dbl",
            "arch/arm64/gunyah/gh_arm_drv",
        ],
    )

    registry.register(
        name = "drivers/soc/qcom/secure_buffer",
        out = "secure_buffer.ko",
        config = "CONFIG_QCOM_SECURE_BUFFER",
        srcs = [
            # do not sort
            "drivers/soc/qcom/secure_buffer.c",
            "drivers/soc/qcom/trace_secure_buffer.h",
        ],
        deps = [
            # do not sort
            "drivers/firmware/qcom/qcom-scm",
        ],
    )

    registry.register(
        name = "drivers/soc/qcom/slc_perfmon",
        out = "slc_perfmon.ko",
        config = "CONFIG_QCOM_SLC_PERFMON",
        srcs = [
            # do not sort
            "drivers/soc/qcom/slc_perfmon.c",
        ],
        deps = [
            "drivers/soc/qcom/dcvs/qcom_scmi_client",
        ],
    )

    registry.register(
        name = "drivers/soc/qcom/smem",
        out = "smem.ko",
        config = "CONFIG_QCOM_SMEM",
        srcs = [
            # do not sort
            "drivers/soc/qcom/smem.c",
        ],
    )

    registry.register(
        name = "drivers/soc/qcom/qcom_dbl",
        out = "qcom_dbl.ko",
        config = "CONFIG_QCOM_DBL",
        srcs = [
            # do not sort
            "drivers/soc/qcom/qcom_dbl.c",
        ],
        deps = [
            # do not sort
            "drivers/remoteproc/rproc_qcom_common",
        ],
    )

    registry.register(
        name = "drivers/soc/qcom/smp2p",
        out = "smp2p.ko",
        config = "CONFIG_QCOM_SMP2P",
        srcs = [
            # do not sort
            "drivers/soc/qcom/smp2p.c",
        ],
        deps = [
            # do not sort
            "kernel/trace/qcom_ipc_logging",
            "drivers/soc/qcom/minidump",
            "drivers/soc/qcom/smem",
            "drivers/soc/qcom/debug_symbol",
            "drivers/dma-buf/heaps/qcom_dma_heaps",
            "drivers/iommu/msm_dma_iommu_mapping",
            "drivers/soc/qcom/mem_buf/mem_buf_dev",
            "drivers/soc/qcom/secure_buffer",
            "drivers/firmware/qcom/qcom-scm",
            "drivers/virt/gunyah/gh_rm_drv",
            "drivers/virt/gunyah/gh_msgq",
            "drivers/virt/gunyah/gh_dbl",
            "arch/arm64/gunyah/gh_arm_drv",
        ],
    )

    registry.register(
        name = "drivers/soc/qcom/smsm",
        out = "smsm.ko",
        config = "CONFIG_QCOM_SMSM",
        srcs = [
            # do not sort
            "drivers/soc/qcom/smsm.c",
        ],
        deps = [
            # do not sort
            "kernel/trace/qcom_ipc_logging",
            "drivers/soc/qcom/smem",
            "drivers/soc/qcom/debug_symbol",
        ],
    )

    registry.register(
        name = "drivers/soc/qcom/smp2p_sleepstate",
        out = "smp2p_sleepstate.ko",
        config = "CONFIG_QCOM_SMP2P_SLEEPSTATE",
        srcs = [
            # do not sort
            "drivers/soc/qcom/smp2p_sleepstate.c",
        ],
        deps = [
            # do not sort
            "drivers/soc/qcom/smem",
        ],
    )

    registry.register(
        name = "drivers/soc/qcom/socinfo",
        out = "socinfo.ko",
        config = "CONFIG_QCOM_SOCINFO",
        srcs = [
            # do not sort
            "drivers/soc/qcom/socinfo.c",
        ],
        deps = [
            # do not sort
            "drivers/soc/qcom/smem",
        ],
    )

    registry.register(
        name = "drivers/soc/qcom/sysmon_subsystem_stats",
        out = "sysmon_subsystem_stats.ko",
        config = "CONFIG_QCOM_SYSMON_SUBSYSTEM_STATS",
        srcs = [
            # do not sort
            "drivers/soc/qcom/sysmon_subsystem_stats.c",
        ],
        deps = [
            # do not sort
            "drivers/soc/qcom/smem",
        ],
    )

    registry.register(
        name = "drivers/soc/qcom/sys_pm_vx",
        out = "sys_pm_vx.ko",
        config = "CONFIG_QTI_SYS_PM_VX",
        srcs = [
            # do not sort
            "drivers/soc/qcom/sys_pm_vx.c",
        ],
        deps = [
            # do not sort
            "drivers/soc/qcom/qcom_stats",
            "drivers/soc/qcom/qcom_aoss",
            "kernel/trace/qcom_ipc_logging",
            "drivers/soc/qcom/minidump",
            "drivers/soc/qcom/smem",
            "drivers/soc/qcom/debug_symbol",
            "drivers/dma-buf/heaps/qcom_dma_heaps",
            "drivers/iommu/msm_dma_iommu_mapping",
            "drivers/soc/qcom/mem_buf/mem_buf_dev",
            "drivers/soc/qcom/secure_buffer",
            "drivers/firmware/qcom/qcom-scm",
            "drivers/virt/gunyah/gh_rm_drv",
            "drivers/virt/gunyah/gh_msgq",
            "drivers/virt/gunyah/gh_dbl",
            "arch/arm64/gunyah/gh_arm_drv",
        ],
    )

    registry.register(
        name = "drivers/soc/qcom/wcd_usbss_i2c",
        out = "wcd_usbss_i2c.ko",
        config = "CONFIG_QCOM_WCD_USBSS_I2C",
        srcs = [
            # do not sort
            "drivers/soc/qcom/wcd-usbss-priv.h",
            "drivers/soc/qcom/wcd-usbss-reg-masks.h",
            "drivers/soc/qcom/wcd-usbss-reg-shifts.h",
            "drivers/soc/qcom/wcd-usbss-registers.h",
            "drivers/soc/qcom/wcd-usbss-regmap.c",
            "drivers/soc/qcom/wcd-usbss-tables.c",
            "drivers/soc/qcom/wcd-usbss-utils.c",
            "drivers/soc/qcom/wcd939x-i2c.c",
            "drivers/misc/hwid/hwid.h",
        ],
        deps = [
            # do not sort
            "drivers/usb/typec/ucsi/ucsi_qti_glink",
            "drivers/base/regmap/qti-regmap-debugfs",
            "drivers/soc/qcom/qti_pmic_glink",
            "drivers/soc/qcom/pdr_interface",
            "drivers/soc/qcom/qmi_helpers",
            "drivers/remoteproc/rproc_qcom_common",
            "drivers/rpmsg/qcom_smd",
            "drivers/rpmsg/qcom_glink_smem",
            "drivers/rpmsg/qcom_glink",
            "kernel/trace/qcom_ipc_logging",
            "drivers/soc/qcom/minidump",
            "drivers/soc/qcom/smem",
            "drivers/soc/qcom/debug_symbol",
            "drivers/dma-buf/heaps/qcom_dma_heaps",
            "drivers/iommu/msm_dma_iommu_mapping",
            "drivers/soc/qcom/mem_buf/mem_buf_dev",
            "drivers/soc/qcom/secure_buffer",
            "drivers/firmware/qcom/qcom-scm",
            "drivers/virt/gunyah/gh_rm_drv",
            "drivers/virt/gunyah/gh_msgq",
            "drivers/virt/gunyah/gh_dbl",
            "arch/arm64/gunyah/gh_arm_drv",
            "drivers/misc/hwid",
        ],
    )

    registry.register(
        name = "drivers/soc/qcom/fsa4480_i2c",
        out = "fsa4480_i2c.ko",
        config = "CONFIG_QCOM_FSA4480_I2C",
        srcs = [
            # do not sort
            "drivers/soc/qcom/fsa4480-i2c.c",
        ],
        deps = [
            # do not sort
            "drivers/usb/typec/ucsi/ucsi_qti_glink",
            "drivers/base/regmap/qti-regmap-debugfs",
            "drivers/soc/qcom/qti_pmic_glink",
            "drivers/soc/qcom/pdr_interface",
            "drivers/soc/qcom/qmi_helpers",
            "drivers/remoteproc/rproc_qcom_common",
            "drivers/rpmsg/qcom_smd",
            "drivers/rpmsg/qcom_glink_smem",
            "drivers/rpmsg/qcom_glink",
            "kernel/trace/qcom_ipc_logging",
            "drivers/soc/qcom/minidump",
            "drivers/soc/qcom/smem",
            "drivers/soc/qcom/debug_symbol",
            "drivers/dma-buf/heaps/qcom_dma_heaps",
            "drivers/iommu/msm_dma_iommu_mapping",
            "drivers/soc/qcom/mem_buf/mem_buf_dev",
            "drivers/soc/qcom/secure_buffer",
            "drivers/firmware/qcom/qcom-scm",
            "drivers/virt/gunyah/gh_rm_drv",
            "drivers/virt/gunyah/gh_msgq",
            "drivers/virt/gunyah/gh_dbl",
            "arch/arm64/gunyah/gh_arm_drv",
        ],
    )

    registry.register(
        name = "drivers/soc/qcom/rename_devices",
        out = "rename_devices.ko",
        config = "CONFIG_RENAME_DEVICES",
        srcs = [
            # do not sort
            "drivers/soc/qcom/rename_devices.c",
        ],
        deps = [
            # do not sort
            "drivers/block/virtio_blk",
        ],
    )

    registry.register(
        name = "drivers/soc/qcom/qti-pmic-lpm",
        out = "qti-pmic-lpm.ko",
        config = "CONFIG_QTI_PMIC_LPM",
        srcs = [
            # do not sort
            "drivers/soc/qcom/qti-pmic-lpm.c",
        ],
        deps = [
            # do not sort
            "drivers/soc/qcom/panel_event_notifier",
            "drivers/soc/qcom/qti_pmic_glink",
            "drivers/soc/qcom/pdr_interface",
            "drivers/soc/qcom/qmi_helpers",
            "drivers/remoteproc/rproc_qcom_common",
            "drivers/rpmsg/qcom_smd",
            "drivers/rpmsg/qcom_glink_smem",
            "drivers/rpmsg/qcom_glink",
            "kernel/trace/qcom_ipc_logging",
            "drivers/soc/qcom/minidump",
            "drivers/soc/qcom/smem",
            "drivers/soc/qcom/debug_symbol",
            "drivers/dma-buf/heaps/qcom_dma_heaps",
            "drivers/iommu/msm_dma_iommu_mapping",
            "drivers/soc/qcom/mem_buf/mem_buf_dev",
            "drivers/soc/qcom/secure_buffer",
            "drivers/firmware/qcom/qcom-scm",
            "drivers/virt/gunyah/gh_rm_drv",
            "drivers/virt/gunyah/gh_msgq",
            "drivers/virt/gunyah/gh_dbl",
            "arch/arm64/gunyah/gh_arm_drv",
        ],
    )

    registry.register(
        name = "drivers/soc/qcom/qcom-pmic-ecid",
        out = "qcom-pmic-ecid.ko",
        config = "CONFIG_QCOM_PMIC_ECID",
        srcs = [
            # do not sort
            "drivers/soc/qcom/qcom-pmic-ecid.c",
        ],
    )

    registry.register(
        name = "drivers/soc/qcom/spmi-pmic-err-debug",
        out = "spmi-pmic-err-debug.ko",
        config = "CONFIG_QCOM_SPMI_PMIC_ERR_DBG",
        srcs = [
            # do not sort
            "drivers/soc/qcom/spmi-pmic-err-debug.c",
        ],
        deps = [
            # do not sort
            "drivers/firmware/qcom/qcom-scm",
            "drivers/nvmem/qfprom.c",
        ],
    )

    registry.register(
        name = "drivers/soc/qcom/rq_stats",
        out = "rq_stats.ko",
        config = "CONFIG_QCOM_RUN_QUEUE_STATS",
        srcs = [
            # do not sort
            "drivers/soc/qcom/rq_stats.c",
        ],
    )

    registry.register(
        name = "drivers/soc/qcom/qcom-geni-se-msm",
        out = "qcom-geni-se-msm.ko",
        config = "CONFIG_QCOM_GENI_SE_MSM",
        srcs = [
            # do not sort
            "drivers/soc/qcom/qcom-geni-se-msm.c",
        ],
    )

    registry.register(
        name = "drivers/soc/qcom/crypto-qti-virt",
        out = "crypto-qti-virt.ko",
        config = "CONFIG_QTI_CRYPTO_VIRTUALIZATION",
        srcs = [
            # do not sort
            "drivers/soc/qcom/crypto-qti-virt.c",
        ],
        hdrs = [
            "include/linux/habmm.h",
        ],
        includes = ["include/linux"],
        deps = [
            # do not sort
            "drivers/soc/qcom/hab/msm_hab",
        ],
    )

    registry.register(
        name = "drivers/soc/qcom/qcom_hib",
        out = "qcom_hib.ko",
        config = "CONFIG_QCOM_S2D_VENDOR_HOOK",
        srcs = [
            # do not sort
            "drivers/soc/qcom/qcom_hib.c",
        ],
        hdrs = [
            "include/soc/qcom/qcom_hibernation.h",
        ],
        includes = ["include"],
    )

    registry.register(
        name = "drivers/soc/qcom/qcom_secure_hibernation",
        out = "qcom_secure_hibernation.ko",
        config = "CONFIG_QCOM_SECURE_HIBERNATION",
        srcs = [
            # do not sort
            "drivers/soc/qcom/qcom_secure_hibernation.c",
        ],
        deps = [
            # do not sort
            "drivers/misc/qseecom_proxy",
            "drivers/soc/qcom/qcom_hib",
            "drivers/firmware/qcom/si_core/si_core_module",
        ],
        includes = ["include"],
    )

    registry.register(
        name = "drivers/soc/qcom/minidump_virtio",
        out = "minidump_virtio.ko",
        config = "CONFIG_QCOM_MINIDUMP_VIRTIO",
        srcs = [
            # do not sort
            "drivers/soc/qcom/minidump_virtio.c",
            "drivers/soc/qcom/debug_symbol.h",
            "drivers/soc/qcom/minidump_private.h",
            "drivers/soc/qcom/elf.h",
        ],
        deps = [
            # do not sort
            "drivers/soc/qcom/minidump",
        ],
    )

    registry.register(
        name = "drivers/soc/qcom/core_hang_detect",
        out = "core_hang_detect.ko",
        config = "CONFIG_MSM_CORE_HANG_DETECT",
        srcs = [
            # do not sort
            "drivers/soc/qcom/core_hang_detect.c",
        ],
        deps = [
            # do not sort
            "drivers/firmware/qcom/qcom-scm",
        ],
        includes = ["include"],
    )

    registry.register(
        name = "drivers/soc/qcom/slatecom_event",
        out = "slatecom_event.ko",
        config = "CONFIG_MSM_SLATECOM_EVENT",
        srcs = [
            # do not sort
            "drivers/soc/qcom/slatecom_event.c",
        ],
        deps = [
            # do not sort
            "drivers/soc/qcom/slate_events_bridge",
        ],
    )

    registry.register(
        name = "drivers/soc/qcom/slate_events_bridge",
        out = "slate_events_bridge.ko",
        config = "CONFIG_MSM_SEB",
        srcs = [
            # do not sort
            "drivers/soc/qcom/slate_events_bridge_rpmsg.h",
            "drivers/soc/qcom/slate_events_bridge.h",
            "drivers/soc/qcom/slate_events_bridge.c",
        ],
        deps = [
            # do not sort
            "drivers/soc/qcom/slate_events_bridge_rpmsg",
            "drivers/remoteproc/rproc_qcom_common",
        ],
    )

    registry.register(
        name = "drivers/soc/qcom/slate_events_bridge_rpmsg",
        out = "slate_events_bridge_rpmsg.ko",
        config = "CONFIG_MSM_SEB_RPMSG",
        srcs = [
            # do not sort
            "drivers/soc/qcom/slate_events_bridge.h",
            "drivers/soc/qcom/slate_events_bridge_rpmsg.h",
            "drivers/soc/qcom/slate_events_bridge_rpmsg.c",
        ],
        deps = [
            # do not sort
            "drivers/soc/qcom/minidump",
        ],
    )

    registry.register(
        name = "drivers/soc/qcom/qcom_dpd_proxy",
        out = "qcom_dpd_proxy.ko",
        config = "CONFIG_QCOM_DPD_PROXY",
        srcs = [
            # do not sort
            "drivers/soc/qcom/qcom_dpd_proxy.c",
        ],
        deps = [
            # do not sort
            "drivers/firmware/qcom/qcom-scm",
            "drivers/soc/qcom/secure_buffer",
            "drivers/firmware/qcom/si_core/si_core_module",
        ],
    )

    registry.register(
        name = "drivers/soc/qcom/qcom-pbs",
        out = "qcom-pbs.ko",
        config = "CONFIG_QCOM_PBS",
        srcs = [
            # do not sort
            "drivers/soc/qcom/qcom-pbs.c",
        ],
        deps = [
            # do not sort
        ],
    )

    registry.register(
        name = "drivers/soc/qcom/bam_dmux",
        out = "bam_dmux.ko",
        config = "CONFIG_MSM_BAM_DMUX",
        srcs = [
            # do not sort
            "drivers/soc/qcom/bam_dmux.c",
        ],
        hdrs = [
            "drivers/soc/qcom/bam_dmux_private.h",
            "include/soc/qcom/bam_dmux.h",
        ],
        deps = [
            # do not sort
            "drivers/soc/qcom/sps/sps_drv",
            "drivers/soc/qcom/smem",
            "drivers/soc/qcom/debug_symbol",
            "drivers/remoteproc/rproc_qcom_common",
            "kernel/trace/qcom_ipc_logging",
        ],
    )
