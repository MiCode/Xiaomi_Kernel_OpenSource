/**
 * @file   rx1618.c
 * @author  <colin>
 * @date   Tuesday, January 9th 2018
 *
 * @brief
 *
 *
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/i2c.h>
#include <linux/sysfs.h>
#include <linux/power_supply.h>
#include <asm/unaligned.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/ioctl.h>
#include <asm/uaccess.h>
#include <soc/qcom/socinfo.h>

#include "rx1618.h"

#define RX1618_DELAY_MS      30000
#define EPP_MODE             1
#define BPP_MODE             0
#define MIN_VBUCK            4000
#define MAX_VBUCK            11000
#define AP_REV_DATA_OK       0xaa
#define AP_SENT_DATA_OK      0x55
#define PRIVATE_VBUCK_SET_CMD 0x80
#define PRIVATE_ID_CMD       0x86
#define PRIVATE_USB_TYPE_CMD 0x87
#define PRIVATE_FAST_CHG_CMD 0x88
#define PRIVATE_PRODUCT_TEST_CMD 0x89
#define PRIVATE_FOD_TEST_CMD 0x8A
#define PRIVATE_TX_HW_ID_CMD 0x8b

/* used registers define */ 
#define REG_RX_SENT_CMD      0x0000
#define REG_RX_SENT_DATA1    0x0001
#define REG_RX_SENT_DATA2    0x0002
#define REG_RX_SENT_DATA3    0x0003
#define REG_RX_SENT_DATA4    0x0004
#define REG_RX_SENT_DATA5    0x0005
#define REG_RX_REV_CMD       0x0020
#define REG_RX_REV_DATA1     0x0021
#define REG_RX_REV_DATA2     0x0022
#define REG_RX_REV_DATA3     0x0023
#define REG_RX_REV_DATA4     0x0024
#define REG_RX_REV_DATA5     0x0025
#define REG_RX_VRECT         0x000a
#define REG_RX_IRECT         0x000b
#define REG_RX_VBUCK_1       0x0008
#define REG_RX_VBUCK_2       0x0009
#define REG_AP_RX_COMM       0x000C

static struct rx1618_chg *g_chip;
static int g_Delta = 0;
static bool int_done_flag = false;
static u8 g_light_screen_flag = 0;
static u8 g_sar_done_flag = 0;
static u8 g_fast_chg_flag = 0;
static u8 g_id_done_flag = 0;
static u8 g_cali_done_flag = 0;
static u8 g_usb_type_flag = 0;
static u8 g_fw_id = 0;
static u8 g_hw_id_h,g_hw_id_l;
static u8 g_epp_or_bpp = BPP_MODE;

struct rx1618_chg {
	char *name;
	struct i2c_client *client;
	struct device *dev;
	struct regmap       *regmap;
	int irq_gpio;
	int enable_gpio;
	int chip_enable;
	int online;
	struct delayed_work    wireless_work;
	struct delayed_work    wireless_int_work;
	struct mutex    irq_complete;
	struct mutex    wireless_chg_lock;
	struct mutex    wireless_chg_int_lock;

	struct power_supply    *wip_psy;
	struct power_supply    *dc_psy;
	struct power_supply_desc    wip_psy_d;
	struct power_supply    *wireless_psy;
	int epp;
	int auth;
};

static int rx1618_read(struct rx1618_chg *chip, u8 *val, u16 addr)
{
	unsigned int temp;
	int rc;

	rc = regmap_read(chip->regmap, addr, &temp);
	if (rc >= 0) {
		*val = (u8)temp;
		//dev_err(chip->dev, "[rx1618] [%s] [0x%04x] = [0x%x] \n", __func__, addr, *val);
	}

	return rc;
}


static int rx1618_write(struct rx1618_chg *chip, u8 val, u16 addr)
{
	int rc = 0;

	rc = regmap_write(chip->regmap, addr, val);
	if (rc >= 0)
	{
		//dev_err(chip->dev, "[rx1618] [%s] [0x%04x] = [0x%x] \n", __func__, addr, *val);
	}

	return rc;
}


unsigned int rx1618_get_rx_vrect(struct rx1618_chg *chip)
{
	u8 data=0;
	u16 vrect=0;

	//if(!chip->online)
	//    return -EINVAL;

	rx1618_read(chip, &data, REG_RX_VRECT);
	vrect = (22500*data) >> 8;

	dev_err(chip->dev, "[rx1618] [%s] data = 0x%x, vrect=%d mV\n",__func__,data, vrect);

	return vrect;
}

unsigned int rx1618_get_rx_irect(struct rx1618_chg *chip)
{
	u8 data=0;
	u16 irect=0;

	//if(!chip->online)
	//    return -EINVAL;

	rx1618_read(chip, &data, REG_RX_IRECT);
	irect = (2500*data) >> 8;

	dev_err(chip->dev, "[rx1618] [%s] data = 0x%x, irect=%d mA \n",__func__,data,irect);

	return irect;
}


unsigned int rx1618_get_rx_vbuck(struct rx1618_chg *chip)
{
	u16 vbuck = 0;
	u8  data1 = 0;
	u8  data2 = 0;
	u16 data = 0;

	//   if(!chip->online)
	//      return -EINVAL;

	rx1618_read(chip, &data1, REG_RX_VBUCK_1);
	rx1618_read(chip, &data2, REG_RX_VBUCK_2);

	data=(data1<<2)+(data2 & 0x03);
	vbuck = (2500*590/120*data) >> 10;

	dev_err(chip->dev, "[rx1618] [%s] Vbuck: data=0x%x, vbuck=%d mV ,data1 = 0x%x, data2= 0x%x \n",__func__, data, vbuck, data1, data2);

	return vbuck; 
}

unsigned int rx1618_get_rx_ibuck(struct rx1618_chg *chip)
{
	u16 ibuck=0;

	//if(!chip->online)
	//    return -EINVAL;

	ibuck = (rx1618_get_rx_vrect(chip) * rx1618_get_rx_irect(chip) * 95) / (rx1618_get_rx_vbuck(chip)*100);

	dev_err(chip->dev, "[rx1618] [%s] Ibuck = %d mA\n",__func__,ibuck);

	return ibuck;
}


bool rx1618_is_vbuck_on(struct rx1618_chg *chip)
{
	bool vbuck_status=false;
	unsigned int  voltage = 0;

	voltage = rx1618_get_rx_vbuck(chip);

	dev_err(chip->dev, "[rx1618] [%s] Vbuck = %d \n",__func__, voltage);

	if((voltage > MIN_VBUCK) && (voltage < MAX_VBUCK))//4V~11V
	{
		vbuck_status=true;
	}
	else
	{
		vbuck_status=false;
	}

	return vbuck_status;
}


int rx1618_set_vbuck(struct rx1618_chg *chip, int volt)
{
	u8 data = 0;

	if((volt < 5166) && (volt > 9609)) //V bus_set =5166000uV+21224uV*[Vbus_set(0:7)] DEC
	{
		data = 0;
	}
	else
	{
		data = (volt-5166)*1000/21224; 
	}

	data = data + g_Delta;
	dev_err(chip->dev, "[rx1618] [%s] data = 0x%x, g_Delta=%d \n",__func__, data, g_Delta);

	rx1618_write(chip, PRIVATE_VBUCK_SET_CMD, REG_RX_SENT_CMD);  //sent set vbuck cmd
	rx1618_write(chip, data, REG_RX_SENT_DATA1);  //sent data0

	rx1618_write(chip, AP_SENT_DATA_OK, REG_AP_RX_COMM);

	return 0;
}


