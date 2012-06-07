/* linux/arch/arm/mach-msm/rpc_hsusb.c
 *
 * Copyright (c) 2008-2012, Code Aurora Forum. All rights reserved.
 *
 * All source code in this file is licensed under the following license except
 * where indicated.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, you can find it at http://www.fsf.org
 */

#include <linux/err.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <mach/rpc_hsusb.h>
#include <asm/mach-types.h>

static struct msm_rpc_endpoint *usb_ep;
static struct msm_rpc_endpoint *chg_ep;

#define MSM_RPC_CHG_PROG 0x3000001a

struct msm_chg_rpc_ids {
	unsigned long	vers_comp;
	unsigned	chg_usb_charger_connected_proc;
	unsigned	chg_usb_charger_disconnected_proc;
	unsigned	chg_usb_i_is_available_proc;
	unsigned	chg_usb_i_is_not_available_proc;
};

struct msm_hsusb_rpc_ids {
	unsigned long	prog;
	unsigned long	vers_comp;
	unsigned long	init_phy;
	unsigned long	vbus_pwr_up;
	unsigned long	vbus_pwr_down;
	unsigned long	update_product_id;
	unsigned long	update_serial_num;
	unsigned long	update_is_serial_num_null;
	unsigned long	reset_rework_installed;
	unsigned long	enable_pmic_ulpi_data0;
	unsigned long	disable_pmic_ulpi_data0;
};

static struct msm_hsusb_rpc_ids usb_rpc_ids;
static struct msm_chg_rpc_ids chg_rpc_ids;

static int msm_hsusb_init_rpc_ids(unsigned long vers)
{
	if (vers == 0x00010001) {
		usb_rpc_ids.prog			= 0x30000064;
		usb_rpc_ids.vers_comp			= 0x00010001;
		usb_rpc_ids.init_phy			= 2;
		usb_rpc_ids.vbus_pwr_up			= 6;
		usb_rpc_ids.vbus_pwr_down		= 7;
		usb_rpc_ids.update_product_id		= 8;
		usb_rpc_ids.update_serial_num		= 9;
		usb_rpc_ids.update_is_serial_num_null	= 10;
		usb_rpc_ids.reset_rework_installed	= 17;
		usb_rpc_ids.enable_pmic_ulpi_data0	= 18;
		usb_rpc_ids.disable_pmic_ulpi_data0	= 19;
		return 0;
	} else if (vers == 0x00010002) {
		usb_rpc_ids.prog			= 0x30000064;
		usb_rpc_ids.vers_comp			= 0x00010002;
		usb_rpc_ids.init_phy			= 2;
		usb_rpc_ids.vbus_pwr_up			= 6;
		usb_rpc_ids.vbus_pwr_down		= 7;
		usb_rpc_ids.update_product_id		= 8;
		usb_rpc_ids.update_serial_num		= 9;
		usb_rpc_ids.update_is_serial_num_null	= 10;
		usb_rpc_ids.reset_rework_installed	= 17;
		usb_rpc_ids.enable_pmic_ulpi_data0	= 18;
		usb_rpc_ids.disable_pmic_ulpi_data0	= 19;
		return 0;
	} else {
		pr_err("%s: no matches found for version\n",
			__func__);
		return -ENODATA;
	}
}

static int msm_chg_init_rpc(unsigned long vers)
{
	if (((vers & RPC_VERSION_MAJOR_MASK) == 0x00010000) ||
	    ((vers & RPC_VERSION_MAJOR_MASK) == 0x00020000) ||
	    ((vers & RPC_VERSION_MAJOR_MASK) == 0x00030000) ||
	    ((vers & RPC_VERSION_MAJOR_MASK) == 0x00040000)) {
		chg_ep = msm_rpc_connect_compatible(MSM_RPC_CHG_PROG, vers,
						     MSM_RPC_UNINTERRUPTIBLE);
		if (IS_ERR(chg_ep))
			return -ENODATA;
		chg_rpc_ids.vers_comp				= vers;
		chg_rpc_ids.chg_usb_charger_connected_proc 	= 7;
		chg_rpc_ids.chg_usb_charger_disconnected_proc 	= 8;
		chg_rpc_ids.chg_usb_i_is_available_proc 	= 9;
		chg_rpc_ids.chg_usb_i_is_not_available_proc 	= 10;
		return 0;
	} else
		return -ENODATA;
}

