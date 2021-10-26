/*
 * Copyright (C) 2017 MediaTek Inc.
 * Licensed under either
 *     BSD Licence, (see NOTICE for more details)
 *     GNU General Public License, version 2.0, (see NOTICE for more details)
 */
#include "nandx_util.h"
#include "nandx_errno.h"
#include "nfc.h"
#include "nfc_core.h"

static void nfc_setup_handler(struct nfc_handler *handler)
{
	/* setup nfc handler, defined by each controller with the same name */
	handler->send_command = nfc_send_command;
	handler->send_address = nfc_send_address;
	handler->read_byte = nfc_read_byte;
	handler->write_byte = nfc_write_byte;
	handler->read_sectors = nfc_read_sectors;
	handler->write_page = nfc_write_page;
	handler->change_interface = nfc_change_interface;
	handler->change_mode = nfc_change_mode;
	handler->get_mode = nfc_get_mode;
	handler->select_chip = nfc_select_chip;
	handler->set_format = nfc_set_format;
	handler->enable_randomizer = nfc_enable_randomizer;
	handler->disable_randomizer = nfc_disable_randomizer;
	handler->wait_busy = nfc_wait_busy;
	handler->calculate_ecc = nfc_calculate_ecc;
	handler->correct_ecc = nfc_correct_ecc;
	handler->calibration = nfc_calibration;
	handler->suspend = nfc_suspend;
	handler->resume = nfc_resume;
}

struct nfc_handler *nfc_init(struct nfc_resource *res)
{
	struct nfc_handler *handler;

	/* setup nfc hw according to platform_data and alloc nfc_handler */
	handler = nfc_setup_hw(res);
	nfc_setup_handler(handler);

	return handler;
}

void nfc_exit(struct nfc_handler *handler)
{
	nfc_release(handler);
}
