/* include/asm-arm/arch-msm/msm_adsp.h
 *
 * Copyright (C) 2008 Google, Inc.
 * Copyright (c) 2009-2010, 2012 The Linux Foundation. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 */

#ifndef __ASM__ARCH_MSM_ADSP_H
#define __ASM__ARCH_MSM_ADSP_H

struct msm_adsp_module;

struct msm_adsp_ops {
	/* event is called from interrupt context when a message
	 * arrives from the DSP.  Use the provided function pointer
	 * to copy the message into a local buffer.  Do NOT call
	 * it multiple times.
	 */
	void (*event)(void *driver_data, unsigned id, size_t len,
		      void (*getevent)(void *ptr, size_t len));
};

/* Get, Put, Enable, and Disable are synchronous and must only
 * be called from thread context.  Enable and Disable will block
 * up to one second in the event of a fatal DSP error but are
 * much faster otherwise.
 */
int msm_adsp_get(const char *name, struct msm_adsp_module **module,
		 struct msm_adsp_ops *ops, void *driver_data);
void msm_adsp_put(struct msm_adsp_module *module);
int msm_adsp_enable(struct msm_adsp_module *module);
int msm_adsp_disable(struct msm_adsp_module *module);
int adsp_set_clkrate(struct msm_adsp_module *module, unsigned long clk_rate);
int msm_adsp_disable_event_rsp(struct msm_adsp_module *module);
int32_t get_adsp_resource(unsigned short client_idx,
				void *cmd_buf, size_t cmd_size);
int32_t put_adsp_resource(unsigned short client_idx,
				void *cmd_buf, size_t cmd_size);

/* Write is safe to call from interrupt context.
 */
int msm_adsp_write(struct msm_adsp_module *module,
		   unsigned queue_id,
		   void *data, size_t len);

/*Explicitly gererate adsp event */
int msm_adsp_generate_event(void *data,
			struct msm_adsp_module *mod,
			unsigned event_id,
			unsigned event_length,
			unsigned event_size,
			void *msg);

#define ADSP_MESSAGE_ID 0xFFFF

/* Command Queue Indexes */
#define QDSP_lpmCommandQueue              0
#define QDSP_mpuAfeQueue                  1
#define QDSP_mpuGraphicsCmdQueue          2
#define QDSP_mpuModmathCmdQueue           3
#define QDSP_mpuVDecCmdQueue              4
#define QDSP_mpuVDecPktQueue              5
#define QDSP_mpuVEncCmdQueue              6
#define QDSP_rxMpuDecCmdQueue             7
#define QDSP_rxMpuDecPktQueue             8
#define QDSP_txMpuEncQueue                9
#define QDSP_uPAudPPCmd1Queue             10
#define QDSP_uPAudPPCmd2Queue             11
#define QDSP_uPAudPPCmd3Queue             12
#define QDSP_uPAudPlay0BitStreamCtrlQueue 13
#define QDSP_uPAudPlay1BitStreamCtrlQueue 14
#define QDSP_uPAudPlay2BitStreamCtrlQueue 15
#define QDSP_uPAudPlay3BitStreamCtrlQueue 16
#define QDSP_uPAudPlay4BitStreamCtrlQueue 17
#define QDSP_uPAudPreProcCmdQueue         18
#define QDSP_uPAudRecBitStreamQueue       19
#define QDSP_uPAudRecCmdQueue             20
#define QDSP_uPDiagQueue                  21
#define QDSP_uPJpegActionCmdQueue         22
#define QDSP_uPJpegCfgCmdQueue            23
#define QDSP_uPVocProcQueue               24
#define QDSP_vfeCommandQueue              25
#define QDSP_vfeCommandScaleQueue         26
#define QDSP_vfeCommandTableQueue         27
#define QDSP_vfeFtmCmdQueue               28
#define QDSP_vfeFtmCmdScaleQueue          29
#define QDSP_vfeFtmCmdTableQueue          30
#define QDSP_uPJpegFtmCfgCmdQueue         31
#define QDSP_uPJpegFtmActionCmdQueue      32
#define QDSP_apuAfeQueue                  33
#define QDSP_mpuRmtQueue                  34
#define QDSP_uPAudPreProcAudRecCmdQueue   35
#define QDSP_uPAudRec0BitStreamQueue      36
#define QDSP_uPAudRec0CmdQueue            37
#define QDSP_uPAudRec1BitStreamQueue      38
#define QDSP_uPAudRec1CmdQueue            39
#define QDSP_apuRmtQueue                  40
#define QDSP_uPAudRec2BitStreamQueue      41
#define QDSP_uPAudRec2CmdQueue            42
#define QDSP_MAX_NUM_QUEUES               43

#endif