/* rpc connect for hsusb */
int msm_hsusb_rpc_connect(void)
{

	if (usb_ep && !IS_ERR(usb_ep)) {
		pr_debug("%s: usb_ep already connected\n", __func__);
		return 0;
	}

	/* Initialize rpc ids */
	if (msm_hsusb_init_rpc_ids(0x00010001)) {
		pr_err("%s: rpc ids initialization failed\n"
			, __func__);
		return -ENODATA;
	}

	usb_ep = msm_rpc_connect_compatible(usb_rpc_ids.prog,
					usb_rpc_ids.vers_comp,
					MSM_RPC_UNINTERRUPTIBLE);

	if (IS_ERR(usb_ep)) {
		pr_err("%s: connect compatible failed vers = %lx\n",
			 __func__, usb_rpc_ids.vers_comp);

		/* Initialize rpc ids */
		if (msm_hsusb_init_rpc_ids(0x00010002)) {
			pr_err("%s: rpc ids initialization failed\n",
				__func__);
			return -ENODATA;
		}
		usb_ep = msm_rpc_connect_compatible(usb_rpc_ids.prog,
					usb_rpc_ids.vers_comp,
					MSM_RPC_UNINTERRUPTIBLE);
	}

	if (IS_ERR(usb_ep)) {
		pr_err("%s: connect compatible failed vers = %lx\n",
				__func__, usb_rpc_ids.vers_comp);
		return -EAGAIN;
	} else
		pr_debug("%s: rpc connect success vers = %lx\n",
				__func__, usb_rpc_ids.vers_comp);

	return 0;
}
EXPORT_SYMBOL(msm_hsusb_rpc_connect);

/* rpc connect for charging */
int msm_chg_rpc_connect(void)
{
	uint32_t chg_vers;

	if (machine_is_msm7x27_surf() || machine_is_qsd8x50_surf())
		return -ENOTSUPP;

	if (chg_ep && !IS_ERR(chg_ep)) {
		pr_debug("%s: chg_ep already connected\n", __func__);
		return 0;
	}

	chg_vers = 0x00040001;
	if (!msm_chg_init_rpc(chg_vers))
		goto chg_found;

	chg_vers = 0x00030001;
	if (!msm_chg_init_rpc(chg_vers))
		goto chg_found;

	chg_vers = 0x00020001;
	if (!msm_chg_init_rpc(chg_vers))
		goto chg_found;

	chg_vers = 0x00010001;
	if (!msm_chg_init_rpc(chg_vers))
		goto chg_found;

	pr_err("%s: connect compatible failed \n",
			__func__);
	return -EAGAIN;

chg_found:
	pr_debug("%s: connected to rpc vers = %x\n",
			__func__, chg_vers);
	return 0;
}
EXPORT_SYMBOL(msm_chg_rpc_connect);

/* rpc call for phy_reset */
int msm_hsusb_phy_reset(void)
{
	int rc = 0;
	struct hsusb_phy_start_req {
		struct rpc_request_hdr hdr;
	} req;

	if (!usb_ep || IS_ERR(usb_ep)) {
		pr_err("%s: phy_reset rpc failed before call,"
			"rc = %ld\n", __func__, PTR_ERR(usb_ep));
		return -EAGAIN;
	}

	rc = msm_rpc_call(usb_ep, usb_rpc_ids.init_phy,
				&req, sizeof(req), 5 * HZ);

	if (rc < 0) {
		pr_err("%s: phy_reset rpc failed! rc = %d\n",
			__func__, rc);
	} else
		pr_debug("msm_hsusb_phy_reset\n");

	return rc;
}
EXPORT_SYMBOL(msm_hsusb_phy_reset);

/* rpc call for vbus powerup */
int msm_hsusb_vbus_powerup(void)
{
	int rc = 0;
	struct hsusb_phy_start_req {
		struct rpc_request_hdr hdr;
	} req;

	if (!usb_ep || IS_ERR(usb_ep)) {
		pr_err("%s: vbus_powerup rpc failed before call,"
			"rc = %ld\n", __func__, PTR_ERR(usb_ep));
		return -EAGAIN;
	}

	rc = msm_rpc_call(usb_ep, usb_rpc_ids.vbus_pwr_up,
		&req, sizeof(req), 5 * HZ);

	if (rc < 0) {
		pr_err("%s: vbus_powerup failed! rc = %d\n",
			__func__, rc);
	} else
		pr_debug("msm_hsusb_vbus_powerup\n");

	return rc;
}
EXPORT_SYMBOL(msm_hsusb_vbus_powerup);

