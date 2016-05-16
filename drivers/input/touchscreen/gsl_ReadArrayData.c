
#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>


#define GSL1688E		0
#define GSL1680E		0
#define GSL2682B		0
#define GSL2681B		0
#define GSL2681A		0
#define GSL26915		0
#define GSL3680B		0
#define GSL3670A		0
#define GSL3692			0
#define GSL3675B		0
#define GSL3676B		0
#define GSL3679B		0
#define GSL3692B		0
#define GSL36762B		0
#define GSL915			1
#define GSL960			0
#define GSL968			0

#define		GSL_STRCAT(ch1, ch2, size)	\
			if ((size-strlen(ch1)) > (strlen(ch2)))	\
				strcat(ch1, ch2);		\
			else if ((size-strlen(ch1)) > 2)		\
				memcpy(ch1, ch2, (size-strlen(ch1)-1))


#define UINT		unsigned int
#define	CONF_PAGE_2680(n)		(0x04 + (n))
#define	START_2680				0x500
#define	START_1682				START_2680
#define	START_1680				(0x9f*0x80 + 0x14)

#define	CPU_TYPE_NONE			0
#define CPU_TYPE_1680			0x1680
#define	CPU_TYPE_1682			0x1682
#define	CPU_TYPE_1688			0x1688
#define	CPU_TYPE_2680			0x2680
#define	CPU_TYPE_3670			0x3670
#define	CPU_TYPE_968			0x968
#define	CPU_TYPE_3692			0x3692

#define GATHER_AVERAGE_NONE			0
#define GATHER_AVERAGE_LINE			1
#define GATHER_AVERAGE_ROW			2
#define GATHER_AVERAGE_ORDER		4
#define GATHER_AVERAGE_EFFECTIVE	8

#define GATHER_DATA_BASE			1
#define GATHER_DATA_SUB				2
#define GATHER_DATA_REFE			3
#define GATHER_DATA_DAC				4
#define	GATHER_DATA_TEMP			5

#define CORE_010400XX			0x10400000
#define CORE_01040001			0x10400001

extern void gsl_I2C_ROnePage(unsigned int addr, char *buf);
extern void gsl_I2C_RTotal_Address(unsigned int addr, unsigned int *data);

#define DATA_SEN_MAX			24
#define TRUE				1
#define FALSE				0
static short gsl_ogv[32][24];
static unsigned int core_vers;
static unsigned int base_addr;
static unsigned int dac_num;
static unsigned int drv_num;
static unsigned int sen_num;
static unsigned int drv_key;
static unsigned int sen_key;
static unsigned int sen_scan_num;
static unsigned int sen_order[24];
static unsigned int ic_addr;


static unsigned int dac_sen_num = 0x0;;
#if (GSL1688E || GSL1680E)
static int cpu_type = CPU_TYPE_1688;
#elif (GSL2682B || GSL2681B)
static int cpu_type = CPU_TYPE_1682;
#elif (GSL915 || GSL3670A || GSL26915)
static int cpu_type = CPU_TYPE_3670;
#elif (GSL960 || GSL968)
static int cpu_type = CPU_TYPE_968;
#elif (GSL3692 || GSL3680B || GSL3675B || GSL3676B || GSL3679B || GSL3692B || GSL36762B)
static int cpu_type = CPU_TYPE_3692;
#elif (GSL2681A)
static int cpu_type = CPU_TYPE_2680;
#else
static int cpu_type = CPU_TYPE_1688;
#endif

static int read_type = GATHER_DATA_BASE;
static int ori_frame = 0x00;
static int dac_type = 0x00;

static union {
	UINT sen_table_int[5];
	unsigned char sen_table[20];
} sen_data;

typedef struct {
	struct {
		unsigned int origin_up_limit[2];
		unsigned int origin_low_limit[2];
	} origin_limit[2];

	struct {
		unsigned int dac_up_limit[4];
		unsigned int dac_low_limit[4];
	} dac_limit[2];

	unsigned int Rate;

	struct {
		unsigned int key_num;
		unsigned int key[24];
	} dac_scope;

	struct {
		unsigned int key_dac_up_limit;
		unsigned int key_dac_low_limit;
	} key_dac;

	int key_and_aa_dac_split;
} Judge_Rule;

