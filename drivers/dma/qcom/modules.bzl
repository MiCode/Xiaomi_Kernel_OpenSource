def register_modules(registry):
    registry.register(
        name = "drivers/dma/qcom/bam_dma",
        out = "bam_dma.ko",
        config = "CONFIG_QCOM_BAM_DMA",
        srcs = [
            # do not sort
            "drivers/dma/qcom/bam_dma.c",
            "drivers/dma/qcom/bam_dma_trace.h",
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
        name = "drivers/dma/qcom/msm_gpi",
        out = "msm_gpi.ko",
        config = "CONFIG_MSM_GPI_DMA",
        srcs = [
            # do not sort
            "drivers/dma/qcom/msm_gpi.c",
            "drivers/dma/qcom/msm_gpi_mmio.h",
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
        name = "drivers/dma/qcom/gpi",
        out = "gpi.ko",
        config = "CONFIG_QCOM_GPI_DMA",
        srcs = [
            # do not sort
            "drivers/dma/qcom/gpi_fixed.c",
            "drivers/dma/dmaengine.h",
            "drivers/dma/virt-dma.h",
        ],
        deps = [
            # do not sort
        ],
    )