/* rpc call for vbus shutdown */
int msm_hsusb_vbus_shutdown(void)
{
	int rc = 0;
	struct hsusb_phy_start_req {
		struct rpc_request_hdr hdr;
	} req;

	if (!usb_ep || IS_ERR(usb_ep)) {
		pr_err("%s: vbus_shutdown rpc failed before call,"
			"rc = %ld\n", __func__, PTR_ERR(usb_ep));
		return -EAGAIN;
	}

	rc = msm_rpc_call(usb_ep, usb_rpc_ids.vbus_pwr_down,
		&req, sizeof(req), 5 * HZ);

	if (rc < 0) {
		pr_err("%s: vbus_shutdown failed! rc = %d\n",
			__func__, rc);
	} else
		pr_debug("msm_hsusb_vbus_shutdown\n");

	return rc;
}
EXPORT_SYMBOL(msm_hsusb_vbus_shutdown);

int msm_hsusb_send_productID(uint32_t product_id)
{
	int rc = 0;
	struct hsusb_phy_start_req {
		struct rpc_request_hdr hdr;
		uint32_t product_id;
	} req;

	if (!usb_ep || IS_ERR(usb_ep)) {
		pr_err("%s: rpc connect failed: rc = %ld\n",
			__func__, PTR_ERR(usb_ep));
		return -EAGAIN;
	}

	req.product_id = cpu_to_be32(product_id);
	rc = msm_rpc_call(usb_ep, usb_rpc_ids.update_product_id,
				&req, sizeof(req),
				5 * HZ);
	if (rc < 0)
		pr_err("%s: rpc call failed! error: %d\n",
			__func__, rc);
	else
		pr_debug("%s: rpc call success\n" , __func__);

	return rc;
}
EXPORT_SYMBOL(msm_hsusb_send_productID);

int msm_hsusb_send_serial_number(const char *serial_number)
{
	int rc = 0, serial_len, rlen;
	struct hsusb_send_sn_req {
		struct rpc_request_hdr hdr;
		uint32_t length;
		char sn[0];
	} *req;

	if (!usb_ep || IS_ERR(usb_ep)) {
		pr_err("%s: rpc connect failed: rc = %ld\n",
			__func__, PTR_ERR(usb_ep));
		return -EAGAIN;
	}

	/*
	 * USB driver passes null terminated string to us. Modem processor
	 * expects serial number to be 32 bit aligned.
	 */
	serial_len  = strlen(serial_number)+1;
	rlen = sizeof(struct rpc_request_hdr) + sizeof(uint32_t) +
			((serial_len + 3) & ~3);

	req = kmalloc(rlen, GFP_KERNEL);
	if (!req)
		return -ENOMEM;

	req->length = cpu_to_be32(serial_len);
	strncpy(req->sn , serial_number, serial_len);
	rc = msm_rpc_call(usb_ep, usb_rpc_ids.update_serial_num,
				req, rlen, 5 * HZ);
	if (rc < 0)
		pr_err("%s: rpc call failed! error: %d\n",
			__func__, rc);
	else
		pr_debug("%s: rpc call success\n", __func__);

	kfree(req);
	return rc;
}
EXPORT_SYMBOL(msm_hsusb_send_serial_number);

int msm_hsusb_is_serial_num_null(uint32_t val)
{
	int rc = 0;
	struct hsusb_phy_start_req {
			struct rpc_request_hdr hdr;
			uint32_t value;
	} req;

	if (!usb_ep || IS_ERR(usb_ep)) {
		pr_err("%s: rpc connect failed: rc = %ld\n",
			__func__, PTR_ERR(usb_ep));
		return -EAGAIN;
	}
	if (!usb_rpc_ids.update_is_serial_num_null) {
		pr_err("%s: proc id not supported \n", __func__);
		return -ENODATA;
	}

	req.value = cpu_to_be32(val);
	rc = msm_rpc_call(usb_ep, usb_rpc_ids.update_is_serial_num_null,
				&req, sizeof(req),
				5 * HZ);
	if (rc < 0)
		pr_err("%s: rpc call failed! error: %d\n" ,
			__func__, rc);
	else
		pr_debug("%s: rpc call success\n", __func__);

	return rc;
}
EXPORT_SYMBOL(msm_hsusb_is_serial_num_null);

