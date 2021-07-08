/*
 * @Codingstyle LinuxKernel
 * @Copyright   Copyright (c) Imagination Technologies Ltd. All Rights Reserved
 * @License     Dual MIT/GPLv2
 *
 * The contents of this file are subject to the MIT license as set out below.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * Alternatively, the contents of this file may be used under the terms of
 * the GNU General Public License Version 2 ("GPL") in which case the provisions
 * of GPL are applicable instead of those above.
 *
 * If you wish to allow use of your version of this file only under the terms of
 * GPL, and not to allow others to use your version of this file under the terms
 * of the MIT license, indicate your decision by deleting the provisions above
 * and replace them with the notice and other provisions required by GPL as set
 * out in the file called "GPL-COPYING" included in this distribution. If you do
 * not delete the provisions above, a recipient may use your version of this file
 * under the terms of either the MIT license or GPL.
 *
 * This License is also included in this distribution in the file called
 * "MIT-COPYING".
 *
 * EXCEPT AS OTHERWISE STATED IN A NEGOTIATED AGREEMENT: (A) THE SOFTWARE IS
 * PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
 * BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE AND NONINFRINGEMENT; AND (B) IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef _ODIN_DRV_H
#define _ODIN_DRV_H

#include "tc_drv_internal.h"
#include "odin_defs.h"
#include "orion_defs.h"

int odin_init(struct tc_device *tc, struct pci_dev *pdev,
	      int *core_clock, int *mem_clock,
	      int pdp_mem_size, int secure_mem_size,
	      int mem_latency, int mem_wresp_latency, int mem_mode,
	      bool fbc_bypass);
int odin_cleanup(struct tc_device *tc);

int odin_register_pdp_device(struct tc_device *tc);
int odin_register_ext_device(struct tc_device *tc);
int odin_register_dma_device(struct tc_device *tc);

struct dma_chan *odin_cdma_chan(struct tc_device *tc, char *name);
void odin_cdma_chan_free(struct tc_device *tc, void *chan_priv);

void odin_enable_interrupt_register(struct tc_device *tc,
				    int interrupt_id);
void odin_disable_interrupt_register(struct tc_device *tc,
				     int interrupt_id);

irqreturn_t odin_irq_handler(int irq, void *data);

int odin_sys_info(struct tc_device *tc, u32 *tmp, u32 *pll);
int odin_sys_strings(struct tc_device *tc,
		     char *str_fpga_rev, size_t size_fpga_rev,
		     char *str_tcf_core_rev, size_t size_tcf_core_rev,
		     char *str_tcf_core_target_build_id,
		     size_t size_tcf_core_target_build_id,
		     char *str_pci_ver, size_t size_pci_ver,
		     char *str_macro_ver, size_t size_macro_ver);

const char *odin_tc_name(struct tc_device *tc);

bool odin_pfim_compatible(struct tc_device *tc);
#endif /* _ODIN_DRV_H */
