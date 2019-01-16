#define SII_HAL_LINUX_I2C_C
#include <linux/i2c.h>
#include "sii_hal.h"
#include "hdmi_cust.h"
#include "sii_hal_priv.h"
#include "si_mhl_tx_api.h"

#include <mach/hardware.h>
/* #include <linux/mx51_pins.h> */
#include <mach/mt_gpio.h>
#include <cust_gpio_usage.h>
#include <cust_eint.h>


/******************************software I2C demo code**********************************/
/* ------------------------------------------------------------------------------ */
#if 0
/******************************software I2C demo code**********************************/
/* ------------------------------------------------------------------------------ */
#define I2C_DELAY_VALUE	3
/* ------------------------------------------------------------------------------ */
/* Local macros to simplify bit operations */
/* ------------------------------------------------------------------------------ */

#define GPIO_SDA	GPIO_HDMI_I2C_SDA
#define GPIO_SCL	GPIO_HDMI_I2C_SCL

uint8_t GET_SDA(void)
{
	unsigned int a;
	/* mt_set_gpio_dir(GPIO_SDA, GPIO_DIR_IN); */
	a = mt_get_gpio_in(GPIO_SDA);
	return a;
}

uint8_t GET_SCL(void)
{
	unsigned int a;

	/* mt_set_gpio_dir(GPIO_SCL, GPIO_DIR_IN); */
	a = mt_get_gpio_in(GPIO_SCL);
	return a;
}

void SET_SDA(void)
{
	mt_set_gpio_out(GPIO_SDA, GPIO_OUT_ONE);
	udelay(I2C_DELAY_VALUE);
}

void SET_SCL(void)
{
	mt_set_gpio_out(GPIO_SCL, GPIO_OUT_ONE);
	udelay(I2C_DELAY_VALUE);
}

void CLEAR_SDA(void)
{
	mt_set_gpio_out(GPIO_SDA, GPIO_OUT_ZERO);
	udelay(I2C_DELAY_VALUE);
}

void CLEAR_SCL(void)
{
	mt_set_gpio_out(GPIO_SCL, GPIO_OUT_ZERO);
	udelay(I2C_DELAY_VALUE);
}


/* ------------------------------------------------------------------------------ */
/* Local constants (function parameters) */
/* ------------------------------------------------------------------------------ */
#undef READ
#define READ   1

#undef WRITE
#define WRITE  0

#define LAST_BYTE      1
#define NOT_LAST_BYTE  0

#define IIC_OK			 0
#define IIC_NOACK		 1
#define IIC_SCL_TIMEOUT  2

/* ------------------------------------------------------------------------------ */
/* Function: SetSCLHigh */
/* Description: */
/* ------------------------------------------------------------------------------ */
static uint8_t SetSCLHigh(void)
{
	volatile uint8_t x = 0;	/* delay variable */
	uint32_t timeout = 0;

	/* set SCL high, and wait for it to go high in case slave is clock stretching */
	SET_SCL();
	x++;
	x++;
	x++;
	x++;

	while (!GET_SCL()) {
		/* do nothing - just wait */
		/* udelay(2); */
		timeout++;
		if (timeout > 100000)	/* about 1s is enough */
		{
			printk("\n ************IIC SCL low timeout...\n");
			return IIC_SCL_TIMEOUT;
		}
	}

	return IIC_OK;
}

/* ------------------------------------------------------------------------------ */
/* Function: SendByte */
/* Description: */
/* ------------------------------------------------------------------------------ */
static uint8_t I2CSendByte(uint8_t abyte)
{
	uint8_t error = 0;
	uint8_t i;

	for (i = 0; i < 8; i++) {
		if (abyte & 0x80)
			SET_SDA();	/* send each bit, MSB first */
		else
			CLEAR_SDA();

		if (SetSCLHigh())	/* do clock cycle */
			return IIC_SCL_TIMEOUT;

		CLEAR_SCL();

		abyte <<= 1;	/* shift over to get next bit */
	}

	SET_SDA();		/* listen for ACK */
#if 1
	if (SetSCLHigh())
		return IIC_SCL_TIMEOUT;

	if (GET_SDA())		/* error if no ACK */
	{
		printk("IIC_NOACK\n");
		error = IIC_NOACK;
	}
#endif
	CLEAR_SCL();

	return (error);		/* return 0 for no ACK, 1 for ACK received */
}


