def register_modules(registry):
    registry.register(
        name = "drivers/dma-buf/heaps/qcom_dma_heaps",
        out = "qcom_dma_heaps.ko",
        config = "CONFIG_QCOM_DMABUF_HEAPS",
        srcs = [
            # do not sort
            "drivers/dma-buf/heaps/qcom_carveout_heap.h",
            "drivers/dma-buf/heaps/qcom_cma_heap.h",
            "drivers/dma-buf/heaps/qcom_dma_heap.c",
            "drivers/dma-buf/heaps/qcom_dma_heap_priv.h",
            "drivers/dma-buf/heaps/qcom_dt_parser.c",
            "drivers/dma-buf/heaps/qcom_dt_parser.h",
            "drivers/dma-buf/heaps/qcom_dynamic_page_pool.h",
            "drivers/dma-buf/heaps/qcom_secure_system_heap.h",
            "drivers/dma-buf/heaps/qcom_sg_ops.c",
            "drivers/dma-buf/heaps/qcom_sg_ops.h",
            "drivers/dma-buf/heaps/qcom_system_heap.h",
            "drivers/dma-buf/heaps/qcom_system_movable_heap.h",
            "drivers/dma-buf/heaps/deferred-free-helper.h",
        ],
        conditional_srcs = {
            "CONFIG_QCOM_DMABUF_HEAPS_SYSTEM": {
                True: [
                    # do not sort
                    "drivers/dma-buf/heaps/mi_dma_dio.h",
                    "drivers/dma-buf/heaps/qcom_system_heap.c",
                    "drivers/dma-buf/heaps/qcom_dynamic_page_pool.c",
                    "drivers/dma-buf/heaps/qcom_dma_heap_secure_utils.h",
                ],
            },
            "CONFIG_QCOM_DMABUF_HEAPS_SYSTEM_SECURE": {
                True: [
                    # do not sort
                    "drivers/dma-buf/heaps/qcom_secure_system_heap.c",
                    "drivers/dma-buf/heaps/qcom_dynamic_page_pool.c",
                    "drivers/dma-buf/heaps/qcom_dma_heap_secure_utils.c",
                    "drivers/dma-buf/heaps/qcom_dma_heap_secure_utils.h",
                ],
            },
            "CONFIG_QCOM_DMABUF_HEAPS_CMA": {
                True: [
                    # do not sort
                    "drivers/dma-buf/heaps/qcom_cma_heap.c",
                ],
            },
            "CONFIG_QCOM_DMABUF_HEAPS_CARVEOUT": {
                True: [
                    # do not sort
                    "drivers/dma-buf/heaps/qcom_carveout_heap.c",
                    "drivers/dma-buf/heaps/qcom_dma_heap_secure_utils.c",
                    "drivers/dma-buf/heaps/qcom_dma_heap_secure_utils.h",
                ],
            },
            "CONFIG_QCOM_DMABUF_HEAPS_UBWCP": {
                True: [
                    # do not sort
                    "drivers/dma-buf/heaps/qcom_ubwcp_heap.c",
                ],
            },
            "CONFIG_QCOM_DMABUF_HEAPS_TVM_CARVEOUT": {
                True: [
                    # do not sort
                    "drivers/dma-buf/heaps/qcom_tvm_carveout_heap.c",
                ],
            },
            "CONFIG_QCOM_DMABUF_HEAPS_SYSTEM_MOVABLE": {
                True: [
                    # do not sort
                    "drivers/dma-buf/heaps/qcom_system_movable_heap.c",
                ],
            },
        },
        deps = [
            # do not sort
            "drivers/iommu/msm_dma_iommu_mapping",
            "drivers/soc/qcom/mem_buf/mem_buf_dev",
            "drivers/soc/qcom/secure_buffer",
            "drivers/firmware/qcom/qcom-scm",
            "drivers/virt/gunyah/gh_rm_drv",
            "drivers/virt/gunyah/gh_msgq",
            "drivers/virt/gunyah/gh_dbl",
            "arch/arm64/gunyah/gh_arm_drv",
            "drivers/dma-buf/heaps/deferred-free-helper",
            "drivers/soc/qcom/debug_symbol",
        ],
    )
    registry.register(
        name = "drivers/dma-buf/heaps/deferred-free-helper",
        out = "deferred-free-helper.ko",
        config = "CONFIG_QCOM_DMABUF_HEAPS_DEFERRED_FREE",
        srcs = [
            # do not sort
            "drivers/dma-buf/heaps/deferred-free-helper.c",
            "drivers/dma-buf/heaps/deferred-free-helper.h",
        ],
    )
    registry.register(
        name = "drivers/dma-buf/heaps/mi_dma_dio",
        out = "mi_dma_dio.ko",
        config = "CONFIG_MI_DMABUF_DIO",
        srcs = [
            # do not sort
            "drivers/dma-buf/heaps/mi_dma_dio.h",
            "drivers/dma-buf/heaps/mi_dma_dio.c",
        ],
        deps = [
            # do not sort
            "drivers/soc/qcom/debug_symbol",
            "drivers/dma-buf/heaps/qcom_dma_heaps",
        ],
    )
