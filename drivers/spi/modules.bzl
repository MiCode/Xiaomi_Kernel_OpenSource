def register_modules(registry):
    registry.register(
        name = "drivers/spi/q2spi-geni",
        out = "q2spi-geni.ko",
        config = "CONFIG_Q2SPI_MSM_GENI",
        srcs = [
            # do not sort
            "drivers/spi/q2spi-gsi.c",
            "drivers/spi/q2spi-gsi.h",
            "drivers/spi/q2spi-msm-geni.c",
            "drivers/spi/q2spi-msm.h",
            "drivers/spi/q2spi-slave-reg.h",
            "drivers/spi/q2spi-trace.h",
        ],
        deps = [
            # do not sort
            "drivers/dma/qcom/msm_gpi",
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
        name = "drivers/spi/spi-msm-geni",
        out = "spi-msm-geni.ko",
        config = "CONFIG_SPI_MSM_GENI",
        srcs = [
            # do not sort
            "drivers/spi/spi-msm-geni.c",
            "drivers/spi/spi-qup-trace.h",
        ],
        deps = [
            # do not sort
            "drivers/dma/qcom/msm_gpi",
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
        name = "drivers/spi/spi-geni-qcom",
        out = "spi-geni-qcom.ko",
        config = "CONFIG_SPI_QCOM_GENI",
        srcs = [
            # do not sort
            "drivers/spi/spi-geni-qcom.c",
        ],
        deps = [
            # do not sort
            "drivers/dma/qcom/gpi",
        ],
    )

    registry.register(
        name = "drivers/spi/spi-geni-qcom-msm",
        out = "spi-geni-qcom-msm.ko",
        config = "CONFIG_SPI_QCOM_GENI_MSM",
        srcs = [
            # do not sort
            "drivers/spi/spi-geni-qcom-msm.c",
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
