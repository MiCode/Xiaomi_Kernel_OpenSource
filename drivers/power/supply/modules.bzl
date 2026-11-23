load(":drivers/power/supply/mca/modules.bzl", register_mca = "register_modules")
load(":drivers/power/supply/qcom/modules.bzl", register_qcom = "register_modules")
def register_modules(registry):
    register_mca(registry)
    register_qcom(registry)

    registry.register(
        name = "drivers/power/supply/qti_battery_charger",
        out = "qti_battery_charger.ko",
        config = "CONFIG_QTI_BATTERY_CHARGER",
        srcs = [
            # do not sort
            "drivers/power/supply/qti_battery_charger.c",
            "drivers/power/supply/qti_charger_boost_lib.c",
            "drivers/power/supply/qti_charger_boost_lib.h",
            "drivers/power/supply/qti_typec_class.c",
            "drivers/power/supply/qti_typec_class.h",
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
            "drivers/regulator/debug-regulator",
        ],
    )
