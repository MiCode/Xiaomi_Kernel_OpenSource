subdir-ccflags-y := -Werror

nvshm-objs := nvshm_init.o
nvshm-objs += nvshm_ipc.o
nvshm-objs += nvshm_queue.o
nvshm-objs += nvshm_iobuf.o
nvshm-objs += nvshm_tty.o
nvshm-objs += nvshm_net.o
nvshm-objs += nvshm_if.o
nvshm-objs += nvshm_rpc.o
nvshm-objs += nvshm_rpc_utils.o
nvshm-objs += nvshm_rpc_dispatcher.o
nvshm-objs += nvshm_stats.o

obj-$(CONFIG_NVSHM) += nvshm.o
obj-$(CONFIG_NVSHM_RPC_RSM) += nvshm_rpc_prog_rsm.o
