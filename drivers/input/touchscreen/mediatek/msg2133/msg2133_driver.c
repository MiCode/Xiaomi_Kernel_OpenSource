#include "tpd.h"
#include <linux/interrupt.h>
#include <cust_eint.h>
#include <linux/i2c.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/rtpm_prio.h>
#include <linux/wait.h>
#include <linux/time.h>
#include <linux/delay.h>

#include "tpd_custom_msg2133.h"

#include <mach/mt_pm_ldo.h>
#include <mach/mt_typedefs.h>
#include <mach/mt_boot.h>
#include <mach/mt_gpio.h>

#include "cust_gpio_usage.h"

//for dma mode
#include <linux/dma-mapping.h>
#include <linux/mm_types.h>
#include <linux/mm.h>
#include <asm/uaccess.h>
#include <asm/page.h>
#include <linux/vmalloc.h>



#define __TPD_DEBUG__ 

/*Ctp Power Off In Sleep ? */
//#define TPD_CLOSE_POWER_IN_SLEEP

/*************************************************************
**msz xb.pang
**
**msg2133,msg2133a,msg2138a Firmware update data transfer select
**
**
** if BB Chip == MT6575,MT7577 , please undef __MSG_DMA_MODE__
**
**
** if BB Chip == MT6589,MT6572 , please define __MSG_DMA_MODE__
**
**
**
**
*************************************************************/
#define __MSG_DMA_MODE__

#ifdef __MSG_DMA_MODE__
	u8 *g_dma_buff_va = NULL;
	u8 *g_dma_buff_pa = NULL;
#endif

 
extern struct tpd_device *tpd;

/*Use For Get CTP Data By I2C*/ 
struct i2c_client *msg_i2c_client = NULL;

/*Use For Firmware Update By I2C*/
//static struct i2c_client     *msg21xx_i2c_client = NULL;

//struct task_struct *thread = NULL;
 
static DECLARE_WAIT_QUEUE_HEAD(waiter);
//static DEFINE_MUTEX(i2c_access);

typedef struct
{
    u16 X;
    u16 Y;
} TouchPoint_t;

/*CTP Data Package*/
typedef struct
{
    u8 nTouchKeyMode;
    u8 nTouchKeyCode;
    u8 nFingerNum;
    TouchPoint_t Point[MAX_TOUCH_FINGER];
} TouchScreenInfo_t;

 
static void tpd_eint_interrupt_handler(void);
static struct work_struct    msg21xx_wq;

#ifdef TPD_HAVE_BUTTON 
static int tpd_keys_local[TPD_KEY_COUNT] = TPD_KEYS;
static int tpd_keys_dim_local[TPD_KEY_COUNT][4] = TPD_KEYS_DIM;
#endif

#if 0
extern void mt65xx_eint_unmask(unsigned int line);
extern void mt65xx_eint_mask(unsigned int line);
extern void mt65xx_eint_set_hw_debounce(unsigned int eint_num, unsigned int ms);
extern unsigned int mt65xx_eint_set_sens(unsigned int eint_num, unsigned int sens);
extern void mt65xx_eint_registration(unsigned int eint_num, unsigned int is_deb_en, unsigned int pol, void (EINT_FUNC_PTR)(void), unsigned int is_auto_umask);
#endif

 
static int __devinit tpd_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int tpd_detect (struct i2c_client *client, struct i2c_board_info *info);
static int __devexit tpd_remove(struct i2c_client *client);
static int touch_event_handler(void *unused);
 

static int tpd_flag = 0;
static int tpd_halt=0;
static int point_num = 0;
static int p_point_num = 0;



#define TPD_OK 0

 
 static const struct i2c_device_id msg2133_tpd_id[] = {{"msg2133",0},{}};

 static struct i2c_board_info __initdata msg2133_i2c_tpd={ I2C_BOARD_INFO("msg2133", (0x26))};
 
 
 static struct i2c_driver tpd_i2c_driver = {
  .driver = {
	 .name = "msg2133",//.name = TPD_DEVICE,
//	 .owner = THIS_MODULE,
  },
  .probe = tpd_probe,
  .remove = __devexit_p(tpd_remove),
  .id_table = msg2133_tpd_id,
  .detect = tpd_detect,
//  .address_data = &addr_data,
 };
 //start for update firmware //msz   for update firmware 20121126
#define __FIRMWARE_UPDATE__
#define MSG2133_UPDATE

//#ifdef MSG2133_UPDATE
#ifdef __FIRMWARE_UPDATE__

#define MSG2133_TS_ADDR			0x26
#define MSG2133_FW_ADDR			0x62
#define MSG2133_FW_UPDATE_ADDR   	0x49

static struct i2c_client     *this_client = NULL;
struct class *firmware_class;
struct device *firmware_cmd_dev;
static int update_switch = 0;
static int FwDataCnt;
static  char *fw_version;
static unsigned char temp[94][1024];
static u8 g_dwiic_info_data[1024];   // Buffer for info data

#ifdef __MSG_DMA_MODE__
static void msg_dma_alloct()
{
	g_dma_buff_va = (u8 *)dma_alloc_coherent(NULL, 4096, &g_dma_buff_pa, GFP_KERNEL);
    if(!g_dma_buff_va)
	{
        TPD_DMESG("[DMA][Error] Allocate DMA I2C Buffer failed!\n");
    }
}

static void msg_dma_release()
{
	if(g_dma_buff_va)
	{
     	dma_free_coherent(NULL, 4096, g_dma_buff_va, g_dma_buff_pa);
        g_dma_buff_va = NULL;
        g_dma_buff_pa = NULL;
		TPD_DMESG("[DMA][release] Allocate DMA I2C Buffer release!\n");
    }
}
#endif


static void msg2133_device_power_on()
{
	#ifdef TPD_POWER_SOURCE_CUSTOM
	hwPowerOn(TPD_POWER_SOURCE_CUSTOM, VOL_2800, "TP");
#else
	hwPowerOn(MT65XX_POWER_LDO_VGP2, VOL_2800, "TP");
#endif
	TPD_DMESG("msg2133: power on\n");
}

static void msg2133_reset()
{
	mt_set_gpio_mode(GPIO_CTP_RST_PIN, GPIO_CTP_RST_PIN_M_GPIO);
    mt_set_gpio_dir(GPIO_CTP_RST_PIN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ONE);
	msleep(10);
	mt_set_gpio_mode(GPIO_CTP_RST_PIN, GPIO_CTP_RST_PIN_M_GPIO);
    mt_set_gpio_dir(GPIO_CTP_RST_PIN, GPIO_DIR_OUT);
    mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ZERO);  
	msleep(50);
	TPD_DMESG(" msg2133 reset\n");
	mt_set_gpio_mode(GPIO_CTP_RST_PIN, GPIO_CTP_RST_PIN_M_GPIO);
    mt_set_gpio_dir(GPIO_CTP_RST_PIN, GPIO_DIR_OUT);
    mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ONE);
	msleep(150);

}


/*static bool msg2133_i2c_read(char *pbt_buf, int dw_lenth)
{
    int ret;
    MSG2133_DBG("The msg_i2c_client->addr=0x%x\n",i2c_client->addr);
    ret = i2c_master_recv(this_client, pbt_buf, dw_lenth);

    if(ret <= 0){
        MSG2133_DBG("msg_i2c_read_interface error\n");
        return false;
    }

    return true;
}

static bool msg2133_i2c_write(char *pbt_buf, int dw_lenth)
{
    int ret;
    MSG2133_DBG("The msg_i2c_client->addr=0x%x\n",i2c_client->addr);
    ret = i2c_master_send(this_client, pbt_buf, dw_lenth);

    if(ret <= 0){
        MSG2133_DBG("msg_i2c_read_interface error\n");
        return false;
    }

    return true;
}*/

static void HalTscrCReadI2CSeq(u8 addr, u8* read_data, u16 size)
{
   //according to your platform.
   	int rc;
	#ifdef __MSG_DMA_MODE__
	if (g_dma_buff_va == NULL)
		return;
	#endif
	struct i2c_msg msgs[] =
    {
		{
			
			.flags = I2C_M_RD,
			.len = size,
			#ifdef __MSG_DMA_MODE__
			.addr = addr & I2C_MASK_FLAG | I2C_DMA_FLAG,
			.buf = g_dma_buff_pa,
			#else
			.addr = addr,
			.buf = read_data,
			#endif
		},
	};

	rc = i2c_transfer(this_client->adapter, msgs, 1);
	if( rc < 0 )
    {
		printk("HalTscrCReadI2CSeq error %d\n", rc);
	}
	#ifdef __MSG_DMA_MODE__
	else
	{
		memcpy(read_data, g_dma_buff_va, size);
	}
	#endif
	
}

static void HalTscrCDevWriteI2CSeq(u8 addr, u8* data, u16 size)
{
    //according to your platform.
   	int rc;
	#ifdef __MSG_DMA_MODE__
	if (g_dma_buff_va == NULL)
		return;
	memcpy(g_dma_buff_va, data, size);
	#endif
	
	struct i2c_msg msgs[] =
    {
		{
			
			.flags = 0,
			.len = size,
			#ifdef __MSG_DMA_MODE__
			.addr = addr & I2C_MASK_FLAG | I2C_DMA_FLAG,
			.buf = g_dma_buff_pa,
			#else
			.addr = addr,
			.buf = data,
			#endif
		},
	};
	rc = i2c_transfer(this_client->adapter, msgs, 1);
	if( rc < 0 )
    {
		printk("HalTscrCDevWriteI2CSeq error %d,addr = %d\n", rc,addr);
	}
}
/*
static bool msg2133_i2c_read(char *pbt_buf, int dw_lenth)
{
    int ret;
    //    pr_ch("The msg_i2c_client->addr=0x%x\n",i2c_client->addr);
    ret = i2c_master_recv(this_client, pbt_buf, dw_lenth);

    if(ret <= 0)
    {
        //pr_tp("msg_i2c_read_interface error\n");
        return false;
    }

    return true;
}

static bool msg2133_i2c_write(char *pbt_buf, int dw_lenth)
{
    int ret;
    //    pr_ch("The msg_i2c_client->addr=0x%x\n",i2c_client->addr);
    ret = i2c_master_send(this_client, pbt_buf, dw_lenth);

    if(ret <= 0)
    {
        //pr_tp("msg_i2c_read_interface error\n");
        return false;
    }

    return true;
}
*/
static void i2c_read_msg2133(unsigned char *pbt_buf, int dw_lenth)
{
    //this_client->addr = MSG2133_FW_ADDR;
	//i2c_master_recv(this_client, pbt_buf, dw_lenth);	//0xC5_8bit
	//this_client->addr = MSG2133_TS_ADDR;
	   	int rc;
	#ifdef __MSG_DMA_MODE__
	if (g_dma_buff_va == NULL)
		return;
	#endif
	struct i2c_msg msgs[] =
    {
		{
			.flags = I2C_M_RD,
			.len = dw_lenth,
		#ifdef __MSG_DMA_MODE__
			.addr = MSG2133_FW_ADDR & I2C_MASK_FLAG | I2C_DMA_FLAG,
			.buf = g_dma_buff_pa,
		#else
			.addr = MSG2133_FW_ADDR,
			.buf = pbt_buf,
		#endif
		},
	};
	rc = i2c_transfer(this_client->adapter, msgs, 1);
	if( rc < 0 )
    {
		printk("i2c_read_msg2133 error %d,addr = %d\n", rc,MSG2133_FW_ADDR);
	}
	#ifdef __MSG_DMA_MODE__
	else
	{
		memcpy(pbt_buf, g_dma_buff_va, dw_lenth);
	}
	#endif
	
}

