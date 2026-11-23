def register_modules(registry):
    registry.register(
        name = "net/qrtr/qrtr-gunyah",
        out = "qrtr-gunyah.ko",
        config = "CONFIG_QRTR_GUNYAH",
        srcs = [
            # do not sort
            "net/qrtr/gunyah.c",
            "net/qrtr/qrtr.h",
        ],
        deps = [
            # do not sort
            "net/qrtr/qrtr",
            "drivers/virt/gunyah/gunyah_loader",
            "drivers/soc/qcom/mdt_loader",
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
        name = "net/qrtr/qrtr-genpool",
        out = "qrtr-genpool.ko",
        config = "CONFIG_QRTR_GENPOOL",
        srcs = [
            # do not sort
            "net/qrtr/genpool.c",
            "net/qrtr/qrtr.h",
        ],
        deps = [
            # do not sort
            "net/qrtr/qrtr",
        ],
    )

    registry.register(
        name = "net/qrtr/qrtr",
        out = "qrtr.ko",
        config = "CONFIG_QRTR",
        srcs = [
            # do not sort
            "net/qrtr/af_qrtr.c",
            "net/qrtr/ns.c",
            "net/qrtr/qrtr.h",
        ],
        includes = [
        "drivers/xiaomi/mi_qmi_trace",
        "vendor/kernel_platform/drivers/xiaomi/mi_qmi_trace",
        "drivers/powerinsight/common",
        "vendor/kernel_platform/drivers/powerinsight/common",
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
            "drivers/xiaomi/mi_qmi_trace",
            "drivers/powerinsight/common",
        ],
    )

    registry.register(
        name = "net/qrtr/qrtr-mhi",
        out = "qrtr-mhi.ko",
        config = "CONFIG_QRTR_MHI",
        srcs = [
            # do not sort
            "net/qrtr/mhi.c",
            "net/qrtr/qrtr.h",
        ],
        deps = [
            # do not sort
            "net/qrtr/qrtr",
            "drivers/bus/mhi/host/mhi",
            "drivers/pci/controller/pci-msm-drv",
            "drivers/remoteproc/rproc_qcom_common",
            "drivers/rpmsg/qcom_smd",
            "drivers/rpmsg/qcom_glink_smem",
            "drivers/rpmsg/qcom_glink",
            "drivers/soc/qcom/pcie-pdc",
            "drivers/pinctrl/qcom/pinctrl-msm",
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
        name = "net/qrtr/qrtr-smd",
        out = "qrtr-smd.ko",
        config = "CONFIG_QRTR_SMD",
        srcs = [
            # do not sort
            "net/qrtr/qrtr.h",
            "net/qrtr/smd.c",
        ],
        deps = [
            # do not sort
            "net/qrtr/qrtr",
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
        name = "net/qrtr/qrtr-tun",
        out = "qrtr-tun.ko",
        config = "CONFIG_QRTR_TUN",
        srcs = [
            # do not sort
            "net/qrtr/qrtr.h",
            "net/qrtr/tun.c",
        ],
        deps = [
            # do not sort
            "net/qrtr/qrtr",
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
