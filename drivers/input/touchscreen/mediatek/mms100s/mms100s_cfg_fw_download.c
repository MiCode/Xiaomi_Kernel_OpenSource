#include <linux/module.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/firmware.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/earlysuspend.h>
#include <linux/i2c.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/input/mt.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/completion.h>
#include <linux/init.h>
#include <linux/dma-mapping.h>
#include <asm/uaccess.h>
#include <asm/unaligned.h>
//#include <mach/mt_gpio.h>
#include "tpd.h"
#include "mms100s_ts.h"

#define __MSG_DMA_MODE__

#define	FUNC_START		TPD_DEBUG("[%s] begin\n", __FUNCTION__);
#define	FUNC_END		TPD_DEBUG("[%s] end  \n", __FUNCTION__);

extern void tpd_hw_enable(void);
extern void tpd_hw_disable(void);

static unsigned char mms_ts_cfg_data[] =
{
#include "mms_ts_cfg.fw"
};

static unsigned char mms_ts_core_data[] =
{
#include "mms_ts.fw"
};

#ifdef __MSG_DMA_MODE__
u8 *g_dma_buff_va = NULL;
u8 *g_dma_buff_pa = NULL;
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

static int HalTscrCReadI2CSeq(struct i2c_client *client, u8* read_data, u16 size)
{
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
			.addr = client->addr & I2C_MASK_FLAG | I2C_DMA_FLAG,
			.buf = g_dma_buff_pa,
			#else
			.addr = client->addr,
			.buf = read_data,
			#endif
		},
	};

	rc = i2c_transfer(client->adapter, msgs, 1);
	if( rc != 1 )
    {
		TPD_DMESG("HalTscrCReadI2CSeq error %d\n", rc);
	}
	#ifdef __MSG_DMA_MODE__
	else
	{
		memcpy(read_data, g_dma_buff_va, size);
	}
	#endif

	return rc;
	
}

static int HalTscrCDevWriteI2CSeq(struct i2c_client *client, u8* data, u16 size)
{
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
			.addr = client->addr & I2C_MASK_FLAG | I2C_DMA_FLAG,
			.buf = g_dma_buff_pa,
			#else
			.addr = client->addr,
			.buf = data,
			#endif
		},
	};
	rc = i2c_transfer(client->adapter, msgs, 1);
	if( rc != 1 )
    {
		TPD_DMESG("HalTscrCDevWriteI2CSeq error %d,addr = %d\n", rc, client->addr);
	}
	
	return rc;
}

static void mms_reboot(struct mms_ts_info *info)
{
	struct i2c_adapter *adapter = to_i2c_adapter(info->client->dev.parent);
	i2c_smbus_write_byte_data(info->client, MMS_ERASE_DEFEND, 1);

	i2c_lock_adapter(adapter);
	msleep(50);
	//mt_set_gpio_out(info->pdata->gpio_vdd_en, 0);  
	tpd_hw_disable();
	msleep(150);
	//mt_set_gpio_out(info->pdata->gpio_vdd_en, 1);  
	tpd_hw_enable();
	msleep(50);

	i2c_unlock_adapter(adapter);
}