/* ------------------------------------------------------------------------------ */
/* Function: SendAddr */
/* Description: */
/* ------------------------------------------------------------------------------ */
static uint8_t I2CSendAddr(uint8_t addr, uint8_t read)
{
	volatile uint8_t x = 0;	/* delay variable */

	/* generate START condition */
	SET_SCL();
#if 0
	if (GET_SCL)
		TX_DEBUG_PRINT(("GET_SCL is high rightly\n"));
	else
		TX_DEBUG_PRINT(("GET_SCL is low wrong\n"));
#endif
	x++;			/* short delay to keep setup times in spec */
	CLEAR_SDA();
#if 0
	if (!GET_SDA)
		TX_DEBUG_PRINT(("GET_SDA is low rightly\n"));
	else
		TX_DEBUG_PRINT(("GET_SDA is high wrong\n"));
#endif
	x++;
	x++;
	x++;
	CLEAR_SCL();
#if 0
	if (!GET_SCL)
		TX_DEBUG_PRINT(("GET_SCL is low rightly\n"));
	else
		TX_DEBUG_PRINT(("GET_SCL is high wrong\n"));
#endif
	x++;

	return (I2CSendByte(addr | read));	/* send address uint8_t with read/write bit */
}

static uint8_t I2CGetByte(uint8_t lastbyte, uint8_t *Data)
{
	uint8_t abyte = 0;
	uint8_t i;

	for (i = 0; i < 8; i++)	/* get each bit, MSB first */
	{
		if (SetSCLHigh())
			return IIC_SCL_TIMEOUT;

		abyte <<= 1;	/* shift result over to make room for next bit */

		if (GET_SDA())
			abyte++;	/* same as 'abyte |= 1', only faster */

		CLEAR_SCL();
	}

	if (lastbyte)
		SET_SDA();	/* do not ACK last uint8_t read */
	else
		CLEAR_SDA();

	if (SetSCLHigh())
		return IIC_SCL_TIMEOUT;

	CLEAR_SCL();
	SET_SDA();

	*Data = abyte;

	return IIC_OK;
}

/* ------------------------------------------------------------------------------ */
/* Function: SendStop */
/* Description: */
/* ------------------------------------------------------------------------------ */
static uint8_t I2CSendStop(void)
{
	CLEAR_SDA();
	if (SetSCLHigh())
		return IIC_SCL_TIMEOUT;

	SET_SDA();
	return IIC_OK;
}

/* ------------------------------------------------------------------- */
uint8_t I2C_ReadByte(uint8_t SlaveAddr, uint8_t RegAddr)
{
#if 0
	return sccb_read(SlaveAddr, RegAddr);
#else
	uint8_t Data = 0;
	I2CSendAddr(SlaveAddr, WRITE);
	I2CSendByte(RegAddr);
	I2CSendAddr(SlaveAddr, READ);
	I2CGetByte(LAST_BYTE, &Data);
	I2CSendStop();

	return Data;
#endif
}

/* ------------------------------------------------------------------- */
void I2C_WriteByte(uint8_t SlaveAddr, uint8_t RegAddr, uint8_t Data)
{
#if 0
	sccb_write(SlaveAddr, RegAddr, Data);

#else

	I2CSendAddr(SlaveAddr, WRITE);
	I2CSendByte(RegAddr);
	I2CSendByte(Data);
	I2CSendStop();
#endif
}

/* ------------------------------------------------------------------------------ */
/* Function Name: I2C_ReadBlock */
/* Function Description: Reads block of data from I2C Device */
/* ------------------------------------------------------------------------------ */
uint8_t I2C_ReadBlock(uint8_t SlaveAddr, uint8_t RegAddr, uint8_t *Data, uint8_t NBytes)
{
	uint8_t i, bState;

	bState = I2CSendAddr(SlaveAddr, WRITE);
	if (bState) {
		I2CSendStop();
		return IIC_NOACK;
	}

	bState = I2CSendByte(RegAddr);
	if (bState) {
		I2CSendStop();
		return IIC_NOACK;
	}

	bState = I2CSendAddr(SlaveAddr, READ);
	if (bState) {
		I2CSendStop();
		return IIC_NOACK;
	}

	for (i = 0; i < NBytes - 1; i++) {
		if (I2CGetByte(NOT_LAST_BYTE, &Data[i])) {
			I2CSendStop();
			return IIC_SCL_TIMEOUT;
		}
	}
	if (I2CGetByte(LAST_BYTE, &Data[i])) {
		I2CSendStop();
		return IIC_SCL_TIMEOUT;
	}
	I2CSendStop();

	return IIC_OK;
}

