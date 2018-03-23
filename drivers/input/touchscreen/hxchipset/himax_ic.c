/* Himax Android Driver Sample Code for HMX83100 chipset
*
* Copyright (C) 2015 Himax Corporation.
*
* This software is licensed under the terms of the GNU General Public
* License version 2, as published by the Free Software Foundation, and
* may be copied, distributed, and modified under those terms.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
*/

#include "himax_ic.h"

static unsigned char i_TP_CRC_FW_128K[]=
{
	#include "HX_CRC_128.i"
};
static unsigned char i_TP_CRC_FW_64K[]=
{
	#include "HX_CRC_64.i"
};
static unsigned char i_TP_CRC_FW_124K[]=
{
	#include "HX_CRC_124.i"
};
static unsigned char i_TP_CRC_FW_60K[]=
{
	#include "HX_CRC_60.i"
};


unsigned long	FW_VER_MAJ_FLASH_ADDR;
unsigned long 	FW_VER_MAJ_FLASH_LENG;
unsigned long 	FW_VER_MIN_FLASH_ADDR;
unsigned long 	FW_VER_MIN_FLASH_LENG;
unsigned long 	CFG_VER_MAJ_FLASH_ADDR;
unsigned long 	CFG_VER_MAJ_FLASH_LENG;
unsigned long 	CFG_VER_MIN_FLASH_ADDR;
unsigned long 	CFG_VER_MIN_FLASH_LENG;

unsigned char	IC_TYPE = 0;
unsigned char	IC_CHECKSUM = 0;

extern struct himax_ic_data* ic_data;

int himax_hand_shaking(struct i2c_client *client)    //0:Running, 1:Stop, 2:I2C Fail
{
	int ret, result;
	uint8_t hw_reset_check[1];
	uint8_t hw_reset_check_2[1];
	uint8_t buf0[2];
	uint8_t	IC_STATUS_CHECK = 0xAA;	

	memset(hw_reset_check, 0x00, sizeof(hw_reset_check));
	memset(hw_reset_check_2, 0x00, sizeof(hw_reset_check_2));

	buf0[0] = 0xF2;
	if (IC_STATUS_CHECK == 0xAA) {
		buf0[1] = 0xAA;
		IC_STATUS_CHECK = 0x55;
	} else {
		buf0[1] = 0x55;
		IC_STATUS_CHECK = 0xAA;
	}

	ret = i2c_himax_master_write(client, buf0, 2, HIMAX_I2C_RETRY_TIMES);
	if (ret < 0) {
		E("[Himax]:write 0xF2 failed line: %d \n",__LINE__);
		goto work_func_send_i2c_msg_fail;
	}
	msleep(50); 
  	
	buf0[0] = 0xF2;
	buf0[1] = 0x00;
	ret = i2c_himax_master_write(client, buf0, 2, HIMAX_I2C_RETRY_TIMES);
	if (ret < 0) {
		E("[Himax]:write 0x92 failed line: %d \n",__LINE__);
		goto work_func_send_i2c_msg_fail;
	}
	usleep_range(2000, 4000);
  	
	ret = i2c_himax_read(client, 0xD1, hw_reset_check, 1, HIMAX_I2C_RETRY_TIMES);
	if (ret < 0) {
		E("[Himax]:i2c_himax_read 0xD1 failed line: %d \n",__LINE__);
		goto work_func_send_i2c_msg_fail;
	}
	
	if ((IC_STATUS_CHECK != hw_reset_check[0])) {
		usleep_range(2000, 4000);
		ret = i2c_himax_read(client, 0xD1, hw_reset_check_2, 1, HIMAX_I2C_RETRY_TIMES);
		if (ret < 0) {
			E("[Himax]:i2c_himax_read 0xD1 failed line: %d \n",__LINE__);
			goto work_func_send_i2c_msg_fail;
		}
	
		if (hw_reset_check[0] == hw_reset_check_2[0]) {
			result = 1; 
		} else {
			result = 0; 
		}
	} else {
		result = 0; 
	}
	
	return result;

work_func_send_i2c_msg_fail:
	return 2;
}

void himax_diag_register_set(struct i2c_client *client, uint8_t diag_command)
{
	uint8_t tmp_addr[4];
	uint8_t tmp_data[4];

	if(diag_command != 0)
		diag_command = diag_command + 5;

	tmp_addr[3] = 0x80; tmp_addr[2] = 0x02; tmp_addr[1] = 0x01; tmp_addr[0] = 0x80;
	tmp_data[3] = 0x00; tmp_data[2] = 0x00; tmp_data[1] = 0x00; tmp_data[0] = diag_command;
	himax_flash_write_burst(client, tmp_addr, tmp_data);
}

void himax_flash_dump_func(struct i2c_client *client, uint8_t local_flash_command, int Flash_Size, uint8_t *flash_buffer)
{
	//struct himax_ts_data *ts = container_of(work, struct himax_ts_data, flash_work);

//	uint8_t sector = 0;
//	uint8_t page = 0;
	uint8_t tmp_addr[4];
	uint8_t tmp_data[4];
	uint8_t out_buffer[20];
	uint8_t in_buffer[260] = {0};
	int page_prog_start = 0;
	int i = 0;

	himax_sense_off(client);
	himax_burst_enable(client, 0);
	/*=============Dump Flash Start=============*/
	//=====================================
	// SPI Transfer Format : 0x8000_0010 ==> 0x0002_0780
	//=====================================
	tmp_addr[3] = 0x80; tmp_addr[2] = 0x00; tmp_addr[1] = 0x00; tmp_addr[0] = 0x10;
	tmp_data[3] = 0x00; tmp_data[2] = 0x02; tmp_data[1] = 0x07; tmp_data[0] = 0x80;
	himax_flash_write_burst(client, tmp_addr, tmp_data);

	for (page_prog_start = 0; page_prog_start < Flash_Size; page_prog_start = page_prog_start + 256)
	{
		//=================================
		// SPI Transfer Control
		// Set 256 bytes page read : 0x8000_0020 ==> 0x6940_02FF
		// Set read start address  : 0x8000_0028 ==> 0x0000_0000
		// Set command			   : 0x8000_0024 ==> 0x0000_003B
		//=================================
		tmp_addr[3] = 0x80; tmp_addr[2] = 0x00; tmp_addr[1] = 0x00; tmp_addr[0] = 0x20;
		tmp_data[3] = 0x69; tmp_data[2] = 0x40; tmp_data[1] = 0x02; tmp_data[0] = 0xFF;
		himax_flash_write_burst(client, tmp_addr, tmp_data);

		tmp_addr[3] = 0x80; tmp_addr[2] = 0x00; tmp_addr[1] = 0x00; tmp_addr[0] = 0x28;
		if (page_prog_start < 0x100)
		{
			tmp_data[3] = 0x00;
			tmp_data[2] = 0x00;
			tmp_data[1] = 0x00;
			tmp_data[0] = (uint8_t)page_prog_start;
		}
		else if (page_prog_start >= 0x100 && page_prog_start < 0x10000)
		{
			tmp_data[3] = 0x00;
			tmp_data[2] = 0x00;
			tmp_data[1] = (uint8_t)(page_prog_start >> 8);
			tmp_data[0] = (uint8_t)page_prog_start;
		}
		else if (page_prog_start >= 0x10000 && page_prog_start < 0x1000000)
		{
			tmp_data[3] = 0x00;
			tmp_data[2] = (uint8_t)(page_prog_start >> 16);
			tmp_data[1] = (uint8_t)(page_prog_start >> 8);
			tmp_data[0] = (uint8_t)page_prog_start;
		}
		himax_flash_write_burst(client, tmp_addr, tmp_data);

		tmp_addr[3] = 0x80; tmp_addr[2] = 0x00; tmp_addr[1] = 0x00; tmp_addr[0] = 0x24;
		tmp_data[3] = 0x00; tmp_data[2] = 0x00; tmp_data[1] = 0x00; tmp_data[0] = 0x3B;
		himax_flash_write_burst(client, tmp_addr, tmp_data);

		//==================================
		// AHB_I2C Burst Read
		// Set SPI data register : 0x8000_002C ==> 0x00
		//==================================
		out_buffer[0] = 0x2C;
		out_buffer[1] = 0x00;
		out_buffer[2] = 0x00;
		out_buffer[3] = 0x80;
		i2c_himax_write(client, 0x00 ,out_buffer, 4, 3);

		//==================================
		// Read access : 0x0C ==> 0x00
		//==================================
		out_buffer[0] = 0x00;
		i2c_himax_write(client, 0x0C ,out_buffer, 1, 3);

		//==================================
		// Read 128 bytes two times
		//==================================
		i2c_himax_read(client, 0x08 ,in_buffer, 128, 3);
		for (i = 0; i < 128; i++)
			flash_buffer[i + page_prog_start] = in_buffer[i];

		i2c_himax_read(client, 0x08 ,in_buffer, 128, 3);
		for (i = 0; i < 128; i++)
			flash_buffer[(i + 128) + page_prog_start] = in_buffer[i];

		I("%s:Verify Progress: %x\n", __func__, page_prog_start);
	}

/*=============Dump Flash End=============*/
		//msleep(100);
		/*
		for( i=0 ; i<8 ;i++)
		{
			for(j=0 ; j<64 ; j++)
			{
				setFlashDumpProgress(i*32 + j);
			}
		}
		*/
	himax_sense_on(client, 0x01);

	return;

}

int himax_chip_self_test(struct i2c_client *client)
{
	uint8_t tmp_addr[4];
	uint8_t tmp_data[128];
	int pf_value=0x00;
	uint8_t test_result_id = 0;
	int j;

	memset(tmp_addr, 0x00, sizeof(tmp_addr));
	memset(tmp_data, 0x00, sizeof(tmp_data));

	himax_interface_on(client);
	himax_sense_off(client);

	//Set criteria
	himax_burst_enable(client, 1);

	tmp_addr[3] = 0x90; tmp_addr[2] = 0x08; tmp_addr[1] = 0x80; tmp_addr[0] = 0x94;
	tmp_data[3] = 0x14; tmp_data[2] = 0xC8; tmp_data[1] = 0x00; tmp_data[0] = 0x00;
	tmp_data[7] = 0x13; tmp_data[6] = 0x60; tmp_data[5] = 0x0A; tmp_data[4] = 0x99;

	himax_flash_write_burst_lenth(client, tmp_addr, tmp_data, 8);

	//start selftest
	// 0x9008_805C ==> 0x0000_0001
	tmp_addr[3] = 0x90; tmp_addr[2] = 0x08; tmp_addr[1] = 0x80; tmp_addr[0] = 0x5C;
	tmp_data[3] = 0x00; tmp_data[2] = 0x00; tmp_data[1] = 0x00; tmp_data[0] = 0x01;
	himax_flash_write_burst(client, tmp_addr, tmp_data);

	himax_sense_on(client, 1);

	msleep(2000);

	himax_sense_off(client);
	msleep(20);

	//=====================================
	// Read test result ID : 0x9008_8078 ==> 0xA/0xB/0xC/0xF
	//=====================================
	tmp_data[3] = 0x00; tmp_data[2] = 0x00; tmp_data[1] = 0x00; tmp_data[0] = 0x00;
	tmp_addr[3] = 0x90; tmp_addr[2] = 0x08; tmp_addr[1] = 0x80; tmp_addr[0] = 0x78;
	himax_register_read(client, tmp_addr, 1, tmp_data);

	test_result_id = tmp_data[0];

	I("%s: check test result, test_result_id=%x, test_result=%x\n", __func__
	,test_result_id,tmp_data[0]);

	if (test_result_id==0xF) {
		I("[Himax]: self-test pass\n");
		pf_value = 0x1;
	} else {
		E("[Himax]: self-test fail\n");
		pf_value = 0x0;
	}
	himax_burst_enable(client, 1);

	for (j = 0;j < 10; j++){
		tmp_data[3] = 0x00; tmp_data[2] = 0x00; tmp_data[1] = 0x00; tmp_data[0] = 0x00;
		tmp_addr[3] = 0x90; tmp_addr[2] = 0x06; tmp_addr[1] = 0x00; tmp_addr[0] = 0x0C;
		himax_register_read(client, tmp_addr, 1, tmp_data);
		I("[Himax]: 9006000C = %d\n", tmp_data[0]);
		if (tmp_data[0] != 0){
		tmp_data[3] = 0x90; tmp_data[2] = 0x06; tmp_data[1] = 0x00; tmp_data[0] = 0x00;
		if ( i2c_himax_write(client, 0x00 ,tmp_data, 4, HIMAX_I2C_RETRY_TIMES) < 0) {
			E("%s: i2c access fail!\n", __func__);
		}
		tmp_data[0] = 0x00;
		if ( i2c_himax_write(client, 0x0C ,tmp_data, 1, HIMAX_I2C_RETRY_TIMES) < 0) {
			E("%s: i2c access fail!\n", __func__);
		}
			i2c_himax_read(client, 0x08, tmp_data, 124,HIMAX_I2C_RETRY_TIMES);
		}else{
			break;
		}
	}

	himax_sense_on(client, 1);
	msleep(120);

	return pf_value;
}

