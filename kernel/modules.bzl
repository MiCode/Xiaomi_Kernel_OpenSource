load(":kernel/locking/modules.bzl", register_locking = "register_modules")
load(":kernel/rcu/modules.bzl", register_rcu = "register_modules")
load(":kernel/sched/walt/modules.bzl", register_sched_walt = "register_modules")
load(":kernel/trace/modules.bzl", register_trace = "register_modules")
load(":kernel/sched/xiaomi_sched/modules.bzl", register_xiaomi_sched = "register_modules")


def register_modules(registry):
    register_locking(registry)
    register_rcu(registry)
    register_sched_walt(registry)
    register_trace(registry)
    register_xiaomi_sched(registry)

    registry.register(
        name = "kernel/msm_sysstats",
        out = "msm_sysstats.ko",
        config = "CONFIG_MSM_SYSSTATS",
        srcs = [
            # do not sort
            "kernel/msm_sysstats.c",
        ],
        deps = [
            # do not sort
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
        name = "kernel/torture",
        out = "torture.ko",
        config = "CONFIG_TORTURE_TEST",
        srcs = [
            # do not sort
            "kernel/torture.c",
        ],
    )