static void i2c_write_msg2133(unsigned char *pbt_buf, int dw_lenth)
{

	//this_client->addr = MSG2133_FW_ADDR;
	//i2c_master_send(this_client, pbt_buf, dw_lenth);		//0xC4_8bit
	//this_client->addr = MSG2133_TS_ADDR;
	int rc;
	#ifdef __MSG_DMA_MODE__
	if (g_dma_buff_va == NULL)
		return;
	memcpy(g_dma_buff_va, pbt_buf, dw_lenth);
	#endif
	struct i2c_msg msgs[] =
    {
		{
			
			.flags = 0,
			.len = dw_lenth,
			#ifdef __MSG_DMA_MODE__
			.addr = MSG2133_FW_ADDR & I2C_MASK_FLAG | I2C_DMA_FLAG,
			.buf = g_dma_buff_pa,
			#else
			.addr = MSG2133_FW_ADDR,
			.buf = pbt_buf,
			#endif
		},
	};
	rc = i2c_transfer(this_client->adapter, msgs, 1);
	if( rc < 0 )
    {
		printk("i2c_write_msg2133 error %d,addr = %d\n", rc,MSG2133_FW_ADDR);
	}
}

static void i2c_read_update_msg2133(unsigned char *pbt_buf, int dw_lenth)
{	

	//this_client->addr = MSG2133_FW_UPDATE_ADDR;
	//i2c_master_recv(this_client, pbt_buf, dw_lenth);	//0x93_8bit
	//this_client->addr = MSG2133_TS_ADDR;
	int rc;
	#ifdef __MSG_DMA_MODE__
	if (g_dma_buff_va == NULL)
		return;
	#endif
	struct i2c_msg msgs[] =
    {
		{
			
			.flags = I2C_M_RD,
			.len = dw_lenth,
			#ifdef __MSG_DMA_MODE__
			.addr = MSG2133_FW_UPDATE_ADDR & I2C_MASK_FLAG | I2C_DMA_FLAG,
			.buf = g_dma_buff_pa,
			#else
			.addr = MSG2133_FW_UPDATE_ADDR,
			.buf = pbt_buf,
			#endif
		},
	};
	rc = i2c_transfer(this_client->adapter, msgs, 1);
	if( rc < 0 )
    {
		printk("i2c_read_update_msg2133 error %d,addr = %d\n", rc,MSG2133_FW_ADDR);
	}
	#ifdef __MSG_DMA_MODE__
	else
	{
		memcpy(pbt_buf, g_dma_buff_va, dw_lenth);
	}
	#endif
}

static void i2c_write_update_msg2133(unsigned char *pbt_buf, int dw_lenth)
{	
  //  this_client->addr = MSG2133_FW_UPDATE_ADDR;
	//i2c_master_send(this_client, pbt_buf, dw_lenth);	//0x92_8bit
	//this_client->addr = MSG2133_TS_ADDR;
		//this_client->addr = MSG2133_TS_ADDR;
	int rc;
  	#ifdef __MSG_DMA_MODE__
	if (g_dma_buff_va == NULL)
		return;
	memcpy(g_dma_buff_va, pbt_buf, dw_lenth);
	#endif
	struct i2c_msg msgs[] =
    {
		{
			
			.flags = 0,
			.len = dw_lenth,
			#ifdef __MSG_DMA_MODE__
			.addr = MSG2133_FW_UPDATE_ADDR & I2C_MASK_FLAG | I2C_DMA_FLAG,
			.buf = g_dma_buff_pa,
			#else
			.addr = MSG2133_FW_UPDATE_ADDR,
			.buf = pbt_buf,
			#endif
			
		},
	};
	rc = i2c_transfer(this_client->adapter, msgs, 1);
	if( rc < 0 )
    {
		printk("i2c_write_update_msg2133 error %d,addr = %d\n", rc,MSG2133_FW_ADDR);
	}
}



void dbbusDWIICEnterSerialDebugMode(void)
{
    unsigned char data[5];
    // Enter the Serial Debug Mode
    data[0] = 0x53;
    data[1] = 0x45;
    data[2] = 0x52;
    data[3] = 0x44;
    data[4] = 0x42;
    i2c_write_msg2133(data, 5);
}

void dbbusDWIICStopMCU(void)
{
    unsigned char data[1];
    // Stop the MCU
    data[0] = 0x37;
    i2c_write_msg2133(data, 1);
}

void dbbusDWIICIICUseBus(void)
{
    unsigned char data[1];
    // IIC Use Bus
    data[0] = 0x35;
    i2c_write_msg2133(data, 1);
}

void dbbusDWIICIICReshape(void)
{
    unsigned char data[1];
    // IIC Re-shape
    data[0] = 0x71;
    i2c_write_msg2133(data, 1);
}

void dbbusDWIICIICNotUseBus(void)
{
    unsigned char data[1];
    // IIC Not Use Bus
    data[0] = 0x34;
    i2c_write_msg2133(data, 1);
}

void dbbusDWIICNotStopMCU(void)
{
    unsigned char data[1];
    // Not Stop the MCU
    data[0] = 0x36;
    i2c_write_msg2133(data, 1);
}

void dbbusDWIICExitSerialDebugMode(void)
{
    unsigned char data[1];
    // Exit the Serial Debug Mode
    data[0] = 0x45;
    i2c_write_msg2133(data, 1);
    // Delay some interval to guard the next transaction
}

void drvISP_EntryIspMode(void)
{
    unsigned char bWriteData[5] =
    {
        0x4D, 0x53, 0x54, 0x41, 0x52
    };
    i2c_write_update_msg2133(bWriteData, 5);
    msleep(10);           // delay about 10ms
}

void drvISP_WriteEnable(void)
{
    unsigned char bWriteData[2] =
    {
        0x10, 0x06
    };
    unsigned char bWriteData1 = 0x12;
    i2c_write_update_msg2133(bWriteData, 2);
    i2c_write_update_msg2133(&bWriteData1, 1);
}

unsigned char drvISP_Read(unsigned char n, unsigned char *pDataToRead)    //First it needs send 0x11 to notify we want to get flash data back.
{
    unsigned char Read_cmd = 0x11;
    unsigned char i = 0;
    unsigned char dbbus_rx_data[16] = {0};
    i2c_write_update_msg2133(&Read_cmd, 1);
    //if (n == 1)
    {
        i2c_read_update_msg2133(&dbbus_rx_data[0], n + 1);

        for(i = 0; i < n; i++)
        {
            *(pDataToRead + i) = dbbus_rx_data[i + 1];
        }
    }
    //else
    {
        //     i2c_read_update_msg2133(pDataToRead, n);
    }
    return 0;
}

unsigned char drvISP_ReadStatus(void)
{
    unsigned char bReadData = 0;
    unsigned char bWriteData[2] =
    {
        0x10, 0x05
    };
    unsigned char bWriteData1 = 0x12;
//    msleep(1);           // delay about 100us
    i2c_write_update_msg2133(bWriteData, 2);
//    msleep(1);           // delay about 100us
    drvISP_Read(1, &bReadData);
//    msleep(10);           // delay about 10ms
    i2c_write_update_msg2133(&bWriteData1, 1);
    return bReadData;
}



void drvISP_BlockErase(unsigned int addr)
{
    unsigned char bWriteData[5] = { 0x00, 0x00, 0x00, 0x00, 0x00 };
    unsigned char bWriteData1 = 0x12;
    unsigned int timeOutCount=0;
	
    drvISP_WriteEnable();
    //Enable write status register
    bWriteData[0] = 0x10;
    bWriteData[1] = 0x50;
    i2c_write_update_msg2133(bWriteData, 2);
    i2c_write_update_msg2133(&bWriteData1, 1);
    //Write Status
    bWriteData[0] = 0x10;
    bWriteData[1] = 0x01;
    bWriteData[2] = 0x00;
    i2c_write_update_msg2133(bWriteData, 3);
    i2c_write_update_msg2133(&bWriteData1, 1);
    //Write disable
    bWriteData[0] = 0x10;
    bWriteData[1] = 0x04;
    i2c_write_update_msg2133(bWriteData, 2);
    i2c_write_update_msg2133(&bWriteData1, 1);

    timeOutCount=0;
    msleep(1);           // delay about 100us
    while((drvISP_ReadStatus() & 0x01) == 0x01)
    {
        timeOutCount++;
	 if ( timeOutCount > 10000 ) 
            break; /* around 1 sec timeout */
    }

    //pr_ch("The drvISP_ReadStatus3=%d\n", drvISP_ReadStatus());
    drvISP_WriteEnable();
    //pr_ch("The drvISP_ReadStatus4=%d\n", drvISP_ReadStatus());
    bWriteData[0] = 0x10;
    bWriteData[1] = 0xC7;        //Block Erase
    //bWriteData[2] = ((addr >> 16) & 0xFF) ;
    //bWriteData[3] = ((addr >> 8) & 0xFF) ;
    // bWriteData[4] = (addr & 0xFF) ;
    i2c_write_update_msg2133(bWriteData, 2);
    //i2c_write_update_msg2133( &bWriteData, 5);
    i2c_write_update_msg2133(&bWriteData1, 1);

    timeOutCount=0;
    msleep(1);           // delay about 100us
    while((drvISP_ReadStatus() & 0x01) == 0x01)
    {
        timeOutCount++;
	 if ( timeOutCount > 10000 ) 
            break; /* around 1 sec timeout */
    }
}

void drvISP_Program(unsigned short k, unsigned char *pDataToWrite)
{
    unsigned short i = 0;
    unsigned short j = 0;
    //U16 n = 0;
    unsigned char TX_data[133];
    unsigned char bWriteData1 = 0x12;
    unsigned int addr = k * 1024;
#if 1

    for(j = 0; j < 8; j++)    //128*8 cycle
    {
        TX_data[0] = 0x10;
        TX_data[1] = 0x02;// Page Program CMD
        TX_data[2] = (addr + 128 * j) >> 16;
        TX_data[3] = (addr + 128 * j) >> 8;
        TX_data[4] = (addr + 128 * j);

        for(i = 0; i < 128; i++)
        {
            TX_data[5 + i] = pDataToWrite[j * 128 + i];
        }

        while((drvISP_ReadStatus() & 0x01) == 0x01)
        {
            ;    //wait until not in write operation
        }

        drvISP_WriteEnable();
        i2c_write_update_msg2133( TX_data, 133);   //write 133 byte per cycle
        i2c_write_update_msg2133(&bWriteData1, 1);
    }

#else

    for(j = 0; j < 512; j++)    //128*8 cycle
    {
        TX_data[0] = 0x10;
        TX_data[1] = 0x02;// Page Program CMD
        TX_data[2] = (addr + 2 * j) >> 16;
        TX_data[3] = (addr + 2 * j) >> 8;
        TX_data[4] = (addr + 2 * j);

        for(i = 0; i < 2; i++)
        {
            TX_data[5 + i] = pDataToWrite[j * 2 + i];
        }

        while((drvISP_ReadStatus() & 0x01) == 0x01)
        {
            ;    //wait until not in write operation
        }

        drvISP_WriteEnable();
        i2c_write_update_msg2133(TX_data, 7);    //write 7 byte per cycle
        i2c_write_update_msg2133(&bWriteData1, 1);
    }

#endif
}

void drvISP_ExitIspMode(void)
{
    unsigned char bWriteData = 0x24;
    i2c_write_update_msg2133(&bWriteData, 1);
}