bool rx1618_check_firmware(struct rx1618_chg *chip)
{
	u8 *read_buffer=NULL;
	int i=0;

	read_buffer = (u8*)vmalloc(20000);

	rx1618_write(chip, 0xB2, 0x0017);
	rx1618_write(chip, 0xB3, 0x0017);

        dev_err(chip->dev, "[rx1618] [%s] enter \n",__func__);

	for(i=0;i<(sizeof(fw_data)/4);i++)
	{
		rx1618_write(chip, ((0+i*4)/256), 0x0010);//address_H
		rx1618_write(chip, ((0+i*4)%256), 0x0011);//address_L

		rx1618_read(chip, (read_buffer+4*i+0), 0x0013);
		rx1618_read(chip, (read_buffer+4*i+1), 0x0014);
		rx1618_read(chip, (read_buffer+4*i+2), 0x0015);
		rx1618_read(chip, (read_buffer+4*i+3), 0x0016);
	}

	rx1618_write(chip, 0xB2, 0x0017);
	rx1618_write(chip, 0x00, 0x0017);
	rx1618_write(chip, 0x55, 0x2017);

	for(i=0;i<sizeof(fw_data);i++)
	{
		if(read_buffer[i] != ((~fw_data[i])&0xff))
		{
			dev_err(chip->dev, "[rx1618] [%s] fw download Fail! read_buffer[i]=0x%x, ~fw_data[i]=0x%x, i=%d, sizeof(fw_data)=%ld\n",__func__,read_buffer[i], (~fw_data[i])&0xff,i,sizeof(fw_data));

			vfree(read_buffer);
			return false;
		}
	}

        dev_err(chip->dev, "[rx1618] [%s] i=%d, sizeof(fw_data)=%ld\n",__func__,i,sizeof(fw_data));

	if(i == sizeof(fw_data))
	{
		dev_err(chip->dev, "[rx1618] [%s] fw download Success! \n",__func__);
		vfree(read_buffer);
		return true;
	}
	else
	{
		vfree(read_buffer);
		return false;
	}

}

bool rx1618_download_firmware(struct rx1618_chg *chip)
{
	bool  check_status=false;
	int   i=0;

	cancel_delayed_work_sync(&chip->wireless_work);
	cancel_delayed_work_sync(&chip->wireless_int_work);

	rx1618_write(chip, 0x55, 0x2017);
	rx1618_write(chip, 0x69, 0x2017);
	rx1618_write(chip, 0x96, 0x2017);
	rx1618_write(chip, 0x66, 0x2017);
	rx1618_write(chip, 0x99, 0x2017);
	rx1618_write(chip, 0x00, 0x2018);
	rx1618_write(chip, 0x00, 0x2019);
	rx1618_write(chip, 0x5A, 0x0001);
	rx1618_write(chip, 0x3C, 0x0002);
	rx1618_write(chip, 0xA5, 0x0003);
	rx1618_write(chip, 0xC3, 0x0004);
	rx1618_write(chip, 0x60, 0x1000);
	rx1618_write(chip, 0x9C, 0x0017);
	rx1618_write(chip, 0x9D, 0x0017);
	rx1618_write(chip, 0x00, 0x0010);
	rx1618_write(chip, 0x00, 0x0011);
	rx1618_write(chip, 0x01, 0x0019);
	mdelay(180);
	rx1618_write(chip, 0x00, 0x0019);
	rx1618_write(chip, 0x9C, 0x0017);
	rx1618_write(chip, 0xA2, 0x0017);
	rx1618_write(chip, 0xA3, 0x0017);
	rx1618_write(chip, 0x01, 0x0019);
	mdelay(20);
	rx1618_write(chip, 0x00, 0x0019);
	rx1618_write(chip, 0xA2, 0x0017);
	rx1618_write(chip, 0x80, 0x0017);
	mdelay(20);
	rx1618_write(chip, 0x5A, 0x0001);
	rx1618_write(chip, 0x3C, 0x0002);
	rx1618_write(chip, 0xA5, 0x0003);
	rx1618_write(chip, 0xC3, 0x0004);
	rx1618_write(chip, 0x60, 0x1000);
	rx1618_write(chip, 0x9C, 0x0017);
	rx1618_write(chip, 0x9D, 0x0017);
	rx1618_write(chip, 0x00, 0x0010);
	rx1618_write(chip, 0x00, 0x0011);
	rx1618_write(chip, 0x01, 0x0019);
	mdelay(180);
	rx1618_write(chip, 0x00, 0x0019);
	rx1618_write(chip, 0x9C, 0x0017);
	rx1618_write(chip, 0xA2, 0x0017);
	rx1618_write(chip, 0xA3, 0x0017);
	rx1618_write(chip, 0x01, 0x0019);
	mdelay(20);
	rx1618_write(chip, 0x00, 0x0019);
	rx1618_write(chip, 0xA2, 0x0017);
	rx1618_write(chip, 0x80, 0x0017);
	rx1618_write(chip, 0x9C, 0x0017);
	rx1618_write(chip, 0x9D, 0x0017);
	rx1618_write(chip, 0x20, 0x0010);
	rx1618_write(chip, 0x00, 0x0011);
	rx1618_write(chip, 0x01, 0x0019);
	rx1618_write(chip, 0x00, 0x0019);
	rx1618_write(chip, 0x9C, 0x0017);
	rx1618_write(chip, 0xA2, 0x0017);
	rx1618_write(chip, 0xA3, 0x0017);
	rx1618_write(chip, 0x01, 0x0019);
	mdelay(20);
	rx1618_write(chip, 0x00, 0x0019);
	rx1618_write(chip, 0xA2, 0x0017);
	rx1618_write(chip, 0x80, 0x0017);
	rx1618_write(chip, 0x00, 0x0010);
	rx1618_write(chip, 0x00, 0x0011);

	rx1618_write(chip, (~fw_data[0]), 0x0012);
	rx1618_write(chip, 0x92, 0x0017);	
	rx1618_write(chip, 0x93, 0x0017);
	rx1618_write(chip, 0x69, 0x001A);

	for(i=1;i<(sizeof(fw_data));i++)
	{
		rx1618_write(chip, (~fw_data[i]), 0x0012);
		udelay(20);
	}

	dev_err(chip->dev, "[rx1618] [%s] fw download Success! Firmware total data: %d bytes \n",__func__,i);

	rx1618_write(chip, 0x92, 0x0017);
	rx1618_write(chip, 0x80, 0x0017);
	rx1618_write(chip, 0x77, 0x001A);

	mdelay(500);
	check_status = rx1618_check_firmware(chip); 

	if(check_status)
	{
		dev_err(chip->dev, "[rx1618] [%s] RX1618 download firmware and check firmware Success! \n",__func__);
		return true;
	}
	else
	{
		dev_err(chip->dev, "[rx1618] [%s] RX1618 download firmware Fail, Please try again !!! \n",__func__);
		return false;
	}
}