static int mms_read_config(struct i2c_client *client, u8 *buf, u8 *get_buf,int len)
{
	u8 cmd = MMS_GET_RUN_CONF;
	int tmp = 0;
	u8 data[MMS_READ_BYTE]={0, };
	int ret = 0;
	int i=0;
	struct i2c_msg msg[3] = {
		{
			.addr = client->addr,
			.flags = 0,
			.buf = buf,
			.len = 4,
		}, {
			.addr =client ->addr,
			.flags = 0,
			.buf = &cmd,
			.len = 1,
		}, {
			.addr = client->addr,
			.flags = I2C_M_RD,
		}
	};

	FUNC_START

#if 0	
	if(len > MMS_READ_BYTE){
		for (i = 0 ; i < len ; i = i + MMS_READ_BYTE){
			msg[0].buf = buf;
			msg[2].buf = data;
			
			if(i2c_transfer(client->adapter, &msg[0],1)!=1){
				TPD_DMESG("failed to configuration read data\n");
				ret = -1;
			}
			if( i < len - MMS_READ_BYTE){
				msg[2].len = MMS_READ_BYTE;
				if(i2c_transfer(client->adapter, &msg[1], 2)!=2){
					TPD_DMESG("failed to configuration read data\n");
					ret = -1;
				}
				memcpy(&get_buf[tmp],data,MMS_READ_BYTE);
			}else{
				buf[3] = i;
				msg[2].len = i;
				if(i2c_transfer(client->adapter, &msg[1], 2)!=2){
					TPD_DMESG("failed to configuration read data\n");
					ret = -1;
				}
				memcpy(&get_buf[tmp],data, len-i );
			}
			
			tmp = tmp + MMS_READ_BYTE;
			buf[2] = buf[2] + MMS_READ_BYTE;
		}
	}
	else{	
		msg[0].buf=buf;
		msg[2].buf =get_buf;
		msg[2].len = len;
		if(i2c_transfer(client->adapter, msg, ARRAY_SIZE(msg)) != ARRAY_SIZE(msg)){
			TPD_DMESG("failed to configuration read data\n");
			ret = -1;
		}
	}
#else
	if(len > MMS_READ_BYTE){
		for (i = 0 ; i < len ; i = i + MMS_READ_BYTE){
			msg[0].buf = buf;
			msg[2].buf = data;
			
			if(HalTscrCDevWriteI2CSeq(client, buf, 4) != 1){
				TPD_DMESG("[mms_read_config][W]failed to write buf\n");
				ret = -1;
			}
			if( i < len - MMS_READ_BYTE){
				msg[2].len = MMS_READ_BYTE;
				if (HalTscrCDevWriteI2CSeq(client, &cmd, 1) != 1) {
					TPD_DMESG("[mms_read_config][W]failed to write cmd\n");
					ret = -1;
				}				
				if (HalTscrCReadI2CSeq(client, data, MMS_READ_BYTE) != 1 ){
					TPD_DMESG("[mms_read_config][R]failed to get cfg\n");
					ret = -1;
				}
				memcpy(&get_buf[tmp], data, MMS_READ_BYTE);
			}else{
				buf[3] = i;
				msg[2].len = i;
				if (HalTscrCDevWriteI2CSeq(client, &cmd, 1) != 1) {
					TPD_DMESG("[mms_read_config][W]failed to write cmd\n");
					ret = -1;
				}				
				if (HalTscrCReadI2CSeq(client, data, len-i) != 1 ){
					TPD_DMESG("[mms_read_config][R]failed to get cfg\n");
					ret = -1;
				}
				memcpy(&get_buf[tmp],data, len-i );
			}
			
			tmp = tmp + MMS_READ_BYTE;
			buf[2] = buf[2] + MMS_READ_BYTE;
		}
	}
	else{	
		msg[0].buf=buf;
		msg[2].buf =get_buf;
		msg[2].len = len;

		if(HalTscrCDevWriteI2CSeq(client, buf, 4) != 1){
			TPD_DMESG("[mms_read_config][W]failed to write buf\n");
			ret = -1;
		}
		if (HalTscrCDevWriteI2CSeq(client, &cmd, 1) != 1) {
			TPD_DMESG("[mms_read_config][W]failed to write cmd\n");
			ret = -1;
		}
		if (HalTscrCReadI2CSeq(client, get_buf, len) != 1 ){
			TPD_DMESG("[mms_read_config][R]failed to get cfg\n");
			ret = -1;
		}
	}

#endif

	FUNC_END
	return ret;
}

static int mms_verify_config(struct mms_ts_info *info,const u8 *buf,const u8 *tmp,int len)
{
	int ret = 0 ;
	if (memcmp(buf, tmp , len))
	{    
		TPD_DMESG("Run-time config verify failed\n");
		mms_reboot(info);
		mms_config_start(info);	
		ret = -1;
		info->run_count--;
	}
	if (info->run_count <= 0){
		mms_reboot(info);
		TPD_DMESG("Run-time config failed\n");
		ret = -1;	
	}
	TPD_DMESG("[mms_verify_config] verify pass\n");
	return ret;     	
}