static ssize_t firmware_version_show(struct device *dev,
                                     struct device_attribute *attr, char *buf)
{
	printk("tyd-tp: firmware_version_show\n");
    TPD_DMESG("*** firmware_version_show fw_version = %s***\n", fw_version);
    return sprintf(buf, "%s\n", fw_version);
}

static ssize_t firmware_version_store(struct device *dev,
                                      struct device_attribute *attr, const char *buf, size_t size)
{
	#ifdef __MSG_DMA_MODE__
	msg_dma_alloct();
	#endif
    unsigned char dbbus_tx_data[3];
    unsigned char dbbus_rx_data[4] ;
    unsigned short major = 0, minor = 0;
    dbbusDWIICEnterSerialDebugMode();
    dbbusDWIICStopMCU();
    dbbusDWIICIICUseBus();
    dbbusDWIICIICReshape();
    fw_version = kzalloc(sizeof(char), GFP_KERNEL);
    TPD_DMESG("\n");
	TPD_DMESG("tyd-tp: firmware_version_store\n");
    //Get_Chip_Version();
    dbbus_tx_data[0] = 0x53;
    dbbus_tx_data[1] = 0x00;
    dbbus_tx_data[2] = 0x2a;//0x74--msg2133a;  0x2A----msg2133a
    //i2c_write(TOUCH_ADDR_MSG20XX, &dbbus_tx_data[0], 3);
    //i2c_read(TOUCH_ADDR_MSG20XX, &dbbus_rx_data[0], 4);
//    msg2133_i2c_write(&dbbus_tx_data[0], 3);
//    msg2133_i2c_read(&dbbus_rx_data[0], 4);
		HalTscrCDevWriteI2CSeq(this_client->addr, &dbbus_tx_data[0], 3);
		HalTscrCReadI2CSeq(this_client->addr, &dbbus_rx_data[0], 4);
    major = (dbbus_rx_data[1] << 8) + dbbus_rx_data[0];
    minor = (dbbus_rx_data[3] << 8) + dbbus_rx_data[2];
    TPD_DMESG("***major = %d ***\n", major);
    TPD_DMESG("***minor = %d ***\n", minor);
    sprintf(fw_version, "%03d%03d", major, minor);
    TPD_DMESG("***fw_version = %s ***\n", fw_version);
	
	#ifdef __MSG_DMA_MODE__
	msg_dma_release();
	#endif
	
    return size;
}

static ssize_t firmware_update_show(struct device *dev,
                                    struct device_attribute *attr, char *buf)
{
	printk("tyd-tp: firmware_update_show\n");
    return sprintf(buf, "%s\n", fw_version);
}
#define _FW_UPDATE_C3_

#define FW_ADDR_MSG21XX   (0xC4>>1)
#define FW_ADDR_MSG21XX_TP   (0x4C>>1)
#define FW_UPDATE_ADDR_MSG21XX   (0x92>>1)
#define TP_DEBUG	printk//(x)		//x
#define DBUG	printk//(x) //x

#ifdef _FW_UPDATE_C3_

u8  Fmr_Loader[1024];
    u32 crc_tab[256];

#define _HalTscrHWReset(...) msg2133_reset(__VA_ARGS__)
//#define disable_irq(...) disable_irq_nosync(__VA_ARGS__)

static ssize_t firmware_update_c2(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    unsigned char i;
    unsigned char dbbus_tx_data[4];
    unsigned char dbbus_rx_data[2] = {0};
    update_switch = 1;
    //drvISP_EntryIspMode();
    //drvISP_BlockErase(0x00000);
    //M by cheehwa _HalTscrHWReset();

    //
  //  disable_irq_nosync(this_client->irq);
	
	msg2133_reset();
    //msctpc_LoopDelay ( 100 );        // delay about 100ms*****
    // Enable slave's ISP ECO mode
    dbbusDWIICEnterSerialDebugMode();
    dbbusDWIICStopMCU();
    dbbusDWIICIICUseBus();
    dbbusDWIICIICReshape();
    //pr_ch("dbbusDWIICIICReshape\n");
    dbbus_tx_data[0] = 0x10;
    dbbus_tx_data[1] = 0x08;
    dbbus_tx_data[2] = 0x0c;
    dbbus_tx_data[3] = 0x08;
    // Disable the Watchdog
    i2c_write_msg2133(dbbus_tx_data, 4);
    //Get_Chip_Version();
    dbbus_tx_data[0] = 0x10;
    dbbus_tx_data[1] = 0x11;
    dbbus_tx_data[2] = 0xE2;
    dbbus_tx_data[3] = 0x00;
    i2c_write_msg2133(dbbus_tx_data, 4);
    dbbus_tx_data[0] = 0x10;
    dbbus_tx_data[1] = 0x3C;
    dbbus_tx_data[2] = 0x60;
    dbbus_tx_data[3] = 0x55;
    i2c_write_msg2133(dbbus_tx_data, 4);
    //pr_ch("update\n");
    dbbus_tx_data[0] = 0x10;
    dbbus_tx_data[1] = 0x3C;
    dbbus_tx_data[2] = 0x61;
    dbbus_tx_data[3] = 0xAA;
    i2c_write_msg2133(dbbus_tx_data, 4);
    //Stop MCU
    dbbus_tx_data[0] = 0x10;
    dbbus_tx_data[1] = 0x0F;
    dbbus_tx_data[2] = 0xE6;
    dbbus_tx_data[3] = 0x01;
    i2c_write_msg2133(dbbus_tx_data, 4);
    //Enable SPI Pad
    dbbus_tx_data[0] = 0x10;
    dbbus_tx_data[1] = 0x1E;
    dbbus_tx_data[2] = 0x02;
    i2c_write_msg2133(dbbus_tx_data, 3);
    i2c_read_msg2133(&dbbus_rx_data[0], 2);
    //pr_tp("dbbus_rx_data[0]=0x%x", dbbus_rx_data[0]);
    dbbus_tx_data[3] = (dbbus_rx_data[0] | 0x20);  //Set Bit 5
    i2c_write_msg2133(dbbus_tx_data, 4);
    dbbus_tx_data[0] = 0x10;
    dbbus_tx_data[1] = 0x1E;
    dbbus_tx_data[2] = 0x25;
    i2c_write_msg2133(dbbus_tx_data, 3);
    dbbus_rx_data[0] = 0;
    dbbus_rx_data[1] = 0;
    i2c_read_msg2133(&dbbus_rx_data[0], 2);
    //pr_tp("dbbus_rx_data[0]=0x%x", dbbus_rx_data[0]);
    dbbus_tx_data[3] = dbbus_rx_data[0] & 0xFC;  //Clear Bit 1,0
    i2c_write_msg2133(dbbus_tx_data, 4);
    /*
    //------------
    // ISP Speed Change to 400K
    dbbus_tx_data[0] = 0x10;
    dbbus_tx_data[1] = 0x11;
    dbbus_tx_data[2] = 0xE2;
    i2c_write_msg2133( dbbus_tx_data, 3);
    i2c_read_msg2133( &dbbus_rx_data[3], 1);
    //pr_tp("dbbus_rx_data[0]=0x%x", dbbus_rx_data[0]);
    dbbus_tx_data[3] = dbbus_tx_data[3]&0xf7;  //Clear Bit3
    i2c_write_msg2133( dbbus_tx_data, 4);
    */
    //WP overwrite
    dbbus_tx_data[0] = 0x10;
    dbbus_tx_data[1] = 0x1E;
    dbbus_tx_data[2] = 0x0E;
    dbbus_tx_data[3] = 0x02;
    i2c_write_msg2133(dbbus_tx_data, 4);
    //set pin high
    dbbus_tx_data[0] = 0x10;
    dbbus_tx_data[1] = 0x1E;
    dbbus_tx_data[2] = 0x10;
    dbbus_tx_data[3] = 0x08;
    i2c_write_msg2133(dbbus_tx_data, 4);
    dbbusDWIICIICNotUseBus();
    dbbusDWIICNotStopMCU();
    dbbusDWIICExitSerialDebugMode();
    ///////////////////////////////////////
    // Start to load firmware
    ///////////////////////////////////////
    drvISP_EntryIspMode();
    TPD_DMESG("entryisp\n");
    drvISP_BlockErase(0x00000);
    //msleep(1000);
    TPD_DMESG("FwVersion=2");

    for(i = 0; i < 94; i++)    // total  94 KB : 1 byte per R/W
    {
        //msleep(1);//delay_100us
        TPD_DMESG("drvISP_Program\n");
        drvISP_Program(i, temp[i]);    // program to slave's flash
        //pr_ch("drvISP_Verify\n");
        //drvISP_Verify ( i, temp[i] ); //verify data
    }

    //MSG2133_DBG("update OK\n");
    drvISP_ExitIspMode();
    FwDataCnt = 0;
    msg2133_reset();
    TPD_DMESG("update OK\n");
    update_switch = 0;
    //
    enable_irq(this_client->irq);
    return size;
}

static u32 Reflect ( u32 ref, char ch ) //unsigned int Reflect(unsigned int ref, char ch)
{
    u32 value = 0;
    u32 i = 0;

    for ( i = 1; i < ( ch + 1 ); i++ )
    {
        if ( ref & 1 )
        {
            value |= 1 << ( ch - i );
        }
        ref >>= 1;
    }
    return value;
}

u32 Get_CRC ( u32 text, u32 prevCRC, u32 *crc32_table )
{
    u32  ulCRC = prevCRC;
	ulCRC = ( ulCRC >> 8 ) ^ crc32_table[ ( ulCRC & 0xFF ) ^ text];
    return ulCRC ;
}
static void Init_CRC32_Table ( u32 *crc32_table )
{
    u32 magicnumber = 0x04c11db7;
    u32 i = 0, j;

    for ( i = 0; i <= 0xFF; i++ )
    {
        crc32_table[i] = Reflect ( i, 8 ) << 24;
        for ( j = 0; j < 8; j++ )
        {
            crc32_table[i] = ( crc32_table[i] << 1 ) ^ ( crc32_table[i] & ( 0x80000000L ) ? magicnumber : 0 );
        }
        crc32_table[i] = Reflect ( crc32_table[i], 32 );
    }
}

typedef enum
{
	EMEM_ALL = 0,
	EMEM_MAIN,
	EMEM_INFO,
} EMEM_TYPE_t;

static void drvDB_WriteReg8Bit ( u8 bank, u8 addr, u8 data )
{
    u8 tx_data[4] = {0x10, bank, addr, data};
    HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG21XX, tx_data, 4 );
}

static void drvDB_WriteReg ( u8 bank, u8 addr, u16 data )
{
    u8 tx_data[5] = {0x10, bank, addr, data & 0xFF, data >> 8};
    HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG21XX, tx_data, 5 );
}

static unsigned short drvDB_ReadReg ( u8 bank, u8 addr )
{
    u8 tx_data[3] = {0x10, bank, addr};
    u8 rx_data[2] = {0};

    HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG21XX, tx_data, 3 );
    HalTscrCReadI2CSeq ( FW_ADDR_MSG21XX, &rx_data[0], 2 );
    return ( rx_data[1] << 8 | rx_data[0] );
}