void himax_set_HSEN_enable(struct i2c_client *client, uint8_t HSEN_enable)
{
	uint8_t tmp_addr[4];
	uint8_t tmp_data[4];

	himax_burst_enable(client, 0);
	tmp_addr[3] = 0x90; tmp_addr[2] = 0x08; tmp_addr[1] = 0x80; tmp_addr[0] = 0x50;
	tmp_data[3] = 0x00; tmp_data[2] = 0x00; tmp_data[1] = 0x00; tmp_data[0] = HSEN_enable;
	himax_flash_write_burst(client, tmp_addr, tmp_data);
}
void himax_get_HSEN_enable(struct i2c_client *client,uint8_t *tmp_data)
{
	uint8_t tmp_addr[4];

	tmp_addr[3] = 0x90; tmp_addr[2] = 0x08; tmp_addr[1] = 0x80; tmp_addr[0] = 0x50;
	himax_register_read(client, tmp_addr, 1, tmp_data);
}

void himax_set_SMWP_enable(struct i2c_client *client, uint8_t SMWP_enable)
{
	uint8_t tmp_addr[4];
	uint8_t tmp_data[4];

	tmp_addr[3] = 0x90; tmp_addr[2] = 0x08; tmp_addr[1] = 0x80; tmp_addr[0] = 0x54;
	tmp_data[3] = 0x00; tmp_data[2] = 0x00; tmp_data[1] = 0x00; tmp_data[0] = SMWP_enable;
	himax_flash_write_burst(client, tmp_addr, tmp_data);
}

void himax_get_SMWP_enable(struct i2c_client *client,uint8_t *tmp_data)
{
	uint8_t tmp_addr[4];

	tmp_addr[3] = 0x90; tmp_addr[2] = 0x08; tmp_addr[1] = 0x80; tmp_addr[0] = 0x54;
	himax_register_read(client, tmp_addr, 1, tmp_data);
}

int himax_burst_enable(struct i2c_client *client, uint8_t auto_add_4_byte)
{
	uint8_t tmp_data[4];

	tmp_data[0] = 0x31;
	if ( i2c_himax_write(client, 0x13 ,tmp_data, 1, HIMAX_I2C_RETRY_TIMES) < 0) {
		E("%s: i2c access fail!\n", __func__);
		return -EBUSY;
	}
	
	tmp_data[0] = (0x10 | auto_add_4_byte);
	if ( i2c_himax_write(client, 0x0D ,tmp_data, 1, HIMAX_I2C_RETRY_TIMES) < 0) {
		E("%s: i2c access fail!\n", __func__);
		return -EBUSY;
	}
	return 0;

}

void himax_register_read(struct i2c_client *client, uint8_t *read_addr, int read_length, uint8_t *read_data)
{
	uint8_t tmp_data[4];
	int i = 0;
	int address = 0;

	if(read_length>256)
	{
		E("%s: read len over 256!\n", __func__);
		return;
	}
	if (read_length > 1)
		himax_burst_enable(client, 1);
	else
	himax_burst_enable(client, 0);
	address = (read_addr[3] << 24) + (read_addr[2] << 16) + (read_addr[1] << 8) + read_addr[0];
	i = address;
		tmp_data[0] = (uint8_t)i;
		tmp_data[1] = (uint8_t)(i >> 8);
		tmp_data[2] = (uint8_t)(i >> 16);
		tmp_data[3] = (uint8_t)(i >> 24);
		if ( i2c_himax_write(client, 0x00 ,tmp_data, 4, HIMAX_I2C_RETRY_TIMES) < 0) {
				E("%s: i2c access fail!\n", __func__);
				return;
		 	}
		tmp_data[0] = 0x00;
		if ( i2c_himax_write(client, 0x0C ,tmp_data, 1, HIMAX_I2C_RETRY_TIMES) < 0) {
				E("%s: i2c access fail!\n", __func__);
				return;
		 	}
		
		if ( i2c_himax_read(client, 0x08 ,read_data, read_length * 4, HIMAX_I2C_RETRY_TIMES) < 0) {
			E("%s: i2c access fail!\n", __func__);
			return;
		}
	if (read_length > 1)
		himax_burst_enable(client, 0);
}

void himax_flash_read(struct i2c_client *client, uint8_t *reg_byte, uint8_t *read_data)
{
    uint8_t tmpbyte[2];
    
    if ( i2c_himax_write(client, 0x00 ,&reg_byte[0], 1, HIMAX_I2C_RETRY_TIMES) < 0) {
		E("%s: i2c access fail!\n", __func__);
		return;
	}

	if ( i2c_himax_write(client, 0x01 ,&reg_byte[1], 1, HIMAX_I2C_RETRY_TIMES) < 0) {
		E("%s: i2c access fail!\n", __func__);
		return;
	}

	if ( i2c_himax_write(client, 0x02 ,&reg_byte[2], 1, HIMAX_I2C_RETRY_TIMES) < 0) {
		E("%s: i2c access fail!\n", __func__);
		return;
	}

	if ( i2c_himax_write(client, 0x03 ,&reg_byte[3], 1, HIMAX_I2C_RETRY_TIMES) < 0) {
		E("%s: i2c access fail!\n", __func__);
		return;
	}

    tmpbyte[0] = 0x00;
    if ( i2c_himax_write(client, 0x0C ,&tmpbyte[0], 1, HIMAX_I2C_RETRY_TIMES) < 0) {
		E("%s: i2c access fail!\n", __func__);
		return;
	}

	if ( i2c_himax_read(client, 0x08 ,&read_data[0], 1, HIMAX_I2C_RETRY_TIMES) < 0) {
		E("%s: i2c access fail!\n", __func__);
		return;
	}

	if ( i2c_himax_read(client, 0x09 ,&read_data[1], 1, HIMAX_I2C_RETRY_TIMES) < 0) {
		E("%s: i2c access fail!\n", __func__);
		return;
	}

	if ( i2c_himax_read(client, 0x0A ,&read_data[2], 1, HIMAX_I2C_RETRY_TIMES) < 0) {
		E("%s: i2c access fail!\n", __func__);
		return;
	}

	if ( i2c_himax_read(client, 0x0B ,&read_data[3], 1, HIMAX_I2C_RETRY_TIMES) < 0) {
		E("%s: i2c access fail!\n", __func__);
		return;
	}

	if ( i2c_himax_read(client, 0x18 ,&tmpbyte[0], 1, HIMAX_I2C_RETRY_TIMES) < 0) {
		E("%s: i2c access fail!\n", __func__);
		return;
	}// No bus request

	if ( i2c_himax_read(client, 0x0F ,&tmpbyte[1], 1, HIMAX_I2C_RETRY_TIMES) < 0) {
		E("%s: i2c access fail!\n", __func__);
		return;
	}// idle state

}

void himax_flash_write_burst(struct i2c_client *client, uint8_t * reg_byte, uint8_t * write_data)
{
    uint8_t data_byte[8];
	int i = 0, j = 0;

     for (i = 0; i < 4; i++)
     { 
         data_byte[i] = reg_byte[i];
     }
     for (j = 4; j < 8; j++)
     {
         data_byte[j] = write_data[j-4];
     }
	 
	 if ( i2c_himax_write(client, 0x00 ,data_byte, 8, HIMAX_I2C_RETRY_TIMES) < 0) {
		 E("%s: i2c access fail!\n", __func__);
		 return;
	 }

}

int himax_flash_write_burst_lenth(struct i2c_client *client, uint8_t *reg_byte, uint8_t *write_data, int length)
{
    uint8_t data_byte[256];
	int i = 0, j = 0;

    for (i = 0; i < 4; i++)
    {
        data_byte[i] = reg_byte[i];
    }
    for (j = 4; j < length + 4; j++)
    {
        data_byte[j] = write_data[j - 4];
    }
   
	if ( i2c_himax_write(client, 0x00 ,data_byte, length + 4, HIMAX_I2C_RETRY_TIMES) < 0) {
		 E("%s: i2c access fail!\n", __func__);
		 return -EBUSY;
	}

    return 0;
}

int himax_register_write(struct i2c_client *client, uint8_t *write_addr, int write_length, uint8_t *write_data)
{
	int i =0, address = 0;
	int ret = 0;

	address = (write_addr[3] << 24) + (write_addr[2] << 16) + (write_addr[1] << 8) + write_addr[0];

	for (i = address; i < address + write_length * 4; i = i + 4)
	{
		if (write_length > 1)
		{
			ret = himax_burst_enable(client, 1);
			if(ret)
				return ret;
		}
		else
		{
			ret = himax_burst_enable(client, 0);
			if(ret)
				return ret;
		}
	ret = himax_flash_write_burst_lenth(client, write_addr, write_data, write_length * 4);
	if(ret < 0)
		return ret;
	}

	return 0;
}

void himax_sense_off(struct i2c_client *client)
{
	uint8_t wdt_off = 0x00;
	uint8_t tmp_addr[4];
	uint8_t tmp_data[5];	

	himax_burst_enable(client, 0);

	while(wdt_off == 0x00)
	{
		// 0x9000_800C ==> 0x0000_AC53
		tmp_addr[3] = 0x90; tmp_addr[2] = 0x00; tmp_addr[1] = 0x80; tmp_addr[0] = 0x0C;
		tmp_data[3] = 0x00; tmp_data[2] = 0x00; tmp_data[1] = 0xAC; tmp_data[0] = 0x53;
		himax_flash_write_burst(client, tmp_addr, tmp_data);

		//=====================================
    // Read Watch Dog disable password : 0x9000_800C ==> 0x0000_AC53
    //=====================================
		tmp_addr[3] = 0x90; tmp_addr[2] = 0x00; tmp_addr[1] = 0x80; tmp_addr[0] = 0x0C;
		himax_register_read(client, tmp_addr, 1, tmp_data);
		
		//Check WDT
		if (tmp_data[0] == 0x53 && tmp_data[1] == 0xAC && tmp_data[2] == 0x00 && tmp_data[3] == 0x00)
			wdt_off = 0x01;
		else
			wdt_off = 0x00;
	}

	// VCOM		//0x9008_806C ==> 0x0000_0001
	tmp_addr[3] = 0x90; tmp_addr[2] = 0x08; tmp_addr[1] = 0x80; tmp_addr[0] = 0x6C;
	tmp_data[3] = 0x00; tmp_data[2] = 0x00; tmp_data[1] = 0x00; tmp_data[0] = 0x01;
	himax_flash_write_burst(client, tmp_addr, tmp_data);

	msleep(20);

	// 0x9000_0010 ==> 0x0000_00DA
	tmp_addr[3] = 0x90; tmp_addr[2] = 0x00; tmp_addr[1] = 0x00; tmp_addr[0] = 0x10;
	tmp_data[3] = 0x00; tmp_data[2] = 0x00; tmp_data[1] = 0x00; tmp_data[0] = 0xDA;
	himax_flash_write_burst(client, tmp_addr, tmp_data);

	//=====================================
	// Read CPU clock off password : 0x9000_0010 ==> 0x0000_00DA
	//=====================================
	tmp_data[3] = 0x00; tmp_data[2] = 0x00; tmp_data[1] = 0x00; tmp_data[0] = 0x00;
	tmp_addr[3] = 0x90; tmp_addr[2] = 0x00; tmp_addr[1] = 0x00; tmp_addr[0] = 0x10;
	himax_register_read(client, tmp_addr, 1, tmp_data);
	I("%s: CPU clock off password data[0]=%x data[1]=%x data[2]=%x data[3]=%x\n", __func__
		,tmp_data[0],tmp_data[1],tmp_data[2],tmp_data[3]);

}

