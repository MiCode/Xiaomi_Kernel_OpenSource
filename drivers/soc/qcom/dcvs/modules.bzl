def register_modules(registry):
    registry.register(
        name = "drivers/soc/qcom/dcvs/bwmon",
        out = "bwmon.ko",
        config = "CONFIG_QCOM_BWMON",
        srcs = [
            # do not sort
            "drivers/soc/qcom/dcvs/bwmon.c",
            "drivers/soc/qcom/dcvs/bwmon.h",
            "drivers/soc/qcom/dcvs/trace-dcvs.h",
        ],
        deps = [
            # do not sort
            "kernel/sched/walt/sched-walt",
            "drivers/soc/qcom/socinfo",
            "drivers/soc/qcom/dcvs/qcom-dcvs",
            "drivers/soc/qcom/dcvs/dcvs_fp",
            "drivers/soc/qcom/qcom_rpmh",
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
            "drivers/mihw/mist",
        ],
    )

    registry.register(
        name = "drivers/soc/qcom/dcvs/dcvs_fp",
        out = "dcvs_fp.ko",
        config = "CONFIG_QCOM_DCVS_FP",
        srcs = [
            # do not sort
            "drivers/soc/qcom/dcvs/dcvs_fp.c",
            "drivers/soc/qcom/dcvs/dcvs_private.h",
        ],
        deps = [
            # do not sort
            "drivers/soc/qcom/qcom_rpmh",
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
        name = "drivers/soc/qcom/dcvs/memlat",
        out = "memlat.ko",
        config = "CONFIG_QCOM_MEMLAT",
        srcs = [
            # do not sort
            "drivers/soc/qcom/dcvs/memlat.c",
            "drivers/soc/qcom/dcvs/trace-dcvs.h",
        ],
        deps = [
            # do not sort
            "drivers/soc/qcom/dcvs/qcom-pmu-lib",
            "drivers/perf/qcom_llcc_pmu",
            "drivers/soc/qcom/cpu_phys_log_map",
            "drivers/soc/qcom/dcvs/qcom-dcvs",
            "drivers/soc/qcom/dcvs/dcvs_fp",
            "drivers/soc/qcom/dcvs/qcom_scmi_client",
            "drivers/soc/qcom/qcom_rpmh",
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
            "drivers/mihw/mist",
        ],
    )

    registry.register(
        name = "drivers/soc/qcom/dcvs/qcom-dcvs",
        out = "qcom-dcvs.ko",
        config = "CONFIG_QCOM_DCVS",
        srcs = [
            # do not sort
            "drivers/soc/qcom/dcvs/dcvs.c",
            "drivers/soc/qcom/dcvs/dcvs_epss.c",
            "drivers/soc/qcom/dcvs/dcvs_icc.c",
            "drivers/soc/qcom/dcvs/dcvs_private.h",
            "drivers/soc/qcom/dcvs/trace-dcvs.c",
            "drivers/soc/qcom/dcvs/trace-dcvs.h",
        ],
        deps = [
            # do not sort
            "drivers/soc/qcom/dcvs/dcvs_fp",
            "drivers/soc/qcom/qcom_rpmh",
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
        name = "drivers/soc/qcom/dcvs/qcom-pmu-lib",
        out = "qcom-pmu-lib.ko",
        config = "CONFIG_QCOM_PMU_LIB",
        srcs = [
            # do not sort
            "drivers/soc/qcom/dcvs/pmu_lib.c",
        ],
        deps = [
            # do not sort
            "drivers/perf/qcom_llcc_pmu",
            "drivers/soc/qcom/cpu_phys_log_map",
            "drivers/soc/qcom/dcvs/qcom_scmi_client",
        ],
    )

    registry.register(
        name = "drivers/soc/qcom/dcvs/qcom_scmi_client",
        out = "qcom_scmi_client.ko",
        config = "CONFIG_QTI_QCOM_SCMI_CLIENT",
        srcs = [
            # do not sort
            "drivers/soc/qcom/dcvs/qcom_scmi_client.c",
        ],
    )

    registry.register(
        name = "drivers/soc/qcom/dcvs/llcc_miss",
        out = "llcc_miss.ko",
        config = "CONFIG_QCOM_LLCC_MISS",
        srcs = [
            # do not sort
            "drivers/soc/qcom/dcvs/llcc_miss.c",
            "drivers/soc/qcom/dcvs/bus_prof.h",
            "drivers/soc/qcom/dcvs/trace-bus-prof.h",
        ],
        deps = [
            # do not sort
            "drivers/firmware/qcom/qcom-scm",
            "drivers/soc/qcom/dcvs/trace-bus-prof",
        ],
    )

    registry.register(
        name = "drivers/soc/qcom/dcvs/trace-bus-prof",
        out = "trace-bus-prof.ko",
        config = "CONFIG_QCOM_BUS_PROF",
        srcs = [
            # do not sort
            "drivers/soc/qcom/dcvs/bus_prof.h",
            "drivers/soc/qcom/dcvs/trace-bus-prof.c",
            "drivers/soc/qcom/dcvs/trace-bus-prof.h",
        ],
        deps = [
            # do not sort
            "drivers/firmware/qcom/qcom-scm",
        ],
    )

    registry.register(
        name = "drivers/soc/qcom/dcvs/llcc_occ",
        out = "llcc_occ.ko",
        config = "CONFIG_QCOM_LLCC_OCCUPANCY",
        srcs = [
            # do not sort
            "drivers/soc/qcom/dcvs/llcc_occ.c",
            "drivers/soc/qcom/dcvs/bus_prof.h",
            "drivers/soc/qcom/dcvs/trace-bus-prof.h",
        ],
        deps = [
            # do not sort
            "drivers/firmware/qcom/qcom-scm",
            "drivers/soc/qcom/dcvs/trace-bus-prof",
        ],
    )

    registry.register(
        name = "drivers/soc/qcom/dcvs/mem_lat_prof",
        out = "mem_lat_prof.ko",
        config = "CONFIG_QCOM_BUS_LAT",
        srcs = [
            # do not sort
            "drivers/soc/qcom/dcvs/mem_lat_prof.c",
            "drivers/soc/qcom/dcvs/smci_mem_lat.h",
            "drivers/soc/qcom/dcvs/bus_prof.h",
            "drivers/soc/qcom/dcvs/trace-bus-prof.h",
        ],
        deps = [
            # do not sort
            "drivers/firmware/qcom/qcom-scm",
            "drivers/soc/qcom/dcvs/trace-bus-prof",
            "drivers/firmware/qcom/si_core/si_core_module",
        ],
    )