static void determine_initial_status(struct rx1618_chg *chip)
{
	bool vbuck_on = false;
	//union power_supply_propval prop = {0, };

	vbuck_on = rx1618_is_vbuck_on(chip);
	if(vbuck_on && !chip->online) {
		chip->online = 1;
		//prop.intval = vbuck_on;
		//power_supply_set_property(chip->batt_psy, POWER_SUPPLY_PROP_WIRELESS_ONLINE, &prop);
	}

	dev_err(chip->dev, "[rx1618] [%s] initial vbuck_on = %d, online = %d\n",__func__,vbuck_on,chip->online);
}


int rx1618_chip_init(struct rx1618_chg *chip)
{
	return 0;
}


void rx1618_dump_reg(void)
{
	u8 data[32] = {0};

	rx1618_read(g_chip, &data[0], 0x0000);
	rx1618_read(g_chip, &data[1], 0x0001);
	rx1618_read(g_chip, &data[2], 0x0002);
	rx1618_read(g_chip, &data[3], 0x0003);
	rx1618_read(g_chip, &data[4], 0x0004);
	rx1618_read(g_chip, &data[5], 0x0005);
	rx1618_read(g_chip, &data[6], 0x0008);
	rx1618_read(g_chip, &data[7], 0x0009);
	rx1618_read(g_chip, &data[8], 0x000A);
	rx1618_read(g_chip, &data[9], 0x000B);
	rx1618_read(g_chip, &data[10], 0x000C);
	rx1618_read(g_chip, &data[11], 0x0020);
	rx1618_read(g_chip, &data[12], 0x0021);
	rx1618_read(g_chip, &data[13], 0x0022);
	rx1618_read(g_chip, &data[14], 0x0023);
	rx1618_read(g_chip, &data[15], 0x0024);

	dev_err(g_chip->dev, "[rx1618] [%s] REG:0x0000=0x%x\n",__func__,data[0]);
	dev_err(g_chip->dev, "[rx1618] [%s] REG:0x0001=0x%x\n",__func__,data[1]);
	dev_err(g_chip->dev, "[rx1618] [%s] REG:0x0002=0x%x\n",__func__,data[2]);
	dev_err(g_chip->dev, "[rx1618] [%s] REG:0x0003=0x%x\n",__func__,data[3]);
	dev_err(g_chip->dev, "[rx1618] [%s] REG:0x0004=0x%x\n",__func__,data[4]);
	dev_err(g_chip->dev, "[rx1618] [%s] REG:0x0005=0x%x\n",__func__,data[5]);
	dev_err(g_chip->dev, "[rx1618] [%s] REG:0x0008=0x%x\n",__func__,data[6]);
	dev_err(g_chip->dev, "[rx1618] [%s] REG:0x0009=0x%x\n",__func__,data[7]);
	dev_err(g_chip->dev, "[rx1618] [%s] REG:0x000A=0x%x\n",__func__,data[8]);
	dev_err(g_chip->dev, "[rx1618] [%s] REG:0x000B=0x%x\n",__func__,data[9]);
	dev_err(g_chip->dev, "[rx1618] [%s] REG:0x000C=0x%x\n",__func__,data[10]);
	dev_err(g_chip->dev, "[rx1618] [%s] REG:0x0020=0x%x\n",__func__,data[11]);
	dev_err(g_chip->dev, "[rx1618] [%s] REG:0x0021=0x%x\n",__func__,data[12]);
	dev_err(g_chip->dev, "[rx1618] [%s] REG:0x0022=0x%x\n",__func__,data[13]);
	dev_err(g_chip->dev, "[rx1618] [%s] REG:0x0023=0x%x\n",__func__,data[14]);
	dev_err(g_chip->dev, "[rx1618] [%s] REG:0x0024=0x%x\n",__func__,data[15]);
}

#define  N   2
//0x000b[0:3]  0000:no charger, 0001:SDP, 0010:CDP, 0011:DCP, 0101:QC2-other,
//0110:QC3-other, 0111:PD, 1000:fail charger, 1001:QC3-27W, 1010:PD-27W
#define EPP_MODE_CURRENT 300000*N
#define DC_OTHER_CURRENT 400000*N
#define DC_LOW_CURRENT 100000*N //200mA
#define DC_SDP_CURRENT 400000*N
#define DC_DCP_CURRENT 400000*N
#define DC_CDP_CURRENT 400000*N
#define DC_QC2_CURRENT 500000*N
#define DC_QC3_CURRENT 500000*N
#define DC_QC3_20W_CURRENT 1000000*N //2A
#define DC_PD_CURRENT  500000*N
#define DC_PD_20W_CURRENT  1000000*N //2A
void rx1618_set_pmi_icl(struct rx1618_chg *chip, int mA)
{
	union power_supply_propval val = {0, };

	if (!chip->dc_psy) {
		chip->dc_psy = power_supply_get_by_name("dc");
		if (!chip->dc_psy) {
			dev_err(chip->dev, "[rx1618] [%s] no dc_psy,return\n",__func__);
			return;
		}
	}
	val.intval = mA;
	power_supply_set_property(chip->dc_psy, POWER_SUPPLY_PROP_CURRENT_MAX, &val);
	dev_err(chip->dev, "[rx1618] [%s] [rx1618] set icl: %d\n",__func__,val.intval);
}


static void rx1618_wireless_work(struct work_struct *work)
{
	struct rx1618_chg *chip = container_of(work, struct rx1618_chg, wireless_work.work);

	mutex_lock(&chip->wireless_chg_lock);
	schedule_delayed_work(&chip->wireless_work, msecs_to_jiffies(RX1618_DELAY_MS));
	mutex_unlock(&chip->wireless_chg_lock);  

	dev_err(chip->dev, "[rx1618] [%s] enter \n",__func__);

	return;
}