void himax_interface_on(struct i2c_client *client)
{
	uint8_t tmp_addr[4];
	uint8_t tmp_data[5];

    //===========================================
    //  Any Cmd for ineterface on : 0x9000_0000 ==> 0x0000_0000
    //===========================================
    tmp_addr[3] = 0x90; tmp_addr[2] = 0x00; tmp_addr[1] = 0x00; tmp_addr[0] = 0x00;
    himax_flash_read(client, tmp_addr, tmp_data); //avoid RD/WR fail
}

bool wait_wip(struct i2c_client *client, int Timing)
{
	uint8_t tmp_addr[4];
	uint8_t tmp_data[4];
	uint8_t in_buffer[10];
	//uint8_t out_buffer[20];
	int retry_cnt = 0;

	//=====================================
	// SPI Transfer Format : 0x8000_0010 ==> 0x0002_0780
	//=====================================
	tmp_addr[3] = 0x80; tmp_addr[2] = 0x00; tmp_addr[1] = 0x00; tmp_addr[0] = 0x10;
	tmp_data[3] = 0x00; tmp_data[2] = 0x02; tmp_data[1] = 0x07; tmp_data[0] = 0x80;
	himax_flash_write_burst(client, tmp_addr, tmp_data);

	in_buffer[0] = 0x01;

	do
	{
		//=====================================
		// SPI Transfer Control : 0x8000_0020 ==> 0x4200_0003
		//=====================================
		tmp_addr[3] = 0x80; tmp_addr[2] = 0x00; tmp_addr[1] = 0x00; tmp_addr[0] = 0x20;
		tmp_data[3] = 0x42; tmp_data[2] = 0x00; tmp_data[1] = 0x00; tmp_data[0] = 0x03;
		himax_flash_write_burst(client, tmp_addr, tmp_data);

		//=====================================
		// SPI Command : 0x8000_0024 ==> 0x0000_0005
		// read 0x8000_002C for 0x01, means wait success
		//=====================================
		tmp_addr[3] = 0x80; tmp_addr[2] = 0x00; tmp_addr[1] = 0x00; tmp_addr[0] = 0x24;
		tmp_data[3] = 0x00; tmp_data[2] = 0x00; tmp_data[1] = 0x00; tmp_data[0] = 0x05;
		himax_flash_write_burst(client, tmp_addr, tmp_data);

		in_buffer[0] = in_buffer[1] = in_buffer[2] = in_buffer[3] = 0xFF;
		tmp_addr[3] = 0x80; tmp_addr[2] = 0x00; tmp_addr[1] = 0x00; tmp_addr[0] = 0x2C;
		himax_register_read(client, tmp_addr, 1, in_buffer);
		
		if ((in_buffer[0] & 0x01) == 0x00)
			return true;

		retry_cnt++;
		
		if (in_buffer[0] != 0x00 || in_buffer[1] != 0x00 || in_buffer[2] != 0x00 || in_buffer[3] != 0x00)
        	I("%s:Wait wip retry_cnt:%d, buffer[0]=%d, buffer[1]=%d, buffer[2]=%d, buffer[3]=%d \n", __func__, 
            retry_cnt,in_buffer[0],in_buffer[1],in_buffer[2],in_buffer[3]);

		if (retry_cnt > 100)
        {        	
			E("%s: Wait wip error!\n", __func__);
            return false;
        }
		msleep(Timing);
	}while ((in_buffer[0] & 0x01) == 0x01);
	return true;
}

void himax_sense_on(struct i2c_client *client, uint8_t FlashMode)
{
	uint8_t tmp_addr[4];
	uint8_t tmp_data[128];

	himax_interface_on(client);
	himax_burst_enable(client, 0);
	//CPU reset
	// 0x9000_0014 ==> 0x0000_00CA
	tmp_addr[3] = 0x90; tmp_addr[2] = 0x00; tmp_addr[1] = 0x00; tmp_addr[0] = 0x14;
	tmp_data[3] = 0x00; tmp_data[2] = 0x00; tmp_data[1] = 0x00; tmp_data[0] = 0xCA;
	himax_flash_write_burst(client, tmp_addr, tmp_data);

	//=====================================
	// Read pull low CPU reset signal : 0x9000_0014 ==> 0x0000_00CA
	//=====================================
	tmp_data[3] = 0x00; tmp_data[2] = 0x00; tmp_data[1] = 0x00; tmp_data[0] = 0x00;
	tmp_addr[3] = 0x90; tmp_addr[2] = 0x00; tmp_addr[1] = 0x00; tmp_addr[0] = 0x14;
	himax_register_read(client, tmp_addr, 1, tmp_data);

	I("%s: check pull low CPU reset signal  data[0]=%x data[1]=%x data[2]=%x data[3]=%x\n", __func__
	,tmp_data[0],tmp_data[1],tmp_data[2],tmp_data[3]);

	// 0x9000_0014 ==> 0x0000_0000
	tmp_addr[3] = 0x90; tmp_addr[2] = 0x00; tmp_addr[1] = 0x00; tmp_addr[0] = 0x14;
	tmp_data[3] = 0x00; tmp_data[2] = 0x00; tmp_data[1] = 0x00; tmp_data[0] = 0x00;
	himax_flash_write_burst(client, tmp_addr, tmp_data);

	//=====================================
	// Read revert pull low CPU reset signal : 0x9000_0014 ==> 0x0000_0000
	//=====================================
	tmp_data[3] = 0x00; tmp_data[2] = 0x00; tmp_data[1] = 0x00; tmp_data[0] = 0x00;
	tmp_addr[3] = 0x90; tmp_addr[2] = 0x00; tmp_addr[1] = 0x00; tmp_addr[0] = 0x14;
	himax_register_read(client, tmp_addr, 1, tmp_data);

	I("%s: revert pull low CPU reset signal data[0]=%x data[1]=%x data[2]=%x data[3]=%x\n", __func__
	,tmp_data[0],tmp_data[1],tmp_data[2],tmp_data[3]);

	//=====================================
  // Reset TCON
  //=====================================
  tmp_addr[3] = 0x80; tmp_addr[2] = 0x02; tmp_addr[1] = 0x01; tmp_addr[0] = 0xE0;
  tmp_data[3] = 0x00; tmp_data[2] = 0x00; tmp_data[1] = 0x00; tmp_data[0] = 0x00;
  himax_flash_write_burst(client, tmp_addr, tmp_data);
	usleep_range(10000, 20000);
  tmp_addr[3] = 0x80; tmp_addr[2] = 0x02; tmp_addr[1] = 0x01; tmp_addr[0] = 0xE0;
  tmp_data[3] = 0x00; tmp_data[2] = 0x00; tmp_data[1] = 0x00; tmp_data[0] = 0x01;
  himax_flash_write_burst(client, tmp_addr, tmp_data);

	if (FlashMode == 0x00)	//SRAM
	{
		//=====================================
		//			Re-map
		//=====================================
		tmp_addr[3] = 0x90; tmp_addr[2] = 0x00; tmp_addr[1] = 0x00; tmp_addr[0] = 0x00;
		tmp_data[3] = 0x00; tmp_data[2] = 0x00; tmp_data[1] = 0x00; tmp_data[0] = 0xF1;
		himax_flash_write_burst_lenth(client, tmp_addr, tmp_data, 4);
		I("%s:83100_Chip_Re-map ON\n", __func__);
	}
	else
	{
		//=====================================
		//			Re-map off
		//=====================================
		tmp_addr[3] = 0x90; tmp_addr[2] = 0x00; tmp_addr[1] = 0x00; tmp_addr[0] = 0x00;
		tmp_data[3] = 0x00; tmp_data[2] = 0x00; tmp_data[1] = 0x00; tmp_data[0] = 0x00;
		himax_flash_write_burst_lenth(client, tmp_addr, tmp_data, 4);
		I("%s:83100_Chip_Re-map OFF\n", __func__);
	}
	//=====================================
	//			CPU clock on
	//=====================================
	tmp_addr[3] = 0x90; tmp_addr[2] = 0x00; tmp_addr[1] = 0x00; tmp_addr[0] = 0x10;
	tmp_data[3] = 0x00; tmp_data[2] = 0x00; tmp_data[1] = 0x00; tmp_data[0] = 0x00;
	himax_flash_write_burst_lenth(client, tmp_addr, tmp_data, 4);

}

void himax_chip_erase(struct i2c_client *client)
{
	uint8_t tmp_addr[4];
	uint8_t tmp_data[4];

	himax_burst_enable(client, 0);

	//=====================================
	// SPI Transfer Format : 0x8000_0010 ==> 0x0002_0780
	//=====================================
	tmp_addr[3] = 0x80; tmp_addr[2] = 0x00; tmp_addr[1] = 0x00; tmp_addr[0] = 0x10;
	tmp_data[3] = 0x00; tmp_data[2] = 0x02; tmp_data[1] = 0x07; tmp_data[0] = 0x80;
	himax_flash_write_burst(client, tmp_addr, tmp_data);

	//=====================================
	// Chip Erase
	// Write Enable : 1. 0x8000_0020 ==> 0x4700_0000
	//				  2. 0x8000_0024 ==> 0x0000_0006
	//=====================================
	tmp_addr[3] = 0x80; tmp_addr[2] = 0x00; tmp_addr[1] = 0x00; tmp_addr[0] = 0x20;
	tmp_data[3] = 0x47; tmp_data[2] = 0x00; tmp_data[1] = 0x00; tmp_data[0] = 0x00;
	himax_flash_write_burst(client, tmp_addr, tmp_data);

	tmp_addr[3] = 0x80; tmp_addr[2] = 0x00; tmp_addr[1] = 0x00; tmp_addr[0] = 0x24;
	tmp_data[3] = 0x00; tmp_data[2] = 0x00; tmp_data[1] = 0x00; tmp_data[0] = 0x06;
	himax_flash_write_burst(client, tmp_addr, tmp_data);

	//=====================================
	// Chip Erase
	// Erase Command : 0x8000_0024 ==> 0x0000_00C7
	//=====================================
	tmp_addr[3] = 0x80; tmp_addr[2] = 0x00; tmp_addr[1] = 0x00; tmp_addr[0] = 0x24;
	tmp_data[3] = 0x00; tmp_data[2] = 0x00; tmp_data[1] = 0x00; tmp_data[0] = 0xC7;
	himax_flash_write_burst(client, tmp_addr, tmp_data);
	
	msleep(2000);
	
	if (!wait_wip(client, 100))
		E("%s:83100_Chip_Erase Fail\n", __func__);

}