/* ------------------------------------------------------------------------------ */
/* Function Name:  I2C_WriteBlock */
/* Function Description: Writes block of data from I2C Device */
/* ------------------------------------------------------------------------------ */
uint8_t I2C_WriteBlock(uint8_t SlaveAddr, uint8_t RegAddr, uint8_t NBytes, uint8_t *Data)
{
	uint8_t i, bState;

	bState = I2CSendAddr(SlaveAddr, WRITE);
	if (bState) {
		I2CSendStop();
		return IIC_NOACK;
	}

	bState = I2CSendByte(RegAddr);
	if (bState) {
		I2CSendStop();
		return IIC_NOACK;
	}

	for (i = 0; i < NBytes; i++)
		I2CSendByte(Data[i]);

	I2CSendStop();

	return IIC_OK;

}

/* ------------------------------------------------------------------------------ */
/* Function Name:  I2C_ReadSegmentBlockEDID */
/* Function Description: Reads segment block of EDID from HDMI Downstream Device */
/* ------------------------------------------------------------------------------ */
uint8_t I2C_ReadSegmentBlockEDID(uint8_t SlaveAddr, uint8_t Segment, uint8_t Offset,
				 uint8_t *Buffer, uint8_t Length)
{
	uint8_t i, bState;
	TX_DEBUG_PRINT(("I2C_ReadSegmentBlockEDID%%%%%%%%%%%%%%%%%%%%%%%%%%%%\n"));

	bState = I2CSendAddr(0x60, WRITE);
	if (bState) {
		I2CSendStop();
		return IIC_NOACK;
	}

	bState = I2CSendByte(Segment);
	if (bState) {
		I2CSendStop();
		return IIC_NOACK;
	}

	bState = I2CSendAddr(SlaveAddr, WRITE);
	if (bState) {
		I2CSendStop();
		return IIC_NOACK;
	}

	bState = I2CSendByte(Offset);
	if (bState) {
		I2CSendStop();
		return IIC_NOACK;
	}

	bState = I2CSendAddr(SlaveAddr, READ);
	if (bState) {
		I2CSendStop();
		return IIC_NOACK;
	}

	for (i = 0; i < Length - 1; i++)
		if (I2CGetByte(NOT_LAST_BYTE, &Buffer[i]))
			return IIC_SCL_TIMEOUT;

	if (I2CGetByte(LAST_BYTE, &Buffer[i]))
		return IIC_SCL_TIMEOUT;

	I2CSendStop();

	return IIC_OK;
}

/*
void SiiRegEdidReadBlock( SiiReg_t segmentAddr, SiiReg_t virtualAddr, uint8_t *pBuffer, uint16_t count )
{
	uint8_t             Seg_regOffset;
    uint8_t             regOffset = (uint8_t)virtualAddr;

    if ((segmentAddr & 0xFF) != 0)
    {
	Seg_regOffset = (uint8_t)segmentAddr;
    }
    regOffset = (uint8_t)virtualAddr;

	I2C_ReadSegmentBlockEDID (0xA0, Seg_regOffset, regOffset, pBuffer, count);
}
*/
/******************************software I2C demo code**********************************/

#endif

/* void SiiRegEdidReadBlock( SiiReg_t segmentAddr, SiiReg_t virtualAddr, uint8_t *pBuffer, uint16_t count ) */

uint8_t I2C_ReadSegmentBlockEDID(uint8_t SlaveAddr, uint8_t Segment, uint8_t Offset,
				 uint8_t *Buffer, uint8_t Length)
{
	return 0;
}

static int32_t MhlI2cProbe(struct i2c_client *client, const struct i2c_device_id *id);
static int32_t MhlI2cRemove(struct i2c_client *client);


static int32_t MhlI2cProbe(struct i2c_client *client, const struct i2c_device_id *id)
{
	printk("%s, client=0x%08x\n", __func__, (unsigned int)client);
	gMhlDevice.pI2cClient = client;
	return 0;
}

static int32_t MhlI2cRemove(struct i2c_client *client)
{
	gMhlDevice.pI2cClient = NULL;
	return 0;
}