static int drvTP_erase_emem_c32 ( void )
{
    /////////////////////////
    //Erase  all
    /////////////////////////
    
    //enter gpio mode
    drvDB_WriteReg ( 0x16, 0x1E, 0xBEAF );

    // before gpio mode, set the control pin as the orginal status
    drvDB_WriteReg ( 0x16, 0x08, 0x0000 );
    drvDB_WriteReg8Bit ( 0x16, 0x0E, 0x10 );
    mdelay ( 10 ); //MCR_CLBK_DEBUG_DELAY ( 10, MCU_LOOP_DELAY_COUNT_MS );

    // ptrim = 1, h'04[2]
    drvDB_WriteReg8Bit ( 0x16, 0x08, 0x04 );
    drvDB_WriteReg8Bit ( 0x16, 0x0E, 0x10 );
    mdelay ( 10 ); //MCR_CLBK_DEBUG_DELAY ( 10, MCU_LOOP_DELAY_COUNT_MS );

    // ptm = 6, h'04[12:14] = b'110
    drvDB_WriteReg8Bit ( 0x16, 0x09, 0x60 );
    drvDB_WriteReg8Bit ( 0x16, 0x0E, 0x10 );

    // pmasi = 1, h'04[6]
    drvDB_WriteReg8Bit ( 0x16, 0x08, 0x44 );
    // pce = 1, h'04[11]
    drvDB_WriteReg8Bit ( 0x16, 0x09, 0x68 );
    // perase = 1, h'04[7]
    drvDB_WriteReg8Bit ( 0x16, 0x08, 0xC4 );
    // pnvstr = 1, h'04[5]
    drvDB_WriteReg8Bit ( 0x16, 0x08, 0xE4 );
    // pwe = 1, h'04[9]
    drvDB_WriteReg8Bit ( 0x16, 0x09, 0x6A );
    // trigger gpio load
    drvDB_WriteReg8Bit ( 0x16, 0x0E, 0x10 );

    return ( 1 );
}

static ssize_t firmware_update_c32 ( struct device *dev, struct device_attribute *attr,
                                     const char *buf, size_t size,  EMEM_TYPE_t emem_type )
{
    u8  dbbus_tx_data[4];
    u8  dbbus_rx_data[2] = {0};
      // Buffer for slave's firmware

    u32 i, j, k;
    u32 crc_main, crc_main_tp;
    u32 crc_info, crc_info_tp;
    u16 reg_data = 0;

    crc_main = 0xffffffff;
    crc_info = 0xffffffff;

#if 1
    /////////////////////////
    // Erase  all
    /////////////////////////
    drvTP_erase_emem_c32();
    mdelay ( 1000 ); //MCR_CLBK_DEBUG_DELAY ( 1000, MCU_LOOP_DELAY_COUNT_MS );

    //ResetSlave();
    _HalTscrHWReset();
    //drvDB_EnterDBBUS();
    dbbusDWIICEnterSerialDebugMode();
    dbbusDWIICStopMCU();
    dbbusDWIICIICUseBus();
    dbbusDWIICIICReshape();
    mdelay ( 300 );

    // Reset Watchdog
    drvDB_WriteReg8Bit ( 0x3C, 0x60, 0x55 );
    drvDB_WriteReg8Bit ( 0x3C, 0x61, 0xAA );

    /////////////////////////
    // Program
    /////////////////////////

    //polling 0x3CE4 is 0x1C70
    do
    {
        reg_data = drvDB_ReadReg ( 0x3C, 0xE4 );
    }
    while ( reg_data != 0x1C70 );


    drvDB_WriteReg ( 0x3C, 0xE4, 0xE38F );  // for all-blocks

    //polling 0x3CE4 is 0x2F43
    do
    {
        reg_data = drvDB_ReadReg ( 0x3C, 0xE4 );
    }
    while ( reg_data != 0x2F43 );


    //calculate CRC 32
    Init_CRC32_Table ( &crc_tab[0] );

    for ( i = 0; i < 33; i++ ) // total  33 KB : 2 byte per R/W
    {
        if ( i < 32 )   //emem_main
        {
            if ( i == 31 )
            {
                temp[i][1014] = 0x5A; //Fmr_Loader[1014]=0x5A;
                temp[i][1015] = 0xA5; //Fmr_Loader[1015]=0xA5;

                for ( j = 0; j < 1016; j++ )
                {
                    //crc_main=Get_CRC(Fmr_Loader[j],crc_main,&crc_tab[0]);
                    crc_main = Get_CRC ( temp[i][j], crc_main, &crc_tab[0] );
                }
            }
            else
            {
                for ( j = 0; j < 1024; j++ )
                {
                    //crc_main=Get_CRC(Fmr_Loader[j],crc_main,&crc_tab[0]);
                    crc_main = Get_CRC ( temp[i][j], crc_main, &crc_tab[0] );
                }
            }
        }
        else  // emem_info
        {
            for ( j = 0; j < 1024; j++ )
            {
                //crc_info=Get_CRC(Fmr_Loader[j],crc_info,&crc_tab[0]);
                crc_info = Get_CRC ( temp[i][j], crc_info, &crc_tab[0] );
            }
        }

        //drvDWIIC_MasterTransmit( DWIIC_MODE_DWIIC_ID, 1024, Fmr_Loader );
        //HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG21XX_TP, temp[i], 1024 );
		for(k=0; k<8; k++)
        {
        	//HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG21XX_TP , &g_dwiic_info_data[i*128], 128 );
        	HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG21XX_TP , &temp[i][k*128], 128 );
			TPD_DMESG ( "firmware_update_c32---g_dwiic_info_data[i*128]: %d\n", i );
			mdelay(50);
        }
        // polling 0x3CE4 is 0xD0BC
        do
        {
            reg_data = drvDB_ReadReg ( 0x3C, 0xE4 );
        }
        while ( reg_data != 0xD0BC );

        drvDB_WriteReg ( 0x3C, 0xE4, 0x2F43 );
    }

    //write file done
    drvDB_WriteReg ( 0x3C, 0xE4, 0x1380 );

    mdelay ( 10 ); //MCR_CLBK_DEBUG_DELAY ( 10, MCU_LOOP_DELAY_COUNT_MS );
    // polling 0x3CE4 is 0x9432
    do
    {
        reg_data = drvDB_ReadReg ( 0x3C, 0xE4 );
    }
    while ( reg_data != 0x9432 );

    crc_main = crc_main ^ 0xffffffff;
    crc_info = crc_info ^ 0xffffffff;

    // CRC Main from TP
    crc_main_tp = drvDB_ReadReg ( 0x3C, 0x80 );
    crc_main_tp = ( crc_main_tp << 16 ) | drvDB_ReadReg ( 0x3C, 0x82 );
 
    //CRC Info from TP
    crc_info_tp = drvDB_ReadReg ( 0x3C, 0xA0 );
    crc_info_tp = ( crc_info_tp << 16 ) | drvDB_ReadReg ( 0x3C, 0xA2 );

    TPD_DMESG ( "crc_main=0x%x, crc_info=0x%x, crc_main_tp=0x%x, crc_info_tp=0x%x\n",
               crc_main, crc_info, crc_main_tp, crc_info_tp );

    //drvDB_ExitDBBUS();
    if ( ( crc_main_tp != crc_main ) || ( crc_info_tp != crc_info ) )
    {
        TPD_DMESG ( "update FAILED\n" );
		_HalTscrHWReset();
        FwDataCnt = 0;
    	enable_irq(this_client->irq);		
        return ( 0 );
    }

    TPD_DMESG ( "update OK\n" );
	_HalTscrHWReset();
    FwDataCnt = 0;
	enable_irq(this_client->irq);

    return size;
#endif
}

static int drvTP_erase_emem_c33 ( EMEM_TYPE_t emem_type )
{
    // stop mcu
    drvDB_WriteReg ( 0x0F, 0xE6, 0x0001 );

    //disable watch dog
    drvDB_WriteReg8Bit ( 0x3C, 0x60, 0x55 );
    drvDB_WriteReg8Bit ( 0x3C, 0x61, 0xAA );

    // set PROGRAM password
    drvDB_WriteReg8Bit ( 0x16, 0x1A, 0xBA );
    drvDB_WriteReg8Bit ( 0x16, 0x1B, 0xAB );

    //proto.MstarWriteReg(F1.loopDevice, 0x1618, 0x80);
    drvDB_WriteReg8Bit ( 0x16, 0x18, 0x80 );

    if ( emem_type == EMEM_ALL )
    {
        drvDB_WriteReg8Bit ( 0x16, 0x08, 0x10 ); //mark
    }

    drvDB_WriteReg8Bit ( 0x16, 0x18, 0x40 );
    mdelay ( 10 );

    drvDB_WriteReg8Bit ( 0x16, 0x18, 0x80 );

    // erase trigger
    if ( emem_type == EMEM_MAIN )
    {
        drvDB_WriteReg8Bit ( 0x16, 0x0E, 0x04 ); //erase main
    }
    else
    {
        drvDB_WriteReg8Bit ( 0x16, 0x0E, 0x08 ); //erase all block
    }

    return ( 1 );
}

static int drvTP_read_emem_dbbus_c33 ( EMEM_TYPE_t emem_type, u16 addr, size_t size, u8 *p, size_t set_pce_high )
{
    u32 i;

    // Set the starting address ( must before enabling burst mode and enter riu mode )
    drvDB_WriteReg ( 0x16, 0x00, addr );

    // Enable the burst mode ( must before enter riu mode )
    drvDB_WriteReg ( 0x16, 0x0C, drvDB_ReadReg ( 0x16, 0x0C ) | 0x0001 );

    // Set the RIU password
    drvDB_WriteReg ( 0x16, 0x1A, 0xABBA );

    // Enable the information block if pifren is HIGH
    if ( emem_type == EMEM_INFO )
    {
        // Clear the PCE
        drvDB_WriteReg ( 0x16, 0x18, drvDB_ReadReg ( 0x16, 0x18 ) | 0x0080 );
        mdelay ( 10 );

        // Set the PIFREN to be HIGH
        drvDB_WriteReg ( 0x16, 0x08, 0x0010 );
    }

    // Set the PCE to be HIGH
    drvDB_WriteReg ( 0x16, 0x18, drvDB_ReadReg ( 0x16, 0x18 ) | 0x0040 );
    mdelay ( 10 );

    // Wait pce becomes 1 ( read data ready )
    while ( ( drvDB_ReadReg ( 0x16, 0x10 ) & 0x0004 ) != 0x0004 );

    for ( i = 0; i < size; i += 4 )
    {
        // Fire the FASTREAD command
        drvDB_WriteReg ( 0x16, 0x0E, drvDB_ReadReg ( 0x16, 0x0E ) | 0x0001 );

        // Wait the operation is done
        while ( ( drvDB_ReadReg ( 0x16, 0x10 ) & 0x0001 ) != 0x0001 );

        p[i + 0] = drvDB_ReadReg ( 0x16, 0x04 ) & 0xFF;
        p[i + 1] = ( drvDB_ReadReg ( 0x16, 0x04 ) >> 8 ) & 0xFF;
        p[i + 2] = drvDB_ReadReg ( 0x16, 0x06 ) & 0xFF;
        p[i + 3] = ( drvDB_ReadReg ( 0x16, 0x06 ) >> 8 ) & 0xFF;
    }

    // Disable the burst mode
    drvDB_WriteReg ( 0x16, 0x0C, drvDB_ReadReg ( 0x16, 0x0C ) & ( ~0x0001 ) );

    // Clear the starting address
    drvDB_WriteReg ( 0x16, 0x00, 0x0000 );

    //Always return to main block
    if ( emem_type == EMEM_INFO )
    {
        // Clear the PCE before change block
        drvDB_WriteReg ( 0x16, 0x18, drvDB_ReadReg ( 0x16, 0x18 ) | 0x0080 );
        mdelay ( 10 );
        // Set the PIFREN to be LOW
        drvDB_WriteReg ( 0x16, 0x08, drvDB_ReadReg ( 0x16, 0x08 ) & ( ~0x0010 ) );

        drvDB_WriteReg ( 0x16, 0x18, drvDB_ReadReg ( 0x16, 0x18 ) | 0x0040 );
        while ( ( drvDB_ReadReg ( 0x16, 0x10 ) & 0x0004 ) != 0x0004 );
    }

    // Clear the RIU password
    drvDB_WriteReg ( 0x16, 0x1A, 0x0000 );

    if ( set_pce_high )
    {
        // Set the PCE to be HIGH before jumping back to e-flash codes
        drvDB_WriteReg ( 0x16, 0x18, drvDB_ReadReg ( 0x16, 0x18 ) | 0x0040 );
        while ( ( drvDB_ReadReg ( 0x16, 0x10 ) & 0x0004 ) != 0x0004 );
    }

    return ( 1 );
}


