def register_modules(registry):
    registry.register(
        name = "drivers/i2c/busses/i2c-msm-geni",
        out = "i2c-msm-geni.ko",
        config = "CONFIG_I2C_MSM_GENI",
        srcs = [
            # do not sort
            "drivers/i2c/busses/i2c-msm-geni.c",
            "drivers/i2c/busses/i2c-qup-trace.h",
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
        name = "drivers/i2c/busses/i2c-qcom-geni-msm",
        out = "i2c-qcom-geni-msm.ko",
        config = "CONFIG_I2C_QCOM_GENI_MSM",
        srcs = [
            # do not sort
            "drivers/i2c/busses/i2c-qcom-geni-msm.c",
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
        name = "drivers/i2c/busses/i2c-qcom-geni",
        out = "i2c-qcom-geni.ko",
        config = "CONFIG_I2C_QCOM_GENI",
        srcs = [
            # do not sort
            "drivers/i2c/busses/i2c-qcom-geni.c",
        ],
        deps = [
            # do not sort
            "drivers/dma/qcom/gpi",
        ],
    )

    registry.register(
        name = "drivers/i2c/busses/i2c-virtio",
        out = "i2c-virtio.ko",
        config = "CONFIG_I2C_VIRTIO",
        srcs = [
            # do not sort
            "drivers/i2c/busses/i2c-virtio.c",
        ],
        deps = [
            # do not sort
            "drivers/virtio/virtio_mmio",
        ],
    )
