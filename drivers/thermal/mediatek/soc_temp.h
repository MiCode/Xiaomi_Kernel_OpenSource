/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

 /* AUXADC Registers */
#define AUXADC_CON1_SET_V	0x008
#define AUXADC_CON1_CLR_V	0x00c
#define AUXADC_CON2_V		0x010
#define AUXADC_DATA(channel)	(0x14 + (channel) * 4)

#define APMIXED_SYS_TS_CON0	0x600
#define APMIXED_SYS_TS_CON1	0x604

/* Thermal Controller Registers */
#define TEMP_MONCTL0		0x000
#define TEMP_MONCTL1		0x004
#define TEMP_MONCTL2		0x008
#define TEMP_MONINT			0x00c
#define TEMP_MONINTSTS		0x010
#define TEMP_MONIDET0		0x014
#define TEMP_MONIDET1		0x018
#define TEMP_MSRCTL0		0x038
#define TEMP_MSRCTL1		0x03c
#define TEMP_AHBPOLL		0x040
#define TEMP_AHBTO			0x044
#define TEMP_ADCPNP0		0x048
#define TEMP_ADCPNP1		0x04c
#define TEMP_ADCPNP2		0x050
#define TEMP_ADCPNP3		0x0b4

#define TEMP_PROTCTL		0x0c0
#define TEMP_PROTTA			0x0c4
#define TEMP_PROTTB			0x0c8
#define TEMP_PROTTC			0x0cc

#define TEMP_ADCMUX			0x054
#define TEMP_ADCEN			0x060
#define TEMP_PNPMUXADDR		0x064
#define TEMP_ADCMUXADDR		0x068
#define TEMP_ADCENADDR		0x074
#define TEMP_ADCVALIDADDR	0x078
#define TEMP_ADCVOLTADDR	0x07c
#define TEMP_RDCTRL			0x080
#define TEMP_ADCVALIDMASK	0x084
#define TEMP_ADCVOLTAGESHIFT	0x088
#define TEMP_ADCWRITECTRL		0x08c
#define TEMP_MSR0				0x090
#define TEMP_MSR1				0x094
#define TEMP_MSR2				0x098
#define TEMP_MSR3				0x0B8
#define THERMINTST				0xF04
#define TEMP_AHBST				0xF18


/**********************************
 * Thermal Controller Register Mask Definition
 **********************************
 */
#define THERMAL_ENABLE_SEN0     0x1
#define THERMAL_ENABLE_SEN1     0x2
#define THERMAL_ENABLE_SEN2     0x4
#define THERMAL_ENABLE_SEN3     0x8
#define THERMAL_MONCTL0_MASK    0x00000007

#define THERMAL_PUNT_MASK       0x00000FFF
#define THERMAL_FSINTVL_MASK    0x03FF0000
#define THERMAL_SPINTVL_MASK    0x000003FF
#define THERMAL_MON_INT_MASK    0x0007FFFF

#define THERMAL_MON_CINTSTS0    0x000001
#define THERMAL_MON_HINTSTS0    0x000002
#define THERMAL_MON_LOINTSTS0   0x000004
#define THERMAL_MON_HOINTSTS0   0x000008
#define THERMAL_MON_NHINTSTS0   0x000010
#define THERMAL_MON_CINTSTS1    0x000020
#define THERMAL_MON_HINTSTS1    0x000040
#define THERMAL_MON_LOINTSTS1   0x000080
#define THERMAL_MON_HOINTSTS1   0x000100
#define THERMAL_MON_NHINTSTS1   0x000200
#define THERMAL_MON_CINTSTS2    0x000400
#define THERMAL_MON_HINTSTS2    0x000800
#define THERMAL_MON_LOINTSTS2   0x001000
#define THERMAL_MON_HOINTSTS2   0x002000
#define THERMAL_MON_NHINTSTS2   0x004000
#define THERMAL_MON_TOINTSTS    0x008000
#define THERMAL_MON_IMMDINTSTS0 0x010000
#define THERMAL_MON_IMMDINTSTS1 0x020000
#define THERMAL_MON_IMMDINTSTS2 0x040000
#define THERMAL_MON_FILTINTSTS0 0x080000
#define THERMAL_MON_FILTINTSTS1 0x100000
#define THERMAL_MON_FILTINTSTS2 0x200000

#define THERMAL_tri_SPM_State0	0x20000000
#define THERMAL_tri_SPM_State1	0x40000000
#define THERMAL_tri_SPM_State2	0x80000000

#define THERMAL_MSRCTL0_MASK    0x00000007
#define THERMAL_MSRCTL1_MASK    0x00000038
#define THERMAL_MSRCTL2_MASK    0x000001C0


