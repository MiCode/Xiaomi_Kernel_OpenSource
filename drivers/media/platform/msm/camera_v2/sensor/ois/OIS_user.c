
/***** ROHM Confidential ***************************************************/
#ifndef OIS_USER_C
#define OIS_USER_C
#endif

#include "OIS_head.h"

OIS_UWORD			FOCUS_VAL	= 0x0122;

void	VCOSET0(void)
{

    OIS_UWORD 	CLK_PS = 24000;
    OIS_UWORD 	FVCO_1 = 36000;
    OIS_UWORD 	FREF   = 25;

    OIS_UWORD	DIV_N  = CLK_PS / FREF - 1;
    OIS_UWORD	DIV_M  = FVCO_1 / FREF - 1;

    I2C_OIS_per_write(0x62, DIV_N);
    I2C_OIS_per_write(0x63, DIV_M);
    I2C_OIS_per_write(0x64, 0x4060);

    I2C_OIS_per_write(0x60, 0x3011);
    I2C_OIS_per_write(0x65, 0x0080);
    I2C_OIS_per_write(0x61, 0x8002);
    I2C_OIS_per_write(0x61, 0x8003);
    I2C_OIS_per_write(0x61, 0x8809);
}


void	VCOSET1(void)
{

    I2C_OIS_per_write(0x05, 0x000C);
    I2C_OIS_per_write(0x05, 0x000D);
}

struct msm_ois_ctrl_t *g_i2c_ctrl;

void	WR_I2C_inner(struct msm_ois_ctrl_t *ctrl, OIS_UBYTE slvadr, OIS_UBYTE size, OIS_UBYTE *dat)
{
	OIS_UWORD       addr = dat[0] << 8 | dat[1];
	OIS_UBYTE	*data_wr   = dat + 2;

	if (!ctrl) {
		ctrl = g_i2c_ctrl;
	}

	ctrl->i2c_client.addr_type = MSM_CAMERA_I2C_WORD_ADDR;
	ctrl->i2c_client.i2c_func_tbl->i2c_write_seq(
		&ctrl->i2c_client, addr, data_wr, size - 2);

	pr_debug("WR_I2C addr:0x%x data:0x%x", addr, data_wr[0]);
}

void	WR_I2C(OIS_UBYTE slvadr, OIS_UBYTE size, OIS_UBYTE *dat)
{
	WR_I2C_inner(g_i2c_ctrl, slvadr, size, dat);
}

OIS_UWORD	RD_I2C_inner(struct msm_ois_ctrl_t *ctrl, OIS_UBYTE slvadr, OIS_UBYTE size, OIS_UBYTE *dat)
{
	OIS_UWORD	read_data = 0;
	OIS_UWORD       addr = dat[0] << 8 | dat[1];

    if (!ctrl) {
		ctrl = g_i2c_ctrl;
	}

	ctrl->i2c_client.addr_type = MSM_CAMERA_I2C_WORD_ADDR;
	ctrl->i2c_client.i2c_func_tbl->i2c_read(
		&ctrl->i2c_client, addr, &read_data, MSM_CAMERA_I2C_WORD_DATA);

	pr_debug("RD_I2C addr:0x%x data:0x%x", addr, read_data);

	return read_data;
}

OIS_UWORD	RD_I2C(OIS_UBYTE slvadr, OIS_UBYTE size, OIS_UBYTE *dat)
{
	return RD_I2C_inner(g_i2c_ctrl, slvadr, size, dat);
}


void	store_FADJ_MEM_to_non_volatile_memory(_FACT_ADJ param)
{
}


void ois_spi_check(struct msm_ois_ctrl_t *ctrl)
{
	OIS_UBYTE wdat[4];
    OIS_UWORD rdat;
    unsigned char adr = 0x75;

    wdat[0] = 0x82;
    wdat[1] = 0x18;
    wdat[2] = (8 + 8 * 1) - 1;
    wdat[3] = 0x00;
    WR_I2C_inner(ctrl, _SLV_OIS_, 4, wdat);

    wdat[0] = 0x82;
    wdat[1] = 0x1B;
    wdat[2] = 0x00;
    wdat[3] = 0x80 + adr;
    WR_I2C_inner(ctrl, _SLV_OIS_, 4, wdat);

    wdat[0] = 0x82;
    wdat[1] = 0x1C;
    wdat[2] = 0x00;
    wdat[3] = 0x00;
    WR_I2C_inner(ctrl, _SLV_OIS_, 4, wdat);

    wdat[0] = 0x82;
    wdat[1] = 0x1C;
    rdat = RD_I2C_inner(ctrl, _SLV_OIS_, 2, wdat);

    pr_info("OIS SPI@0x%x is 0x%x \n", adr, rdat);
}


