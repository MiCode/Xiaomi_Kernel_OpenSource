def register_modules(registry):
    registry.register(
        name = "drivers/usb/gadget/function/f_fs_ipc_log",
        out = "f_fs_ipc_log.ko",
        config = "CONFIG_USB_F_FS_IPC_LOGGING",
        srcs = [
            # do not sort
            "drivers/usb/gadget/function/f_fs_ipc_log.c",
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
        name = "drivers/usb/gadget/function/usb_f_ccid",
        out = "usb_f_ccid.ko",
        config = "CONFIG_USB_F_CCID",
        srcs = [
            # do not sort
            "drivers/usb/gadget/function/f_ccid.c",
            "drivers/usb/gadget/function/f_ccid.h",
        ],
    )

    registry.register(
        name = "drivers/usb/gadget/function/usb_f_cdev",
        out = "usb_f_cdev.ko",
        config = "CONFIG_USB_F_CDEV",
        srcs = [
            # do not sort
            "drivers/usb/gadget/function/f_cdev.c",
        ],
    )

    registry.register(
        name = "drivers/usb/gadget/function/xm_usb_pri_interface",
        out = "xm_usb_pri_interface.ko",
        config = "CONFIG_XM_USB_PRI_INTERFACE",
        srcs = [
            # do not sort
            "drivers/usb/gadget/function/xm_usb_pri_interface.c",
        ],
    )

    registry.register(
        name = "drivers/usb/gadget/function/usb_f_gsi",
        out = "usb_f_gsi.ko",
        config = "CONFIG_USB_F_GSI",
        srcs = [
            # do not sort
            "drivers/usb/gadget/function/f_gsi.c",
            "drivers/usb/gadget/function/f_gsi.h",
            "drivers/usb/gadget/function/ndis.h",
            "drivers/usb/gadget/function/rndis.c",
            "drivers/usb/gadget/function/rndis.h",
            "drivers/usb/gadget/function/u_rndis.h",
        ],
        deps = [
            # do not sort
            "drivers/usb/dwc3/dwc3-msm",
            "drivers/soc/qcom/wcd_usbss_i2c",
            "drivers/usb/typec/ucsi/ucsi_qti_glink",
            "drivers/usb/redriver/redriver",
            "drivers/usb/repeater/repeater",
            "drivers/base/regmap/qti-regmap-debugfs",
            "drivers/soc/qcom/qti_pmic_glink",
            "drivers/soc/qcom/pdr_interface",
            "drivers/soc/qcom/qmi_helpers",
            "drivers/remoteproc/rproc_qcom_common",
            "drivers/rpmsg/qcom_smd",
            "drivers/rpmsg/qcom_glink_smem",
            "drivers/rpmsg/qcom_glink",
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
        name = "drivers/usb/gadget/function/usb_f_qdss",
        out = "usb_f_qdss.ko",
        config = "CONFIG_USB_F_QDSS",
        srcs = [
            # do not sort
            "drivers/usb/gadget/function/f_qdss.c",
            "drivers/usb/gadget/function/f_qdss.h",
            "drivers/usb/gadget/function/u_qdss.c",
        ],
        deps = [
            # do not sort
            "drivers/usb/dwc3/dwc3-msm",
            "drivers/soc/qcom/wcd_usbss_i2c",
            "drivers/usb/typec/ucsi/ucsi_qti_glink",
            "drivers/usb/redriver/redriver",
            "drivers/usb/repeater/repeater",
            "drivers/base/regmap/qti-regmap-debugfs",
            "drivers/soc/qcom/qti_pmic_glink",
            "drivers/soc/qcom/pdr_interface",
            "drivers/soc/qcom/qmi_helpers",
            "drivers/remoteproc/rproc_qcom_common",
            "drivers/rpmsg/qcom_smd",
            "drivers/rpmsg/qcom_glink_smem",
            "drivers/rpmsg/qcom_glink",
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