static int drvTP_read_info_dwiic_c33 ( void )
{
    u8  dwiic_tx_data[5];
    u8  dwiic_rx_data[4];
    u16 reg_data=0;
    mdelay ( 300 );

    // Stop Watchdog
    TPD_DMESG ("drvTP_read_info_dwiic_c33---1 \n");
    drvDB_WriteReg8Bit ( 0x3C, 0x60, 0x55 );
    drvDB_WriteReg8Bit ( 0x3C, 0x61, 0xAA );
	TPD_DMESG ("drvTP_read_info_dwiic_c33---2 \n");

    drvDB_WriteReg ( 0x3C, 0xE4, 0xA4AB );
	TPD_DMESG ("drvTP_read_info_dwiic_c33---3 \n");

	drvDB_WriteReg ( 0x1E, 0x04, 0x7d60 );
	TPD_DMESG ("drvTP_read_info_dwiic_c33---4 \n");

    // TP SW reset
    drvDB_WriteReg ( 0x1E, 0x04, 0x829F );
	mdelay ( 100 );
    dwiic_tx_data[0] = 0x10;
    dwiic_tx_data[1] = 0x0F;
    dwiic_tx_data[2] = 0xE6;
    dwiic_tx_data[3] = 0x00;
    HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG21XX, dwiic_tx_data, 4 );	
    mdelay ( 100 );
	TPD_DMESG ("drvTP_read_info_dwiic_c33---5 \n");

    do{
        reg_data = drvDB_ReadReg ( 0x3C, 0xE4 );
    }
    while ( reg_data != 0x5B58 );
	TPD_DMESG ("drvTP_read_info_dwiic_c33---6 \n");

    dwiic_tx_data[0] = 0x72;
    dwiic_tx_data[1] = 0x80;
    dwiic_tx_data[2] = 0x00;
    dwiic_tx_data[3] = 0x04;
    dwiic_tx_data[4] = 0x00;
    HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG21XX_TP , dwiic_tx_data, 5 );

    mdelay ( 50 );
	TPD_DMESG ("drvTP_read_info_dwiic_c33---7 \n");

    // recive info data
    
    for(reg_data=0;reg_data<8;reg_data++)
    {
		TPD_DMESG ("drvTP_read_info_dwiic_c33---8---ADDR \n");
		 dwiic_tx_data[1] = 0x80+(((reg_data*128)&0xff00)>>8);            // address High
         dwiic_tx_data[2] = (reg_data*128)&0x00ff;                                   // address low
         HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG21XX_TP , dwiic_tx_data, 5 );
         mdelay (10 );
		 TPD_DMESG ("drvTP_read_info_dwiic_c33---8---READ START\n");
    	// recive info data
         HalTscrCReadI2CSeq ( FW_ADDR_MSG21XX_TP, &g_dwiic_info_data[reg_data*128], 128);
		mdelay (200 );
		TPD_DMESG ("drvTP_read_info_dwiic_c33---8---READ END \n");
    }

	
	TPD_DMESG ("drvTP_read_info_dwiic_c33---8 \n");

    return ( 1 );
}

static int drvTP_info_updata_C33 ( u16 start_index, u8 *data, u16 size )
{
    // size != 0, start_index+size !> 1024
    u16 i;
    for ( i = 0; i < size; i++ )
    {
        g_dwiic_info_data[start_index] = * ( data + i );
        start_index++;
    }
    return ( 1 );
}

static ssize_t firmware_update_c33 ( struct device *dev, struct device_attribute *attr,
                                     const char *buf, size_t size, EMEM_TYPE_t emem_type )
{
    u8  dbbus_tx_data[4];
    u8  dbbus_rx_data[2] = {0};
    u8  life_counter[2];
    u32 i, j, k;
    u32 crc_main, crc_main_tp;
    u32 crc_info, crc_info_tp;
  
    int update_pass = 1;
    u16 reg_data = 0;

    crc_main = 0xffffffff;
    crc_info = 0xffffffff;

    drvTP_read_info_dwiic_c33();
	
    if ( g_dwiic_info_data[0] == 'M' && g_dwiic_info_data[1] == 'S' && g_dwiic_info_data[2] == 'T' && g_dwiic_info_data[3] == 'A' && g_dwiic_info_data[4] == 'R' && g_dwiic_info_data[5] == 'T' && g_dwiic_info_data[6] == 'P' && g_dwiic_info_data[7] == 'C' )
    {
        // updata FW Version
        //drvTP_info_updata_C33 ( 8, &temp[32][8], 5 );

		g_dwiic_info_data[8]=temp[32][8];
		g_dwiic_info_data[9]=temp[32][9];
		g_dwiic_info_data[10]=temp[32][10];
		g_dwiic_info_data[11]=temp[32][11];
        // updata life counter
        life_counter[1] = (( ( (g_dwiic_info_data[13] << 8 ) | g_dwiic_info_data[12]) + 1 ) >> 8 ) & 0xFF;
        life_counter[0] = ( ( (g_dwiic_info_data[13] << 8 ) | g_dwiic_info_data[12]) + 1 ) & 0xFF;
		g_dwiic_info_data[12]=life_counter[0];
		g_dwiic_info_data[13]=life_counter[1];
		TPD_DMESG ( "life_counter[0]=%d life_counter[1]=%d\n",life_counter[0],life_counter[1] );
        //drvTP_info_updata_C33 ( 10, &life_counter[0], 3 );
        drvDB_WriteReg ( 0x3C, 0xE4, 0x78C5 );
		drvDB_WriteReg ( 0x1E, 0x04, 0x7d60 );
        // TP SW reset
        drvDB_WriteReg ( 0x1E, 0x04, 0x829F );

        mdelay ( 50 );

        //polling 0x3CE4 is 0x2F43
        do
        {
            reg_data = drvDB_ReadReg ( 0x3C, 0xE4 );

        }
        while ( reg_data != 0x2F43 );

        // transmit lk info data---xb.pang for 1024
        //HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG21XX_TP , &g_dwiic_info_data[0], 1024 );
		for(i=0;i<8;i++)
        {
        	HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG21XX_TP , &g_dwiic_info_data[i*128], 128 );
			TPD_DMESG ( "HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG21XX_TP , &g_dwiic_info_data[%*128], 128 ); \n", i);
			mdelay(50);
        }
        //polling 0x3CE4 is 0xD0BC
        do
        {
            reg_data = drvDB_ReadReg ( 0x3C, 0xE4 );
        }
        while ( reg_data != 0xD0BC );

    }

    //erase main
    drvTP_erase_emem_c33 ( EMEM_MAIN );
    mdelay ( 1000 );

    //ResetSlave();
    _HalTscrHWReset();

    //drvDB_EnterDBBUS();
    dbbusDWIICEnterSerialDebugMode();
    dbbusDWIICStopMCU();
    dbbusDWIICIICUseBus();
    dbbusDWIICIICReshape();
    mdelay ( 300 );

    /////////////////////////
    // Program
    /////////////////////////

    //polling 0x3CE4 is 0x1C70
    if ( ( emem_type == EMEM_ALL ) || ( emem_type == EMEM_MAIN ) )
    {
        do
        {
            reg_data = drvDB_ReadReg ( 0x3C, 0xE4 );
        }
        while ( reg_data != 0x1C70 );
    }

    switch ( emem_type )
    {
        case EMEM_ALL:
            drvDB_WriteReg ( 0x3C, 0xE4, 0xE38F );  // for all-blocks
            break;
        case EMEM_MAIN:
            drvDB_WriteReg ( 0x3C, 0xE4, 0x7731 );  // for main block
            break;
        case EMEM_INFO:
            drvDB_WriteReg ( 0x3C, 0xE4, 0x7731 );  // for info block

            drvDB_WriteReg8Bit ( 0x0F, 0xE6, 0x01 );

            drvDB_WriteReg8Bit ( 0x3C, 0xE4, 0xC5 ); //
            drvDB_WriteReg8Bit ( 0x3C, 0xE5, 0x78 ); //

            drvDB_WriteReg8Bit ( 0x1E, 0x04, 0x9F );
            drvDB_WriteReg8Bit ( 0x1E, 0x05, 0x82 );

            drvDB_WriteReg8Bit ( 0x0F, 0xE6, 0x00 );
            mdelay ( 100 );
            break;
    }

    // polling 0x3CE4 is 0x2F43
    do
    {
        reg_data = drvDB_ReadReg ( 0x3C, 0xE4 );
    }
    while ( reg_data != 0x2F43 );

    // calculate CRC 32
    Init_CRC32_Table ( &crc_tab[0] );

    for ( i = 0; i < 33; i++ ) // total  33 KB : 2 byte per R/W
    {
        if ( emem_type == EMEM_INFO )
			i = 32;

        if ( i < 32 )   //emem_main
        {
            if ( i == 31 )
            {
                temp[i][1014] = 0x5A; //Fmr_Loader[1014]=0x5A;
                temp[i][1015] = 0xA5; //Fmr_Loader[1015]=0xA5;

                for ( j = 0; j < 1016; j++ )
                {
                    //crc_main=Get_CRC(Fmr_Loader[j],crc_main,&crc_tab[0]);
                    crc_main = Get_CRC ( temp[i][j], crc_main, &crc_tab[0] );
                }
            }
            else
            {
                for ( j = 0; j < 1024; j++ )
                {
                    //crc_main=Get_CRC(Fmr_Loader[j],crc_main,&crc_tab[0]);
                    crc_main = Get_CRC ( temp[i][j], crc_main, &crc_tab[0] );
                }
            }
        }
        else  //emem_info
        {
            for ( j = 0; j < 1024; j++ )
            {
                //crc_info=Get_CRC(Fmr_Loader[j],crc_info,&crc_tab[0]);
                crc_info = Get_CRC ( g_dwiic_info_data[j], crc_info, &crc_tab[0] );
            }
            if ( emem_type == EMEM_MAIN ) break;
        }

        //drvDWIIC_MasterTransmit( DWIIC_MODE_DWIIC_ID, 1024, Fmr_Loader );
        //HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG21XX_TP, temp[i], 1024 );
		for(k=0; k<8; k++)
        {
        	HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG21XX_TP , &temp[i][k*128], 128 );
			TPD_DMESG ( "temp[i] \n");
			mdelay(50);
        }
        // polling 0x3CE4 is 0xD0BC
        do
        {
            reg_data = drvDB_ReadReg ( 0x3C, 0xE4 );
        }
        while ( reg_data != 0xD0BC );

        drvDB_WriteReg ( 0x3C, 0xE4, 0x2F43 );
    }

    if ( ( emem_type == EMEM_ALL ) || ( emem_type == EMEM_MAIN ) )
    {
        // write file done and check crc
        drvDB_WriteReg ( 0x3C, 0xE4, 0x1380 );
    }
    mdelay ( 10 ); //MCR_CLBK_DEBUG_DELAY ( 10, MCU_LOOP_DELAY_COUNT_MS );

    if ( ( emem_type == EMEM_ALL ) || ( emem_type == EMEM_MAIN ) )
    {
        // polling 0x3CE4 is 0x9432
        do
        {
            reg_data = drvDB_ReadReg ( 0x3C, 0xE4 );
			TPD_DMESG("polling \n");
        }while ( reg_data != 0x9432 );
    }

    crc_main = crc_main ^ 0xffffffff;
    crc_info = crc_info ^ 0xffffffff;

    if ( ( emem_type == EMEM_ALL ) || ( emem_type == EMEM_MAIN ) )
    {
        // CRC Main from TP
        crc_main_tp = drvDB_ReadReg ( 0x3C, 0x80 );
        crc_main_tp = ( crc_main_tp << 16 ) | drvDB_ReadReg ( 0x3C, 0x82 );

        // CRC Info from TP
        crc_info_tp = drvDB_ReadReg ( 0x3C, 0xA0 );
        crc_info_tp = ( crc_info_tp << 16 ) | drvDB_ReadReg ( 0x3C, 0xA2 );
    }
    TPD_DMESG ( "crc_main=0x%x, crc_info=0x%x, crc_main_tp=0x%x, crc_info_tp=0x%x\n",
               crc_main, crc_info, crc_main_tp, crc_info_tp );

    //drvDB_ExitDBBUS();

    update_pass = 1;
	if ( ( emem_type == EMEM_ALL ) || ( emem_type == EMEM_MAIN ) )
    {
        if ( crc_main_tp != crc_main )
            update_pass = 0;

        if ( crc_info_tp != crc_info )
            update_pass = 0;
    }

    if ( !update_pass )
    {
        TPD_DMESG ( "update FAILED\n" );
		_HalTscrHWReset();
        FwDataCnt = 0;
    	enable_irq(this_client->irq);
        return ( 0 );
    }

    TPD_DMESG ( "update OK\n" );
	_HalTscrHWReset();
    FwDataCnt = 0;
    enable_irq(this_client->irq);
    return size;
}