bool himax_block_erase(struct i2c_client *client)
{
	uint8_t tmp_addr[4];
	uint8_t tmp_data[4];

	himax_burst_enable(client, 0);

	//=====================================
	// SPI Transfer Format : 0x8000_0010 ==> 0x0002_0780
	//=====================================
	tmp_addr[3] = 0x80; tmp_addr[2] = 0x00; tmp_addr[1] = 0x00; tmp_addr[0] = 0x10;
	tmp_data[3] = 0x00; tmp_data[2] = 0x02; tmp_data[1] = 0x07; tmp_data[0] = 0x80;
	himax_flash_write_burst(client, tmp_addr, tmp_data);

	//=====================================
	// Chip Erase
	// Write Enable : 1. 0x8000_0020 ==> 0x4700_0000
	//				  2. 0x8000_0024 ==> 0x0000_0006
	//=====================================
	tmp_addr[3] = 0x80; tmp_addr[2] = 0x00; tmp_addr[1] = 0x00; tmp_addr[0] = 0x20;
	tmp_data[3] = 0x47; tmp_data[2] = 0x00; tmp_data[1] = 0x00; tmp_data[0] = 0x00;
	himax_flash_write_burst(client, tmp_addr, tmp_data);

	tmp_addr[3] = 0x80; tmp_addr[2] = 0x00; tmp_addr[1] = 0x00; tmp_addr[0] = 0x24;
	tmp_data[3] = 0x00; tmp_data[2] = 0x00; tmp_data[1] = 0x00; tmp_data[0] = 0x06;
	himax_flash_write_burst(client, tmp_addr, tmp_data);

	//=====================================
	// Block Erase
	// Erase Command : 0x8000_0028 ==> 0x0000_0000 //SPI addr
	//				   0x8000_0020 ==> 0x6700_0000 //control
	//				   0x8000_0024 ==> 0x0000_0052 //BE
	//=====================================
	tmp_addr[3] = 0x80; tmp_addr[2] = 0x00; tmp_addr[1] = 0x00; tmp_addr[0] = 0x28;
	tmp_data[3] = 0x00; tmp_data[2] = 0x00; tmp_data[1] = 0x00; tmp_data[0] = 0x00;
	himax_flash_write_burst(client, tmp_addr, tmp_data);
	
	tmp_addr[3] = 0x80; tmp_addr[2] = 0x00; tmp_addr[1] = 0x00; tmp_addr[0] = 0x20;
	tmp_data[3] = 0x67; tmp_data[2] = 0x00; tmp_data[1] = 0x00; tmp_data[0] = 0x00;
	himax_flash_write_burst(client, tmp_addr, tmp_data);
	
	tmp_addr[3] = 0x80; tmp_addr[2] = 0x00; tmp_addr[1] = 0x00; tmp_addr[0] = 0x24;
	tmp_data[3] = 0x00; tmp_data[2] = 0x00; tmp_data[1] = 0x00; tmp_data[0] = 0x52;
	himax_flash_write_burst(client, tmp_addr, tmp_data);

	msleep(1000);

	if (!wait_wip(client, 100))
	{
		E("%s:83100_Erase Fail\n", __func__);
		return false;
	}
	else
	{
		return true;
	}

}

bool himax_sector_erase(struct i2c_client *client, int start_addr)
{
	uint8_t tmp_addr[4];
	uint8_t tmp_data[4];
	int page_prog_start = 0;

	himax_burst_enable(client, 0);

	//=====================================
	// SPI Transfer Format : 0x8000_0010 ==> 0x0002_0780
	//=====================================
	tmp_addr[3] = 0x80; tmp_addr[2] = 0x00; tmp_addr[1] = 0x00; tmp_addr[0] = 0x10;
	tmp_data[3] = 0x00; tmp_data[2] = 0x02; tmp_data[1] = 0x07; tmp_data[0] = 0x80;
	himax_flash_write_burst(client, tmp_addr, tmp_data);
	for (page_prog_start = start_addr; page_prog_start < start_addr + 0x0F000; page_prog_start = page_prog_start + 0x1000)
		{
			//=====================================
			// Chip Erase
			// Write Enable : 1. 0x8000_0020 ==> 0x4700_0000
			//				  2. 0x8000_0024 ==> 0x0000_0006
			//=====================================
			tmp_addr[3] = 0x80; tmp_addr[2] = 0x00; tmp_addr[1] = 0x00; tmp_addr[0] = 0x20;
			tmp_data[3] = 0x47; tmp_data[2] = 0x00; tmp_data[1] = 0x00; tmp_data[0] = 0x00;
			himax_flash_write_burst(client, tmp_addr, tmp_data);

			tmp_addr[3] = 0x80; tmp_addr[2] = 0x00; tmp_addr[1] = 0x00; tmp_addr[0] = 0x24;
			tmp_data[3] = 0x00; tmp_data[2] = 0x00; tmp_data[1] = 0x00; tmp_data[0] = 0x06;
			himax_flash_write_burst(client, tmp_addr, tmp_data);

			//=====================================
			// Sector Erase
			// Erase Command : 0x8000_0028 ==> 0x0000_0000 //SPI addr
			// 				0x8000_0020 ==> 0x6700_0000 //control
			// 				0x8000_0024 ==> 0x0000_0020 //SE
			//=====================================
			tmp_addr[3] = 0x80; tmp_addr[2] = 0x00; tmp_addr[1] = 0x00; tmp_addr[0] = 0x28;
			if (page_prog_start < 0x100)
			{
			 tmp_data[3] = 0x00; tmp_data[2] = 0x00; tmp_data[1] = 0x00; tmp_data[0] = (uint8_t)page_prog_start;
			}
			else if (page_prog_start >= 0x100 && page_prog_start < 0x10000)
			{
			 tmp_data[3] = 0x00; tmp_data[2] = 0x00; tmp_data[1] = (uint8_t)(page_prog_start >> 8); tmp_data[0] = (uint8_t)page_prog_start;
			}
			else if (page_prog_start >= 0x10000 && page_prog_start < 0x1000000)
			{
			 tmp_data[3] = 0x00; tmp_data[2] = (uint8_t)(page_prog_start >> 16); tmp_data[1] = (uint8_t)(page_prog_start >> 8); tmp_data[0] = (uint8_t)page_prog_start;
			}
			himax_flash_write_burst(client, tmp_addr, tmp_data);

			tmp_addr[3] = 0x80; tmp_addr[2] = 0x00; tmp_addr[1] = 0x00; tmp_addr[0] = 0x20;
			tmp_data[3] = 0x67; tmp_data[2] = 0x00; tmp_data[1] = 0x00; tmp_data[0] = 0x00;
			himax_flash_write_burst(client, tmp_addr, tmp_data);

			tmp_addr[3] = 0x80; tmp_addr[2] = 0x00; tmp_addr[1] = 0x00; tmp_addr[0] = 0x24;
			tmp_data[3] = 0x00; tmp_data[2] = 0x00; tmp_data[1] = 0x00; tmp_data[0] = 0x20;
			himax_flash_write_burst(client, tmp_addr, tmp_data);

			msleep(200);

			if (!wait_wip(client, 100))
			{
				E("%s:83100_Erase Fail\n", __func__);
				return false;
			}
		}	
		return true;
}

void himax_sram_write(struct i2c_client *client, uint8_t *FW_content)
{
	int i = 0;
	uint8_t tmp_addr[4];
	uint8_t tmp_data[128];
	int FW_length = 0x4000; // 0x4000 = 16K bin file
	
	//himax_sense_off(client);

	for (i = 0; i < FW_length; i = i + 128) 
	{
		himax_burst_enable(client, 1);

		if (i < 0x100)
		{
			tmp_addr[3] = 0x08; 
			tmp_addr[2] = 0x00; 
			tmp_addr[1] = 0x00; 
			tmp_addr[0] = i;
		}
		else if (i >= 0x100 && i < 0x10000)
		{
			tmp_addr[3] = 0x08; 
			tmp_addr[2] = 0x00; 
			tmp_addr[1] = (i >> 8); 
			tmp_addr[0] = i;
		}

		memcpy(&tmp_data[0], &FW_content[i], 128);
		himax_flash_write_burst_lenth(client, tmp_addr, tmp_data, 128);

	}

	if (!wait_wip(client, 100))
		E("%s:83100_Sram_Write Fail\n", __func__);
}

bool himax_sram_verify(struct i2c_client *client, uint8_t *FW_File, int FW_Size)
{
	int i = 0;
	uint8_t out_buffer[20];
	uint8_t in_buffer[128];
	uint8_t *get_fw_content;

	get_fw_content = kzalloc(0x4000*sizeof(uint8_t), GFP_KERNEL);
	if (!get_fw_content)
		return false;

	for (i = 0; i < 0x4000; i = i + 128)
	{
		himax_burst_enable(client, 1);

		//==================================
		//	AHB_I2C Burst Read
		//==================================
		if (i < 0x100)
		{
			out_buffer[3] = 0x08; 
			out_buffer[2] = 0x00; 
			out_buffer[1] = 0x00; 
			out_buffer[0] = i;
		}
		else if (i >= 0x100 && i < 0x10000)
		{
			out_buffer[3] = 0x08; 
			out_buffer[2] = 0x00; 
			out_buffer[1] = (i >> 8); 
			out_buffer[0] = i;
		}

		if ( i2c_himax_write(client, 0x00 ,out_buffer, 4, HIMAX_I2C_RETRY_TIMES) < 0) {
			E("%s: i2c access fail!\n", __func__);
			return false;
		}

		out_buffer[0] = 0x00;		
		if ( i2c_himax_write(client, 0x0C ,out_buffer, 1, HIMAX_I2C_RETRY_TIMES) < 0) {
			E("%s: i2c access fail!\n", __func__);
			return false;
		}

		if ( i2c_himax_read(client, 0x08 ,in_buffer, 128, HIMAX_I2C_RETRY_TIMES) < 0) {
			E("%s: i2c access fail!\n", __func__);
			return false;
		}
		memcpy(&get_fw_content[i], &in_buffer[0], 128);
	}

	for (i = 0; i < FW_Size; i++)
		{
	        if (FW_File[i] != get_fw_content[i])
	        	{
					E("%s: fail! SRAM[%x]=%x NOT CRC_ifile=%x\n", __func__,i,get_fw_content[i],FW_File[i]);
		            return false;
	        	}
		}

	kfree(get_fw_content);

	return true;
}

