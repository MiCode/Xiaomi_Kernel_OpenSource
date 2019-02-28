/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include "ddp_m4u.h"
#include "m4u.h"
#include "ddp_dump.h"
#include "ddp_hal.h"
#include "ddp_reg.h"
#include "ddp_log.h"
#include "disp_helper.h"

/* display m4u port / display module mapping table
 * -- by chip
 */
static struct module_to_m4u_port_t module_to_m4u_port_mapping[] = {
#if 1
	{DISP_MODULE_OVL0, DISP_M4U_PORT_DISP_OVL0},
	{DISP_MODULE_RDMA0, DISP_M4U_PORT_DISP_RDMA0},
	{DISP_MODULE_WDMA0, DISP_M4U_PORT_DISP_WDMA0},
#endif
};


int module_to_m4u_port(enum DISP_MODULE_ENUM module)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(module_to_m4u_port_mapping); i++)
		if (module_to_m4u_port_mapping[i].module == module)
			return module_to_m4u_port_mapping[i].port;

	DDPERR("module_to_m4u_port, get m4u port fail(module=%d)\n", module);
	return M4U_PORT_NR;

}

enum DISP_MODULE_ENUM m4u_port_to_module(int port)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(module_to_m4u_port_mapping); i++)
		if (module_to_m4u_port_mapping[i].port == port)
			return module_to_m4u_port_mapping[i].module;

	DDPERR("m4u_port_to_module, unknown port=%d\n", port);
	return DISP_MODULE_UNKNOWN;
}

m4u_callback_ret_t disp_m4u_callback(int port, unsigned long mva, void *data)
{

	DDPERR("fault call port=%d, mva=0x%lx, data=0x%p\n", port, mva, data);
	ddp_dump_analysis(DISP_MODULE_OVL0);
	ddp_dump_reg(DISP_MODULE_OVL0);
	ddp_dump_analysis(DISP_MODULE_RDMA0);
	ddp_dump_reg(DISP_MODULE_RDMA0);
	ddp_dump_analysis(DISP_MODULE_WDMA0);
	ddp_dump_reg(DISP_MODULE_WDMA0);
	return 0;
}

void disp_m4u_init(void)
{
	unsigned int i;

	if (disp_helper_get_option(DISP_OPT_USE_M4U)) {
		/* init M4U callback */
		DDPMSG("register m4u callback\n");
		for (i = 0; i < ARRAY_SIZE(module_to_m4u_port_mapping); i++)
			m4u_register_fault_callback(module_to_m4u_port_mapping[i].port,
				(m4u_fault_callback_t *)disp_m4u_callback, 0);
	} else {
		/* disable m4u port, used for m4u not ready */
		DDPMSG("m4u not enable, disable m4u port\n");
#if 1
		for (i = 0; i < 4; i++)
			DISP_REG_SET_FIELD(0, REG_FLD_MMU_EN, DISP_REG_SMI_LARB0_NON_SEC_CON + i*4, 0);
#if 0
		for (i = 0; i < 3; i++)
			DISP_REG_SET_FIELD(0, REG_FLD_MMU_EN, DISP_REG_SMI_LARB1_NON_SEC_CON + i*4, 0);
#endif
#endif
	}
}

int config_display_m4u_port(void)
{
	unsigned int i;
	int ret = 0;
	M4U_PORT_STRUCT sPort;
	char *m4u_usage = disp_helper_get_option(DISP_OPT_USE_M4U) ? "virtual" : "physical";

	sPort.ePortID = DISP_M4U_PORT_DISP_OVL0;
	sPort.Virtuality = disp_helper_get_option(DISP_OPT_USE_M4U);
	sPort.Security = 0;
	sPort.Distance = 1;
	sPort.Direction = 0;

	for (i = 0; i < ARRAY_SIZE(module_to_m4u_port_mapping); i++) {
		sPort.ePortID = module_to_m4u_port_mapping[i].port;
		ret = m4u_config_port(&sPort);
		if (ret) {
			DISPERR("config M4U Port %s to %s FAIL(ret=%d)\n",
				  ddp_get_module_name(module_to_m4u_port_mapping[i].module), m4u_usage, ret);
			return -1;
		}
	}
	return ret;
}
#if 0
unsigned int disp_allocate_mva(unsigned int pa, unsigned int size, M4U_PORT_ID port)
{
	int ret = 0;
	unsigned int mva = 0;

	m4u_client_t *client = NULL;
	struct sg_table *sg_table = kzalloc(sizeof(struct sg_table), GFP_ATOMIC);

	sg_alloc_table(sg_table, 1, GFP_KERNEL);

	sg_dma_address(sg_table->sgl) = pa;
	sg_dma_len(sg_table->sgl) = size;
	client = m4u_create_client();
	if (IS_ERR_OR_NULL(client))
		DISPERR("create client fail!\n");

	mva = pa;
	ret = m4u_alloc_mva(client, port, 0, sg_table, size, M4U_PROT_READ | M4U_PROT_WRITE,
			  M4U_FLAGS_FIX_MVA, &mva);
	if (ret)
		DISPERR("m4u_alloc_mva returns fail: %d\n", ret);
	DISPMSG("[DISPHAL] FB MVA is 0x%08X PA is 0x%08X\n", mva, pa);

	return mva;
}
#endif
int disp_allocate_mva(m4u_client_t *client, enum DISP_MODULE_ENUM module,
								unsigned long va, struct sg_table *sg_table,
								unsigned int size, unsigned int prot,
								unsigned int flags, unsigned int *pMva)
{
	int port = module_to_m4u_port(module);

	if (port == M4U_PORT_NR)
		return 1; /* err */

	return m4u_alloc_mva(client, port, va, sg_table, size, prot, flags, pMva);
}