halReturn_t I2cAccessCheck(void)
{
	halReturn_t retStatus;
	retStatus = HalInitCheck();
	if (retStatus != HAL_RET_SUCCESS) {
		return retStatus;
	}
	if (gMhlDevice.pI2cClient == NULL) {
		printk("I2C device not currently open\n");
		/* retStatus = HAL_RET_DEVICE_NOT_OPEN; */
		retStatus = HAL_RET_SUCCESS;
	}
	return retStatus;
}

struct i2c_device_id gMhlI2cIdTable[] = {
	{
	 "Sil_MHL", 0}
};


#if 0 ///SII_I2C_ADDR == (0x76)  default is 0x72
static struct i2c_board_info i2c_mhl __initdata = {
	.type = "Sil_MHL",
	.addr = 0x3B,
	.irq = 8,
};
#else ///default is 0x72
static struct i2c_board_info __initdata i2c_mhl = {
	.type = "Sil_MHL",
	.addr = 0x39,
	.irq = 8,
};
#endif

struct i2c_driver mhl_i2c_driver = {
	.probe = MhlI2cProbe,
	.remove = MhlI2cRemove,
	/* .detect = hdmi_i2c_detect, */
	.driver = {.name = "Sil_MHL",},
	.id_table = gMhlI2cIdTable,
	/* .address_list = (const unsigned short*) forces, */
};

halReturn_t HalOpenI2cDevice(char const *DeviceName, char const *DriverName)
{
	halReturn_t retStatus;
	int32_t retVal;

	retStatus = HalInitCheck();
	if (retStatus != HAL_RET_SUCCESS) {
		return retStatus;
	}
	retVal = strnlen(DeviceName, I2C_NAME_SIZE);
	if (retVal >= I2C_NAME_SIZE) {
		printk("I2c device name too long!\n");
		return HAL_RET_PARAMETER_ERROR;
	}

    if( get_hdmi_i2c_addr()== 0x76)
        i2c_mhl.addr = 0x3B;

	i2c_register_board_info(get_hdmi_i2c_channel(), &i2c_mhl, 1);

	memcpy(gMhlI2cIdTable[0].name, DeviceName, retVal);
	gMhlI2cIdTable[0].name[retVal] = 0;
	gMhlI2cIdTable[0].driver_data = 0;

	gMhlDevice.driver.driver.name = "Sil_MHL";
	gMhlDevice.driver.id_table = gMhlI2cIdTable;
	gMhlDevice.driver.probe = MhlI2cProbe;
	gMhlDevice.driver.remove = MhlI2cRemove;

	/* printk("gMhlDevice.driver.driver.name=%s\n", gMhlDevice.driver.driver.name); */
	/* printk("gMhlI2cIdTable.name=%s\n", gMhlI2cIdTable[0].name); */
	retVal = i2c_add_driver(&mhl_i2c_driver);
	/* printk("gMhlDevice.pI2cClient =%p\n", gMhlDevice.pI2cClient); */
	if (retVal != 0) {
		printk("I2C driver add failed, retVal=%d\n", retVal);
		retStatus = HAL_RET_FAILURE;
	} else {
#if 0
		if (gMhlDevice.pI2cClient == NULL) {
			i2c_del_driver(&gMhlDevice.driver);
			printk("I2C driver add failed, retVal=%d\n", retVal);
			retStatus = HAL_RET_NO_DEVICE;
		} else
#endif
		{
			retStatus = HAL_RET_SUCCESS;
		}
	}

	/* printk("GPIO_SCL=%d, GPIO_SDA=%d\n", GPIO_SCL, GPIO_SDA); */

	return retStatus;
}

halReturn_t HalCloseI2cDevice(void)
{
	halReturn_t retStatus;
	retStatus = HalInitCheck();
	if (retStatus != HAL_RET_SUCCESS) {
		return retStatus;
	}
	if (gMhlDevice.pI2cClient == NULL) {
		printk("I2C device not currently open\n");
		retStatus = HAL_RET_DEVICE_NOT_OPEN;
	} else {
		i2c_del_driver(&gMhlDevice.driver);
		gMhlDevice.pI2cClient = NULL;
		retStatus = HAL_RET_SUCCESS;
	}
	return retStatus;
}

#define USE_DEFAULT_I2C_CODE  0
static int mhl_i2c_status;	/* bit0: read, bit1: write, bit2: block_read, bit3:block_write */