void himax_flash_programming(struct i2c_client *client, uint8_t *FW_content, int FW_Size)
{
	int page_prog_start = 0;
	int program_length = 48;
	int i = 0, j = 0, k = 0;
	uint8_t tmp_addr[4];
	uint8_t tmp_data[4];
	uint8_t buring_data[256];    // Read for flash data, 128K
									 // 4 bytes for 0x80002C padding

	//himax_interface_on(client);
	himax_burst_enable(client, 0);

	//=====================================
	// SPI Transfer Format : 0x8000_0010 ==> 0x0002_0780
	//=====================================
	tmp_addr[3] = 0x80; tmp_addr[2] = 0x00; tmp_addr[1] = 0x00; tmp_addr[0] = 0x10;
	tmp_data[3] = 0x00; tmp_data[2] = 0x02; tmp_data[1] = 0x07; tmp_data[0] = 0x80;
	himax_flash_write_burst(client, tmp_addr, tmp_data);

	for (page_prog_start = 0; page_prog_start < FW_Size; page_prog_start = page_prog_start + 256)
	{
		//msleep(5);
		//=====================================
		// Write Enable : 1. 0x8000_0020 ==> 0x4700_0000
		//				  2. 0x8000_0024 ==> 0x0000_0006
		//=====================================
		tmp_addr[3] = 0x80; tmp_addr[2] = 0x00; tmp_addr[1] = 0x00; tmp_addr[0] = 0x20;
		tmp_data[3] = 0x47; tmp_data[2] = 0x00; tmp_data[1] = 0x00; tmp_data[0] = 0x00;
		himax_flash_write_burst(client, tmp_addr, tmp_data);

		tmp_addr[3] = 0x80; tmp_addr[2] = 0x00; tmp_addr[1] = 0x00; tmp_addr[0] = 0x24;
		tmp_data[3] = 0x00; tmp_data[2] = 0x00; tmp_data[1] = 0x00; tmp_data[0] = 0x06;
		himax_flash_write_burst(client, tmp_addr, tmp_data);

		//=================================
		// SPI Transfer Control
		// Set 256 bytes page write : 0x8000_0020 ==> 0x610F_F000
		// Set read start address	: 0x8000_0028 ==> 0x0000_0000			
		//=================================
		tmp_addr[3] = 0x80; tmp_addr[2] = 0x00; tmp_addr[1] = 0x00; tmp_addr[0] = 0x20;
		tmp_data[3] = 0x61; tmp_data[2] = 0x0F; tmp_data[1] = 0xF0; tmp_data[0] = 0x00;
		// data bytes should be 0x6100_0000 + ((word_number)*4-1)*4096 = 0x6100_0000 + 0xFF000 = 0x610F_F000
		// Programmable size = 1 page = 256 bytes, word_number = 256 byte / 4 = 64
		himax_flash_write_burst(client, tmp_addr, tmp_data);

		tmp_addr[3] = 0x80; tmp_addr[2] = 0x00; tmp_addr[1] = 0x00; tmp_addr[0] = 0x28;
		//tmp_data[3] = 0x00; tmp_data[2] = 0x00; tmp_data[1] = 0x00; tmp_data[0] = 0x00; // Flash start address 1st : 0x0000_0000

		if (page_prog_start < 0x100)
		{
			tmp_data[3] = 0x00; 
			tmp_data[2] = 0x00; 
			tmp_data[1] = 0x00; 
			tmp_data[0] = (uint8_t)page_prog_start;
		}
		else if (page_prog_start >= 0x100 && page_prog_start < 0x10000)
		{
			tmp_data[3] = 0x00; 
			tmp_data[2] = 0x00; 
			tmp_data[1] = (uint8_t)(page_prog_start >> 8); 
			tmp_data[0] = (uint8_t)page_prog_start;
		}
		else if (page_prog_start >= 0x10000 && page_prog_start < 0x1000000)
		{
			tmp_data[3] = 0x00; 
			tmp_data[2] = (uint8_t)(page_prog_start >> 16); 
			tmp_data[1] = (uint8_t)(page_prog_start >> 8); 
			tmp_data[0] = (uint8_t)page_prog_start;
		}
		
		himax_flash_write_burst(client, tmp_addr, tmp_data);


		//=================================
		// Send 16 bytes data : 0x8000_002C ==> 16 bytes data	  
		//=================================
		buring_data[0] = 0x2C;
		buring_data[1] = 0x00;
		buring_data[2] = 0x00;
		buring_data[3] = 0x80;
		
		for (i = /*0*/page_prog_start, j = 0; i < 16 + page_prog_start/**/; i++, j++)	/// <------ bin file
		{
			buring_data[j + 4] = FW_content[i];
		}


		if ( i2c_himax_write(client, 0x00 ,buring_data, 20, HIMAX_I2C_RETRY_TIMES) < 0) {
			E("%s: i2c access fail!\n", __func__);
			return;
		}
		//=================================
		// Write command : 0x8000_0024 ==> 0x0000_0002
		//=================================
		tmp_addr[3] = 0x80; tmp_addr[2] = 0x00; tmp_addr[1] = 0x00; tmp_addr[0] = 0x24;
		tmp_data[3] = 0x00; tmp_data[2] = 0x00; tmp_data[1] = 0x00; tmp_data[0] = 0x02;
		himax_flash_write_burst(client, tmp_addr, tmp_data);

		//=================================
		// Send 240 bytes data : 0x8000_002C ==> 240 bytes data	 
		//=================================

		for (j = 0; j < 5; j++)
		{
			for (i = (page_prog_start + 16 + (j * 48)), k = 0; i < (page_prog_start + 16 + (j * 48)) + program_length; i++, k++)   /// <------ bin file
			{
				buring_data[k+4] = FW_content[i];//(byte)i;
			}

			if ( i2c_himax_write(client, 0x00 ,buring_data, program_length+4, HIMAX_I2C_RETRY_TIMES) < 0) {
				E("%s: i2c access fail!\n", __func__);
				return;
			}

		}

		if (!wait_wip(client, 1))
			E("%s:83100_Flash_Programming Fail\n", __func__);
	}
}

bool himax_check_chip_version(struct i2c_client *client)
{
	uint8_t tmp_addr[4];
	uint8_t tmp_data[4];
	uint8_t ret_data = 0x00;
	int i = 0;
	int ret = 0;
	himax_sense_off(client);
	for (i = 0; i < 5; i++)
	{
		// 1. Set DDREG_Req = 1 (0x9000_0020 = 0x0000_0001) (Lock register R/W from driver)
		tmp_addr[3] = 0x90; tmp_addr[2] = 0x00; tmp_addr[1] = 0x00; tmp_addr[0] = 0x20;
		tmp_data[3] = 0x00; tmp_data[2] = 0x00; tmp_data[1] = 0x00; tmp_data[0] = 0x01;
		ret = himax_register_write(client, tmp_addr, 1, tmp_data);
		if(ret)
			return false;

		// 2. Set bank as 0 (0x8001_BD01 = 0x0000_0000)
		tmp_addr[3] = 0x80; tmp_addr[2] = 0x01; tmp_addr[1] = 0xBD; tmp_addr[0] = 0x01;
		tmp_data[3] = 0x00; tmp_data[2] = 0x00; tmp_data[1] = 0x00; tmp_data[0] = 0x00;
		ret = himax_register_write(client, tmp_addr, 1, tmp_data);
		if(ret)
			return false;

		// 3. Read driver ID register RF4H 1 byte (0x8001_F401)
		//	  Driver register RF4H 1 byte value = 0x84H, read back value will become 0x84848484
		tmp_addr[3] = 0x80; tmp_addr[2] = 0x01; tmp_addr[1] = 0xF4; tmp_addr[0] = 0x01;
		himax_register_read(client, tmp_addr, 1, tmp_data);
		ret_data = tmp_data[0];

		I("%s:Read driver IC ID = %X\n", __func__, ret_data);
		if (ret_data == 0x84)
		{
			IC_TYPE         = HX_83100_SERIES_PWON;
			//himax_sense_on(client, 0x01);
			ret_data = true;
			break;
		}
		else
		{
			ret_data = false;
			E("%s:Read driver ID register Fail:\n", __func__);
		}
	}
	// 4. After read finish, set DDREG_Req = 0 (0x9000_0020 = 0x0000_0000) (Unlock register R/W from driver)
	tmp_addr[3] = 0x90; tmp_addr[2] = 0x00; tmp_addr[1] = 0x00; tmp_addr[0] = 0x20;
	tmp_data[3] = 0x00; tmp_data[2] = 0x00; tmp_data[1] = 0x00; tmp_data[0] = 0x00;
	himax_register_write(client, tmp_addr, 1, tmp_data);
	//himax_sense_on(client, 0x01);
	return ret_data;
}

#if 1
int himax_check_CRC(struct i2c_client *client, int mode)
{
	bool burnFW_success = false;
	uint8_t tmp_addr[4];
	uint8_t tmp_data[4];
	int tmp_value;
	int CRC_value = 0;

	memset(tmp_data, 0x00, sizeof(tmp_data));

	if (1)
	{
		if(mode == fw_image_60k)
		{
			himax_sram_write(client, (i_TP_CRC_FW_60K));
			burnFW_success = himax_sram_verify(client, i_TP_CRC_FW_60K, 0x4000);
		}
		else if(mode == fw_image_64k)
		{
			himax_sram_write(client, (i_TP_CRC_FW_64K));
			burnFW_success = himax_sram_verify(client, i_TP_CRC_FW_64K, 0x4000);
		}
		else if(mode == fw_image_124k)
		{
			himax_sram_write(client, (i_TP_CRC_FW_124K));
			burnFW_success = himax_sram_verify(client, i_TP_CRC_FW_124K, 0x4000);
		}
		else if(mode == fw_image_128k)
		{
			himax_sram_write(client, (i_TP_CRC_FW_128K));
			burnFW_success = himax_sram_verify(client, i_TP_CRC_FW_128K, 0x4000);
		}
		if (burnFW_success)
		{
			I("%s: Start to do CRC FW mode=%d \n", __func__,mode);
			himax_sense_on(client, 0x00);	// run CRC firmware

			while(true)
			{
				msleep(100);

				tmp_addr[3] = 0x90; 
				tmp_addr[2] = 0x08; 
				tmp_addr[1] = 0x80; 
				tmp_addr[0] = 0x94;
				himax_register_read(client, tmp_addr, 1, tmp_data);

				I("%s: CRC from firmware is %x, %x, %x, %x \n", __func__,tmp_data[3],
					tmp_data[2],tmp_data[1],tmp_data[0]);

				if (tmp_data[3] == 0xFF && tmp_data[2] == 0xFF && tmp_data[1] == 0xFF && tmp_data[0] == 0xFF)
				{ 
					}
				else
					break;
			}

			CRC_value = tmp_data[3];

			tmp_value = tmp_data[2] << 8;
			CRC_value += tmp_value;

			tmp_value = tmp_data[1] << 16;
			CRC_value += tmp_value;

			tmp_value = tmp_data[0] << 24;
			CRC_value += tmp_value;

			I("%s: CRC Value is %x \n", __func__, CRC_value);

			//Close Remapping
	        //=====================================
	        //          Re-map close
	        //=====================================
	        tmp_addr[3] = 0x90; tmp_addr[2] = 0x00; tmp_addr[1] = 0x00; tmp_addr[0] = 0x00;
	        tmp_data[3] = 0x00; tmp_data[2] = 0x00; tmp_data[1] = 0x00; tmp_data[0] = 0x00;
	        himax_flash_write_burst_lenth(client, tmp_addr, tmp_data, 4);
			return CRC_value;				
		}
		else
		{
			E("%s: SRAM write fail\n", __func__);
			return 0;
		}		
	}
	else
		I("%s: NO CRC Check File \n", __func__);

	return 0;
}

bool Calculate_CRC_with_AP(unsigned char *FW_content , int CRC_from_FW, int mode)
{
	uint8_t tmp_data[4];
	int i, j;
	int fw_data;
	int fw_data_2;
	int CRC = 0xFFFFFFFF;
	int PolyNomial = 0x82F63B78;
	int length = 0;
	
	if (mode == fw_image_128k)
		length = 0x8000;
	else if (mode == fw_image_124k)
		length = 0x7C00;
	else if (mode == fw_image_64k)
		length = 0x4000;
	else //if (mode == fw_image_60k)
		length = 0x3C00;

	for (i = 0; i < length; i++)
	{
		fw_data = FW_content[i * 4 ];
		
		for (j = 1; j < 4; j++)
		{
			fw_data_2 = FW_content[i * 4 + j];
			fw_data += (fw_data_2) << (8 * j);
		}

		CRC = fw_data ^ CRC;

		for (j = 0; j < 32; j++)
		{
			if ((CRC % 2) != 0)
			{
				CRC = ((CRC >> 1) & 0x7FFFFFFF) ^ PolyNomial;				
			}
			else
			{
				CRC = (((CRC >> 1) ^ 0x7FFFFFFF)& 0x7FFFFFFF);				
			}
		}
	}

	I("%s: CRC calculate from bin file is %x \n", __func__, CRC);

	tmp_data[0] = (uint8_t)(CRC >> 24);
	tmp_data[1] = (uint8_t)(CRC >> 16);
	tmp_data[2] = (uint8_t)(CRC >> 8);
	tmp_data[3] = (uint8_t) CRC;

	CRC = tmp_data[0];
	CRC += tmp_data[1] << 8;			
	CRC += tmp_data[2] << 16;
	CRC += tmp_data[3] << 24;

	I("%s: CRC calculate from bin file REVERSE %x \n", __func__, CRC);
	I("%s: CRC calculate from FWis %x \n", __func__, CRC_from_FW);
	if (CRC_from_FW == CRC)
		return true;
	else
		return false;
}
#endif