static int mms_config_flash(struct mms_ts_info *info, const u8 *buf,const u8 len, char *text)
{
	struct i2c_client *client;
	struct i2c_msg msg;
	int ret, cnt, size;
	client = info->client;

	TPD_DMESG("[mms_config_flash] write len=%d %s\n", len, text);
#if 0	
	msg.addr = client->addr;
	msg.flags = 0;
	msg.buf = (u8 *)buf;
	msg.len = len;
	
	if(i2c_transfer(client->adapter, &msg,1) != 1){
		TPD_DMESG("failed to transfer %s data\n",text);
		mms_reboot(info);
		mms_config_start(info);
		ret = 1;
		return ret;
	}else{
		ret = 0;
	}
#else	
	for (cnt = 0; cnt < len; cnt += 128)
	{
		size  = ((len-cnt) < 128) ? (len-cnt) : 128;
		if(HalTscrCDevWriteI2CSeq(client, (buf+cnt*128), size) < 0)
		{
			TPD_DMESG("ERROR : [mms_config_flash] cnt=%d size=%d", cnt, size);
			mms_reboot(info);
			mms_config_start(info);
			ret = 1;
			return ret;
		}
		TPD_DMESG("[mms_config_flash] transfre size=%d, cnt=%d, len=%d\n", size, cnt, len);
	}		
#endif
	ret = 0;
	return ret;
}

static int mms_config_finish(struct mms_ts_info *info)
{
	struct i2c_client *client;
	u8 mms_conf_buffer[4] = {MMS_UNIVERSAL_CMD, MMS_CMD_CONTROL, MMS_SUBCMD_START,
					RUN_STOP};
	client = info->client;

	mms_config_flash(info, mms_conf_buffer, 4,"finish-packit" );
	msleep(200);
	info->tx_num = i2c_smbus_read_byte_data(client, MMS_TX_NUM);	
	info->rx_num = i2c_smbus_read_byte_data(client, MMS_RX_NUM);
	
	dev_info(&client->dev, "succeed to runtime-config firmware update\n");
	info->run_count=3;
	return 0;
}