static ssize_t firmware_update_store ( struct device *dev,
                                       struct device_attribute *attr, const char *buf, size_t size )
{
    u8 i;
    u8 dbbus_tx_data[4];
    unsigned char dbbus_rx_data[2] = {0};
	//disable_irq(this_client->irq);
#ifdef __MSG_DMA_MODE__
	msg_dma_alloct();
#endif
    _HalTscrHWReset();

    // Erase TP Flash first
    dbbusDWIICEnterSerialDebugMode();
    dbbusDWIICStopMCU();
    dbbusDWIICIICUseBus();
    dbbusDWIICIICReshape();
    mdelay ( 300 );

    // Disable the Watchdog
    dbbus_tx_data[0] = 0x10;
    dbbus_tx_data[1] = 0x3C;
    dbbus_tx_data[2] = 0x60;
    dbbus_tx_data[3] = 0x55;
    HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG21XX, dbbus_tx_data, 4 );
    dbbus_tx_data[0] = 0x10;
    dbbus_tx_data[1] = 0x3C;
    dbbus_tx_data[2] = 0x61;
    dbbus_tx_data[3] = 0xAA;
    HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG21XX, dbbus_tx_data, 4 );
    // Stop MCU
    dbbus_tx_data[0] = 0x10;
    dbbus_tx_data[1] = 0x0F;
    dbbus_tx_data[2] = 0xE6;
    dbbus_tx_data[3] = 0x01;
    HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG21XX, dbbus_tx_data, 4 );
    /////////////////////////
    // Difference between C2 and C3
    /////////////////////////
	// c2:2133 c32:2133a(2) c33:2138
    //check id
    dbbus_tx_data[0] = 0x10;
    dbbus_tx_data[1] = 0x1E;
    dbbus_tx_data[2] = 0xCC;
    HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG21XX, dbbus_tx_data, 3 );
    HalTscrCReadI2CSeq ( FW_ADDR_MSG21XX, &dbbus_rx_data[0], 2 );
    if ( dbbus_rx_data[0] == 2 )//update for 21XXA
    {
        // check version
        dbbus_tx_data[0] = 0x10;
        dbbus_tx_data[1] = 0x3C;
        dbbus_tx_data[2] = 0xEA;
        HalTscrCDevWriteI2CSeq ( FW_ADDR_MSG21XX, dbbus_tx_data, 3 );
        HalTscrCReadI2CSeq ( FW_ADDR_MSG21XX, &dbbus_rx_data[0], 2 );
        TPD_DMESG ( "dbbus_rx version[0]=0x%x", dbbus_rx_data[0] );

        if ( dbbus_rx_data[0] == 3 )//update for 21XXA u03
		{
             firmware_update_c33 ( dev, attr, buf, size, EMEM_MAIN );
		}
        else//update for 21XXA U02
		{
             firmware_update_c32 ( dev, attr, buf, size, EMEM_ALL );
        }
    }
    else//update for 21XX
    {
         firmware_update_c2 ( dev, attr, buf, size );
    } 

	#ifdef __MSG_DMA_MODE__
	msg_dma_release();
	#endif

	return 1;
}
#endif //endif _FW_UPDATE_C3_

static ssize_t firmware_data_show(struct device *dev,
                                  struct device_attribute *attr, char *buf)
{
	TPD_DMESG("tyd-tp: firmware_data_show\n");
    return FwDataCnt;
}

static ssize_t firmware_data_store(struct device *dev,
                                   struct device_attribute *attr, const char *buf, size_t size)
{
    int i;
    TPD_DMESG("***FwDataCnt = %d ***\n", FwDataCnt);
	TPD_DMESG("tyd-tp: firmware_data_store\n");
   // for(i = 0; i < 1024; i++)
    {
        memcpy(temp[FwDataCnt], buf, 1024);
    }

    FwDataCnt++;
    return size;
}

static ssize_t firmware_clear_show(struct device *dev,
                                   struct device_attribute *attr, char *buf)
{
    unsigned short k = 0, i = 0, j = 0;
    unsigned char bWriteData[5] =
    {
        0x10, 0x03, 0, 0, 0
    };
    unsigned char RX_data[256];
    unsigned char bWriteData1 = 0x12;
    unsigned int addr = 0;
    TPD_DMESG("\n");
	TPD_DMESG("tyd-tp: firmware_clear_show\n");
	#ifdef __MSG_DMA_MODE__
	msg_dma_alloct();
	#endif
    for(k = 0; k < 94; i++)    // total  94 KB : 1 byte per R/W
    {
        addr = k * 1024;

        for(j = 0; j < 8; j++)    //128*8 cycle
        {
            bWriteData[2] = (unsigned char)((addr + j * 128) >> 16);
            bWriteData[3] = (unsigned char)((addr + j * 128) >> 8);
            bWriteData[4] = (unsigned char)(addr + j * 128);

            while((drvISP_ReadStatus() & 0x01) == 0x01)
            {
                ;    //wait until not in write operation
            }

            i2c_write_update_msg2133(bWriteData, 5);     //write read flash addr
            drvISP_Read(128, RX_data);
            i2c_write_update_msg2133(&bWriteData1, 1);    //cmd end

            for(i = 0; i < 128; i++)    //log out if verify error{
                if(RX_data[i] != 0xFF){
                    TPD_DMESG("k=%d,j=%d,i=%d===============erase not clean================", k, j, i);
                }
            }
     }
    TPD_DMESG("read finish\n");
	#ifdef __MSG_DMA_MODE__
	msg_dma_release();
	#endif
    return sprintf(buf, "%s\n", fw_version);
}

static ssize_t firmware_clear_store(struct device *dev,
                                    struct device_attribute *attr, const char *buf, size_t size)
{
    unsigned char dbbus_tx_data[4];
    unsigned char dbbus_rx_data[2] = {0};
    //msctpc_LoopDelay ( 100 );        // delay about 100ms*****
    // Enable slave's ISP ECO mode
    /*
    dbbusDWIICEnterSerialDebugMode();
    dbbusDWIICStopMCU();
    dbbusDWIICIICUseBus();
    dbbusDWIICIICReshape();*/
    #ifdef __MSG_DMA_MODE__
	msg_dma_alloct();
	#endif
    TPD_DMESG("\n");
	TPD_DMESG("tyd-tp: firmware_clear_store\n");
    dbbus_tx_data[0] = 0x10;
    dbbus_tx_data[1] = 0x08;
    dbbus_tx_data[2] = 0x0c;
    dbbus_tx_data[3] = 0x08;
    // Disable the Watchdog
    i2c_write_msg2133(dbbus_tx_data, 4);
    //Get_Chip_Version();
    //FwVersion  = 2;
    //if (FwVersion  == 2)
    {
        dbbus_tx_data[0] = 0x10;
        dbbus_tx_data[1] = 0x11;
        dbbus_tx_data[2] = 0xE2;
        dbbus_tx_data[3] = 0x00;
        i2c_write_msg2133(dbbus_tx_data, 4);
    }
    dbbus_tx_data[0] = 0x10;
    dbbus_tx_data[1] = 0x3C;
    dbbus_tx_data[2] = 0x60;
    dbbus_tx_data[3] = 0x55;
    i2c_write_msg2133(dbbus_tx_data, 4);
    dbbus_tx_data[0] = 0x10;
    dbbus_tx_data[1] = 0x3C;
    dbbus_tx_data[2] = 0x61;
    dbbus_tx_data[3] = 0xAA;
    i2c_write_msg2133(dbbus_tx_data, 4);
    //Stop MCU
    dbbus_tx_data[0] = 0x10;
    dbbus_tx_data[1] = 0x0F;
    dbbus_tx_data[2] = 0xE6;
    dbbus_tx_data[3] = 0x01;
    i2c_write_msg2133(dbbus_tx_data, 4);
    //Enable SPI Pad
    dbbus_tx_data[0] = 0x10;
    dbbus_tx_data[1] = 0x1E;
    dbbus_tx_data[2] = 0x02;
    i2c_write_msg2133(dbbus_tx_data, 3);
    i2c_read_msg2133(&dbbus_rx_data[0], 2);
    TPD_DMESG("dbbus_rx_data[0]=0x%x", dbbus_rx_data[0]);
    dbbus_tx_data[3] = (dbbus_rx_data[0] | 0x20);  //Set Bit 5
    i2c_write_msg2133(dbbus_tx_data, 4);
    dbbus_tx_data[0] = 0x10;
    dbbus_tx_data[1] = 0x1E;
    dbbus_tx_data[2] = 0x25;
    i2c_write_msg2133(dbbus_tx_data, 3);
    dbbus_rx_data[0] = 0;
    dbbus_rx_data[1] = 0;
    i2c_read_msg2133(&dbbus_rx_data[0], 2);
    TPD_DMESG("dbbus_rx_data[0]=0x%x", dbbus_rx_data[0]);
    dbbus_tx_data[3] = dbbus_rx_data[2] & 0xFC;  //Clear Bit 1,0
    i2c_write_msg2133(dbbus_tx_data, 4);
    //WP overwrite
    dbbus_tx_data[0] = 0x10;
    dbbus_tx_data[1] = 0x1E;
    dbbus_tx_data[2] = 0x0E;
    dbbus_tx_data[3] = 0x02;
    i2c_write_msg2133(dbbus_tx_data, 4);
    //set pin high
    dbbus_tx_data[0] = 0x10;
    dbbus_tx_data[1] = 0x1E;
    dbbus_tx_data[2] = 0x10;
    dbbus_tx_data[3] = 0x08;
    i2c_write_msg2133(dbbus_tx_data, 4);
    dbbusDWIICIICNotUseBus();
    dbbusDWIICNotStopMCU();
    dbbusDWIICExitSerialDebugMode();
    ///////////////////////////////////////
    // Start to load firmware
    ///////////////////////////////////////
    drvISP_EntryIspMode();
    TPD_DMESG("chip erase+\n");
    drvISP_BlockErase(0x00000);
    TPD_DMESG("chip erase-\n");
    drvISP_ExitIspMode();
	#ifdef __MSG_DMA_MODE__
	msg_dma_release();
	#endif
    return size;
}