static Judge_Rule Test_Rule = {

	{
		{{6500, 6500},
			{1500, 1500} }
		,
		{{6500, 6500},
			{1500, 1500} }
	},

	{
		{{95, 95, 95, 95},
			{30, 30, 30, 30} }
		,
		{{4, 3, 2, 1},
			{0, 0, 0, 0} }
	},
	100,
	{
		0,
		{2, 5, 8, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24}
	},
	{
		75,
		45
	},
	1

};

static unsigned int dac_save[4][24];
static unsigned int dac_data[4][24];
static unsigned int sen[24];

static UINT scan_sen[2];
static UINT scan_num;
static char onepage[128] = {0};


static UINT CoreVersion(UINT *data);
static int GetSenAddr(int n)
{
	int i, t;
	for (i = 0; i < 24; i++) {
		t = sen_order[i];
		if (t == n)
			break;
	}
	return i;
}

static void ReadDataShort(int start_address)
{
	int i, t, s, read_page;
	unsigned short read_data[32*2];
	read_page = 0;
	t = (0x80/4);
	gsl_I2C_ROnePage(start_address/0x80, onepage);
	while (1) {

		memcpy(read_data, onepage, 128);
		for (i = 0; i < 32*2; i++) {
			t = ((read_page + start_address/0x80)*0x80 - start_address + i*2) ;
			if (t < 0)
				continue;
			if (t >= (drv_num + drv_key) * sen_scan_num * 2)
				break;
			t /= 2;
			s = GetSenAddr(t%sen_scan_num);
			if (s >= sen_num+sen_key)
				continue;
			gsl_ogv[t/sen_scan_num][s] = read_data[i];
		}
		if (i < 32*2)
			break;
		else
			read_page++;

		gsl_I2C_ROnePage(start_address/0x80+read_page, onepage);
	}
}

static void ReadDataInt(int start_address, int line_num, int ex)
{
	int i, t, s, d, read_page;
	unsigned int read_data[32];

	read_page = 0;

	gsl_I2C_ROnePage(start_address/0x80+read_page, onepage);
	while (1) {

		memcpy(read_data, onepage, 128);
		for (i = 0; i < 32; i++) {
			t = ((read_page + start_address/0x80)*0x80 - start_address + i*4) ;
			if (t < 0)
				continue;
			if (t >= (drv_num + 1 + drv_key + 1) * line_num * 4)
				break;
			t /= 4;
			s = t%line_num - 1;
			d = t/line_num - 1;
			if (s < 0 || d < 0)
				continue;
			if (s < sen_num && d < drv_num) {
				gsl_ogv[d][s] = read_data[i];
				continue;
			}
			if (s > sen_num && s < sen_num+sen_key+1 && d < drv_num+drv_key)
				gsl_ogv[d][s-1] = read_data[i];
			else if (d > drv_num && d < drv_num+drv_key+1 && s < sen_num)
				gsl_ogv[d-1][s] = read_data[i];
		}
		if (i < 32)
			break;
		else
			read_page++;

		gsl_I2C_ROnePage(start_address/0x80+read_page, onepage);
	}

}