static void mms_config_set(const u8 *fw2, void *context)
{
	struct mms_ts_info *info = context;
	struct i2c_client *client;
	struct mms_config_hdr *conf_hdr;
	struct mms_config_item **conf_item;
	int offset;
	int offset_tmp = 0;
	int i;
	u8 config_flash_set[4];
	u8 config_flash_data[5];
	u8 *config_array;
	u8 flash_array[50];
	u8 cmp_data[30];

	FUNC_START

	client = info->client;
	conf_hdr = (struct mms_config_hdr *)fw2;
	
	if ((conf_hdr->core_version & 0xff ) != info->ver[1]){
		mms_reboot(info);
		TPD_DMESG("mfsp-version is not correct : 0x%x 0x%x :: 0x%x 0x%x\n",
			conf_hdr->core_version, conf_hdr->config_version& 0xff,info->ver[1],info->ver[2]);
		//release_firmware(fw2);
		return;
	
	}
	
	TPD_DMESG("mfsp-ver & fw-ver is correct -> 0x%x : 0x%x\n",
				conf_hdr->core_version& 0xff ,info->ver[1]);

	if (conf_hdr->mark[3] != 0x02){
		mms_reboot(info);
		TPD_DMESG("failed to mfsp-version : %x \n",conf_hdr->mark[3]);
		//release_firmware(fw2);
		return;
	}
	TPD_DMESG("mfsp-version is chacked : 0x%x \n",conf_hdr->mark[3]);
	offset = conf_hdr->data_offset;
	conf_item = kzalloc(sizeof(*conf_item)*conf_hdr->data_count,GFP_KERNEL);
	if (!conf_item)
		TPD_DMESG("[mms_config_set] kzalloc allocate memory failed\n");

	
	for (i=0 ;; i++ , offset += MMS_MFSP_OFFSET)
	{

 		conf_item[i] = (struct mms_config_item *)(fw2 + offset);

		if(conf_item[i]->type == MMS_RUN_TYPE_INFO)
		{
			offset_tmp = conf_item[i]->data_blocksize;
			offset += offset_tmp;
		}

		if(conf_item[i]->type == MMS_RUN_TYPE_SINGLE  )
		{
			TPD_DEBUG("[mms_config_set]MMS_RUN_TYPE_SINGLE\n");
			config_flash_set[0] = MMS_RUN_CONF_POINTER;
			config_flash_set[1] = conf_item[i]->category;
			config_flash_set[2] = conf_item[i]->offset;
			config_flash_set[3] = conf_item[i]->datasize;	

			if(mms_config_flash(info, config_flash_set,4,"config-set"))
				break;
		}
		if(conf_item[i]->type == MMS_RUN_TYPE_ARRAY)
		{
			TPD_DEBUG("[mms_config_set]MMS_RUN_TYPE_ARRAY\n");
			config_flash_set[0] = MMS_RUN_CONF_POINTER;
			config_flash_set[1] = conf_item[i]->category;
			config_flash_set[2] = conf_item[i]->offset;
			config_flash_set[3] = conf_item[i]->datasize;

			if(mms_config_flash(info,config_flash_set,4,"array-set"))
				break;
      			
			offset_tmp = conf_item[i]->data_blocksize;
			config_array =(u8 *)(fw2 + (offset + MMS_MFSP_OFFSET));
			offset += offset_tmp;

			flash_array[0]=MMS_SET_RUN_CONF;
			memcpy(&flash_array[1], config_array, conf_item[i]->datasize);

			if(mms_config_flash(info, flash_array, conf_item[i]->datasize + 1,"array_data"))
				break;

			mms_read_config(client, config_flash_set, cmp_data, conf_item[i]->datasize);		
			if (mms_verify_config(info, &flash_array[1], cmp_data, conf_item[i]->datasize)!=0)
			{
				break;
			}
		}		
		
		config_flash_data[0] = MMS_SET_RUN_CONF;
		if(conf_item[i]->datasize == 1)
		{
			TPD_DEBUG("[mms_config_set] 1 \n");
			config_flash_data[1] = (u8)conf_item[i]->value;
			if(mms_config_flash(info,config_flash_data,2,"config-data"))
				break;
			mms_read_config(client, config_flash_set, cmp_data, 
				   conf_item[i]->datasize);

                        if (mms_verify_config(info, &config_flash_data[1], cmp_data, 1)!=0)
                        {
                                break;
                        }

		}
		else if(conf_item[i]->datasize == 2)
		{
			TPD_DEBUG("[mms_config_set] 2 \n");
			config_flash_data[1] = (u8)((conf_item[i]->value&0x00FF)>>0);
			config_flash_data[2] = (u8)((conf_item[i]->value&0xFF00)>>8);
			if(mms_config_flash(info,config_flash_data,3,"config-data"))
				break;
			mms_read_config(client, config_flash_set, cmp_data,
				   conf_item[i]->datasize);
                        if (mms_verify_config(info, &config_flash_data[1], cmp_data, 2)!=0)
                        {
                                break;
                        }
		}
		else if(conf_item[i]->datasize == 4)
		{
			TPD_DEBUG("[mms_config_set] 3 \n");
			config_flash_data[1] = (u8)((conf_item[i]->value&0x000000FF)>>0);
			config_flash_data[2] = (u8)((conf_item[i]->value&0x0000FF00)>>8);
			config_flash_data[3] = (u8)((conf_item[i]->value&0x00FF0000)>>16);
			config_flash_data[4] = (u8)((conf_item[i]->value&0xFF000000)>>24);
			if(mms_config_flash(info,config_flash_data,5,"config-data"))
				break;
			mms_read_config(client, config_flash_set, cmp_data,
				   conf_item[i]->datasize);

			if (mms_verify_config(info, &config_flash_data[1], cmp_data, 4)!=0)
			{
				break;
			}
		}
	if(conf_item[i]->type == MMS_RUN_TYPE_END )
	{
		TPD_DEBUG("[mms_config_set]MMS_RUN_TYPE_END\n");
		mms_config_finish(info);
		break;
	}
	}

	//release_firmware(fw2);
	kfree(conf_item);

	FUNC_END	
	return;
}

int mms_config_start(struct mms_ts_info *info)
{
#ifdef MMS_RUNTIME
	struct i2c_client *client;
	const char *fw_name = FW_CONFIG_NAME;
	int ret = 0;
	u8  mms_conf_buffer[4] = {MMS_UNIVERSAL_CMD, MMS_CMD_CONTROL, MMS_SUBCMD_START,
					RUN_START};
	client = info->client;

	FUNC_START
		
	msleep(50);
	if(mms_config_flash(info, mms_conf_buffer, 4, "start-packit"))
		TPD_DMESG("failed to runtime configuration start-packet\n");

#if 0 //vin
	ret = request_firmware_nowait(THIS_MODULE, true, fw_name, &client->dev, GFP_KERNEL, info, mms_config_set);
	if (ret) {
		TPD_DMESG("failed to schedule firmware update\n");
		return -EIO;
	}
#else
	mms_config_set(&mms_ts_cfg_data[0], info); 
#endif

#endif //MMS_RUNTIME

	FUNC_END
	return 0;	
}

static int get_fw_version(struct i2c_client *client, u8 *buf)
{
	u8 cmd = MMS_FW_VERSION;
	struct i2c_msg msg[2] = {
		{
			.addr = client->addr,
			.flags = 0,
			.buf = &cmd,
			.len = 1,
		}, {
			.addr = client->addr,
			.flags = I2C_M_RD,
			.buf = buf,
			.len = MAX_SECTION_NUM,
		},
	};

	return (i2c_transfer(client->adapter, msg, ARRAY_SIZE(msg)) != ARRAY_SIZE(msg));
}