int fadj_ois_gyro_offset_calibraion (void)
{
	OIS_UWORD    u16_avrN;
	OIS_LONG     s32_dat1;
	OIS_LONG     s32_dat2;
	OIS_UWORD    u16_i;
	OIS_UWORD    u16_tmp_read1;
	OIS_UWORD    u16_tmp_read2;
	OIS_UWORD    sid;
    int16_t      MaxNGTimes = 5;
    int16_t      NGTimes = 0;
	OIS_UWORD	 Times = 32;
	int16_t		 NGLimit = 3026;
    int16_t      rc = 0;
	OIS_LONG     s32_min_data1 = NGLimit;
	OIS_LONG     s32_max_data1 = -NGLimit;
	OIS_LONG     s32_min_data2 = NGLimit;
	OIS_LONG     s32_max_data2 = -NGLimit;
	int16_t		 OffsetLimit = 131;

	pr_info("NGLimit = (%d) \n", NGLimit);

	do {
		if (NGTimes >= MaxNGTimes) {
			break;
	}

	s32_dat1 = 0;
	s32_dat2 = 0;
	u16_avrN = 0;

		for (u16_i = 1; u16_i <= Times; u16_i += 1) {
			usleep_range(5000, 6000);
			u16_tmp_read1 = I2C_OIS_mem__read(_M_DigGx);
			u16_tmp_read2 = I2C_OIS_mem__read(_M_DigGy);
		}
		pr_info("Read %d times for ready. \n", Times);

		for (u16_i = 1; u16_i <= Times; u16_i += 1) {
			usleep_range(5000, 6000);
			u16_tmp_read1 = I2C_OIS_mem__read(_M_DigGx);
			u16_tmp_read2 = I2C_OIS_mem__read(_M_DigGy);
			if ((u16_tmp_read1 != 0) || (u16_tmp_read2 != 0)) {
				if (((int16_t)u16_tmp_read1) < s32_min_data1) {
					s32_min_data1 = (int16_t)u16_tmp_read1;
				} else if (((int16_t)u16_tmp_read1) > s32_max_data1) {
					s32_max_data1 = (int16_t)u16_tmp_read1;
				}

				if (((int16_t)u16_tmp_read2) < s32_min_data2) {
					s32_min_data2 = (int16_t)u16_tmp_read2;
				} else if (((int16_t)u16_tmp_read2) > s32_max_data2) {
					s32_max_data2 = (int16_t)u16_tmp_read2;
				}

				s32_dat1 += (int16_t)u16_tmp_read1;
				s32_dat2 += (int16_t)u16_tmp_read2;
				u16_avrN += 1;
			}
			pr_info("%02d,g 0x%04x 0x%04x -> %d,%d s:%ld,%ld\n", u16_i, u16_tmp_read1, u16_tmp_read2,
						(int16_t)u16_tmp_read1, (int16_t)u16_tmp_read2, s32_dat1, s32_dat2);
		}
		u16_tmp_read1 = s32_dat1 / u16_avrN;
		u16_tmp_read2 = s32_dat2 / u16_avrN;

		pr_info("gx:0x%04x gy:0x%04x -> %d,%d max:min=%ld:%ld %ld:%ld. \n", u16_tmp_read1, u16_tmp_read2,
			(int16_t)u16_tmp_read1, (int16_t)u16_tmp_read2,
			s32_max_data1, s32_min_data1, s32_max_data2, s32_min_data2);


		if (((((int16_t)u16_tmp_read1) > NGLimit || ((int16_t)u16_tmp_read1) < -NGLimit)) ||
		    ((((int16_t)u16_tmp_read2) > NGLimit || ((int16_t)u16_tmp_read2) < -NGLimit))) {
			pr_err("illegal calibration value, out of NG limit. \n");
			rc = -1;
			NGTimes++;
			continue;
		}

		if (s32_max_data1 - s32_min_data1 > OffsetLimit
			|| s32_max_data2 - s32_min_data2 > OffsetLimit) {
			pr_err("illegal calibration value, out of Offset limit. \n");



		}

		if (s32_dat1 == 0 || s32_dat2 == 0) {
			pr_err("Gyro IC output is always 0. \n");
			rc = -1;
			NGTimes++;
			continue;
		}

	NGTimes = 0;
	} while (NGTimes);

	if (rc) {
		pr_err("After %d times retry, calibration failed", MaxNGTimes - 1);
		return rc;
	}

	FADJ_MEM.gl_GX_OFS = u16_tmp_read1;
	FADJ_MEM.gl_GY_OFS = u16_tmp_read2;


	pr_info("Write back to EEPROM to save it\n");
	sid = g_i2c_ctrl->i2c_client.cci_client->sid;
	g_i2c_ctrl->i2c_client.cci_client->sid = 0xA0 >> 1;
	g_i2c_ctrl->i2c_client.addr_type = MSM_CAMERA_I2C_WORD_ADDR;

	g_i2c_ctrl->i2c_client.i2c_func_tbl->i2c_write(
		&g_i2c_ctrl->i2c_client, 0x48, FADJ_MEM.gl_GX_OFS, MSM_CAMERA_I2C_BYTE_DATA);
	usleep_range(1000, 2000);
	g_i2c_ctrl->i2c_client.i2c_func_tbl->i2c_write(
		&g_i2c_ctrl->i2c_client, 0x49, FADJ_MEM.gl_GX_OFS >> 8, MSM_CAMERA_I2C_BYTE_DATA);
	usleep_range(1000, 2000);
	g_i2c_ctrl->i2c_client.i2c_func_tbl->i2c_write(
		&g_i2c_ctrl->i2c_client, 0x4A, FADJ_MEM.gl_GY_OFS, MSM_CAMERA_I2C_BYTE_DATA);
	usleep_range(1000, 2000);
	g_i2c_ctrl->i2c_client.i2c_func_tbl->i2c_write(
		&g_i2c_ctrl->i2c_client, 0x4B, FADJ_MEM.gl_GY_OFS >> 8, MSM_CAMERA_I2C_BYTE_DATA);

	g_i2c_ctrl->i2c_client.cci_client->sid = sid;
	g_i2c_ctrl->i2c_client.addr_type = MSM_CAMERA_I2C_BYTE_ADDR;
	return 1;
}