static int InitGather(void)
{
	UINT ret = TRUE;
	int i;
	unsigned int *data_temp;
	union data {
		UINT data_int[6];
		unsigned char data_char[24];
	} data;


	data_temp = (unsigned int *)kzalloc(32*4, GFP_KERNEL);
	if (!data_temp)
		return FALSE;
	gsl_I2C_ROnePage(0x2, (unsigned char *)data_temp);
	core_vers = CoreVersion(data_temp);
	gsl_I2C_RTotal_Address(CONF_PAGE_2680(-1)+0, &base_addr);
	if ((base_addr & 0xffffffc0) == 0xa5a5ffc0)
		base_addr = 0;
	else
		base_addr = 1;
	kfree(data_temp);
	gsl_I2C_RTotal_Address(CONF_PAGE_2680(3)*0x80+0x54, &dac_num);
	gsl_I2C_RTotal_Address(CONF_PAGE_2680(2)*0x80+0x00, &drv_num);
	gsl_I2C_RTotal_Address(CONF_PAGE_2680(2)*0x80+0x08, &sen_num);
	gsl_I2C_RTotal_Address(CONF_PAGE_2680(2)*0x80+0x04, &drv_key);
	gsl_I2C_RTotal_Address(CONF_PAGE_2680(2)*0x80+0x7c, &sen_key);
	gsl_I2C_RTotal_Address(CONF_PAGE_2680(4)*0x80+0x7c, &sen_scan_num);
	gsl_I2C_RTotal_Address(CONF_PAGE_2680(1)*0X80+0X7C, &dac_sen_num);
	gsl_I2C_RTotal_Address(0xff050080, &ic_addr);
	printk("why====ic_addr: ===0x%04x::\n", ic_addr);
	for (i = 0; i < 6; i++)
		gsl_I2C_RTotal_Address(CONF_PAGE_2680(3)*0x80+i*4, &data.data_int[i]);

	for (i = 0; i < 24; i++)
		sen_order[i] = data.data_char[i^3]^1;
	if (sen_key < 0 || sen_key > 24)
		sen_key = 0;
	if (drv_key < 0 || drv_key > 32)
		drv_key = 0;
	for (i = 0; i < 24; i++)
		if (sen_order[i] < 0 || sen_order[i] > 24)
			sen_order[i] = 0;
		if (ret != TRUE
			|| drv_num < 0 || drv_num+drv_key > 32
			|| sen_num < 0 || sen_num+sen_key > 24
			|| sen_scan_num < 0 || sen_scan_num > 24
			|| dac_num < 0 || dac_num > 4) {
			drv_num = 32;
			sen_num = 24;
			drv_key = 0;
			sen_key = 0;
			sen_scan_num = 24;
			for (i = 0; i < 24; i++)
				sen_order[i] = i;
		}
	if (ret == TRUE)
		return TRUE;
	else
	return FALSE;
}

static void ReadFrame(void)
{
	if (cpu_type == CPU_TYPE_1682 || (cpu_type == CPU_TYPE_1688 && core_vers < CORE_01040001)) {
		if (base_addr == 0) {
			if (read_type == GATHER_DATA_BASE)
				ReadDataShort(0x5980 + ori_frame*23*12*2);
			else if (read_type == GATHER_DATA_REFE)
				ReadDataShort(0x5980+23*12*2*2);
			else if (read_type == GATHER_DATA_SUB)
				ReadDataInt(0x5980+23*12*2*4, 14, TRUE);
		} else {
			if (read_type == GATHER_DATA_BASE)
				ReadDataShort(0x59d0 + ori_frame*23*12*2);
			else if (read_type == GATHER_DATA_REFE)
				ReadDataShort(0x59d0+23*12*2*2);
			else if (read_type == GATHER_DATA_SUB)
				ReadDataInt(0x59d0+23*12*2*4, 14, TRUE);

		}
	} else if (cpu_type == CPU_TYPE_1688 && core_vers >= CORE_01040001) {
		if (read_type == GATHER_DATA_BASE)
			ReadDataShort(0x5f80 + ori_frame*16*10*2);
		else if (read_type == GATHER_DATA_REFE)
			ReadDataShort(0x5f80+16*10*2*2);
		else if (read_type == GATHER_DATA_SUB)
			ReadDataInt(0x5f80+16*10*2*4, 12, TRUE);

	} else if (cpu_type == CPU_TYPE_2680) {
		if (read_type == GATHER_DATA_BASE)
			ReadDataShort(0x6100 + ori_frame*31*20*2);
		else if (read_type == GATHER_DATA_REFE)
			ReadDataShort(0x6100+31*20*2*2);
		else if (read_type == GATHER_DATA_SUB)
			ReadDataInt(0x6100+31*20*2*4, 22, TRUE);

	} else if (cpu_type == CPU_TYPE_1680) {
		if (read_type == GATHER_DATA_BASE)
			ReadDataShort(0x4000 + ori_frame*16*12*2);
		else if (read_type == GATHER_DATA_REFE)
			ReadDataShort(0x4800);
		else if (read_type == GATHER_DATA_SUB)
			ReadDataInt(0x5600, 12, TRUE);

	} else if (cpu_type == CPU_TYPE_3670) {
		if (read_type == GATHER_DATA_BASE)
			ReadDataShort(ic_addr + ori_frame*26*14*2);
		else if (read_type == GATHER_DATA_REFE)
			ReadDataShort(ic_addr+26*14*2*2);
		else if (read_type == GATHER_DATA_SUB)
			ReadDataInt(ic_addr+26*14*2*4, 16, TRUE);

	} else if (cpu_type == CPU_TYPE_968) {
		if (read_type == GATHER_DATA_BASE)
			ReadDataShort(0x5f00 + ori_frame*17*10*2);
		else if (read_type == GATHER_DATA_REFE)
			ReadDataShort(0x5f00+17*10*2*2);
		else if (read_type == GATHER_DATA_SUB)
			ReadDataInt(0x5f00+17*10*2*4, 12, TRUE);

	} else if (cpu_type == CPU_TYPE_3692) {
		if (read_type == GATHER_DATA_BASE)
			ReadDataShort(0x5a00 + ori_frame*32*24*2);
		else if (read_type == GATHER_DATA_REFE)
			ReadDataShort(0x5a00+32*24*2*2);
		else if (read_type == GATHER_DATA_SUB)
			ReadDataInt(0x5a00+32*24*2*4, 26, TRUE);
	}
}

