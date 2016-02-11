
#if CONFIG_BATTERY_STC3117

int null_fn(void)
{
	return 0;                // for discharging status
}

int Temperature_fn(void)
{
	return (25);
}

static struct stc311x_platform_data stc3117_data = {
	.battery_online = NULL,
	.charger_online = null_fn, 		// used in stc311x_get_status()
	.charger_enable = null_fn,		// used in stc311x_get_status()
	.power_supply_register = NULL,
	.power_supply_unregister = NULL,

	.Vmode= 0,       /*REG_MODE, BIT_VMODE 1=Voltage mode, 0=mixed mode */
	.Alm_SOC = 10,      /* SOC alm level %*/
	.Alm_Vbat = 3600,   /* Vbat alm level mV*/
	.CC_cnf = 525,      /* nominal CC_cnf, coming from battery characterisation*/
	.VM_cnf = 558,      /* nominal VM cnf , coming from battery characterisation*/
	.Rint = 200,			/* nominal internal impedance*/
	.Cnom = 2600,       /* nominal capacity in mAh, coming from battery characterisation*/
	.Rsense = 10,       /* sense resistor mOhms*/
	.RelaxCurrent = 150, /* current for relaxation in mA (< C/20) */
	.Adaptive = 1,     /* 1=Adaptive mode enabled, 0=Adaptive mode disabled */

	/* Elentec Co Ltd Battery pack - 80 means 8% */
	.CapDerating[6] = 71,   /* capacity derating in 0.1%, for temp = -20°C */
	.CapDerating[5] = 42,   /* capacity derating in 0.1%, for temp = -10°C */
	.CapDerating[4] = 13,   /* capacity derating in 0.1%, for temp = 0°C */
	.CapDerating[3] = 5,   /* capacity derating in 0.1%, for temp = 10°C */
	.CapDerating[2] = 0,   /* capacity derating in 0.1%, for temp = 25°C */
	.CapDerating[1] = 0,   /* capacity derating in 0.1%, for temp = 40°C */
	.CapDerating[0] = 0,   /* capacity derating in 0.1%, for temp = 60°C */

	.OCVValue[15] = 0,   /* OCV curve adjustment */
	.OCVValue[14] = 0,   /* OCV curve adjustment */
	.OCVValue[13] = 0,   /* OCV curve adjustment */
	.OCVValue[12] = 0,   /* OCV curve adjustment */
	.OCVValue[11] = 0,   /* OCV curve adjustment */
	.OCVValue[10] = 0,   /* OCV curve adjustment */
	.OCVValue[9] = 0,    /* OCV curve adjustment */
	.OCVValue[8] = 0,    /* OCV curve adjustment */
	.OCVValue[7] = 0,    /* OCV curve adjustment */
	.OCVValue[6] = 0,    /* OCV curve adjustment */
	.OCVValue[5] = 0,    /* OCV curve adjustment */
	.OCVValue[4] = 0,    /* OCV curve adjustment */
	.OCVValue[3] = 0,    /* OCV curve adjustment */
	.OCVValue[2] = 0,    /* OCV curve adjustment */
	.OCVValue[1] = 0,    /* OCV curve adjustment */
	.OCVValue[0] = 0,    /* OCV curve adjustment */

	/*if the application temperature data is preferred than the STC3117 temperature*/
	.ExternalTemperature = Temperature_fn, /*External temperature fonction, return °C*/
	.ForceExternalTemperature = 0, /* 1=External temperature, 0=STC3117 temperature */

};
#endif


static struct i2c_board_info __initdata beagle_i2c2_boardinfo[] = {

#if CONFIG_BATTERY_STC3117
	{
		I2C_BOARD_INFO("stc3117", 0x70),
			.platform_data = &stc3117_data,
	},
#endif

};