int mms_get_fw_version(struct i2c_client *client, u8 *buf)
{
    return get_fw_version(client,buf);
}
EXPORT_SYMBOL(mms_get_fw_version);

static int mms_isc_read_status(struct mms_ts_info *info, u32 val)
{
	struct i2c_client *client = info->client;
	u8 cmd = ISC_CMD_READ_STATUS;
	u32 result = 0;
	int cnt = 100;
	int ret = 0;

	FUNC_START

	do {
		i2c_smbus_read_i2c_block_data(client, cmd, 4, (u8 *)&result); //vin
		//HalTscrCReadI2CSeq(client, (u8 *)&result, 4);
		TPD_DMESG("[mms_isc_read_status] result = 0x%x\n", result);
		if (result == val)
			break;
		msleep(1);
	} while (--cnt);

	if (!cnt){
		TPD_DMESG(
			"status read fail. cnt : %d, val : 0x%x != 0x%x\n",
			cnt, result, val);
	}

	FUNC_END
	return ret;
}

static int mms_isc_transfer_cmd(struct mms_ts_info *info, int cmd)
{
	int ret;
	struct i2c_client *client = info->client;
	struct isc_packet pkt = { ISC_ADDR, cmd };
	struct i2c_msg msg = {
		.addr = client->addr,
		.flags = 0,
		.len = sizeof(struct isc_packet),
		.buf = (u8 *)&pkt,
	};

	FUNC_START
		
	//ret = i2c_transfer(client->adapter, &msg, 1); 
	ret = HalTscrCDevWriteI2CSeq(client, &pkt, sizeof(struct isc_packet)); //vin
	if (ret != 1)	
	{
		TPD_DMESG("[mms_isc_transfer_cmd] i2c_transfer failed\n");
		return 1;
	}

	FUNC_END

	return 0;

}

static int mms_isc_erase_page(struct mms_ts_info *info, int page)
{
	return mms_isc_transfer_cmd(info, ISC_CMD_PAGE_ERASE | page) ||
		mms_isc_read_status(info, ISC_PAGE_ERASE_DONE | ISC_PAGE_ERASE_ENTER | page);
}

static int mms_isc_enter(struct mms_ts_info *info)
{
	FUNC_START
	return i2c_smbus_write_byte_data(info->client, MMS_CMD_ENTER_ISC, true);
	FUNC_END
}

static int mms_isc_exit(struct mms_ts_info *info)
{
	return mms_isc_transfer_cmd(info, ISC_CMD_EXIT);
}

static int mms_flash_section(struct mms_ts_info *info, struct mms_fw_img *img, const u8 *data)
{
	struct i2c_client *client = info->client;
	struct isc_packet *isc_packet;
	int ret;
	int page, i;
	struct i2c_msg msg[2] = {
		{
			.addr = client->addr,
			.flags = 0,
		}, {
			.addr = client->addr,
			.flags = I2C_M_RD,
			.len = ISC_XFER_LEN,
		},
	};
	int ptr = img->offset;

	isc_packet = kzalloc(sizeof(*isc_packet) + ISC_XFER_LEN, GFP_KERNEL);
	isc_packet->cmd = ISC_ADDR;

	msg[0].buf = (u8 *)isc_packet;
	msg[1].buf = kzalloc(ISC_XFER_LEN, GFP_KERNEL);

	for (page = img->start_page; page <= img->end_page; page++) {
		mms_isc_erase_page(info, page);

		for (i = 0; i < ISC_BLOCK_NUM; i++, ptr += ISC_XFER_LEN) {
			/* flash firmware */
			u32 tmp = page * 256 + i * (ISC_XFER_LEN / 4);
			put_unaligned_le32(tmp, &isc_packet->addr);
			msg[0].len = sizeof(struct isc_packet) + ISC_XFER_LEN;

			memcpy(isc_packet->data, data + ptr, ISC_XFER_LEN);
#if 0 //vin			
			if (i2c_transfer(client->adapter, msg, 1) != 1)
				goto i2c_err;
#else
			if (HalTscrCDevWriteI2CSeq(client, msg[0].buf, msg[0].len) != 1)
				goto i2c_err;		
#endif
		}
	}

	TPD_DMESG("section [%d] update succeeded\n", img->type);

	ret = 0;
	goto out;

i2c_err:
	TPD_DMESG( "i2c failed @ %s\n", __func__);
	ret = -1;

out:
	kfree(isc_packet);
	kfree(msg[1].buf);

	return ret;
}