int fts_ctpm_fw_upgrade_with_sys_fs_60k(struct i2c_client *client, unsigned char *fw, int len, bool change_iref)
{
	int CRC_from_FW = 0;
	int burnFW_success = 0;

	if (len != 0x10000)   //64k
    {
    	E("%s: The file size is not 64K bytes\n", __func__);
    	return false;
		}
	himax_sense_off(client);
	msleep(500);
	himax_interface_on(client);
  if (!himax_sector_erase(client, 0x00000))
			{
            E("%s:Sector erase fail!Please restart the IC.\n", __func__);
            return false;
      }
	himax_flash_programming(client, fw, 0x0F000);

	//burnFW_success = himax_83100_Verify(fw, len);
	//if(burnFW_success==false)
	//	return burnFW_success;

	CRC_from_FW = himax_check_CRC(client,fw_image_60k);
	burnFW_success = Calculate_CRC_with_AP(fw, CRC_from_FW,fw_image_60k);
	//himax_sense_on(client, 0x01);
	return burnFW_success;
}

int fts_ctpm_fw_upgrade_with_sys_fs_64k(struct i2c_client *client, unsigned char *fw, int len, bool change_iref)
{
	int CRC_from_FW = 0;
	int burnFW_success = 0;

	if (len != 0x10000)   //64k
  {
    	E("%s: The file size is not 64K bytes\n", __func__);
    	return false;
	}
	himax_sense_off(client);
	msleep(500);
	himax_interface_on(client);
	himax_chip_erase(client);
	himax_flash_programming(client, fw, len);

	//burnFW_success = himax_83100_Verify(fw, len);
	//if(burnFW_success==false)
	//	return burnFW_success;

	CRC_from_FW = himax_check_CRC(client,fw_image_64k);
	burnFW_success = Calculate_CRC_with_AP(fw, CRC_from_FW,fw_image_64k);
	//himax_sense_on(client, 0x01);
	return burnFW_success;
}

int fts_ctpm_fw_upgrade_with_sys_fs_124k(struct i2c_client *client, unsigned char *fw, int len, bool change_iref)
{
	int CRC_from_FW = 0;
	int burnFW_success = 0;

	if (len != 0x20000)   //128k
  {
    	E("%s: The file size is not 128K bytes\n", __func__);
    	return false;
	}
	himax_sense_off(client);
	msleep(500);
	himax_interface_on(client);
	if (!himax_block_erase(client))
    	{
            E("%s:Block erase fail!Please restart the IC.\n", __func__);
            return false;
      }

    if (!himax_sector_erase(client, 0x10000))
    	{
            E("%s:Sector erase fail!Please restart the IC.\n", __func__);
            return false;
      }
	himax_flash_programming(client, fw, 0x1F000);


	//burnFW_success = himax_83100_Verify(fw, len);
	//if(burnFW_success==false)
	//	return burnFW_success;

	CRC_from_FW = himax_check_CRC(client,fw_image_124k);
	burnFW_success = Calculate_CRC_with_AP(fw, CRC_from_FW,fw_image_124k);
	//himax_sense_on(client, 0x01);
	return burnFW_success;
}

int fts_ctpm_fw_upgrade_with_sys_fs_128k(struct i2c_client *client, unsigned char *fw, int len, bool change_iref)
{
	int CRC_from_FW = 0;
	int burnFW_success = 0;

	if (len != 0x20000)   //128k
  {
    	E("%s: The file size is not 128K bytes\n", __func__);
    	return false;
	}
	himax_sense_off(client);
	msleep(500);
	himax_interface_on(client);
	himax_chip_erase(client);

	himax_flash_programming(client, fw, len);

	//burnFW_success = himax_83100_Verify(fw, len);
	//if(burnFW_success==false)
	//	return burnFW_success;

	CRC_from_FW = himax_check_CRC(client,fw_image_128k);
	burnFW_success = Calculate_CRC_with_AP(fw, CRC_from_FW,fw_image_128k);
	//himax_sense_on(client, 0x01);
	return burnFW_success;
}

void himax_touch_information(struct i2c_client *client)
{
	uint8_t cmd[4];
	char data[12] = {0};

	I("%s:IC_TYPE =%d\n", __func__,IC_TYPE);

	if(IC_TYPE == HX_83100_SERIES_PWON)
	{
		cmd[3] = 0x08; cmd[2] = 0x00; cmd[1] = 0x00; cmd[0] = 0xF8;
		himax_register_read(client, cmd, 1, data);

		ic_data->HX_RX_NUM				= data[1];
		ic_data->HX_TX_NUM				= data[2];
		ic_data->HX_MAX_PT				= data[3];

		cmd[3] = 0x08; cmd[2] = 0x00; cmd[1] = 0x00; cmd[0] = 0xFC;
		himax_register_read(client, cmd, 1, data);

	  if((data[1] & 0x04) == 0x04) {
			ic_data->HX_XY_REVERSE = true;
	  } else {
			ic_data->HX_XY_REVERSE = false;
	  }
		ic_data->HX_Y_RES = data[3]*256;
		cmd[3] = 0x08; cmd[2] = 0x00; cmd[1] = 0x01; cmd[0] = 0x00;
		himax_register_read(client, cmd, 1, data);
		ic_data->HX_Y_RES = ic_data->HX_Y_RES + data[0];
		ic_data->HX_X_RES = data[1]*256 + data[2];
		cmd[3] = 0x08; cmd[2] = 0x00; cmd[1] = 0x00; cmd[0] = 0x8C;
		himax_register_read(client, cmd, 1, data);
	  if((data[0] & 0x01) == 1) {
			ic_data->HX_INT_IS_EDGE = true;
	  } else {
			ic_data->HX_INT_IS_EDGE = false;
	  }
	  if (ic_data->HX_RX_NUM > 40)
			ic_data->HX_RX_NUM = 29;
	  if (ic_data->HX_TX_NUM > 20)
			ic_data->HX_TX_NUM = 16;
	  if (ic_data->HX_MAX_PT > 10)
			ic_data->HX_MAX_PT = 10;
	  if (ic_data->HX_Y_RES > 2000)
			ic_data->HX_Y_RES = 1280;
	  if (ic_data->HX_X_RES > 2000)
			ic_data->HX_X_RES = 720;
#ifdef HX_EN_MUT_BUTTON
		cmd[3] = 0x08; cmd[2] = 0x00; cmd[1] = 0x00; cmd[0] = 0xE8;
		himax_register_read(client, cmd, 1, data);
		ic_data->HX_BT_NUM				= data[3];
#endif
		I("%s:HX_RX_NUM =%d,HX_TX_NUM =%d,HX_MAX_PT=%d \n", __func__,ic_data->HX_RX_NUM,ic_data->HX_TX_NUM,ic_data->HX_MAX_PT);
		I("%s:HX_XY_REVERSE =%d,HX_Y_RES =%d,HX_X_RES=%d \n", __func__,ic_data->HX_XY_REVERSE,ic_data->HX_Y_RES,ic_data->HX_X_RES);
		I("%s:HX_INT_IS_EDGE =%d \n", __func__,ic_data->HX_INT_IS_EDGE);
	}
	else
	{
		ic_data->HX_RX_NUM				= 0;
		ic_data->HX_TX_NUM				= 0;
		ic_data->HX_BT_NUM				= 0;
		ic_data->HX_X_RES				= 0;
		ic_data->HX_Y_RES				= 0;
		ic_data->HX_MAX_PT				= 0;
		ic_data->HX_XY_REVERSE		= false;
		ic_data->HX_INT_IS_EDGE		= false;
	}
}

void himax_read_FW_ver(struct i2c_client *client)
{
	uint8_t cmd[4];
	uint8_t data[64] = {0};

	//=====================================
	// Read FW version : 0x0000_E303
	//=====================================
	cmd[3] = 0x00; cmd[2] = 0x00; cmd[1] = 0xE3; cmd[0] = 0x00;
	himax_register_read(client, cmd, 1, data);

	ic_data->vendor_config_ver = data[3]<<8;

	cmd[3] = 0x00; cmd[2] = 0x00; cmd[1] = 0xE3; cmd[0] = 0x04;
	himax_register_read(client, cmd, 1, data);

	ic_data->vendor_config_ver = data[0] | ic_data->vendor_config_ver;
	I("CFG_VER : %X \n",ic_data->vendor_config_ver);

	cmd[3] = 0x08; cmd[2] = 0x00; cmd[1] = 0x00; cmd[0] = 0x28;
	himax_register_read(client, cmd, 1, data);

	ic_data->vendor_fw_ver = data[0]<<8 | data[1];
	I("FW_VER : %X \n",ic_data->vendor_fw_ver);


	return;
}