int msm_chg_usb_charger_connected(uint32_t device)
{
	int rc = 0;
	struct hsusb_start_req {
		struct rpc_request_hdr hdr;
		uint32_t otg_dev;
	} req;

	if (!chg_ep || IS_ERR(chg_ep))
		return -EAGAIN;
	req.otg_dev = cpu_to_be32(device);
	rc = msm_rpc_call(chg_ep, chg_rpc_ids.chg_usb_charger_connected_proc,
			&req, sizeof(req), 5 * HZ);

	if (rc < 0) {
		pr_err("%s: charger_connected failed! rc = %d\n",
			__func__, rc);
	} else
		pr_debug("msm_chg_usb_charger_connected\n");

	return rc;
}
EXPORT_SYMBOL(msm_chg_usb_charger_connected);

int msm_chg_usb_i_is_available(uint32_t sample)
{
	int rc = 0;
	struct hsusb_start_req {
		struct rpc_request_hdr hdr;
		uint32_t i_ma;
	} req;

	if (!chg_ep || IS_ERR(chg_ep))
		return -EAGAIN;
	req.i_ma = cpu_to_be32(sample);
	rc = msm_rpc_call(chg_ep, chg_rpc_ids.chg_usb_i_is_available_proc,
			&req, sizeof(req), 5 * HZ);

	if (rc < 0) {
		pr_err("%s: charger_i_available failed! rc = %d\n",
			__func__, rc);
	} else
		pr_debug("msm_chg_usb_i_is_available(%u)\n", sample);

	return rc;
}
EXPORT_SYMBOL(msm_chg_usb_i_is_available);

int msm_chg_usb_i_is_not_available(void)
{
	int rc = 0;
	struct hsusb_start_req {
		struct rpc_request_hdr hdr;
	} req;

	if (!chg_ep || IS_ERR(chg_ep))
		return -EAGAIN;
	rc = msm_rpc_call(chg_ep, chg_rpc_ids.chg_usb_i_is_not_available_proc,
			&req, sizeof(req), 5 * HZ);

	if (rc < 0) {
		pr_err("%s: charger_i_not_available failed! rc ="
			"%d \n", __func__, rc);
	} else
		pr_debug("msm_chg_usb_i_is_not_available\n");

	return rc;
}
EXPORT_SYMBOL(msm_chg_usb_i_is_not_available);

int msm_chg_usb_charger_disconnected(void)
{
	int rc = 0;
	struct hsusb_start_req {
		struct rpc_request_hdr hdr;
	} req;

	if (!chg_ep || IS_ERR(chg_ep))
		return -EAGAIN;
	rc = msm_rpc_call(chg_ep, chg_rpc_ids.chg_usb_charger_disconnected_proc,
			&req, sizeof(req), 5 * HZ);

	if (rc < 0) {
		pr_err("%s: charger_disconnected failed! rc = %d\n",
			__func__, rc);
	} else
		pr_debug("msm_chg_usb_charger_disconnected\n");

	return rc;
}
EXPORT_SYMBOL(msm_chg_usb_charger_disconnected);

/* rpc call to close connection */
int msm_hsusb_rpc_close(void)
{
	int rc = 0;

	if (IS_ERR(usb_ep)) {
		pr_err("%s: rpc_close failed before call, rc = %ld\n",
			__func__, PTR_ERR(usb_ep));
		return -EAGAIN;
	}

	rc = msm_rpc_close(usb_ep);
	usb_ep = NULL;

	if (rc < 0) {
		pr_err("%s: close rpc failed! rc = %d\n",
			__func__, rc);
		return -EAGAIN;
	} else
		pr_debug("rpc close success\n");

	return rc;
}
EXPORT_SYMBOL(msm_hsusb_rpc_close);

/* rpc call to close charging connection */
int msm_chg_rpc_close(void)
{
	int rc = 0;

	if (IS_ERR(chg_ep)) {
		pr_err("%s: rpc_close failed before call, rc = %ld\n",
			__func__, PTR_ERR(chg_ep));
		return -EAGAIN;
	}

	rc = msm_rpc_close(chg_ep);
	chg_ep = NULL;

	if (rc < 0) {
		pr_err("%s: close rpc failed! rc = %d\n",
			__func__, rc);
		return -EAGAIN;
	} else
		pr_debug("rpc close success\n");

	return rc;
}
EXPORT_SYMBOL(msm_chg_rpc_close);