static void rx1618_wireless_int_work(struct work_struct *work)
{
	int i = 0;
	int uA = 0; 
	u16 irect = 0;
	u16 vrect = 0;
	u8 data1,data2,data3,data4,privite_cmd,tx_cmd,usb_type;
	u8 data_h,data_l,header,header_length;
	union power_supply_propval val = {0, };

	struct rx1618_chg *chip = container_of(work, struct rx1618_chg, wireless_int_work.work);

	chip->wireless_psy = power_supply_get_by_name("wireless");
	if (!chip->wireless_psy) {
		dev_err(chip->dev, "[rx1618] no wireless_psy,return\n");
	}

	mutex_lock(&chip->wireless_chg_int_lock);
	dev_err(chip->dev, "[rx1618] [%s] enter \n",__func__);

	//read data
	rx1618_read(chip, &privite_cmd, REG_RX_REV_CMD);
	rx1618_read(chip, &data1, REG_RX_REV_DATA1);
	rx1618_read(chip, &data2, REG_RX_REV_DATA2);
	rx1618_read(chip, &data3, REG_RX_REV_DATA3);
	rx1618_read(chip, &data4, REG_RX_REV_DATA4);

	dev_err(chip->dev, "[rx1618] [%s] privite_cmd,data=0x%x,0x%x,0x%x,0x%x,0x%x\n",__func__,privite_cmd, data1,data2,data3,data4);

	switch (privite_cmd) {

		case 0x01: //Vrect10
			rx1618_write(chip, AP_REV_DATA_OK, REG_AP_RX_COMM);
			msleep(10);
			break;
		case 0x02: //Duty L
			rx1618_write(chip, AP_REV_DATA_OK, REG_AP_RX_COMM);
			msleep(10);
			break;
		case 0x03: //Duty H
			rx1618_write(chip, AP_REV_DATA_OK, REG_AP_RX_COMM);
			msleep(10);
			break;
		case 0x04: //OVPH
			rx1618_write(chip, AP_REV_DATA_OK, REG_AP_RX_COMM);
			msleep(10);
			break;
		case 0x05: //OCPH
			rx1618_write(chip, AP_REV_DATA_OK, REG_AP_RX_COMM);
			msleep(10);
			break;
		case 0x06: //Plimit
			rx1618_write(chip, AP_REV_DATA_OK, REG_AP_RX_COMM);
			msleep(10);
			break;
		case 0x07: //FCM20
			rx1618_write(chip, AP_REV_DATA_OK, REG_AP_RX_COMM);
			msleep(10);
			break;

		case 0x08: //calibration
			if(g_cali_done_flag == 0)
			{
				rx1618_write(chip, AP_REV_DATA_OK, REG_AP_RX_COMM);
				chip->epp = 1;
				g_cali_done_flag = 1;
				dev_err(chip->dev, "[rx1618] [%s] Calibration OK! \n",__func__);

				for(i = 0; i <= 2; i++) {
					uA = (EPP_MODE_CURRENT + N*50000*i);
					rx1618_set_pmi_icl(chip, uA);
					msleep(100);
				}
			}
			break;

		case 0x09: //ID ok
			if(g_id_done_flag == 0)
			{
				rx1618_write(chip, AP_REV_DATA_OK, REG_AP_RX_COMM);    //receive ok
				msleep(10);

				rx1618_write(chip, PRIVATE_ID_CMD, REG_RX_SENT_CMD);    //sent sar
				rx1618_write(chip, AP_SENT_DATA_OK, REG_AP_RX_COMM);
				g_id_done_flag = 1;
				dev_err(chip->dev, "[rx1618] [%s] ID OK! \n",__func__);
			}
			break;

		case 0x0A: //SAR ok
			if(g_sar_done_flag == 0)
			{
				rx1618_write(chip, AP_REV_DATA_OK, REG_AP_RX_COMM);    //receive ok
				msleep(10);

				rx1618_write(chip, PRIVATE_USB_TYPE_CMD, REG_RX_SENT_CMD);    //sent usb type
				rx1618_write(chip, AP_SENT_DATA_OK, REG_AP_RX_COMM);
				g_sar_done_flag = 1;
				dev_err(chip->dev, "[rx1618] [%s] SAR OK! \n",__func__);
				chip->auth = 1;
			} 
			break;

		case 0x12: //fast charge ok
			if(g_fast_chg_flag == 0)
			{
				rx1618_write(chip, AP_REV_DATA_OK, REG_AP_RX_COMM);    //receive ok
				msleep(10);

				g_fast_chg_flag = 1;
				dev_err(chip->dev, "[rx1618] [%s] Fast charge ok! \n",__func__);

				rx1618_write(chip, 0x18, REG_RX_SENT_DATA1);
				rx1618_write(chip, 0x4c, REG_RX_SENT_DATA2);
				rx1618_write(chip, PRIVATE_TX_HW_ID_CMD, REG_RX_SENT_CMD);    //sent tx hw id req
				rx1618_write(chip, AP_SENT_DATA_OK, REG_AP_RX_COMM);
				dev_err(chip->dev, "[rx1618] [%s] sent tx hw id req\n",__func__);
			}
   

			break;


		case 0x0B: //USB type
			if(g_usb_type_flag == 0)
			{
				rx1618_write(chip, AP_REV_DATA_OK, REG_AP_RX_COMM);    //receive ok
				msleep(10);

				rx1618_read(chip, &usb_type, 0x0021);  

				rx1618_write(chip, PRIVATE_FAST_CHG_CMD, REG_RX_SENT_CMD);    //sent fast charge
				rx1618_write(chip, AP_SENT_DATA_OK, REG_AP_RX_COMM);
				g_usb_type_flag = 1;
				dev_err(chip->dev, "[rx1618] [%s] usb_type=0x%x \n",__func__,usb_type);

				switch (usb_type) {
					case 0: //other charger
						if((g_id_done_flag == 0) && (g_epp_or_bpp == BPP_MODE))//bpp and no id
						{
							rx1618_set_pmi_icl(chip, DC_OTHER_CURRENT);
							dev_err(chip->dev, "[rx1618] [%s] bpp and no id---800mA \n",__func__);
						}
						else//ID ok
						{
							rx1618_set_pmi_icl(chip, DC_LOW_CURRENT);
							dev_err(chip->dev, "[rx1618] [%s] bpp and id ok---200mA \n",__func__);
						}
						break;

					case 1: //SDP--1W
						rx1618_set_pmi_icl(chip, DC_SDP_CURRENT);
						dev_err(chip->dev, "[rx1618] [%s] SDP--1W \n",__func__);
						break;

					case 2: //CDP--1W
						rx1618_set_pmi_icl(chip, DC_CDP_CURRENT);
						dev_err(chip->dev, "[rx1618] [%s] CDP--1W \n",__func__);
						break;

					case 3: //DCP--1W
						rx1618_set_pmi_icl(chip, DC_DCP_CURRENT);
						dev_err(chip->dev, "[rx1618] [%s] DCP--1W \n",__func__);
						break;

					case 5: //QC2-other -- 7.5W
						for(i = 0; i <= 8; i++) {
							uA = (DC_LOW_CURRENT + N*50000*i);
							rx1618_set_pmi_icl(chip, uA);
							msleep(100);
						}
						dev_err(chip->dev, "[rx1618] [%s] QC2 other--7.5W \n",__func__);
						break;

					case 6: //QC3-other -- 10W
						for(i = 0; i <= 8; i++) {
							uA = (DC_LOW_CURRENT + N*50000*i);
							rx1618_set_pmi_icl(chip, uA);
							msleep(100);
						}
						dev_err(chip->dev, "[rx1618] [%s] QC3 other--10W \n",__func__);
						break;

					case 7: //PD-other -- 10W
						for(i = 0; i <= 8; i++) {
							uA = (DC_LOW_CURRENT + N*50000*i);
							rx1618_set_pmi_icl(chip, uA);
							msleep(100);
						}
						dev_err(chip->dev, "[rx1618] [%s] PD other--7.5W \n",__func__);
						break;

					case 8: //fail charger
						break;

					case 9: //QC3-27W(20W)
						for(i = 0; i <= 11; i++) {
							uA = (DC_QC3_CURRENT + N*50000*i);
							rx1618_set_pmi_icl(chip, uA);
							msleep(100);
						}
						if(chip->epp && chip->auth && chip->wireless_psy) {
							val.intval = 1;
							power_supply_set_property(chip->wireless_psy, POWER_SUPPLY_PROP_WIRELESS_CP_EN, &val);
						}
						dev_err(chip->dev, "[rx1618] [%s] QC3-27W(20W) \n",__func__);
						break;

					case 10: //PD-27W(20W)
						for(i = 0; i <= 11; i++) {
							uA = (DC_QC3_CURRENT + N*50000*i);
							rx1618_set_pmi_icl(chip, uA);
							msleep(100);
						}
						if(chip->epp && chip->auth && chip->wireless_psy) {
							val.intval = 1;
							power_supply_set_property(chip->wireless_psy, POWER_SUPPLY_PROP_WIRELESS_CP_EN, &val);
						}
						dev_err(chip->dev, "[rx1618] [%s] PD-27W(20W) \n",__func__);
						break;

					default:
						dev_err(chip->dev, "[rx1618] [%s] other Usb_type\n",__func__);
						break;
				}

			}
			break;

		case 0x0C: //Light screen
			if(g_light_screen_flag == 0)
			{
				rx1618_write(chip, AP_REV_DATA_OK, REG_AP_RX_COMM);    //receive ok
				msleep(10);

				rx1618_read(chip, &g_hw_id_h, REG_RX_REV_DATA1);
				rx1618_read(chip, &g_hw_id_l, REG_RX_REV_DATA2);
				rx1618_read(chip, &g_fw_id, REG_RX_REV_DATA3);
                                rx1618_read(chip, &g_epp_or_bpp, REG_RX_REV_DATA4);

                                if(g_epp_or_bpp==EPP_MODE)//EPP
                                {
					rx1618_set_pmi_icl(chip, EPP_MODE_CURRENT);
					dev_err(chip->dev, "[rx1618] [%s] EPP--600mA \n",__func__);
                                }
                                else //BPP mode
                                {
					rx1618_set_pmi_icl(chip, DC_LOW_CURRENT);
					dev_err(chip->dev, "[rx1618] [%s] BPP--200mA \n",__func__);
                                }

				g_light_screen_flag = 1;

				dev_err(chip->dev, "[rx1618] [%s] Light screen,fw_id,hw_id_h,hw_id_l,g_epp_or_bpp=0x%x, 0x%x, 0x%x, 0x%x\n",__func__,g_fw_id,g_hw_id_h,g_hw_id_l,g_epp_or_bpp);
			}
			break;

		case 0x4C: //tx hw id
			rx1618_write(chip, AP_REV_DATA_OK, REG_AP_RX_COMM);
			msleep(10);

			rx1618_read(chip, &data1, REG_RX_REV_DATA1);
			rx1618_read(chip, &data2, REG_RX_REV_DATA2);
			rx1618_read(chip, &data3, REG_RX_REV_DATA3);

			dev_err(chip->dev, "[rx1618] [%s] tx hw id,data1-3=0x%x, 0x%x, 0x%x, 0x%x\n",__func__,data1,data2,data3,data4);
			break;

		case 0x0D: //TX test request
			rx1618_write(chip, AP_REV_DATA_OK, REG_AP_RX_COMM);
			msleep(10);

			rx1618_read(chip, &header, REG_RX_REV_DATA1);
			header_length = ((header & 0xf0) >> 4);

			dev_err(chip->dev, "[rx1618] [%s] product Test mode, header=0x%x, header_length=0x%x\n",__func__,header,header_length);

			switch (header_length) {
				case 1: 
					rx1618_read(chip, &tx_cmd, REG_RX_REV_DATA2);
					break;

				case 2:   
					rx1618_read(chip, &tx_cmd, REG_RX_REV_DATA2);
					rx1618_read(chip, &data1, REG_RX_REV_DATA3);
					break;

				case 3: 
					rx1618_read(chip, &tx_cmd, REG_RX_REV_DATA2);
					rx1618_read(chip, &data1, REG_RX_REV_DATA3);
					rx1618_read(chip, &data2, REG_RX_REV_DATA4);
					break;

				case 4: 
					rx1618_read(chip, &tx_cmd, REG_RX_REV_DATA2);
					rx1618_read(chip, &data1, REG_RX_REV_DATA3);
					rx1618_read(chip, &data2, REG_RX_REV_DATA4);
					rx1618_read(chip, &data3, REG_RX_REV_DATA5);
					break;

				default: 
					break;
			}

			dev_err(chip->dev, "[rx1618] [%s] tx_cmd,data=0x%x, 0x%x, 0x%x, 0x%x\n",__func__,tx_cmd,data1,data2,data3);

			if(tx_cmd==0x12) //irect
			{
				rx1618_write(chip, 0x38, REG_RX_SENT_DATA1);  //sent header
				rx1618_write(chip, 0x12, REG_RX_SENT_DATA2);  //sent cmd

				irect = rx1618_get_rx_irect(chip);
				data_h = (irect & 0xff00) >> 8;
				rx1618_write(chip, data_h, 0x0003);
				data_l = (irect & 0x00ff);
				rx1618_write(chip, data_l, 0x0004);
				dev_err(chip->dev, "[rx1618] [%s] product test--0x12--irect=%d \n",__func__,irect);
			}
			else if(tx_cmd==0x13) //vrect
			{
				rx1618_write(chip, 0x38, REG_RX_SENT_DATA1);  //sent header
				rx1618_write(chip, 0x13, REG_RX_SENT_DATA2);  //sent cmd

				vrect = rx1618_get_rx_vrect(chip);
				data_h = (vrect & 0xff00) >> 8;
				rx1618_write(chip, data_h, 0x0003);
				data_l = (vrect & 0x00ff);
				rx1618_write(chip, data_l, 0x0004);
				dev_err(chip->dev, "[rx1618] [%s] product test--0x13--vrect=%d \n",__func__,vrect);
			}
			else if(tx_cmd==0x24) //fw id
			{
				rx1618_write(chip, 0x28, REG_RX_SENT_DATA1);  //sent header
				rx1618_write(chip, 0x24, REG_RX_SENT_DATA2);  //sent cmd
				rx1618_write(chip, g_fw_id, 0x0003);
				dev_err(chip->dev, "[rx1618] [%s] product test--0x24--g_fw_id=0x%x \n",__func__,g_fw_id);
			}
			else if(tx_cmd==0x25) //hw id
			{
				rx1618_write(chip, 0x38, REG_RX_SENT_DATA1);  //sent header
				rx1618_write(chip, 0x25, REG_RX_SENT_DATA2);  //sent cmd
				rx1618_write(chip, g_hw_id_h, 0x0003);
				rx1618_write(chip, g_hw_id_l, 0x0004);
				dev_err(chip->dev, "[rx1618] [%s] product test--0x25--g_hw_id=0x%x, 0x%x \n",__func__,g_hw_id_h,g_hw_id_l);
			}
			else
			{ 
				dev_err(chip->dev, "[rx1618] [%s] product test--other cmd \n",__func__);
				break;
			}

			dev_err(chip->dev, "[rx1618] [%s] header,tx_cmd,data=0x%x, 0x%x, 0x%x, 0x%x, 0x%x \n",__func__,header,tx_cmd,data1,data2,data3);

			rx1618_write(chip, PRIVATE_PRODUCT_TEST_CMD, REG_RX_SENT_CMD);   //sent test cmd
			rx1618_write(chip, AP_SENT_DATA_OK, REG_AP_RX_COMM);
			break;

		default:
			rx1618_write(chip, AP_REV_DATA_OK, REG_AP_RX_COMM);    //receive ok
			msleep(10);
			dev_err(chip->dev, "[rx1618] [%s] other private cmd \n",__func__);
			break;
	} 

	int_done_flag = false;
	mutex_unlock(&chip->wireless_chg_int_lock);

	dev_err(chip->dev, "[rx1618] [%s] exit \n",__func__);

	return;
}