static DEVICE_ATTR(version, 0777, firmware_version_show, firmware_version_store);
static DEVICE_ATTR(update, 0777, firmware_update_show, firmware_update_store);
static DEVICE_ATTR(data, 0777, firmware_data_show, firmware_data_store);
static DEVICE_ATTR(clear, 0777, firmware_clear_show, firmware_clear_store);

void msg2133_init_class()
{
	firmware_class = class_create(THIS_MODULE,"ms-touchscreen-msg20xx" );//client->name

	if(IS_ERR(firmware_class))
		pr_err("Failed to create class(firmware)!\n");

	firmware_cmd_dev = device_create(firmware_class,
	                                     NULL, 0, NULL, "device");//device

	if(IS_ERR(firmware_cmd_dev))
		pr_err("Failed to create device(firmware_cmd_dev)!\n");
		
	// version /sys/class/mtk-tpd/device/version
	if(device_create_file(firmware_cmd_dev, &dev_attr_version) < 0)
		pr_err("Failed to create device file(%s)!\n", dev_attr_version.attr.name);

	// update /sys/class/mtk-tpd/device/update
	if(device_create_file(firmware_cmd_dev, &dev_attr_update) < 0)
		pr_err("Failed to create device file(%s)!\n", dev_attr_update.attr.name);

	// data /sys/class/mtk-tpd/device/data
	if(device_create_file(firmware_cmd_dev, &dev_attr_data) < 0)
		pr_err("Failed to create device file(%s)!\n", dev_attr_data.attr.name);

	// clear /sys/class/mtk-tpd/device/clear
	if(device_create_file(firmware_cmd_dev, &dev_attr_clear) < 0)
		pr_err("Failed to create device file(%s)!\n", dev_attr_clear.attr.name);
}

#endif //endif __FIRMWARE_UPDATE__ MSG2133_UPDATE

//end for update firmware

 static u8 Calculate_8BitsChecksum( u8 *msg, s32 s32Length )
 {
	 s32 s32Checksum = 0;
	 s32 i;
 
	 for( i = 0 ; i < s32Length; i++ )
	 {
		 s32Checksum += msg[i];
	 }
 
	 return ( u8 )( ( -s32Checksum ) & 0xFF );
 }

 static int tpd_touchinfo(TouchScreenInfo_t *touchData)
 {

    u8 val[8] = {0};
    u8 Checksum = 0;
    u8 i;
    u32 delta_x = 0, delta_y = 0;
    u32 u32X = 0;
    u32 u32Y = 0;
    

    TPD_DEBUG("[msg2133]==tpd_touchinfo() \n");


#ifdef SWAP_X_Y
    int tempx;
    int tempy;
#endif

    /*Get Touch Raw Data*/
    i2c_master_recv( msg_i2c_client, &val[0], REPORT_PACKET_LENGTH );
    TPD_DEBUG("[tpd_touchinfo]--val[0]:%x, REPORT_PACKET_LENGTH:%x \n",val[0], REPORT_PACKET_LENGTH);
    Checksum = Calculate_8BitsChecksum( &val[0], 7 ); //calculate checksum
    TPD_DEBUG("[tpd_touchinfo]--Checksum:%x, val[7]:%x, val[0]:%x \n",Checksum, val[7], val[0]);

    if( ( Checksum == val[7] ) && ( val[0] == 0x52 ) ) //check the checksum  of packet
    {
        u32X = ( ( ( val[1] & 0xF0 ) << 4 ) | val[2] );   //parse the packet to coordinates
        u32Y = ( ( ( val[1] & 0x0F ) << 8 ) | val[3] );

        delta_x = ( ( ( val[4] & 0xF0 ) << 4 ) | val[5] );
        delta_y = ( ( ( val[4] & 0x0F ) << 8 ) | val[6] );
		TPD_DEBUG("[tpd_touchinfo]--u32X:%d, u32Y:%d, delta_x:%d, delta_y:%d \n",u32X, u32Y,delta_x, delta_y);

#ifdef SWAP_X_Y
        tempy = u32X;
        tempx = u32Y;
        u32X = tempx;
        u32Y = tempy;

        tempy = delta_x;
        tempx = delta_y;
        delta_x = tempx;
        delta_y = tempy;
#endif
#ifdef REVERSE_X
        u32X = 2047 - u32X;
        delta_x = 4095 - delta_x;
#endif
#ifdef REVERSE_Y
        u32Y = 2047 - u32Y;
        delta_y = 4095 - delta_y;
#endif

		TPD_DEBUG("[tpd_touchinfo]--u32X:%d, u32Y:%d, delta_x:%d, delta_y:%d \n",u32X, u32Y,delta_x, delta_y);

        if( ( val[1] == 0xFF ) && ( val[2] == 0xFF ) && ( val[3] == 0xFF ) && ( val[4] == 0xFF ) && ( val[6] == 0xFF ) )
        {  
            touchData->Point[0].X = 0; // final X coordinate
            touchData->Point[0].Y = 0; // final Y coordinate

            if( ( val[5] == 0x0 ) || ( val[5] == 0xFF ) )
            {
                touchData->nFingerNum = 0; //touch end
                touchData->nTouchKeyCode = 0; //TouchKeyMode
                touchData->nTouchKeyMode = 0; //TouchKeyMode
            }
            else
            {
                touchData->nTouchKeyMode = 1; //TouchKeyMode
                touchData->nTouchKeyCode = val[5]; //TouchKeyCode
                touchData->nFingerNum = 1;
            }
        }
        else
        {
            touchData->nTouchKeyMode = 0; //Touch on screen...

            if(
#ifdef REVERSE_X
                ( delta_x == 4095 )
#else
                ( delta_x == 0 )
#endif
                &&
#ifdef REVERSE_Y
                ( delta_y == 4095 )
#else
                ( delta_y == 0 )
#endif
            )
            {
                touchData->nFingerNum = 1; //one touch
                touchData->Point[0].X = ( u32X * MS_TS_MSG21XX_X_MAX ) / 2048;
                touchData->Point[0].Y = ( u32Y * MS_TS_MSG21XX_Y_MAX ) / 2048;
				TPD_DEBUG("[tpd_touchinfo]--FingerNum = 1 \n");
				TPD_DEBUG("[tpd_touchinfo]--touchData->Point[0].X = %d,  touchData->Point[0].Y = %d \n",touchData->Point[0].X,touchData->Point[0].Y);
            }
            else
            {
                u32 x2, y2;

                touchData->nFingerNum = 2; //two touch

                /* Finger 1 */
                touchData->Point[0].X = ( u32X * MS_TS_MSG21XX_X_MAX ) / 2048;
                touchData->Point[0].Y = ( u32Y * MS_TS_MSG21XX_Y_MAX ) / 2048;

                /* Finger 2 */
                if( delta_x > 2048 )    //transform the unsigh value to sign value
                {
                    delta_x -= 4096;
                }
                if( delta_y > 2048 )
                {
                    delta_y -= 4096;
                }

                x2 = ( u32 )( u32X + delta_x );
                y2 = ( u32 )( u32Y + delta_y );

                touchData->Point[1].X = ( x2 * MS_TS_MSG21XX_X_MAX ) / 2048;
                touchData->Point[1].Y = ( y2 * MS_TS_MSG21XX_Y_MAX ) / 2048;
				TPD_DEBUG("[tpd_touchinfo]--FingerNum = 2 \n");
            }
        }

       
    }
    else
    {
        //DBG("Packet error 0x%x, 0x%x, 0x%x", val[0], val[1], val[2]);
        //DBG("             0x%x, 0x%x, 0x%x", val[3], val[4], val[5]);
        //DBG("             0x%x, 0x%x, 0x%x", val[6], val[7], Checksum);
        TPD_DEBUG("err status in tp\n" );
    }

    //enable_irq( msg21xx_irq );
  ///
	 return true;

 };
 
 static  void tpd_down(int x, int y, int p) {
 	
	 // input_report_abs(tpd->dev, ABS_PRESSURE, p);
	  input_report_key(tpd->dev, BTN_TOUCH, 1);
	  input_report_abs(tpd->dev, ABS_MT_TOUCH_MAJOR, 25);
	  input_report_abs(tpd->dev, ABS_MT_POSITION_X, x);
	  input_report_abs(tpd->dev, ABS_MT_POSITION_Y, y);

	  /* track id Start 0 */
		//input_report_abs(tpd->dev, ABS_MT_TRACKING_ID, p); 
	  input_mt_sync(tpd->dev);
	  if (FACTORY_BOOT == get_boot_mode()|| RECOVERY_BOOT == get_boot_mode())
	  {   
		tpd_button(x, y, 1);  
	  }
	  if(y > TPD_RES_Y) //virtual key debounce to avoid android ANR issue
	  {
		  msleep(50);
		  TPD_DEBUG("D virtual key \n");
	  }
	  TPD_EM_PRINT(x, y, x, y, p-1, 1);
  }
  
 static  void tpd_up(int x, int y,int *count) {

	  input_report_key(tpd->dev, BTN_TOUCH, 0);
	  input_mt_sync(tpd->dev);
	  TPD_EM_PRINT(x, y, x, y, 0, 0);
		  
	  if(FACTORY_BOOT == get_boot_mode()|| RECOVERY_BOOT == get_boot_mode())
	  {   
	  	 TPD_DEBUG("[msg2133]--tpd_up-BOOT MODE--X:%d, Y:%d; \n", x, y);
		 tpd_button(x, y, 0); 
	  } 		  
 
  }

 static int touch_event_handler(void *unused)
 {
  
    TouchScreenInfo_t touchData;
	u8 touchkeycode = 0;
	static u32 preKeyStatus = 0;
	int i=0;
 
    TPD_DEBUG("[msg2133]touch_event_handler() do while \n");

	touchData.nFingerNum = 0;
	TPD_DEBUG("[msg2133]touch_event_handler() do while \n");
	 
	if (tpd_touchinfo(&touchData)) 
	{
	 
		TPD_DEBUG("[msg2133]--KeyMode:%d, KeyCode:%d, FingerNum =%d \n", touchData.nTouchKeyMode, touchData.nTouchKeyCode, touchData.nFingerNum );
	 
		//key...
		if( touchData.nTouchKeyMode )
		{
	    	//key mode change virtual key mode
			touchData.nFingerNum = 1;
			if( touchData.nTouchKeyCode == 1 )
			{
				//touchkeycode = KEY_MENU;
				touchData.Point[0].X = 80;
				touchData.Point[0].Y = 850;
			}
			if( touchData.nTouchKeyCode == 2 )
			{
				//touchkeycode = KEY_HOMEPAGE ;
				touchData.Point[0].X = 240;
				touchData.Point[0].Y = 850;

			}
			if( touchData.nTouchKeyCode == 4 )
			{
				//touchkeycode = KEY_BACK;
				touchData.Point[0].X = 400;
				touchData.Point[0].Y = 850;

			}
			if( touchData.nTouchKeyCode == 8 )
			{
				//touchkeycode = KEY_SEARCH;
				//touchData.Point[0].X = 560;
				//touchData.Point[0].Y = 850;

			}
					
		}
				//report
		{
	 
			if( ( touchData.nFingerNum ) == 0 ) //touch end
			{
				TPD_DEBUG("------UP------ \n");
				TPD_DEBUG("[msg2133]---X:%d, Y:%d; \n", touchData.Point[0].X, touchData.Point[0].Y);
				tpd_up(touchData.Point[0].X, touchData.Point[0].Y, 0);
				input_sync( tpd->dev );
			}
			else //touch on screen
			{
	 
				for( i = 0; i < ( (int)touchData.nFingerNum ); i++ )
				{
				    TPD_DEBUG("------DOWN------ \n");
					tpd_down(touchData.Point[i].X, touchData.Point[i].Y, 1);
					TPD_DEBUG("[msg2133]---X:%d, Y:%d; i=%d \n", touchData.Point[i].X, touchData.Point[i].Y, i);
				}
	 
				input_sync( tpd->dev );
			}
		}//end if(touchData->nTouchKeyMode)
	 
			}

     mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM); 
	 return 0;
 }
 
 static int tpd_detect (struct i2c_client *client, struct i2c_board_info *info) 
 {
	 strcpy(info->type, TPD_DEVICE);	
	  return 0;
 }
 
 static void tpd_eint_interrupt_handler(void)
 {
	 mt_eint_mask(CUST_EINT_TOUCH_PANEL_NUM);
	 schedule_work( &msg21xx_wq );
 }

 static int __devinit tpd_probe(struct i2c_client *client, const struct i2c_device_id *id)
 {	 
 
	int retval = TPD_OK;
	char data;
	u8 report_rate=0;
	int err=0;
	int reset_count = 0;

	msg_i2c_client = client;
	//msg21xx_i2c_client = client;
	this_client = client;
	/*reset I2C clock*/
    //i2c_client->timing = 0;
    
   INIT_WORK( &msg21xx_wq, touch_event_handler );
//power on, need confirm with SA
#ifdef TPD_POWER_SOURCE_CUSTOM
	hwPowerOn(TPD_POWER_SOURCE_CUSTOM, VOL_2800, "TP");
#else
	hwPowerOn(MT65XX_POWER_LDO_VGP2, VOL_2800, "TP");
#endif
#ifdef TPD_POWER_SOURCE_1800
	hwPowerOn(TPD_POWER_SOURCE_1800, VOL_1800, "TP");
#endif 


#ifdef TPD_CLOSE_POWER_IN_SLEEP	 
	hwPowerDown(TPD_POWER_SOURCE,"TP");
	hwPowerOn(TPD_POWER_SOURCE,VOL_2800,"TP");
	msleep(100);
#else

	mt_set_gpio_mode(GPIO_CTP_RST_PIN, GPIO_CTP_RST_PIN_M_GPIO);
    mt_set_gpio_dir(GPIO_CTP_RST_PIN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ONE);
	msleep(10);
	mt_set_gpio_mode(GPIO_CTP_RST_PIN, GPIO_CTP_RST_PIN_M_GPIO);
    mt_set_gpio_dir(GPIO_CTP_RST_PIN, GPIO_DIR_OUT);
    mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ZERO);  
	msleep(50);
	TPD_DMESG(" msg2133 reset\n");
	mt_set_gpio_mode(GPIO_CTP_RST_PIN, GPIO_CTP_RST_PIN_M_GPIO);
    mt_set_gpio_dir(GPIO_CTP_RST_PIN, GPIO_DIR_OUT);
    mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ONE);
	msleep(50);
	