static UINT CoreVersion(UINT *data)
{
	int i;
	int ret = 0;
	for (i = 0; i < 32-1; i++) {
		if ((data[i+0] & 0xffffffc0) == (0x03000000|(0x5a5a<<6)) && (data[i+1] & 0xfffffc00) == 0x82106000)
			ret |= (((data[i+0] & 0x3f)<<10) | (data[i+1]&0x3ff))<<16;
		if ((data[i+0] & 0xffffffc0) == (0x03000000|(0xa5a5<<6)) && (data[i+1] & 0xfffffc00) == 0x82106000)
			ret |= (((data[i+0] & 0x3f)<<10) | (data[i+1]&0x3ff));
	}
	return ret;
}

static int InitSenData(void)
{
	UINT i;

	{
		gsl_I2C_RTotal_Address(0xff08000c, &scan_sen[0]);
		gsl_I2C_RTotal_Address(0xff080008, &scan_sen[1]);

		gsl_I2C_RTotal_Address(CONF_PAGE_2680(4)*0x80+0x7c, &scan_num);
		gsl_I2C_RTotal_Address(CONF_PAGE_2680(1)*0x80+0x7c, &sen_num);

		for (i = 0; i < 5; i++)

			gsl_I2C_RTotal_Address(CONF_PAGE_2680(3)*0x80+i*4, &sen_data.sen_table_int[i]);
		return TRUE;
	}
}
static void GetSenOrder2680(UINT sen[])
{
	UINT i, j, k;
	UINT scan[(DATA_SEN_MAX+1)/2];
	UINT base[DATA_SEN_MAX];
	for (i = 0; i < sizeof(scan)/sizeof(scan[0]); i++) {
		if (i < 6)
			scan[i] = (scan_sen[0] >> (5 - i)*4) & 0xf;
		else
			scan[i] = (scan_sen[1] >> (11 - i)*4) & 0xf;
	}
	for (i = 0; i < scan_num && i < DATA_SEN_MAX; i++) {
		if (i < scan_num/2)
			base[i] = scan[i]*2;
		else
			base[i] = scan[i - scan_num/2]*2+1;
	}
	for (i = 0; i < sen_num && i < DATA_SEN_MAX; i++)
		sen[i] = base[sen_data.sen_table[i^3]^1];
	for (; i < DATA_SEN_MAX; i++)
		sen[i] = 0;

	for (i = 0; i < DATA_SEN_MAX; i++)
		if (sen[i] >= DATA_SEN_MAX)
			sen[i] = 0;
		for (j = 1; j < DATA_SEN_MAX; j++) {
			for (i = 0; i < j; i++) {
				if (sen[j] == sen[i])
					break;
			}
			if (i >= j)
				continue;
			for (i = 0; i < DATA_SEN_MAX; i++) {
				for (k = 0; k < DATA_SEN_MAX; k++) {
					if (sen[k] == i)
						break;
				}
				if (k >= DATA_SEN_MAX)
					break;
			}
			if (i < DATA_SEN_MAX)
				sen[j] = i;
		}
}

void DacRefresh(unsigned int w_r, unsigned int m_mode)
{
	int i, j, i2;
	InitSenData();
	GetSenOrder2680(sen);
	if (m_mode == 0) {
		for (j = 0; j < 4; j++) {
			for (i = 0; i < DATA_SEN_MAX; i++) {
				i2 = sen[i];
				if (w_r == FALSE) {
					dac_data[j][i] = dac_save[j][i2];
				} else
					dac_save[j][i2] = dac_data[j][i];

				if (w_r == FALSE && !(j < dac_num && i < dac_sen_num)) {
					dac_data[j][i] = 0;
				}
			}
		}
	}
}