static irqreturn_t rx1618_chg_stat_handler(int irq, void *dev_id)
{
	struct rx1618_chg *chip = dev_id;
	u8 rev_cmd=0;

	mutex_lock(&chip->irq_complete);

	if(!int_done_flag)
	{
		rx1618_read(chip, &rev_cmd, REG_RX_REV_CMD);
		if(rev_cmd==0)
		{
			mdelay(100);
			int_done_flag = false;
			//chip->online = 0;
			g_light_screen_flag = 0;
			g_cali_done_flag = 0;
			g_id_done_flag = 0;
			g_sar_done_flag = 0;
			g_fast_chg_flag = 0;
			g_usb_type_flag = 0;
                        g_epp_or_bpp = BPP_MODE;
			chip->epp = 0;
			chip->auth = 0;
			cancel_delayed_work_sync(&chip->wireless_work);
			cancel_delayed_work_sync(&chip->wireless_int_work);
			dev_err(chip->dev, "[rx1618] [%s] Wireless Offline \n",__func__);
		}
		else
		{
			int_done_flag = true;
			//chip->online = 1;

			schedule_delayed_work(&chip->wireless_work, 0);
			schedule_delayed_work(&chip->wireless_int_work, 0);
			dev_err(chip->dev, "[rx1618] [%s] Wireless Online \n",__func__);
		}
	}

	mutex_unlock(&chip->irq_complete);

	dev_err(chip->dev, "[rx1618] [%s] enter \n",__func__);

	return IRQ_HANDLED;
}