uint8_t I2C_ReadByte(uint8_t deviceID, uint8_t offset)
{
	/* printk("hdmi enter I2C_ReadByte(0x%02x, 0x%02x)\n", */
	/* deviceID, offset); */

	uint8_t accessI2cAddr;
	union i2c_smbus_data data;
	int32_t status;
	u32 client_main_addr;
	char buf = offset;
	if (I2cAccessCheck() != HAL_RET_SUCCESS) {
		return 0xFF;
	}
	mhl_i2c_status |= 1;
	if ((mhl_i2c_status & 0xfe) != 0)
		printk("MHL R mhl_i2c_status(0x%02x)\n", mhl_i2c_status);

	accessI2cAddr = deviceID >> 1;

	/* backup addr */
	client_main_addr = gMhlDevice.pI2cClient->addr;
	gMhlDevice.pI2cClient->addr = (accessI2cAddr & I2C_MASK_FLAG) | I2C_WR_FLAG;

#if 1
#ifdef MHL_SET_EXT_GPIO
	gMhlDevice.pI2cClient->ext_flag |= I2C_DIRECTION_FLAG;
#endif
	status = i2c_master_send(gMhlDevice.pI2cClient, &buf, 0x101);

	/* /gMhlDevice.pI2cClient->ext_flag |= I2C_DIRECTION_FLAG; */
	/* /status = i2c_master_recv(gMhlDevice.pI2cClient, (char*)&buf, 1); */
	data.byte = buf;

	if (status < 0) {
		if (deviceID != 0xfc) {
			printk("I2C_ReadByte(0x%02x, 0x%02x), i2c_transfer error: %d\n",
			       deviceID, offset, status);
		}
		data.byte = 0xFF;
	}
#else
	char buf = offset;
	u32 ext_flag = I2C_DIRECTION_FLAG | I2C_WR_FLAG;
	status = mt_i2c_master_send(gMhlDevice.pI2cClient, &buf, (1 << 8) | 1, ext_flag);

	data.byte = buf;

	if (status < 0) {
		if (deviceID != 0xfc) {
			printk("I2C_ReadByte(0x%02x, 0x%02x), i2c_transfer error: %d\n",
			       deviceID, offset, status);
		}
		data.byte = 0xFF;
	}
#endif

	mhl_i2c_status &= 0xfe;
	gMhlDevice.pI2cClient->addr = client_main_addr;
	return data.byte;
}

void I2C_WriteByte(uint8_t deviceID, uint8_t offset, uint8_t value)
{
	/* printk("hdmi enter I2C_WriteByte(0x%02x, 0x%02x, 0x%02x)\n", */
	/* deviceID, offset, value); */

	uint8_t accessI2cAddr;
#if USE_DEFAULT_I2C_CODE
	union i2c_smbus_data data;
#endif
	int32_t status;
	u32 client_main_addr;
	u8 buf[2];
	if (I2cAccessCheck() != HAL_RET_SUCCESS) {
		return;
	}
	accessI2cAddr = deviceID >> 1;	/* ? */
	mhl_i2c_status |= 2;
	if ((mhl_i2c_status & 0xfd) != 0)
		printk("MHL W mhl_i2c_status(0x%02x)\n", mhl_i2c_status);

	/* backup addr */
	client_main_addr = gMhlDevice.pI2cClient->addr;
	gMhlDevice.pI2cClient->addr = accessI2cAddr;

#if USE_DEFAULT_I2C_CODE
	data.byte = value;
	status = i2c_smbus_xfer(gMhlDevice.pI2cClient->adapter, accessI2cAddr,
				0, I2C_SMBUS_WRITE, offset, I2C_SMBUS_BYTE_DATA, &data);
#else
#ifdef MHL_SET_EXT_GPIO
	gMhlDevice.pI2cClient->ext_flag |= I2C_DIRECTION_FLAG;
#endif
	buf[0] = offset;
	buf[1] = value;
	status = i2c_master_send(gMhlDevice.pI2cClient, (const char *)buf, 2 /*sizeof(buf) */);
#endif

	if (status < 0) {
		printk("I2C_WriteByte(0x%02x, 0x%02x, 0x%02x), i2c_transfer error: %d\n",
		       deviceID, offset, value, status);
	}
	mhl_i2c_status &= 0xfd;
	/* restore default client address */
	gMhlDevice.pI2cClient->addr = client_main_addr;
}