#define TEMP_SPARE0		0x0f0

#define PTPCORESEL		0xF00

#define TEMP_MONCTL1_PERIOD_UNIT(x)	((x) & 0x3ff)

#define TEMP_MONCTL2_FILTER_INTERVAL(x)	(((x) & 0x3ff) << 16)
#define TEMP_MONCTL2_SENSOR_INTERVAL(x)	((x) & 0x3ff)

#define TEMP_AHBPOLL_ADC_POLL_INTERVAL(x)	(x)

#define TEMP_ADCWRITECTRL_ADC_PNP_WRITE		BIT(0)
#define TEMP_ADCWRITECTRL_ADC_MUX_WRITE		BIT(1)

#define TEMP_ADCVALIDMASK_VALID_HIGH		BIT(5)
#define TEMP_ADCVALIDMASK_VALID_POS(bit)	(bit)

#define TEMP_MSRCTL1_BUS_STA	(BIT(0) | BIT(7))
#define TEMP_MSRCTL1_SENSING_POINTS_PAUSE	(0x10E)

#define _BIT_(_bit_)		((unsigned int)(1 << (_bit_)))
#define _BITMASK_(_bits_)	(((unsigned int) -1 >> (31 - ((1) ? _bits_)))\
			& ~((1U << ((0) ? _bits_)) - 1))

#define MIN(a, b) ({			\
	typeof(a) _max1 = (a);		\
	typeof(b) _max2 = (b);		\
	(void)(&_max1 == &_max2);	\
	_max1 < _max2 ? _max1 : _max2;	\
})

#define MAX(a, b) ({			\
		typeof(a) _max1 = (a);		\
		typeof(b) _max2 = (b);		\
		(void)(&_max1 == &_max2);	\
		_max1 > _max2 ? _max1 : _max2;	\
})

#define MASK_FILED_VALUE(DATA, MSB, LSB) ((DATA & ((unsigned int) -1 >> (31 -  MSB))) >> (LSB))

struct soc_thermal;

struct therm_rst_ctrl {
	unsigned int id;
	void __iomem *rst_base;
	u64 rst_phys_base;
	u32 rst_set_offset;
	u32 rst_set_bit;
	u32 rst_clr_offset;
	u32 rst_clr_bit;
	u32 rst_sta_offset;
	u32 rst_sta_bit;
};

struct thermal_bank_cfg {
	unsigned int num_sensors;
	const int *sensors;
	u64 bank_offset;
	int protect_high;
	int protect_middle;
	int protect_low;
};

struct soc_thermal_bank {
	struct soc_thermal *mt;
	int id;
	void __iomem *bank_base;
};

struct sensor_data {
	unsigned int id; /* if id is 0, get max temperature of all sensors */
	unsigned long update_flag;
	int temp;		/* Current temperature */
	unsigned int msr_raw;	/* MSR raw data from LVTS */
	struct soc_thermal *soc_thermal;
};

struct tc_speed {
	unsigned int tempMonCtl1;
	unsigned int tempMonCtl2;
	unsigned int tempAhbPoll;
};

struct soc_thermal_data {
	s32 num_banks;
	s32 num_sensors;
	/* virtual sensor, for  max in all sensor, id = 0xFF
	 * so sox_max in dts, must set 0xFF
	 */
	struct sensor_data vir_sen_data;
	struct sensor_data *sen_data;
	/*thermal controller design info*/
	s32 auxadc_channel;
	const int *sensor_mux_values;
	const int *msr;
	const int *adcpnp;
	/* Calibration values */
	s32 adc_ge;
	s32 degc_cali;
	s32 o_slope_sign;
	s32 o_slope;
	s32 o_cali_id;
	s32 o_cali_en;
	s32 o_const;
	s32 o_offset;
	int *vts;
	/*adc buffer register*/
	int TS2ADCMUX_Addr_offset;
	int TS2ADCMUX_BITS;
	int TS2ADCMUX_EnVAL;
	int TSVBE_SEL_Addr_offset;
	//for thermal
	struct tc_speed tc_speed;

	/*each bank info*/
	struct thermal_bank_cfg bank_data[];
};

struct soc_thermal {
	struct device *dev;
	void __iomem *thermal_base;
	void __iomem *apmixed_base;
	void __iomem *auxadc_base;
	u64 apmixed_phys_base;
	u64 auxadc_phys_base;

	int therm_rst_num;
	struct therm_rst_ctrl *therm_rst_ctrl;

	struct clk *clk_peri_therm;
	struct clk *clk_auxadc;
	/* lock: for getting and putting banks */
	struct mutex lock;

	struct soc_thermal_data *conf;
	struct soc_thermal_bank banks[];
};