static int rx1618_parse_dt(struct rx1618_chg *chip)
{
	int ret = 0;

	struct device_node *node = chip->dev->of_node;
	if (!node) {
		dev_err(chip->dev, "[rx1618] [%s] No DT data Failing Probe\n",__func__);
		return -EINVAL;
	}

	//Get the Interrupt GPIO resource
	chip->irq_gpio = of_get_named_gpio(node, "rx1618_irq_gpio", 0);
	if(gpio_is_valid(chip->irq_gpio)) {
		ret = gpio_request(chip->irq_gpio, "rx1618_irq_gpio");
		if(ret) {
			dev_err(chip->dev, "[rx1618] [%s] irq gpio request failed, ret = %d\n",__func__,ret);
			return -EPROBE_DEFER;
		}
		ret = gpio_direction_input(chip->irq_gpio);
		if(ret) {
			dev_err(chip->dev, "[rx1618] [%s] set direction failed, ret = %d\n",__func__,ret);
			goto fail_irq_gpio;
		}

		chip->client->irq=gpio_to_irq(chip->irq_gpio);
		if (chip->client->irq)
		{
			dev_err(chip->dev, "[rx1618] [%s] gpio_to_irq Success! \n",__func__);
		}
		else
		{
			dev_err(chip->dev, "[rx1618] [%s] gpio_to_irq Fail! \n",__func__);
			goto fail_irq_gpio;
		}
	} else {
		goto fail_irq_gpio;
	}

	return ret;

fail_irq_gpio:
	dev_err(chip->dev, "[rx1618] [%s] fail_irq_gpio \n",__func__);
	if(gpio_is_valid(chip->irq_gpio))
		gpio_free(chip->irq_gpio);

	return ret;
}


/*************FOD**************/
//CMD : 0x8A
//DATA0 : Power Level, (2W, 4W, 8W, 6W, 10W… 20W)
//DATA1 : gain * 1024 hight 8bits
//DATA2 : gain * 1024 low 8bits
//DATA3: offset(mW) hight 8bits
//DATA4: offset(mW) low 8bits
/*************FOD**************/
u8 g_power_level=0;
u8 g_gain_h=0;
u8 g_gain_l=0;
u8 g_offset_h=0;
u8 g_offset_l=0;
void FODturring(void)
{
	rx1618_write(g_chip, g_power_level, REG_RX_SENT_DATA1);
	rx1618_write(g_chip, g_gain_h, REG_RX_SENT_DATA2);
	rx1618_write(g_chip, g_gain_l, REG_RX_SENT_DATA3);
	rx1618_write(g_chip, g_offset_h, REG_RX_SENT_DATA4);
	rx1618_write(g_chip, g_offset_l, REG_RX_SENT_DATA5);

	dev_err(g_chip->dev, "[rx1618] [%s] FODturring:g_power_level,g_gain_h,g_gain_l,g_offset_h,g_offset_l %d,%d,%d,%d,%d\n",__func__,g_power_level,g_gain_h,g_gain_l,g_offset_h,g_offset_l);

	msleep(5);

	rx1618_write(g_chip, PRIVATE_FOD_TEST_CMD, REG_RX_SENT_CMD);
	rx1618_write(g_chip, AP_SENT_DATA_OK, REG_AP_RX_COMM);
}

static ssize_t chip_fod_power_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf,
		size_t count)
{
	int index;
	index = (int)simple_strtoul(buf, NULL, 10);

	g_power_level = (index & 0xff);
	dev_err(g_chip->dev, "[rx1618] [%s] g_power_level=0x%x \n",__func__,g_power_level);

	return count;
}

static ssize_t chip_fod_gain_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf,
		size_t count)
{
	u16 index;
	index = (u16)simple_strtoul(buf, NULL, 10);

	g_gain_l = (index & 0x00ff);
	g_gain_h = (index & 0xff00) >> 8;

	dev_err(g_chip->dev, "[rx1618] [%s] g_gain_l=0x%x, g_gain_h=0x%x \n",__func__,g_gain_l,g_gain_h);

	return count;
}

static ssize_t chip_fod_offset_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf,
		size_t count)
{
	u16 index;
	index = (u16)simple_strtoul(buf, NULL, 10);

	g_offset_l = (index & 0x00ff);
	g_offset_h = (index & 0xff00) >> 8;

	dev_err(g_chip->dev, "[rx1618] [%s] g_offset_l=0x%x, g_offset_h=0x%x \n",__func__,g_offset_l,g_offset_h);

	FODturring();

	return count;
}


static ssize_t chip_ibuck_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	unsigned int ibuck = 0;

	ibuck = rx1618_get_rx_ibuck(g_chip);

	return sprintf(buf, "rx1618 Ibuck: %d mA\n", ibuck);
}


static ssize_t chip_vrect_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	unsigned int vrect = 0;

	vrect = rx1618_get_rx_vrect(g_chip);

	return sprintf(buf, "rx1618 Vrect : %d mV\n", vrect);
}

static ssize_t chip_irect_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	unsigned int irect = 0;

	irect = rx1618_get_rx_irect(g_chip);

	return sprintf(buf, "rx1618 Irect: %d mA\n", irect);
}


static ssize_t chip_vbuck_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf,
		size_t count)
{
	int index;

	index = (int)simple_strtoul(buf, NULL, 10);
	dev_err(g_chip->dev, "[rx1618] [%s] --Store output_voltage = %d\n",__func__,index);
	if ((index < 5166) || (index > 9609)) {
		dev_err(g_chip->dev, "[rx1618] [%s] Store Voltage %s is invalid\n",__func__,buf);
		rx1618_set_vbuck(g_chip, 0);
		return count;
	}

	rx1618_set_vbuck(g_chip, index);

	return count;
}

static ssize_t chip_vbuck_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	unsigned int vbuck = 0;

	vbuck = rx1618_get_rx_vbuck(g_chip);

	return sprintf(buf, "rx1618 Vbuck : %d mV\n", vbuck);
}