static UINT CoreVersionCPU(void)
{
	union {
		UINT data_int[32];
		char data_char[128];
	} data_;
	UINT ret;
	gsl_I2C_ROnePage(0x01, data_.data_char);
	ret = CoreVersion(data_.data_int);
	if (ret)
		return ret;
	gsl_I2C_ROnePage(0x02, data_.data_char);
		return FALSE;
	ret = CoreVersion(data_.data_int);
	return ret;
}

static void DacRead(void)
{
	if (cpu_type == CPU_TYPE_1682
		|| cpu_type == CPU_TYPE_1688
		|| cpu_type == CPU_TYPE_2680
		|| cpu_type == CPU_TYPE_3670
		|| cpu_type == CPU_TYPE_968
		|| cpu_type == CPU_TYPE_3692) {
		union {
			UINT data_int[32];
			char data_char[128];
		} data_;
		int i;


		gsl_I2C_ROnePage(0x02, data_.data_char);

		if (CoreVersionCPU() >= CORE_01040001)
			dac_type = CORE_01040001;
		else
			dac_type = FALSE;

		if (dac_type == FALSE) {

			gsl_I2C_ROnePage(0x0b, onepage);
			memcpy(data_.data_int, onepage, 128);

			for (i = 0; i < 5*4*4; i++)
				dac_save[i/20][i%20] = data_.data_char[0x30+i];
		} else if (dac_type == CORE_01040001) {

			gsl_I2C_ROnePage(0x0b, onepage);
			memcpy(data_.data_int, onepage, 128);
			for (i = 0; i < 0x80-0x30; i++)
				dac_save[i/24][i%24] = data_.data_char[0x30+i];


			gsl_I2C_ROnePage(0x0c, onepage);
			memcpy(data_.data_int, onepage, 128);
			for (; i < 6*4*4; i++)
				dac_save[i/24][i%24] = data_.data_char[0x30+i-0x80];

		}

	}
	DacRefresh(FALSE, 0);
}

static unsigned char TestBase(char *str_result, int size)
{
	int i = 0, j = 0;
	unsigned char err_end = 1;
	unsigned char OK_NG = 1, OK_NG_1 = 1, OK_NG_2 = 1;
	char *up_origin, up_origin_temp[10] = {'\0'};
	char *low_origin, low_origin_temp[10] = {'\0'};
	if (!(up_origin = kzalloc(1024, GFP_KERNEL))) {
		return OK_NG;
	}
	if (!(low_origin = kzalloc(1024, GFP_KERNEL))) {
		kfree(up_origin);
		return OK_NG;
	}

	memset(up_origin, '\0', sizeof(up_origin));
	memset(low_origin, '\0', sizeof(low_origin));
	memset(str_result, '\0', size);

	for (i = 0; i < drv_num; i++) {
		for (j = 0; j < sen_num; j++) {
			printk("why=========%d>>>>>>>>>%d\n", gsl_ogv[i][j], Test_Rule.origin_limit[0].origin_up_limit[0]);
			printk("why=========%d>>>>>>>>>%d\n", gsl_ogv[i][j], Test_Rule.origin_limit[0].origin_low_limit[0]);
			if (gsl_ogv[i][j] > Test_Rule.origin_limit[0].origin_up_limit[0]) {
				GSL_STRCAT(up_origin, up_origin_temp, 1024);
				memset(up_origin_temp, '\0', sizeof(up_origin_temp));
				OK_NG_1 = 0;
				OK_NG = 0;
			}

			if (gsl_ogv[i][j] < Test_Rule.origin_limit[0].origin_low_limit[0]) {

				GSL_STRCAT(low_origin, low_origin_temp, 1024);

				memset(low_origin_temp, '\0', sizeof(low_origin_temp));
				OK_NG_2 = 0;
				OK_NG = 0;
			}
		}
	}
	GSL_STRCAT(low_origin, "\n", 1024);
	GSL_STRCAT(up_origin, "\n", 1024);
	if (!OK_NG) {
		if (!OK_NG_1) {
			err_end = 0;
			GSL_STRCAT(str_result, "orgin lager:\n", size);
			GSL_STRCAT(str_result, up_origin, size);
			printk("[%s] orgin lager:\n", __func__);
		} else {
			GSL_STRCAT(str_result, "origin lager test pass!\n", size);
			printk("[%s] origin lager test pass!\n", __func__);
		}

		if (!OK_NG_2) {
			err_end = 0;
			GSL_STRCAT(str_result, "orgin lower:\n", size);
			GSL_STRCAT(str_result, low_origin, size);
			printk("[%s] orgin lower:\n", __func__);
		} else {
			GSL_STRCAT(str_result, "orgin lower test pass!\n", size);
			printk("[%s] orgin lower test pass!\n", __func__);
		}
	} else {
		GSL_STRCAT(str_result, "Test Pass", size);
		printk("[%s] Test Pass\n", __func__);
	}
	kfree(up_origin);
	kfree(low_origin);
	return err_end;
}

