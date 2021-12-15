/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
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
	{DISP_MODULE_OVL0, DISP_M4U_PORT_DISP_OVL0},
	{DISP_MODULE_RDMA0, DISP_M4U_PORT_DISP_RDMA0},
	{DISP_MODULE_WDMA0, DISP_M4U_PORT_DISP_WDMA0},
};

int module_to_m4u_port(enum DISP_MODULE_ENUM module)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(module_to_m4u_port_mapping); i++)
		if (module_to_m4u_port_mapping[i].module == module)
			return module_to_m4u_port_mapping[i].port;

	DDPERR("%s, get m4u port fail(module=%s)\n",
	       __func__, ddp_get_module_name(module));
	return M4U_PORT_NR;
}

enum DISP_MODULE_ENUM m4u_port_to_module(int port)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(module_to_m4u_port_mapping); i++)
		if (module_to_m4u_port_mapping[i].port == port)
			return module_to_m4u_port_mapping[i].module;

	DDPERR("%s, unknown port=%d\n", __func__, port);
	return DISP_MODULE_UNKNOWN;
}

int disp_m4u_callback(
		int port, unsigned long mva, void *data)
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
			m4u_register_fault_callback(
				module_to_m4u_port_mapping[i].port,
				(m4u_fault_callback_t *)disp_m4u_callback, 0);
	} else {
		/* disable m4u port, used for m4u not ready */
		DDPMSG("m4u not enable, disable m4u port\n");

		for (i = 0; i < 4; i++)
			DISP_REG_SET_FIELD(0, REG_FLD_MMU_EN,
				DISP_REG_SMI_LARB0_NON_SEC_CON + i * 4, 0);
	}
}

int config_display_m4u_port(void)
{
	unsigned int i;
	int ret = 0;
	struct m4u_port_config_struct sPort;
	char *m4u_usage = disp_helper_get_option(DISP_OPT_USE_M4U) ? "virtual"
								   : "physical";

	sPort.ePortID = DISP_M4U_PORT_DISP_OVL0;
	sPort.Virtuality = disp_helper_get_option(DISP_OPT_USE_M4U);
	sPort.Security = 0;
	sPort.Distance = 1;
	sPort.Direction = 0;

	for (i = 0; i < ARRAY_SIZE(module_to_m4u_port_mapping); i++) {
		sPort.ePortID = module_to_m4u_port_mapping[i].port;
		ret = m4u_config_port(&sPort);
		if (ret) {
			enum DISP_MODULE_ENUM module;

			module = module_to_m4u_port_mapping[i].module;
			DISPERR("config M4U Port %s to %s FAIL(ret=%d)\n",
				ddp_get_module_name(module), m4u_usage, ret);
			return -1;
		}
	}
	return ret;
}

int disp_allocate_mva(struct m4u_client_t *client, enum DISP_MODULE_ENUM module,
		      unsigned long va, struct sg_table *sg_table,
		      unsigned int size, unsigned int prot, unsigned int flags,
		      unsigned int *pMva)
{
	int port = module_to_m4u_port(module);

	if (port == M4U_PORT_NR)
		return 1; /* err */

	return m4u_alloc_mva(client, port, va, sg_table,
			     size, prot, flags, pMva);
}