static ssize_t chip_ap_req_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf,
		size_t count)
{
	//int index;
	//index = (int)simple_strtoul(buf, NULL, 10);
	return count;
}

static ssize_t chip_ap_req_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	u8 data = 0;
	return sprintf(buf, "AP REQ DATA : 0x%x \n", data);
}

static ssize_t chip_firmware_update_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf,
		size_t count)
{
	bool enable = strncmp(buf, "update", 6) ? false : true;

	dev_err(g_chip->dev, "[rx1618] [%s] Firmware Update enable = %d\n",__func__,enable);
	if(enable) {
		int ret = rx1618_download_firmware(g_chip);
		if (!ret) {
			dev_err(g_chip->dev, "[rx1618] [%s] Firmware Update failed! Please try again! \n",__func__);
		}
		dev_err(g_chip->dev, "[rx1618] [%s] Firmware Update Success!!! \n",__func__);
	}

	return count;
}


static ssize_t chip_firmware_update_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	bool ret = rx1618_check_firmware(g_chip);
	if(ret)
	{
		return sprintf(buf, "rx1618_download_firmware check Success!!!\n");
	}
	else
	{
		return sprintf(buf, "rx1618_download_firmware check Fail!!!\n");
	}
}


static ssize_t chip_enable_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf,
		size_t count)
{
	//bool enable = strncmp(buf, "1", 1) ? false : true;

	//rx1618_chip_enable(g_chip, enable);
	//dev_err(g_chip->dev, "[rx1618] [%s] chip enable:%d\n",__func__,enable);

	return count;
}

static ssize_t chip_enable_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	return sprintf(buf, "rx1618 chip enable status: %d \n",g_chip->chip_enable);
}

static ssize_t chip_fw_version_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	return sprintf(buf, "Firmware ID: 0x%x \t\n", g_fw_id);
}

static ssize_t chip_hw_version_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	return sprintf(buf, "Hardware ID: 0x%x, 0x%x \t\n", g_hw_id_h, g_hw_id_l);
}


static ssize_t chip_vbuck_calibration_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf,
		size_t count)
{
	int index = 0;

	index = (int)simple_strtoul(buf, NULL, 10);

	g_Delta = index;
	dev_err(g_chip->dev, "[rx1618] [%s] g_Delta = %d \n",__func__, g_Delta);

	return count;
}

static DEVICE_ATTR(chip_fod_power, S_IWUSR, NULL, chip_fod_power_store);
static DEVICE_ATTR(chip_fod_gain, S_IWUSR, NULL, chip_fod_gain_store);
static DEVICE_ATTR(chip_fod_offset, S_IWUSR, NULL, chip_fod_offset_store);
static DEVICE_ATTR(chip_vrect, S_IRUGO, chip_vrect_show, NULL);
static DEVICE_ATTR(chip_irect, S_IRUGO, chip_irect_show, NULL);
static DEVICE_ATTR(chip_vbuck_calibration, S_IWUSR, NULL, chip_vbuck_calibration_store);
static DEVICE_ATTR(chip_firmware_update, S_IWUSR | S_IRUGO, chip_firmware_update_show, chip_firmware_update_store);
static DEVICE_ATTR(chip_fw_version, S_IRUGO, chip_fw_version_show, NULL);
static DEVICE_ATTR(chip_hw_version, S_IRUGO, chip_hw_version_show, NULL);
static DEVICE_ATTR(chip_enable, S_IWUSR | S_IRUGO, chip_enable_show, chip_enable_store);
static DEVICE_ATTR(chip_vbuck, S_IWUSR | S_IRUGO, chip_vbuck_show, chip_vbuck_store);
static DEVICE_ATTR(chip_ibuck, S_IRUGO, chip_ibuck_show, NULL);
static DEVICE_ATTR(chip_ap_req, S_IWUSR | S_IRUGO, chip_ap_req_show, chip_ap_req_store);

static struct attribute *rx1618_sysfs_attrs[] = {
	&dev_attr_chip_vrect.attr,
	&dev_attr_chip_irect.attr,
	&dev_attr_chip_fw_version.attr,
	&dev_attr_chip_hw_version.attr,
	&dev_attr_chip_enable.attr,
	&dev_attr_chip_vbuck.attr,
	&dev_attr_chip_ibuck.attr,
	&dev_attr_chip_ap_req.attr,
	&dev_attr_chip_firmware_update.attr,
	&dev_attr_chip_vbuck_calibration.attr,
	&dev_attr_chip_fod_power.attr,
	&dev_attr_chip_fod_gain.attr,
	&dev_attr_chip_fod_offset.attr,
	NULL,
};

static const struct attribute_group rx1618_sysfs_group_attrs = {
	.attrs = rx1618_sysfs_attrs,
};

#if 1
static enum power_supply_property rx1618_wireless_properties[] = {
	/*
	   POWER_SUPPLY_PROP_PRESENT,
	   POWER_SUPPLY_PROP_ONLINE,
	   POWER_SUPPLY_PROP_CHARGING_ENABLED,
	   POWER_SUPPLY_PROP_RX_CHIP_ID, //RX chip id
	   POWER_SUPPLY_PROP_RX_VRECT, //RX vrect
	   POWER_SUPPLY_PROP_RX_IOUT, //RX output current
	   POWER_SUPPLY_PROP_RX_VOUT, //RX output voltage
	   POWER_SUPPLY_PROP_RX_ILIMIT, //RX Main LDO output current limit
	   POWER_SUPPLY_PROP_VOUT_SET, //Vout voltage set
	 */
	POWER_SUPPLY_PROP_INPUT_VOLTAGE_REGULATION,
};


static int rx1618_wireless_set_property(struct power_supply *psy,
		enum power_supply_property prop,
		const union power_supply_propval *val)
{
	int ret;
	struct rx1618_chg *chip = power_supply_get_drvdata(psy);
	int data;

	switch (prop) {
		/*
		   case POWER_SUPPLY_PROP_PRESENT:
		   break;
		   case POWER_SUPPLY_PROP_CHARGING_ENABLED:
		   rx1618_chip_enable(chip, val->intval);
		   break;
		   case POWER_SUPPLY_PROP_VOUT_SET:
		   ret = rx1618_set_vbuck(chip, val->intval);
		   if(ret < 0)
		   return ret;
		   break;
		 */
		case POWER_SUPPLY_PROP_INPUT_VOLTAGE_REGULATION:
			data = val->intval/1000;
			ret = rx1618_set_vbuck(chip, data);
			break;
		default:
			return -EINVAL;
	}

	return 0;
}


static int rx1618_wireless_get_property(struct power_supply *psy,
		enum power_supply_property prop,
		union power_supply_propval *val)
{
	struct rx1618_chg *chip = power_supply_get_drvdata(psy);