static unsigned char TestDac(char *str_result, int size)
{
	unsigned char OK_NG = 1, OK_NG_1 = 1, OK_NG_2 = 1;
	unsigned char err_end = 1;
	int i = 0, j = 0;

	char *up_dac, up_dac_temp[10] = {'\0'};
	char *low_dac, low_dac_temp[10] = {'\0'};
	if (!(up_dac = kzalloc(1024, GFP_KERNEL))) {
		return OK_NG;
	}
	if (!(low_dac = kzalloc(1024, GFP_KERNEL))) {
		kfree(up_dac);
		return OK_NG;
	}
	memset(up_dac, '\0', 1024);
	memset(low_dac, '\0', 1024);
	memset(str_result, '\0', size);
	for (i = 0; i < dac_num-drv_key; i++) {
		for (j = 0; j < sen_num; j++) {
			if (dac_data[i][j] > Test_Rule.dac_limit[0].dac_up_limit[i]) {
				printk("why======>>>>>>>>>>>%d::(%d, %d)\n", dac_data[i][j], i, j);
				GSL_STRCAT(up_dac, up_dac_temp, 1024);
				memset(up_dac_temp, '\0', sizeof(up_dac_temp));

				OK_NG_1 = 0;
				OK_NG = 0;
			}
			if (dac_data[i][j] < Test_Rule.dac_limit[0].dac_low_limit[i]) {
				printk("why======<<<<<<<<<<%d::(%d, %d)\n", dac_data[i][j], i, j);
				GSL_STRCAT(low_dac, low_dac_temp, 1024);
				memset(low_dac_temp, '\0', sizeof(low_dac_temp));

				OK_NG_2 = 0;
				OK_NG = 0;
			}
		}
	}

	for (i = 0; i < Test_Rule.dac_scope.key_num; i++) {
		if (dac_data[dac_num-1][Test_Rule.dac_scope.key[i]] > Test_Rule.key_dac.key_dac_up_limit) {

			GSL_STRCAT(up_dac, up_dac_temp, 1024);
			memset(up_dac_temp, '\0', sizeof(up_dac_temp));
			OK_NG_1 = 0;
			OK_NG = 0;
		}
		if (dac_data[dac_num-1][Test_Rule.dac_scope.key[i]] < Test_Rule.key_dac.key_dac_low_limit) {

			GSL_STRCAT(low_dac, low_dac_temp, 1024);
			memset(low_dac_temp, '\0', sizeof(low_dac_temp));
			OK_NG_2 = 0;
			OK_NG = 0;
		}
	}
	GSL_STRCAT(up_dac, "\n", 1024);
	GSL_STRCAT(low_dac, "\n", 1024);
	if (!OK_NG) {
		if (!OK_NG_1) {
			err_end = 0;
			GSL_STRCAT(str_result, "up lager:", size);
			GSL_STRCAT(str_result, up_dac, size);
		} else {
			GSL_STRCAT(str_result, "dac lager test pass!\n", size);
		}

		if (!OK_NG_2) {
			err_end = 0;
			GSL_STRCAT(str_result, "dac lower:", size);
			GSL_STRCAT(str_result, low_dac, size);
		} else {
			GSL_STRCAT(str_result, "dac lower test pass!\n", size);
		}
	} else {
		GSL_STRCAT(str_result, "dac test pass!\n", size);
	}
	kfree(up_dac);
	kfree(low_dac);
	return err_end;
}