bool himax_ic_package_check(struct i2c_client *client)
{
#if 0
	uint8_t cmd[3];
	uint8_t data[3];

	memset(cmd, 0x00, sizeof(cmd));
	memset(data, 0x00, sizeof(data));

	if (i2c_himax_read(client, 0xD1, cmd, 3, HIMAX_I2C_RETRY_TIMES) < 0)
		return false ;

	if (i2c_himax_read(client, 0x31, data, 3, HIMAX_I2C_RETRY_TIMES) < 0)
		return false;

	if((data[0] == 0x85 && data[1] == 0x29))
		{
			IC_TYPE         = HX_85XX_F_SERIES_PWON;
    	IC_CHECKSUM 		= HX_TP_BIN_CHECKSUM_CRC;
    	//Himax: Set FW and CFG Flash Address                                          
    	FW_VER_MAJ_FLASH_ADDR   = 64901;  //0xFD85                              
    	FW_VER_MAJ_FLASH_LENG   = 1;
    	FW_VER_MIN_FLASH_ADDR   = 64902;  //0xFD86                                     
    	FW_VER_MIN_FLASH_LENG   = 1;
    	CFG_VER_MAJ_FLASH_ADDR 	= 64928;   //0xFDA0         
    	CFG_VER_MAJ_FLASH_LENG 	= 12;
    	CFG_VER_MIN_FLASH_ADDR 	= 64940;   //0xFDAC
    	CFG_VER_MIN_FLASH_LENG 	= 12;
			I("Himax IC package 852x F\n");
			}
	if((data[0] == 0x85 && data[1] == 0x30) || (cmd[0] == 0x05 && cmd[1] == 0x85 && cmd[2] == 0x29))
		{
			IC_TYPE 		= HX_85XX_E_SERIES_PWON;
			IC_CHECKSUM = HX_TP_BIN_CHECKSUM_CRC;
			//Himax: Set FW and CFG Flash Address                                          
			FW_VER_MAJ_FLASH_ADDR	= 133;	//0x0085                              
			FW_VER_MAJ_FLASH_LENG	= 1;                                               
			FW_VER_MIN_FLASH_ADDR	= 134;  //0x0086                                     
			FW_VER_MIN_FLASH_LENG	= 1;                                                
			CFG_VER_MAJ_FLASH_ADDR = 160;	//0x00A0         
			CFG_VER_MAJ_FLASH_LENG = 12; 	                                 
			CFG_VER_MIN_FLASH_ADDR = 172;	//0x00AC
			CFG_VER_MIN_FLASH_LENG = 12;   
			I("Himax IC package 852x E\n");
		}
	else if((data[0] == 0x85 && data[1] == 0x31))
		{
			IC_TYPE         = HX_85XX_ES_SERIES_PWON;
    	IC_CHECKSUM 		= HX_TP_BIN_CHECKSUM_CRC;
    	//Himax: Set FW and CFG Flash Address                                          
    	FW_VER_MAJ_FLASH_ADDR   = 133;  //0x0085                              
    	FW_VER_MAJ_FLASH_LENG   = 1;
    	FW_VER_MIN_FLASH_ADDR   = 134;  //0x0086                                     
    	FW_VER_MIN_FLASH_LENG   = 1;
    	CFG_VER_MAJ_FLASH_ADDR 	= 160;   //0x00A0         
    	CFG_VER_MAJ_FLASH_LENG 	= 12;
    	CFG_VER_MIN_FLASH_ADDR 	= 172;   //0x00AC
    	CFG_VER_MIN_FLASH_LENG 	= 12;
			I("Himax IC package 852x ES\n");
			}
	else if ((data[0] == 0x85 && data[1] == 0x28) || (cmd[0] == 0x04 && cmd[1] == 0x85 &&
		(cmd[2] == 0x26 || cmd[2] == 0x27 || cmd[2] == 0x28))) {
		IC_TYPE                = HX_85XX_D_SERIES_PWON;
		IC_CHECKSUM            = HX_TP_BIN_CHECKSUM_CRC;
		//Himax: Set FW and CFG Flash Address
		FW_VER_MAJ_FLASH_ADDR  = 133;                    // 0x0085
		FW_VER_MAJ_FLASH_LENG  = 1;
		FW_VER_MIN_FLASH_ADDR  = 134;                    // 0x0086
		FW_VER_MIN_FLASH_LENG  = 1;
		CFG_VER_MAJ_FLASH_ADDR = 160;                    // 0x00A0
		CFG_VER_MAJ_FLASH_LENG = 12;
		CFG_VER_MIN_FLASH_ADDR = 172;                    // 0x00AC
		CFG_VER_MIN_FLASH_LENG = 12;
		I("Himax IC package 852x D\n");
	} else if ((data[0] == 0x85 && data[1] == 0x23) || (cmd[0] == 0x03 && cmd[1] == 0x85 &&
			(cmd[2] == 0x26 || cmd[2] == 0x27 || cmd[2] == 0x28 || cmd[2] == 0x29))) {
		IC_TYPE                = HX_85XX_C_SERIES_PWON;
		IC_CHECKSUM            = HX_TP_BIN_CHECKSUM_SW;
		//Himax: Set FW and CFG Flash Address
		FW_VER_MAJ_FLASH_ADDR  = 133;                   // 0x0085
		FW_VER_MAJ_FLASH_LENG  = 1;
		FW_VER_MIN_FLASH_ADDR  = 134;                   // 0x0086
		FW_VER_MIN_FLASH_LENG  = 1;
		CFG_VER_MAJ_FLASH_ADDR = 135;                   // 0x0087
		CFG_VER_MAJ_FLASH_LENG = 12;
		CFG_VER_MIN_FLASH_ADDR = 147;                   // 0x0093
		CFG_VER_MIN_FLASH_LENG = 12;
		I("Himax IC package 852x C\n");
	} else if ((data[0] == 0x85 && data[1] == 0x26) ||
		   (cmd[0] == 0x02 && cmd[1] == 0x85 &&
		   (cmd[2] == 0x19 || cmd[2] == 0x25 || cmd[2] == 0x26))) {
		IC_TYPE                = HX_85XX_B_SERIES_PWON;
		IC_CHECKSUM            = HX_TP_BIN_CHECKSUM_SW;
		//Himax: Set FW and CFG Flash Address
		FW_VER_MAJ_FLASH_ADDR  = 133;                   // 0x0085
		FW_VER_MAJ_FLASH_LENG  = 1;
		FW_VER_MIN_FLASH_ADDR  = 728;                   // 0x02D8
		FW_VER_MIN_FLASH_LENG  = 1;
		CFG_VER_MAJ_FLASH_ADDR = 692;                   // 0x02B4
		CFG_VER_MAJ_FLASH_LENG = 3;
		CFG_VER_MIN_FLASH_ADDR = 704;                   // 0x02C0
		CFG_VER_MIN_FLASH_LENG = 3;
		I("Himax IC package 852x B\n");
	} else if ((data[0] == 0x85 && data[1] == 0x20) || (cmd[0] == 0x01 &&
			cmd[1] == 0x85 && cmd[2] == 0x19)) {
		IC_TYPE     = HX_85XX_A_SERIES_PWON;
		IC_CHECKSUM = HX_TP_BIN_CHECKSUM_SW;
		I("Himax IC package 852x A\n");
	} else {
		E("Himax IC package incorrect!!\n");
	}*/
#else
		IC_TYPE         = HX_83100_SERIES_PWON;
    IC_CHECKSUM 		= HX_TP_BIN_CHECKSUM_CRC;
    //Himax: Set FW and CFG Flash Address
    FW_VER_MAJ_FLASH_ADDR   = 57384;   //0xE028
    FW_VER_MAJ_FLASH_LENG   = 1;
    FW_VER_MIN_FLASH_ADDR   = 57385;   //0xE029
    FW_VER_MIN_FLASH_LENG   = 1;
    CFG_VER_MAJ_FLASH_ADDR 	= 58115;  //0xE303
    CFG_VER_MAJ_FLASH_LENG 	= 1;
    CFG_VER_MIN_FLASH_ADDR 	= 58116;  //0xE304
    CFG_VER_MIN_FLASH_LENG 	= 1;
		I("Himax IC package 83100_in\n");

#endif
	return true;
}

void himax_read_event_stack(struct i2c_client *client, 	uint8_t *buf, uint8_t length)
{
	uint8_t cmd[4];

	cmd[3] = 0x90; cmd[2] = 0x06; cmd[1] = 0x00; cmd[0] = 0x00;	
	if ( i2c_himax_write(client, 0x00 ,cmd, 4, HIMAX_I2C_RETRY_TIMES) < 0) {
		E("%s: i2c access fail!\n", __func__);		
	}

	cmd[0] = 0x00;		
	if ( i2c_himax_write(client, 0x0C ,cmd, 1, HIMAX_I2C_RETRY_TIMES) < 0) {
		E("%s: i2c access fail!\n", __func__);		
	}

	i2c_himax_read(client, 0x08, buf, length, HIMAX_I2C_RETRY_TIMES);
}

#if 0
static void himax_83100_Flash_Write(uint8_t * reg_byte, uint8_t * write_data)
{
	uint8_t tmpbyte[2];

    if ( i2c_himax_write(private_ts->client, 0x00 ,&reg_byte[0], 1, HIMAX_I2C_RETRY_TIMES) < 0) {
		E("%s: i2c access fail!\n", __func__);
		return;
	}

	if ( i2c_himax_write(private_ts->client, 0x01 ,&reg_byte[1], 1, HIMAX_I2C_RETRY_TIMES) < 0) {
		E("%s: i2c access fail!\n", __func__);
		return;
	}

	if ( i2c_himax_write(private_ts->client, 0x02 ,&reg_byte[2], 1, HIMAX_I2C_RETRY_TIMES) < 0) {
		E("%s: i2c access fail!\n", __func__);
		return;
	}

	if ( i2c_himax_write(private_ts->client, 0x03 ,&reg_byte[3], 1, HIMAX_I2C_RETRY_TIMES) < 0) {
		E("%s: i2c access fail!\n", __func__);
		return;
	}

	if ( i2c_himax_write(private_ts->client, 0x04 ,&write_data[0], 1, HIMAX_I2C_RETRY_TIMES) < 0) {
		E("%s: i2c access fail!\n", __func__);
		return;
	}

	if ( i2c_himax_write(private_ts->client, 0x05 ,&write_data[1], 1, HIMAX_I2C_RETRY_TIMES) < 0) {
		E("%s: i2c access fail!\n", __func__);
		return;
	}

	if ( i2c_himax_write(private_ts->client, 0x06 ,&write_data[2], 1, HIMAX_I2C_RETRY_TIMES) < 0) {
		E("%s: i2c access fail!\n", __func__);
		return;
	}

	if ( i2c_himax_write(private_ts->client, 0x07 ,&write_data[3], 1, HIMAX_I2C_RETRY_TIMES) < 0) {
		E("%s: i2c access fail!\n", __func__);
		return;
	}

    if (isBusrtOn == false)
    {
        tmpbyte[0] = 0x01;
		if ( i2c_himax_write(private_ts->client, 0x0C ,&tmpbyte[0], 1, HIMAX_I2C_RETRY_TIMES) < 0) {
		E("%s: i2c access fail!\n", __func__);
		return;
		}
    }
}
#endif
#if 0
static void himax_83100_Flash_Burst_Write(uint8_t * reg_byte, uint8_t * write_data)
{
    //uint8_t tmpbyte[2];
	int i = 0;

	if ( i2c_himax_write(private_ts->client, 0x00 ,&reg_byte[0], 1, HIMAX_I2C_RETRY_TIMES) < 0) {
		E("%s: i2c access fail!\n", __func__);
		return;
	}

	if ( i2c_himax_write(private_ts->client, 0x01 ,&reg_byte[1], 1, HIMAX_I2C_RETRY_TIMES) < 0) {
		E("%s: i2c access fail!\n", __func__);
		return;
	}

	if ( i2c_himax_write(private_ts->client, 0x02 ,&reg_byte[2], 1, HIMAX_I2C_RETRY_TIMES) < 0) {
		E("%s: i2c access fail!\n", __func__);
		return;
	}

	if ( i2c_himax_write(private_ts->client, 0x03 ,&reg_byte[3], 1, HIMAX_I2C_RETRY_TIMES) < 0) {
		E("%s: i2c access fail!\n", __func__);
		return;
	}

    // Write 256 bytes with continue burst mode
    for (i = 0; i < 256; i = i + 4)
    {
		if ( i2c_himax_write(private_ts->client, 0x04 ,&write_data[i], 1, HIMAX_I2C_RETRY_TIMES) < 0) {
			E("%s: i2c access fail!\n", __func__);
			return;
		}

		if ( i2c_himax_write(private_ts->client, 0x05 ,&write_data[i+1], 1, HIMAX_I2C_RETRY_TIMES) < 0) {
			E("%s: i2c access fail!\n", __func__);
			return;
		}

		if ( i2c_himax_write(private_ts->client, 0x06 ,&write_data[i+2], 1, HIMAX_I2C_RETRY_TIMES) < 0) {
			E("%s: i2c access fail!\n", __func__);
			return;
		}

		if ( i2c_himax_write(private_ts->client, 0x07 ,&write_data[i+3], 1, HIMAX_I2C_RETRY_TIMES) < 0) {
			E("%s: i2c access fail!\n", __func__);
			return;
		}
    }

    //if (isBusrtOn == false)
    //{
    //   tmpbyte[0] = 0x01;
	//	if ( i2c_himax_write(private_ts->client, 0x0C ,&tmpbyte[0], 1, 3) < 0) {
	//	E("%s: i2c access fail!\n", __func__);
	//	return;
	//	}
    //}

}
#endif