static int fadj_got;
extern uint8_t g_cal_fadj_data[128];
void	get_FADJ_MEM_from_non_volatile_memory(void)
{

	OIS_UBYTE       *data = (OIS_UBYTE *)(&FADJ_MEM);
	OIS_UBYTE       buf[128];

	if (fadj_got)
		return;

	memcpy((void *) buf, (void *) g_cal_fadj_data, 128);

	printk("ois module %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x", buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7], buf[8], buf[9], buf[10], buf[11]);

	printk("ois fadj %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x", buf[0x3A], buf[0x3B], buf[0x3C], buf[0x3D], buf[0x3E], buf[0x3F], buf[0x40], buf[0x41], buf[0x42], buf[0x43], buf[0x44], buf[0x45]);

	memcpy((void *) data, (void *) (&buf[0x3A]), 38);

	printk("ois fadj %02x %02x %02x %02x %02x %02x %02x %02x ", data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7]);
	printk("ois fadj %02x %02x %02x %02x %02x %02x %02x %02x ", data[8], data[9], data[10], data[11], data[12], data[13], data[14], data[15]);
	printk("ois fadj 0x%04x 0x%04x 0x%04x\n", FADJ_MEM.gl_CURDAT, FADJ_MEM.gl_HALOFS_X, FADJ_MEM.gl_HALOFS_Y);


	fadj_got = 1;
}

int debug_print(const char *format, ...)
{
	return 0;

}