static unsigned char TestRate(char *str_result, int size)
{
	unsigned int dac_max[4] = {0x00}, dac_min[4] = {0x00};
	int i = 0, j = 0, k = 0, l = 0;
	unsigned char err_end = 1;
	char *temp;
	unsigned char err = 0;
	char dac_rate_temp[800] = {'\0'};
	if (!(temp = kzalloc(1024, GFP_KERNEL))) {
		return 0;
	}
	memset(str_result, '\0', size);
	for (i = 0; i < dac_num-1; i++) {
		dac_max[i] = dac_data[i][0];
		dac_min[i] = dac_data[i][0];
		for (j = 0; j < sen_num; j++) {
			if (dac_max[i] < dac_data[i][j]) {
				k = j;
				dac_max[i] = dac_data[i][j];
			}

			if (dac_min[i] > dac_data[i][j]) {
				l = j;
				dac_min[i] = dac_data[i][j];
			}
		}

		if (dac_max[i]*10 > (10+Test_Rule.Rate/10)*dac_min[i]) {
			err_end = 0;

			GSL_STRCAT(str_result, temp, size);
			GSL_STRCAT(dac_rate_temp, temp, sizeof(dac_rate_temp));
			memset(temp, '\0', sizeof(temp));
			err = 0;
		} else {
			GSL_STRCAT(dac_rate_temp, "Dac Rate Test Pass", sizeof(dac_rate_temp));
			err = 1;
		}
	}
	kfree(temp);
	GSL_STRCAT(str_result, dac_rate_temp, size);
	return err_end;
}

void gsl_write_test_config(unsigned int cmd, int value)
{
	unsigned int max = sizeof(Test_Rule)/sizeof(unsigned int);
	unsigned int *rule_buf = (unsigned int *)&Test_Rule;
	if (cmd < max)
		rule_buf[cmd] = value;
	return;

}
unsigned int gsl_read_test_config(unsigned int cmd)
{
	unsigned int ret = 0;
	unsigned int max = sizeof(Test_Rule)/sizeof(unsigned int);
	unsigned int *rule_buf = (unsigned int *)&Test_Rule;
	if (cmd < max)
		ret = rule_buf[cmd];
	return ret;
}
int gsl_obtain_array_data_ogv(unsigned int *gsl_ogv_new, int i_max, int j_max)
{
	int i, j;
	unsigned int ret = 1;
	int j_tmp = (j_max > 24 ? 24 : j_max);
	int i_tmp = (i_max > 32 ? 32 : i_max);
	printk("enter gsl_obtain_array_data_ogv\n");
	for (i = 0; i < i_tmp; i++) {
		for (j = 0; j < j_tmp; j++) {
		gsl_ogv_new[i*11+j] = (int)gsl_ogv[i][j];
		printk("%4d ", gsl_ogv_new[i*11+j]);

	}
		printk("\n");
		}
	return ret;
}
int gsl_obtain_array_data_dac(unsigned int *dac, int i_max, int j_max)
{
	int i;
	unsigned int ret = 1;
	int j_tmp = (j_max > 24 ? 24 : j_max);
	int i_tmp = (i_max > 4 ? 4 : i_max);
	for (i = 0; i < i_tmp; i++) {
		memcpy(&dac[i*j_max], dac_data[i], j_tmp*(sizeof(unsigned int)));
	}
	return ret;
}

int gsl_tp_module_test(char *buf, int size)
{
	int ret = size/3;
	int err, i;
	unsigned char tmp1, tmp2, tmp3;
	for (i = 0; i < 3; i++) {
		err = InitGather();
		if (err == TRUE)
			break;
	}
	if (err == FALSE) {
		printk("[%s] err == FALSE", __func__);
		return -EPERM;
	}

	printk("why====%s::111111111111\n", __func__);
	ReadFrame();
	printk("why====%s::222222222222\n", __func__);
	DacRead();
	printk("why====%s::333333333333\n", __func__);

	tmp1 = TestBase(buf, ret);
	buf[ret-1] = '\0';
	printk("[%s] TestBase Result: %d\n", __func__, tmp1);

	tmp2 = TestDac(&buf[ret], ret);
	buf[ret*2-1] = '\0';
	printk("[%s] TestDac Result: %d\n", __func__, tmp2);

	tmp3 = TestRate(&buf[ret*2], ret);
	buf[size] = '\0';
	printk("[%s] TestRate Result: %d\n", __func__, tmp3);

	if (tmp1 && tmp2 && tmp3)
		return 1;
	return -EPERM;
}

