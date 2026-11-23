def register_modules(registry):
    registry.register(
        name = "drivers/ufs/host/ufshcd-crypto-qti",
        out = "ufshcd-crypto-qti.ko",
        config = "CONFIG_SCSI_UFS_CRYPTO_QTI",
        srcs = [
            # do not sort
            "drivers/ufs/host/ufs-qcom.h",
            "drivers/ufs/host/ufshcd-crypto-qti.c",
        ],
        deps = [
            # do not sort
            "drivers/soc/qcom/qcom_ice",
            "drivers/firmware/qcom/qcom-scm",
            "drivers/virt/gunyah/gh_rm_drv",
            "drivers/virt/gunyah/gh_msgq",
            "drivers/virt/gunyah/gh_dbl",
            "arch/arm64/gunyah/gh_arm_drv",
        ],
    )

    registry.register(
        name = "drivers/ufs/host/ufs-qcom",
        out = "ufs-qcom.ko",
        config = "CONFIG_SCSI_UFS_QCOM",
        srcs = [
            # do not sort
            "drivers/ufs/host/ufs-qcom-trace.h",
            "drivers/ufs/host/ufs-qcom.c",
            "drivers/ufs/host/ufs-qcom.h",
        ],
        deps = [
            # do not sort
            "drivers/power/reset/qcom-reboot-reason",
            "drivers/misc/hwid",
            "drivers/ufs/host/ufshcd-crypto-qti",
            "drivers/soc/qcom/qcom_ice",
            "kernel/sched/walt/sched-walt",
            "drivers/soc/qcom/socinfo",
            "drivers/phy/qualcomm/phy-qcom-ufs",
            "drivers/clk/qcom/clk-qcom",
            "drivers/clk/qcom/gdsc-regulator",
            "drivers/regulator/debug-regulator",
            "drivers/regulator/proxy-consumer",
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
        name = "drivers/ufs/host/ufs-qcom-scmi",
        out = "ufs-qcom-scmi.ko",
        config = "CONFIG_SCSI_UFS_QCOM_SCMI",
        srcs = [
            # do not sort
            "drivers/ufs/host/ufs-qcom-trace.h",
            "drivers/ufs/host/ufs-qcom-scmi.c",
            "drivers/ufs/host/ufs-qcom.h",
        ],
        deps = [
            # do not sort
            "drivers/ufs/host/ufshcd-crypto-qti",
            "drivers/soc/qcom/qcom_ice",
            "kernel/sched/walt/sched-walt",
            "drivers/soc/qcom/socinfo",
            "drivers/phy/qualcomm/phy-qcom-ufs",
            "drivers/clk/qcom/clk-qcom",
            "drivers/clk/qcom/gdsc-regulator",
            "drivers/regulator/debug-regulator",
            "drivers/regulator/proxy-consumer",
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
