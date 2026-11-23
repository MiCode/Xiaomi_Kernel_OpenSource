load(":drivers/xiaomi/mi_kernel_monitor/modules.bzl", register_mi_kernel_monitor = "register_modules")
load(":drivers/xiaomi/fault_report/modules.bzl", register_fault_report = "register_modules")
load(":drivers/xiaomi/mi_qmi_trace/modules.bzl", register_mi_qmi_trace = "register_modules")

def register_modules(registry):
    register_mi_kernel_monitor(registry)
    register_fault_report(registry)
    register_mi_qmi_trace(registry)