int msm_hsusb_reset_rework_installed(void)
{
	int rc = 0;
	struct hsusb_start_req {
		struct rpc_request_hdr hdr;
	} req;
	struct hsusb_rpc_rep {
		struct rpc_reply_hdr hdr;
		uint32_t rework;
	} rep;

	memset(&rep, 0, sizeof(rep));

	if (!usb_ep || IS_ERR(usb_ep)) {
		pr_err("%s: hsusb rpc connection not initialized, rc = %ld\n",
			__func__, PTR_ERR(usb_ep));
		return -EAGAIN;
	}

	rc = msm_rpc_call_reply(usb_ep, usb_rpc_ids.reset_rework_installed,
				&req, sizeof(req),
				&rep, sizeof(rep), 5 * HZ);

	if (rc < 0) {
		pr_err("%s: rpc call failed! error: (%d)"
				"proc id: (%lx)\n",
				__func__, rc,
				usb_rpc_ids.reset_rework_installed);
		return rc;
	}

	pr_info("%s: rework: (%d)\n", __func__, rep.rework);
	return be32_to_cpu(rep.rework);
}
EXPORT_SYMBOL(msm_hsusb_reset_rework_installed);

static int msm_hsusb_pmic_ulpidata0_config(int enable)
{
	int rc = 0;
	struct hsusb_start_req {
		struct rpc_request_hdr hdr;
	} req;

	if (!usb_ep || IS_ERR(usb_ep)) {
		pr_err("%s: hsusb rpc connection not initialized, rc = %ld\n",
			__func__, PTR_ERR(usb_ep));
		return -EAGAIN;
	}

	if (enable)
		rc = msm_rpc_call(usb_ep, usb_rpc_ids.enable_pmic_ulpi_data0,
					&req, sizeof(req), 5 * HZ);
	else
		rc = msm_rpc_call(usb_ep, usb_rpc_ids.disable_pmic_ulpi_data0,
					&req, sizeof(req), 5 * HZ);

	if (rc < 0)
		pr_err("%s: rpc call failed! error: %d\n",
				__func__, rc);
	return rc;
}

int msm_hsusb_enable_pmic_ulpidata0(void)
{
	return msm_hsusb_pmic_ulpidata0_config(1);
}
EXPORT_SYMBOL(msm_hsusb_enable_pmic_ulpidata0);

int msm_hsusb_disable_pmic_ulpidata0(void)
{
	return msm_hsusb_pmic_ulpidata0_config(0);
}
EXPORT_SYMBOL(msm_hsusb_disable_pmic_ulpidata0);


/* wrapper for sending pid and serial# info to bootloader */
int usb_diag_update_pid_and_serial_num(uint32_t pid, const char *snum)
{
	int ret;

	ret = msm_hsusb_send_productID(pid);
	if (ret)
		return ret;

	if (!snum) {
		ret = msm_hsusb_is_serial_num_null(1);
		if (ret)
			return ret;
	}

	ret = msm_hsusb_is_serial_num_null(0);
	if (ret)
		return ret;
	ret = msm_hsusb_send_serial_number(snum);
	if (ret)
		return ret;

	return 0;
}


#ifdef CONFIG_USB_MSM_72K
/* charger api wrappers */
int hsusb_chg_init(int connect)
{
	if (connect)
		return msm_chg_rpc_connect();
	else
		return msm_chg_rpc_close();
}
EXPORT_SYMBOL(hsusb_chg_init);

void hsusb_chg_vbus_draw(unsigned mA)
{
	msm_chg_usb_i_is_available(mA);
}
EXPORT_SYMBOL(hsusb_chg_vbus_draw);

void hsusb_chg_connected(enum chg_type chgtype)
{
	char *chg_types[] = {"STD DOWNSTREAM PORT",
			"CARKIT",
			"DEDICATED CHARGER",
			"INVALID"};

	if (chgtype == USB_CHG_TYPE__INVALID) {
		msm_chg_usb_i_is_not_available();
		msm_chg_usb_charger_disconnected();
		return;
	}

	pr_info("\nCharger Type: %s\n", chg_types[chgtype]);

	msm_chg_usb_charger_connected(chgtype);
}
EXPORT_SYMBOL(hsusb_chg_connected);
#endif