#if 0
static bool himax_83100_Verify(uint8_t *FW_File, int FW_Size)
{
	uint8_t tmp_addr[4];
	uint8_t tmp_data[4];
	uint8_t out_buffer[20];
	uint8_t in_buffer[260];

	int fail_addr=0, fail_cnt=0;
	int page_prog_start = 0;
	int i = 0;

	himax_interface_on(private_ts->client);
	himax_burst_enable(private_ts->client, 0);

	//=====================================
	// SPI Transfer Format : 0x8000_0010 ==> 0x0002_0780
	//=====================================
	tmp_addr[3] = 0x80; tmp_addr[2] = 0x00; tmp_addr[1] = 0x00; tmp_addr[0] = 0x10;
	tmp_data[3] = 0x00; tmp_data[2] = 0x02; tmp_data[1] = 0x07; tmp_data[0] = 0x80;
	himax_83100_Flash_Write(tmp_addr, tmp_data);

	for (page_prog_start = 0; page_prog_start < FW_Size; page_prog_start = page_prog_start + 256)
	{
		//=================================
		// SPI Transfer Control
		// Set 256 bytes page read : 0x8000_0020 ==> 0x6940_02FF
		// Set read start address  : 0x8000_0028 ==> 0x0000_0000
		// Set command			   : 0x8000_0024 ==> 0x0000_003B
		//=================================
		tmp_addr[3] = 0x80; tmp_addr[2] = 0x00; tmp_addr[1] = 0x00; tmp_addr[0] = 0x20;
		tmp_data[3] = 0x69; tmp_data[2] = 0x40; tmp_data[1] = 0x02; tmp_data[0] = 0xFF;
		himax_83100_Flash_Write(tmp_addr, tmp_data);

		tmp_addr[3] = 0x80; tmp_addr[2] = 0x00; tmp_addr[1] = 0x00; tmp_addr[0] = 0x28;
		if (page_prog_start < 0x100)
		{
			tmp_data[3] = 0x00;
			tmp_data[2] = 0x00;
			tmp_data[1] = 0x00;
			tmp_data[0] = (uint8_t)page_prog_start;
		}
		else if (page_prog_start >= 0x100 && page_prog_start < 0x10000)
		{
			tmp_data[3] = 0x00;
			tmp_data[2] = 0x00;
			tmp_data[1] = (uint8_t)(page_prog_start >> 8);
			tmp_data[0] = (uint8_t)page_prog_start;
		}
		else if (page_prog_start >= 0x10000 && page_prog_start < 0x1000000)
		{
			tmp_data[3] = 0x00;
			tmp_data[2] = (uint8_t)(page_prog_start >> 16);
			tmp_data[1] = (uint8_t)(page_prog_start >> 8);
			tmp_data[0] = (uint8_t)page_prog_start;
		}
		himax_83100_Flash_Write(tmp_addr, tmp_data);

		tmp_addr[3] = 0x80; tmp_addr[2] = 0x00; tmp_addr[1] = 0x00; tmp_addr[0] = 0x24;
		tmp_data[3] = 0x00; tmp_data[2] = 0x00; tmp_data[1] = 0x00; tmp_data[0] = 0x3B;
		himax_83100_Flash_Write(tmp_addr, tmp_data);

		//==================================
		// AHB_I2C Burst Read
		// Set SPI data register : 0x8000_002C ==> 0x00
		//==================================
		out_buffer[0] = 0x2C;
		out_buffer[1] = 0x00;
		out_buffer[2] = 0x00;
		out_buffer[3] = 0x80;
		i2c_himax_write(private_ts->client, 0x00 ,out_buffer, 4, HIMAX_I2C_RETRY_TIMES);

		//==================================
		// Read access : 0x0C ==> 0x00
		//==================================
		out_buffer[0] = 0x00;
		i2c_himax_write(private_ts->client, 0x0C ,out_buffer, 1, HIMAX_I2C_RETRY_TIMES);

		//==================================
		// Read 128 bytes two times
		//==================================
		i2c_himax_read(private_ts->client, 0x08 ,in_buffer, 128, HIMAX_I2C_RETRY_TIMES);
		for (i = 0; i < 128; i++)
			flash_buffer[i + page_prog_start] = in_buffer[i];

		i2c_himax_read(private_ts->client, 0x08 ,in_buffer, 128, HIMAX_I2C_RETRY_TIMES);
		for (i = 0; i < 128; i++)
			flash_buffer[(i + 128) + page_prog_start] = in_buffer[i];

		//tmp_addr[3] = 0x80; tmp_addr[2] = 0x00; tmp_addr[1] = 0x00; tmp_addr[0] = 0x2C;
		//himax_register_read(tmp_addr, 32, out in_buffer);
		//for (int i = 0; i < 128; i++)
		//	  flash_buffer[i + page_prog_start] = in_buffer[i];
		//tmp_addr[3] = 0x80; tmp_addr[2] = 0x00; tmp_addr[1] = 0x00; tmp_addr[0] = 0x2C;
		//himax_register_read(tmp_addr, 32, out in_buffer);
		//for (int i = 0; i < 128; i++)
		//	  flash_buffer[i + page_prog_start] = in_buffer[i];

		I("%s:Verify Progress: %x\n", __func__, page_prog_start);
	}

	fail_cnt = 0;
	for (i = 0; i < FW_Size; i++)
	{
		if (FW_File[i] != flash_buffer[i])
		{
			if (fail_cnt == 0)
				fail_addr = i;

			fail_cnt++;
			//E("%s Fail Block:%x\n", __func__, i);
			//return false;
		}
	}
	if (fail_cnt > 0)
	{
		E("%s:Start Fail Block:%x and fail block count=%x\n" , __func__,fail_addr,fail_cnt);
		return false;
	}

	I("%s:Byte read verify pass.\n", __func__);
	return true;

}
#endif

void himax_get_DSRAM_data(struct i2c_client *client, uint8_t *info_data)
{
	int i;
	int cnt = 0;
	unsigned char tmp_addr[4];
	unsigned char tmp_data[4];
  uint8_t max_i2c_size = 32;
	int total_size = ic_data->HX_TX_NUM * ic_data->HX_RX_NUM * 2;
	int total_size_4bytes = total_size / 4;
	int total_read_times = 0;
	unsigned long address = 0x08000468;
  tmp_addr[3] = 0x08; tmp_addr[2] = 0x00; tmp_addr[1] = 0x04; tmp_addr[0] = 0x64;
	tmp_data[3] = 0x00; tmp_data[2] = 0x00; tmp_data[1] = 0x5A; tmp_data[0] = 0xA5;
	himax_flash_write_burst(client, tmp_addr, tmp_data);
  do
	{
		cnt++;
		himax_register_read(client, tmp_addr, 1, tmp_data);
		usleep_range(10000, 20000);
	} while ((tmp_data[1] != 0xA5 || tmp_data[0] != 0x5A) && cnt < 100);
  tmp_addr[3] = 0x08; tmp_addr[2] = 0x00; tmp_addr[1] = 0x04; tmp_addr[0] = 0x68;
  if (total_size_4bytes % max_i2c_size == 0)
	{
		total_read_times = total_size_4bytes / max_i2c_size;
	}
	else
	{
		total_read_times = total_size_4bytes / max_i2c_size + 1;
	}
	for (i = 0; i < (total_read_times); i++)
	{
		if ( total_size_4bytes >= max_i2c_size)
		{
			himax_register_read(client, tmp_addr, max_i2c_size, &info_data[i*max_i2c_size*4]);
			total_size_4bytes = total_size_4bytes - max_i2c_size;
		}
		else
		{
			himax_register_read(client, tmp_addr, total_size_4bytes % max_i2c_size, &info_data[i*max_i2c_size*4]);
		}
		address += max_i2c_size*4;
		tmp_addr[1] = (uint8_t)((address>>8)&0x00FF);
		tmp_addr[0] = (uint8_t)((address)&0x00FF);
	}
	tmp_addr[3] = 0x08; tmp_addr[2] = 0x00; tmp_addr[1] = 0x04; tmp_addr[0] = 0x64;
	tmp_data[3] = 0x11; tmp_data[2] = 0x22; tmp_data[1] = 0x33; tmp_data[0] = 0x44;
	himax_flash_write_burst(client, tmp_addr, tmp_data);
}
//ts_work
int cal_data_len(int raw_cnt_rmd, int HX_MAX_PT, int raw_cnt_max){
	int RawDataLen;
	if (raw_cnt_rmd != 0x00) {
		RawDataLen = 124 - ((HX_MAX_PT+raw_cnt_max+3)*4) - 1;
	}else{
		RawDataLen = 124 - ((HX_MAX_PT+raw_cnt_max+2)*4) - 1;
	}
	return RawDataLen;
}

bool read_event_stack(struct i2c_client *client, uint8_t *buf, int length)
{
	uint8_t cmd[4];

	if(length > 56)
		length = 124;
	//=====================
	//AHB I2C Burst Read
	//=====================
	cmd[0] = 0x31;
	if ( i2c_himax_write(client, 0x13 ,cmd, 1, HIMAX_I2C_RETRY_TIMES) < 0) {
		E("%s: i2c access fail!\n", __func__);
		goto err_workqueue_out;
	}

	cmd[0] = 0x10;
	if ( i2c_himax_write(client, 0x0D ,cmd, 1, HIMAX_I2C_RETRY_TIMES) < 0) {
		E("%s: i2c access fail!\n", __func__);
		goto err_workqueue_out;
	}
	//=====================
	//Read event stack
	//=====================
	cmd[3] = 0x90; cmd[2] = 0x06; cmd[1] = 0x00; cmd[0] = 0x00;
	if ( i2c_himax_write(client, 0x00 ,cmd, 4, HIMAX_I2C_RETRY_TIMES) < 0) {
		E("%s: i2c access fail!\n", __func__);
		goto err_workqueue_out;
	}

	cmd[0] = 0x00;
	if ( i2c_himax_write(client, 0x0C ,cmd, 1, HIMAX_I2C_RETRY_TIMES) < 0) {
		E("%s: i2c access fail!\n", __func__);
		goto err_workqueue_out;
	}
	i2c_himax_read(client, 0x08, buf, length,HIMAX_I2C_RETRY_TIMES);
	return 1;
	
	err_workqueue_out:
	return 0;
}

bool post_read_event_stack(struct i2c_client *client)
{
	return 1;
}
bool diag_check_sum( uint8_t hx_touch_info_size, uint8_t *buf) //return checksum value
{
	uint16_t check_sum_cal = 0;
	int i;

	//Check 124th byte CRC
	for (i = hx_touch_info_size, check_sum_cal = 0; i < 124; i=i+2)
	{
		check_sum_cal += (buf[i+1]*256 + buf[i]);
	}
	if (check_sum_cal % 0x10000 != 0)
	{
		I("%s: diag check sum fail! check_sum_cal=%X, hx_touch_info_size=%d, \n",__func__,check_sum_cal, hx_touch_info_size);
		return 0;
	}
	return 1;
}


void diag_parse_raw_data(int hx_touch_info_size, int RawDataLen, int mul_num, int self_num, uint8_t *buf, uint8_t diag_cmd, int16_t *mutual_data, int16_t *self_data)
{
	int RawDataLen_word;
	int index = 0;
	int temp1, temp2,i;
	
	if (buf[hx_touch_info_size] == 0x3A && buf[hx_touch_info_size+1] == 0xA3 && buf[hx_touch_info_size+2] > 0 && buf[hx_touch_info_size+3] == diag_cmd+5 )
	{
			RawDataLen_word = RawDataLen/2;
			index = (buf[hx_touch_info_size+2] - 1) * RawDataLen_word;
		//I("Header[%d]: %x, %x, %x, %x, mutual: %d, self: %d\n", index, buf[56], buf[57], buf[58], buf[59], mul_num, self_num);
			for (i = 0; i < RawDataLen_word; i++)
		{
			temp1 = index + i;

			if (temp1 < mul_num)
			{ //mutual
					mutual_data[index + i] = buf[i*2 + hx_touch_info_size+4+1]*256 + buf[i*2 + hx_touch_info_size+4];	//4: RawData Header, 1:HSB
			}
			else
			{//self
				temp1 = i + index;
				temp2 = self_num + mul_num;
				
				if (temp1 >= temp2)
				{
					break;
				}

					self_data[i+index-mul_num] = buf[i*2 + hx_touch_info_size+4];	//4: RawData Header
					self_data[i+index-mul_num+1] = buf[i*2 + hx_touch_info_size+4+1];
			}
		}
	}
	else
	{
		I("[HIMAX TP MSG]%s: header format is wrong!\n", __func__);
			I("Header[%d]: %x, %x, %x, %x, mutual: %d, self: %d\n", index, buf[56], buf[57], buf[58], buf[59], mul_num, self_num);
	}
}