	switch (prop) {
		/*
		   case POWER_SUPPLY_PROP_PRESENT:
		   val->intval = rx1618_is_rx_present(chip);
		   break;
		   case POWER_SUPPLY_PROP_ONLINE:
		   val->intval = chip->online;
		   break;
		   case POWER_SUPPLY_PROP_CHARGING_ENABLED:
		   val->intval = chip->chip_enable;
		   break;
		   case POWER_SUPPLY_PROP_RX_CHIP_ID:
		   val->intval = rx1618_get_rx_chip_id(chip);
		   break;
		   case POWER_SUPPLY_PROP_RX_VRECT:
		   val->intval = rx1618_get_rx_vrect(chip);
		   break;
		   case POWER_SUPPLY_PROP_RX_IOUT:
		   val->intval = rx1618_get_rx_ibuck(chip);
		   break;
		   case POWER_SUPPLY_PROP_RX_VOUT:
		   val->intval = rx1618_get_rx_vbuck(chip);
		   break;
		   case POWER_SUPPLY_PROP_VOUT_SET:
		   val->intval = 0;
		   break;
		 */
		case POWER_SUPPLY_PROP_INPUT_VOLTAGE_REGULATION:
			val->intval = rx1618_get_rx_vbuck(chip);
			break;
		default:
			return -EINVAL;
	}
	return 0;
}  
#endif

// first step: define regmap_config
static const struct regmap_config rx1618_regmap_config = {
	.reg_bits = 16,
	.val_bits = 8,
	.max_register = 0xFFFF,
};

static int rx1618_prop_is_writeable(struct power_supply *psy,
		enum power_supply_property psp)
{
	int rc;

	switch (psp) {
		case POWER_SUPPLY_PROP_INPUT_VOLTAGE_REGULATION:
			return 1;
		default:
			rc = 0;
			break;
	}

	return rc;
}

static int rx1618_probe(struct i2c_client *client,const struct i2c_device_id *id)
{
	int ret = 0;
	struct rx1618_chg *chip;
	struct kobject *rx1618_kobj;

	struct power_supply_config wip_psy_cfg = {};
	int hw_id;
	/*
	   struct power_supply *batt_psy;

	   batt_psy = power_supply_get_by_name("battery");
	   if (!batt_psy) {
	   dev_err(&client->dev, "Battery supply not found; defer probe\n");
	   return -EPROBE_DEFER;
	   }
	 */

	hw_id = get_hw_country_version();
	dev_info(&client->dev, "[rx1618] %s: hw_id is %d\n", __func__, hw_id);
	if (hw_id)  /*hw_id=1 is idt */
		return 0;

	chip = devm_kzalloc(&client->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip) {
		dev_err(&client->dev, "i2c allocated device info data failed!\n");
		return -ENOMEM;
	}

	chip->regmap = regmap_init_i2c(client, &rx1618_regmap_config);
	if (!chip->regmap) {
		dev_err(&client->dev, "parent regmap is missing\n");
		return -EINVAL;
	}

	chip->client = client;
	chip->dev = &client->dev;

	//chip->batt_psy = batt_psy;
	//chip->online = 0;
	chip->chip_enable = false;

	device_init_wakeup(&client->dev, true);
	i2c_set_clientdata(client, chip);

	rx1618_parse_dt(chip);

	mutex_init(&chip->irq_complete);
	mutex_init(&chip->wireless_chg_lock);
	mutex_init(&chip->wireless_chg_int_lock);
	INIT_DELAYED_WORK(&chip->wireless_work, rx1618_wireless_work);
	INIT_DELAYED_WORK(&chip->wireless_int_work, rx1618_wireless_int_work);

	chip->wip_psy_d.name                    = "rx1618";
	chip->wip_psy_d.type                    = POWER_SUPPLY_TYPE_WIRELESS;
	chip->wip_psy_d.get_property            = rx1618_wireless_get_property;
	chip->wip_psy_d.set_property            = rx1618_wireless_set_property;
	chip->wip_psy_d.properties            = rx1618_wireless_properties;
	chip->wip_psy_d.num_properties        = ARRAY_SIZE(rx1618_wireless_properties);
	chip->wip_psy_d.property_is_writeable = rx1618_prop_is_writeable,

	wip_psy_cfg.drv_data = chip;

	chip->wip_psy = devm_power_supply_register(chip->dev, &chip->wip_psy_d, &wip_psy_cfg);
	if (IS_ERR(chip->wip_psy)) {
		dev_err(chip->dev, "Couldn't register wip psy rc=%ld\n", PTR_ERR(chip->wip_psy));
		return ret;
	}

	if(chip->client->irq) {
		ret = devm_request_threaded_irq(&chip->client->dev, chip->client->irq, NULL,
				rx1618_chg_stat_handler,
				(IRQF_TRIGGER_FALLING |  IRQF_TRIGGER_RISING | IRQF_ONESHOT),
				"rx1618_chg_stat_irq", chip);
		if (ret) {
			dev_err(chip->dev, "Failed irq = %d ret = %d\n", chip->client->irq, ret);
		}
	} 

	enable_irq_wake(chip->client->irq);

	rx1618_kobj = kobject_create_and_add("rx1618", NULL);
	if (!rx1618_kobj) {
		dev_err(chip->dev, "sysfs_create_group fail");
		goto error_sysfs;
	}
	ret = sysfs_create_group(rx1618_kobj,&rx1618_sysfs_group_attrs);
	if (ret < 0)
	{
		dev_err(chip->dev, "sysfs_create_group fail %d\n", ret);
		goto error_sysfs;
	}

	g_chip = chip;

	determine_initial_status(chip); 
	rx1618_chip_init(chip);

	rx1618_dump_reg(); 

	dev_err(chip->dev, "[rx1618] [%s] success! \n",__func__);

	return 0;


error_sysfs:
	sysfs_remove_group(rx1618_kobj, &rx1618_sysfs_group_attrs);
	dev_err(chip->dev, "[rx1618] [%s] rx1618 probe error_sysfs! \n",__func__);

	mutex_destroy(&chip->irq_complete);
	mutex_destroy(&chip->wireless_chg_lock);
	mutex_destroy(&chip->wireless_chg_int_lock);
	if (chip->irq_gpio > 0)
		gpio_free(chip->irq_gpio);

	return 0;
}

static int rx1618_remove(struct i2c_client *client)
{
	struct rx1618_chg *chip = i2c_get_clientdata(client);

	mutex_destroy(&chip->irq_complete);
	cancel_delayed_work_sync(&chip->wireless_work);
	cancel_delayed_work_sync(&chip->wireless_int_work);

	return 0;
}

static const struct i2c_device_id rx1618_id[] = {
	{rx1618_DRIVER_NAME, 0},
	{},
};
MODULE_DEVICE_TABLE(i2c, rx1618_id);

static struct of_device_id  rx1618_match_table[] = {
	{ .compatible = "nuvolta,wl_charger_rx1618",},
	{}
};

static struct i2c_driver rx1618_driver = {
	.driver = {
		.name = rx1618_DRIVER_NAME,
		.of_match_table = rx1618_match_table,
	},
	.probe = rx1618_probe,
	.remove = rx1618_remove,
	.id_table = rx1618_id,
};

static int __init rx1618_init(void)
{
	int ret;

	ret = i2c_add_driver(&rx1618_driver);
	if (ret)
		printk(KERN_ERR "rx1618 i2c driver init failed!\n");

	return ret;
}

static void __exit rx1618_exit(void)
{
	i2c_del_driver(&rx1618_driver);
}

module_init(rx1618_init);
module_exit(rx1618_exit);

MODULE_AUTHOR("colin");
MODULE_DESCRIPTION("NUVOLTA Wireless Power Charger Monitor driver");
MODULE_LICENSE("GPL/BSD");   
