/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */
/*! \file
*    \brief  Declaration of library functions
*
*    Any definitions in this file will be shared among GLUE Layer and internal Driver Stack.
*/

#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/proc_fs.h>
#include <linux/slab.h>
#include <linux/mailbox_client.h>
#include "consys_hw.h"
#include "conninfra_core_test.h"

/*******************************************************************************
*                         C O M P I L E R   F L A G S
********************************************************************************
*/

/*******************************************************************************
*                                 M A C R O S
********************************************************************************
*/


/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/

/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/

/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/

/*******************************************************************************
*                  F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/


/*******************************************************************************
*                            P U B L I C   D A T A
********************************************************************************
*/


/*******************************************************************************
*                           P R I V A T E   D A T A
********************************************************************************
*/


/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/

struct demo_client {
	struct mbox_client cl;
	struct mbox_chan *mbox;
	struct completion c;
	bool async;
};


static void message_from_remote(struct mbox_client *cl, void *msg)
{
	struct demo_client *dc = container_of(cl, struct demo_client, cl);
	if (dc->async) {
		pr_info("AAAAsync");
	} else {
		pr_info("SSSSSSSync");
	}
}

static void sample_sent(struct mbox_client *cl, void *msg, int r)
{
	struct demo_client *dc = container_of(cl, struct demo_client, cl);
	complete(&dc->c);
}

int mailbox_test(void)
{
	struct demo_client *dc_sync;
	struct platform_device *pdev = get_consys_device();

	dc_sync = kzalloc(sizeof(*dc_sync), GFP_KERNEL);
	dc_sync->cl.dev = &pdev->dev;
	dc_sync->cl.rx_callback = message_from_remote;
	dc_sync->cl.tx_done = sample_sent;
	dc_sync->cl.tx_block = true;
	dc_sync->cl.tx_tout = 500;
	dc_sync->cl.knows_txdone = false;
	dc_sync->async = false;

	dc_sync->mbox = mbox_request_channel(&dc_sync->cl, 0);

	if (IS_ERR(dc_sync->mbox)) {
		pr_err("request channel fail [%d]", dc_sync->mbox);
		return -1;
	}
	mbox_send_message(dc_sync->mbox, 0);

	wait_for_completion(&dc_sync->c);

	return 0;
}