static int mms_flash_fw(const u8 *fw, struct mms_ts_info *info)
{
	int ret;
	int i;
	struct mms_bin_hdr *fw_hdr;
	struct mms_fw_img **img;
	struct i2c_client *client = info->client;
	u8 ver[MAX_SECTION_NUM];
	u8 target[MAX_SECTION_NUM];
	int offset = sizeof(struct mms_bin_hdr);
	int retires = 3;
	bool update_flag = false;
	bool isc_flag = true;
	fw_hdr = (struct mms_bin_hdr *)fw;

	FUNC_START
	
	img = kzalloc(sizeof(*img) * fw_hdr->section_num, GFP_KERNEL);

	while (retires--) {
		if (!get_fw_version(client, ver)){
			memcpy(info->ver,ver,MAX_SECTION_NUM); 
			break;
		}else{
			mms_reboot(info);
		}
	}

	if (retires < 0) {
		TPD_DMESG("failed to obtain ver. info\n");

		#if FW_DL_To_Change_IIC_Address
		client->addr = 0x48;//anderson
		#endif
		
		isc_flag = false;
		memset(ver, 0xff, sizeof(ver));
		return -1;
	} else {
		print_hex_dump(KERN_INFO, "mms_ts fw ver : ", DUMP_PREFIX_NONE, 16, 1,
				ver, MAX_SECTION_NUM, false);
		TPD_DMESG("mms_ts fw ver : 0x%x, 0x%x, 0x%x\n", ver[0], ver[1], ver[2]);

		//client->addr = 0x39;//anderson
	}

	for (i = 0; i < fw_hdr->section_num; i++, offset += sizeof(struct mms_fw_img)) {
		img[i] = (struct mms_fw_img *)(fw + offset);
		target[i] = img[i]->version;

		if (ver[img[i]->type] != target[i]) {
			if(isc_flag){
				if (mms_isc_enter(info) < 0)
					TPD_DMESG("[mms_flash_fw] isc enter cmd failed\n");
				isc_flag = false;
			}
			update_flag = true;
			TPD_DMESG("section [%d] is need to be updated. ver : 0x%x, bin : 0x%x\n",
				img[i]->type, ver[img[i]->type], target[i]);

			if ((ret = mms_flash_section(info, img[i], fw + fw_hdr->binary_offset))) {
				mms_reboot(info);
				goto out;
			}
		
		} else {
			TPD_DMESG("section [%d] is up to date\n", i);
		}
	}

	if (update_flag){
		mms_isc_exit(info);

		#if FW_DL_To_Change_IIC_Address
		client->addr = 0x39;//anderson
		#endif
		
		msleep(5);
		mms_reboot(info);
	}

	if (get_fw_version(client, ver)) {
		TPD_DMESG("failed to obtain version after flash\n");
		ret = -1;
		goto out;
	} else {
		for (i = 0; i < fw_hdr->section_num; i++) {
			if (ver[img[i]->type] != target[i]) {
			TPD_DMESG("version mismatch after flash. [%d] 0x%x != 0x%x\n",
					i, ver[img[i]->type], target[i]);

				ret = -1;
				goto out;
			}
		}
		TPD_DMESG("[mms_flash_fw]version match after flash\n");
	}
	ret = 0;

out:
	//client->addr = 0x39;//anderson
	
	kfree(img);
	mms_config_start(info);	
	//release_firmware(fw);

	FUNC_END
	return ret;
}

void mms_fw_update_controller(const struct firmware *fw, void * context)
{
	struct mms_ts_info *info = context;
	int retires = 3;
	int ret;

	FUNC_START

#ifdef __MSG_DMA_MODE__
	msg_dma_alloct();
#endif

	/* vin
	if (!fw) {
		TPD_DMESG(&info->client->dev, "failed to read firmware\n");
		complete_all(&info->init_done);
		return;
	}
	*/

	do {
		ret = mms_flash_fw(&mms_ts_core_data[0], info);
	} while (ret && --retires);

	if (!retires) {
		TPD_DMESG("failed to flash firmware after retires\n");
	}

#ifdef __MSG_DMA_MODE__
	msg_dma_release();
#endif

	FUNC_END
}

