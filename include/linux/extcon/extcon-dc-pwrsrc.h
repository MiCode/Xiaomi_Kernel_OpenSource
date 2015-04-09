
#ifndef __DC_XPWR_PWRSRC_H_
#define __DC_XPWR_PWRSRC_H_

struct dc_xpwr_pwrsrc_pdata {
	bool	en_chrg_det;
	/* Gpio based mux to switch D+/D- line between pmic and soc */
	struct gpio_desc *gpio_mux_cntl;
	bool chrg_usb_compliance;
};

#ifdef CONFIG_INTEL_SOC_PMIC
int dc_xpwr_vbus_on_status(void);
#else
static int dc_xpwr_vbus_on_status(void)
{
	return 0;
}
#endif

#endif
