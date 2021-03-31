# SPDX-License-Identifier: GPL-2.0-only
CFLAGS_hab.o := -I$(src)

msm_hab-objs = \
	hab.o \
	hab_msg.o \
	hab_vchan.o \
	hab_pchan.o \
	hab_open.o \
	hab_mimex.o \
	hab_pipe.o \
	hab_parser.o \
	hab_stat.o

msm_hab_linux-objs = \
	khab.o \
	hab_linux.o \
	hab_mem_linux.o \
	khab_test.o

ifdef CONFIG_GHS_VMM
msm_hab_hyp-objs = \
	ghs_comm.o \
	ghs_comm_linux.o \
	hab_ghs.o \
	hab_ghs_linux.o
else
ifdef CONFIG_QTI_QUIN_GVM
msm_hab_hyp-objs = \
	qvm_comm.o \
	qvm_comm_linux.o \
	hab_qvm.o \
	hab_qvm_linux.o
else
msm_hab_hyp-objs = \
	hab_comm.o \
	hyp_stub.o
endif
endif

obj-$(CONFIG_MSM_HAB) += msm_hab.o msm_hab_linux.o msm_hab_hyp.o
