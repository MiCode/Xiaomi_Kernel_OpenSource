// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

static bool leave_pe4;
static bool leave_pdc;

static int charger_disable_charging(void)
{
	pe40_stop();
	pdc_stop();

	leave_pe4 = true;
	leave_pdc = true;
}

static int charger_enable_charging(void)
{
	leave_pe4 = false;
	leave_pdc = false;
}

static int charger_plug_out(void)
{
	pe40_stop();
	pdc_stop();

	leave_pe4 = false;
	leave_pdc = false;
}

static int charger_routine_thread(void *arg)
{
	struct pe40_data *pe4data;
	struct pdc_data *pdcdata;

	while (1) {

		if (pe40_is_ready() && !leave_pe4) {
			if ([check enter pe4 condition]) {
				chr_err("enter PE4.0!\n");

				pe4data = pe40_get_data();
				[update pe40 data]

				ret = pe40_run();

				if ([check leave pe4 condition]) {
					chr_err("leave pe4\n");
					pe40_stop();
					leave_pe4 = true;
				}
			}
		}

		if (pdc_is_ready() && !leave_pdc) {
			if ([check enter pdc condition]) {
				chr_err("enter PDC!\n");

				pdcdata = pdc_get_data();
				[update pdc data]

				ret = pdc_run();

				if ([check leave pdc condition]) {
					chr_err("leave pdc\n");
					pdc_stop();
					leave_pdc = true;
				}
			}
		}

	}

	return 0;
}


static int mtk_charger_probe(struct platform_device *pdev)
{
	int ret;

	ret = pe40_init();
	ret = pdc_init();

	leave_pe4 = false;
	leave_pdc = false;

	return 0;
}
