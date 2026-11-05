# SPDX-License-Identifier: GPL-2.0-only

si-core-wq-$(CONFIG_QCOM_SI_CORE_WQ) += si_core_wq.o
si_core_module-objs := qcom_scm_invoke.o si_core_async.o si_core.o si_core_adci.o $(si-core-wq-y)
obj-$(CONFIG_QCOM_SI_CORE) += si_core_module.o

# Add si_core extenstions here!

obj-$(CONFIG_QCOM_SI_CORE_MEM_OBJECT) += mem_object.o
mem_object-objs := xts/mem-object.o