uint8_t I2C_ReadBlock(uint8_t deviceID, uint8_t offset, uint8_t *buf, uint8_t len)
{
	/* printk("hdmi enter %s (0x%02x, 0x%02x, 0x%02x)\n", __func__, deviceID, offset, len); */
	int i;
	uint8_t accessI2cAddr;
#if USE_DEFAULT_I2C_CODE
	union i2c_smbus_data data;
#endif
	int32_t status;
	u32 client_main_addr;
	if (I2cAccessCheck() != HAL_RET_SUCCESS) {
		return 0x00;
	}
	accessI2cAddr = deviceID >> 1;

	/* backup addr */
	client_main_addr = gMhlDevice.pI2cClient->addr;
	gMhlDevice.pI2cClient->addr = accessI2cAddr;
	mhl_i2c_status |= 4;
	if ((mhl_i2c_status & 0xfb) != 0)
		printk("MHL BR mhl_i2c_status(0x%02x)\n", mhl_i2c_status);


	memset(buf, 0xff, len);
	for (i = 0; i < len; i++) {
#if USE_DEFAULT_I2C_CODE
		status = i2c_smbus_xfer(gMhlDevice.pI2cClient->adapter, accessI2cAddr,
					0, I2C_SMBUS_READ, offset + i, I2C_SMBUS_BYTE_DATA, &data);
		if (status < 0) {
			return 0;
		}
		*buf = data.byte;
#else
		u8 tmp;
		tmp = offset + i;
#ifdef MHL_SET_EXT_GPIO
		gMhlDevice.pI2cClient->ext_flag |= I2C_DIRECTION_FLAG;
#endif
		status = i2c_master_send(gMhlDevice.pI2cClient, (const char *)&tmp, 1);
		if (status < 0) {
			printk("I2C_ReadByte(0x%02x, 0x%02x), i2c_transfer error: %d\n",
			       deviceID, offset, status);
		}

		status = i2c_master_recv(gMhlDevice.pI2cClient, (char *)&tmp, 1);
		*buf = tmp;
#endif

		buf++;
	}
	mhl_i2c_status &= 0xfb;
	/* restore default client address */
	gMhlDevice.pI2cClient->addr = client_main_addr;
	return len;
}

void I2C_WriteBlock(uint8_t deviceID, uint8_t offset, uint8_t *buf, uint8_t len)
{
	/* printk("hdmi enter %s (0x%02x, 0x%02x, 0x%02x)\n",__func__, deviceID, offset, len); */
	int i;
	uint8_t accessI2cAddr;
#if USE_DEFAULT_I2C_CODE
	union i2c_smbus_data data;
#endif
	int32_t status;
	u8 tmp[2] = { 0 };
	u32 client_main_addr;

	if (I2cAccessCheck() != HAL_RET_SUCCESS) {
		return;
	}
	accessI2cAddr = deviceID >> 1;
	mhl_i2c_status |= 8;
	if ((mhl_i2c_status & 0xf7) != 0)
		printk("MHL BW mhl_i2c_status(0x%02x)\n", mhl_i2c_status);

	/* backup addr */
	client_main_addr = gMhlDevice.pI2cClient->addr;
	gMhlDevice.pI2cClient->addr = accessI2cAddr;


	for (i = 0; i < len; i++) {
#if USE_DEFAULT_I2C_CODE
		data.byte = *buf;
		status = i2c_smbus_xfer(gMhlDevice.pI2cClient->adapter, accessI2cAddr,
					0, I2C_SMBUS_WRITE, offset + i, I2C_SMBUS_BYTE_DATA, &data);
#else
		tmp[0] = offset + i;
		tmp[1] = *buf;
#ifdef MHL_SET_EXT_GPIO
		gMhlDevice.pI2cClient->ext_flag |= I2C_DIRECTION_FLAG;
#endif
		status = i2c_master_send(gMhlDevice.pI2cClient, (const char *)tmp, 2);

#endif
		if (status < 0) {
			/* restore default client address */
			gMhlDevice.pI2cClient->addr = client_main_addr;
			return;
		}
		buf++;
	}
	mhl_i2c_status &= 0xf7;
	/* restore default client address */
	gMhlDevice.pI2cClient->addr = client_main_addr;
	return;
}