#endif
	

	mt_set_gpio_mode(GPIO_CTP_EINT_PIN, GPIO_CTP_EINT_PIN_M_EINT);
    mt_set_gpio_dir(GPIO_CTP_EINT_PIN, GPIO_DIR_IN);
   	mt_set_gpio_pull_enable(GPIO_CTP_EINT_PIN, GPIO_PULL_ENABLE);
    mt_set_gpio_pull_select(GPIO_CTP_EINT_PIN, GPIO_PULL_DOWN);
		

    msleep(10);

	//mt65xx_eint_set_sens(CUST_EINT_TOUCH_PANEL_NUM, CUST_EINT_TOUCH_PANEL_SENSITIVE);
	//mt65xx_eint_set_hw_debounce(CUST_EINT_TOUCH_PANEL_NUM, CUST_EINT_TOUCH_PANEL_DEBOUNCE_CN);
	mt_eint_registration(CUST_EINT_TOUCH_PANEL_NUM, CUST_EINT_TOUCH_PANEL_TYPE, tpd_eint_interrupt_handler, 1);
	msleep(50);
	mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);
	msleep(200);
/*
	 char dbbus_tx_data[3];
	 char dbbus_rx_data[2]; 
	 dbbusDWIICEnterSerialDebugMode();
	 dbbusDWIICStopMCU();
	 dbbusDWIICIICUseBus();
	 dbbusDWIICIICReshape();
	 dbbus_tx_data[0] = 0x10;
	 dbbus_tx_data[1] = 0x3A;
	 dbbus_tx_data[2] = 0x08; 
	 dbbus_rx_data[0]=0xFF;
	 dbbus_rx_data[1]=0xFF; 
	 HalTscrCDevWriteI2CSeq(0XC4, &dbbus_tx_data[0], 3);
	 HalTscrCReadI2CSeq(0XC4, &dbbus_rx_data[0], 2);
	 dbbusDWIICIICNotUseBus();
	 dbbusDWIICNotStopMCU();
	 dbbusDWIICExitSerialDebugMode();

	 if(dbbus_rx_data[0]==0||dbbus_rx_data[0]==0xff)
	 {
		 return -1;//	
	 }

	*/
    if((i2c_smbus_read_i2c_block_data(msg_i2c_client, 0x00, 1, &data))< 0)
	{
		TPD_DMESG("I2C transfer error, line: %d\n", __LINE__);
		return -1; 
	}
	
    tpd_load_status = 1;

	TPD_DMESG("msg2133 Touch Panel Device Probe %s\n", (retval < TPD_OK) ? "FAIL" : "PASS");
	
	TPD_DEBUG("msg2133--frameware upgrade \n");

    /*frameware upgrade*/	
#ifdef __FIRMWARE_UPDATE__
		firmware_class = class_create( THIS_MODULE, "ms-touchscreen-msg20xx" );
		if( IS_ERR( firmware_class ) )
			pr_err( "Failed to create class(firmware)!\n" );
		firmware_cmd_dev = device_create( firmware_class,
										  NULL, 0, NULL, "device" );
		if( IS_ERR( firmware_cmd_dev ) )
			pr_err( "Failed to create device(firmware_cmd_dev)!\n" );
	
		// version
		if( device_create_file( firmware_cmd_dev, &dev_attr_version ) < 0 )
			pr_err( "Failed to create device file(%s)!\n", dev_attr_version.attr.name );
		// update
		if( device_create_file( firmware_cmd_dev, &dev_attr_update ) < 0 )
			pr_err( "Failed to create device file(%s)!\n", dev_attr_update.attr.name );
		// data
		if( device_create_file( firmware_cmd_dev, &dev_attr_data ) < 0 )
			pr_err( "Failed to create device file(%s)!\n", dev_attr_data.attr.name );
		// clear
		if( device_create_file( firmware_cmd_dev, &dev_attr_clear ) < 0 )
			pr_err( "Failed to create device file(%s)!\n", dev_attr_clear.attr.name );
	
		dev_set_drvdata( firmware_cmd_dev, NULL );
#endif

   return 0;
   
 }

 static int __devexit tpd_remove(struct i2c_client *client)
 
 {
   
	 TPD_DEBUG("TPD removed\n");
 
   return 0;
 }
 
 
 static int tpd_local_init(void)
 {

 
  	TPD_DMESG("Mstar msg2133 I2C Touchscreen Driver (Built %s @ %s)\n", __DATE__, __TIME__);
 
 
    if(i2c_add_driver(&tpd_i2c_driver)!=0)
   	{
  		TPD_DMESG("msg2133 unable to add i2c driver.\n");
      	return -1;
    }
    if(tpd_load_status == 0) 
    {
    	TPD_DMESG("msg2133 add error touch panel driver.\n");
    	i2c_del_driver(&tpd_i2c_driver);
    	return -1;
    }
	
#ifdef TPD_HAVE_BUTTON     
    tpd_button_setting(TPD_KEY_COUNT, tpd_keys_local, tpd_keys_dim_local);// initialize tpd button data
#endif   
  
//#if (defined(TPD_WARP_START) && defined(TPD_WARP_END))    
//WARP CHECK IS NEED --XB.PANG
//#endif 

	TPD_DMESG("end %s, %d\n", __FUNCTION__, __LINE__);  
		
    return 0; 
 }

 static void tpd_resume( struct early_suspend *h )
 {
 
   TPD_DMESG("TPD wake up\n");
#ifdef TPD_CLOSE_POWER_IN_SLEEP	
	hwPowerOn(TPD_POWER_SOURCE,VOL_2800,"TP");
#endif
	msleep(100);
	mt_set_gpio_mode(GPIO_CTP_RST_PIN, GPIO_CTP_RST_PIN_M_GPIO);
    mt_set_gpio_dir(GPIO_CTP_RST_PIN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ONE);
	mt_set_gpio_mode(GPIO_CTP_RST_PIN, GPIO_CTP_RST_PIN_M_GPIO);
    mt_set_gpio_dir(GPIO_CTP_RST_PIN, GPIO_DIR_OUT);
    mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ZERO);  
	msleep(50);
	TPD_DMESG(" msg2133 reset\n");
	mt_set_gpio_mode(GPIO_CTP_RST_PIN, GPIO_CTP_RST_PIN_M_GPIO);
    mt_set_gpio_dir(GPIO_CTP_RST_PIN, GPIO_DIR_OUT);
    mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ONE);
	msleep(200);
	mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);
	TPD_DMESG("TPD wake up done\n");
	
 }

 static void tpd_suspend( struct early_suspend *h )
 {
 	
	TPD_DMESG("TPD enter sleep\n");
	mt_eint_mask(CUST_EINT_TOUCH_PANEL_NUM);
	
	mt_set_gpio_mode(GPIO_CTP_RST_PIN, GPIO_CTP_RST_PIN_M_GPIO);
    mt_set_gpio_dir(GPIO_CTP_RST_PIN, GPIO_DIR_OUT);
    mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ZERO);  
	 
#ifdef TPD_CLOSE_POWER_IN_SLEEP	
	hwPowerDown(TPD_POWER_SOURCE,"TP");
#else
	//TP enter sleep mode----XB.PANG NEED CHECK
	//if have sleep mode
#endif
    TPD_DMESG("TPD enter sleep done\n");
 } 


 static struct tpd_driver_t tpd_device_driver = {
		 .tpd_device_name = "msg2133",
		 .tpd_local_init = tpd_local_init,
		 .suspend = tpd_suspend,
		 .resume = tpd_resume,
#ifdef TPD_HAVE_BUTTON
		 .tpd_have_button = 1,
#else
		 .tpd_have_button = 0,
#endif		
 };
 /* called when loaded into kernel */
 static int __init tpd_driver_init(void) {
	 TPD_DEBUG("MediaTek MSG2133 touch panel driver init\n");
	   i2c_register_board_info(0, &msg2133_i2c_tpd, 1);
		 if(tpd_driver_add(&tpd_device_driver) < 0)
			 TPD_DMESG("add MSG2133 driver failed\n");
	 return 0;
 }
 
 /* should never be called */
 static void __exit tpd_driver_exit(void) {
	 TPD_DMESG("MediaTek MSG2133 touch panel driver exit\n");
	 tpd_driver_remove(&tpd_device_driver);
 }
 
 module_init(tpd_driver_init);
 module_exit(tpd_driver_exit);


