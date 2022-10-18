#include "cam_ispv3_dev.h"
#include "cam_req_mgr_dev.h"
#include "cam_ispv3_core.h"
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/of_platform.h>
#include <linux/workqueue.h>
#include <linux/genalloc.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/remoteproc.h>
#include <linux/component.h>


static void cam_ispv3_subdev_handle_message(
		struct v4l2_subdev *sd,
		enum cam_subdev_message_type_t message_type,
		void *data)
{
	struct cam_ispv3_ctrl_t *s_ctrl = v4l2_get_subdevdata(sd);

	if (s_ctrl->ispv3_state == CAM_ISPV3_ACQUIRE){
		switch (message_type) {
			case CAM_SUBDEV_MESSAGE_SOF:
				s_ctrl->frame_id = *(uint64_t *)data;
				break;
			case CAM_SUBDEV_MESSAGE_REQ_ID:
				s_ctrl->req_id = *(uint64_t *)data;
				break;

			default:
				break;
		}
	}else{
		CAM_DBG(CAM_ISPV3, "ISPV3: %s",
			s_ctrl->device_name);
	}
}

static int cam_ispv3_pci_check_link_status(struct ispv3_image_device *priv)
{
	struct ispv3_data *data = priv->pdata;
	u16 dev_id;

	if (atomic_read(&data->pci_link_state) == ISPV3_PCI_LINK_DOWN) {
		CAM_ERR(CAM_ISPV3, "PCIe link is suspended");
		return -EINVAL;
	}

	pci_read_config_word(data->pci, PCI_DEVICE_ID, &dev_id);
	if (dev_id != ISPV3_PCI_DEVICE_ID)  {
		CAM_ERR(CAM_ISPV3, "PCI device ID mismatch, link possibly down, current ID: 0x%x",
			dev_id);
		return -EINVAL;

	}

	return 0;
}

static inline int cam_ispv3_spi_read(struct ispv3_image_device *priv,
				     uint32_t reg_addr,
				     uint32_t *data_buf,
				     uint32_t size)
{
	struct ispv3_data *data = priv->pdata;
	struct spi_transfer transfer;
	struct spi_message message;
	uint8_t *local_buf;
	uint8_t *preamble;
	int index, ret = 0;

	if (!data) {
		CAM_ERR(CAM_ISPV3, "The ispv3 data struct is NULL!");
		return -EINVAL;
	}

	if (!data->spi)
		return -ENXIO;

	local_buf = kmalloc(size + ISPV3_SPI_OP_READ_SIZE,
			    GFP_KERNEL);
	if (!local_buf)
		return -ENOMEM;

	spi_message_init(&message);
	memset(&transfer, 0, sizeof(transfer));

	local_buf[0] = ((reg_addr & MITOP_REG_MASK) == MITOP_REG_MASK) ?
			ISPV3_SPI_REG_READ_OP : ISPV3_SPI_RAM_READ_OP;
	local_buf[1] = reg_addr & 0xff;
	local_buf[2] = (reg_addr >> 8) & 0xff;
	local_buf[3] = (reg_addr >> 16) & 0xff;
	local_buf[4] = (reg_addr >> 24) & 0xff;

	transfer.tx_buf = local_buf;
	transfer.rx_buf = local_buf;
	transfer.len = size + ISPV3_SPI_OP_READ_SIZE;

	spi_message_add_tail(&transfer, &message);

	mutex_lock(&data->ispv3_interf_mutex);
	ret = spi_sync(data->spi, &message);
	mutex_unlock(&data->ispv3_interf_mutex);
	if (ret) {
		CAM_ERR(CAM_ISPV3, "spi_sync failed! ret=%d, reg_addr=%x", ret, reg_addr);
		kfree(local_buf);
		return ret;
	}

	preamble = &local_buf[5];
	for (index = 0; index < ISPV3_SPI_OP_READ_SIZE - 5; index++) {
		if (*preamble++ == 0xa5)
			break;
	}

	if (index == ISPV3_SPI_OP_READ_SIZE - 5) {
		CAM_ERR(CAM_ISPV3, "ispv3 spi read failed, not receive 0xa5, reg_addr: %x", reg_addr);
		ret = -EIO;
	} else {
		memcpy(data_buf, preamble, size);
	}

	kfree(local_buf);

	return ret;
}

static inline int cam_ispv3_spi_write(struct ispv3_image_device *priv,
				      uint32_t reg_addr,
				      uint32_t *data_buf,
				      uint32_t size)
{
	struct ispv3_data *data = priv->pdata;
	struct spi_transfer transfer;
	struct spi_message message;
	uint8_t *local_buf;
	int ret = 0;
	uint32_t offset = 0;
	uint32_t count;

	if (!data) {
		CAM_ERR(CAM_ISPV3, "The ispv3 data struct is NULL!");
		return -EINVAL;
	}

	if (!data->spi)
		return -ENXIO;

	while (size) {
		if (size > 4088) {
			count = 4088;
		} else {
			count = size;
		}
		local_buf = kmalloc(count + ISPV3_SPI_OP_WRITE_SIZE,
				    GFP_KERNEL);
		if (!local_buf)
			return -ENOMEM;

		spi_message_init(&message);
		memset(&transfer, 0, sizeof(transfer));

		local_buf[0] = ((reg_addr & MITOP_REG_MASK) == MITOP_REG_MASK) ?
				ISPV3_SPI_REG_WRITE_OP : ISPV3_SPI_RAM_WRITE_OP;
		local_buf[1] = (reg_addr + offset) & 0xff;
		local_buf[2] = ((reg_addr + offset) >> 8) & 0xff;
		local_buf[3] = ((reg_addr + offset) >> 16) & 0xff;
		local_buf[4] = ((reg_addr + offset) >> 24) & 0xff;

		memcpy(&local_buf[ISPV3_SPI_OP_WRITE_SIZE], (uint8_t *)data_buf + offset, count);

		transfer.tx_buf = local_buf;
		transfer.len = count + ISPV3_SPI_OP_WRITE_SIZE;

		spi_message_add_tail(&transfer, &message);

		mutex_lock(&data->ispv3_interf_mutex);
		ret = spi_sync(data->spi, &message);
		mutex_unlock(&data->ispv3_interf_mutex);
		if (ret) {
			kfree(local_buf);
			CAM_ERR(CAM_ISPV3, "spi_sync failed! ret=%d, reg_addr=%x", ret, reg_addr);
			return ret;
		}

		kfree(local_buf);
		size -= count;
		offset += count;
	}

	return ret;
}

int cam_ispv3_spi_change_speed(struct ispv3_image_device *priv,
			       struct cam_control *cmd)
{
	struct ispv3_data *data = priv->pdata;
	uint32_t tmp;
	int ret = 0;

	tmp = data->spi->max_speed_hz;

	if (cmd->reserved)
		data->spi->max_speed_hz = cmd->reserved;
	else
		data->spi->max_speed_hz = ISPV3_SPI_SPEED_HZ;

	ret = spi_setup(data->spi);
	if (ret)
		data->spi->max_speed_hz = tmp;

	CAM_DBG(CAM_ISPV3, "spi_setup to %d, ret = %d, current speed is %d",
		cmd->reserved, ret, data->spi->max_speed_hz);

	return ret;
}

static inline int cam_ispv3_spi_set(struct ispv3_image_device *priv,
				    uint32_t reg_addr,
				    uint32_t reg_data,
				    uint32_t size)
{
	struct ispv3_data *data = priv->pdata;
	struct spi_transfer transfer;
	struct spi_message message;
	uint8_t *local_buf;
	int ret = 0;

	if (!data) {
		CAM_ERR(CAM_ISPV3, "The ispv3 data struct is NULL!\n");
		return -EINVAL;
	}

	if (!data->spi)
		return -ENXIO;

	local_buf = kmalloc(size + ISPV3_SPI_OP_WRITE_SIZE,
			    GFP_KERNEL);
	if (!local_buf)
		return -ENOMEM;

	spi_message_init(&message);
	memset(&transfer, 0, sizeof(transfer));

	local_buf[0] = ((reg_addr & MITOP_REG_MASK) == MITOP_REG_MASK) ?
			ISPV3_SPI_REG_WRITE_OP : ISPV3_SPI_RAM_WRITE_OP;
	local_buf[1] = reg_addr & 0xff;
	local_buf[2] = (reg_addr >> 8) & 0xff;
	local_buf[3] = (reg_addr >> 16) & 0xff;
	local_buf[4] = (reg_addr >> 24) & 0xff;

	memset(&local_buf[ISPV3_SPI_OP_WRITE_SIZE], reg_data, size);

	transfer.tx_buf = local_buf;
	transfer.len = size + ISPV3_SPI_OP_WRITE_SIZE;

	spi_message_add_tail(&transfer, &message);

	ret = spi_sync(data->spi, &message);
	if (ret)
		CAM_ERR(CAM_ISPV3, "spi_sync failed!\n");

	kfree(local_buf);

	return ret;
}

static int cam_ispv3_spi_single_read(struct ispv3_image_device *priv,
				     struct ispv3_reg_array *reg_setting,
				     unsigned int size)
{
	unsigned int index;
	int ret = 0;

	for (index = 0; index < size; index++) {
		ret = cam_ispv3_spi_read(priv, reg_setting->reg_addr,
					 &reg_setting->reg_data,
					 sizeof(uint32_t));
		reg_setting++;

		if (ret)
			return ret;
	}

	return ret;
}

static int cam_ispv3_spi_single_write(struct ispv3_image_device *priv,
				      struct ispv3_reg_array *reg_setting,
				      unsigned int size)
{
	unsigned int index;
	int ret = 0;

	for (index = 0; index < size; index++) {
		ret = cam_ispv3_spi_write(priv,
					  reg_setting->reg_addr,
					  &reg_setting->reg_data,
					  sizeof(uint32_t));
		reg_setting++;

		if (ret)
			return ret;
	}

	return ret;
}

static int cam_ispv3_pci_single_read(struct ispv3_image_device *priv,
				     struct ispv3_reg_array *reg_setting,
				     unsigned int size)
{
	struct ispv3_data *data = priv->pdata;
	unsigned int index;
	uint32_t offset;

	for (index = 0; index < size; index++) {
		offset = reg_setting[index].reg_addr;
		if ((offset & MITOP_REG_MASK) != MITOP_REG_MASK) {
			CAM_ERR(CAM_ISPV3, "pci supports to read regs in noburst mode only\n");
			return -EIO;
		}
		reg_setting[index].reg_data = readl_relaxed(data->base +
						(offset & ~MITOP_REG_MASK));
	}

	return 0;
}

static int cam_ispv3_pci_single_write(struct ispv3_image_device *priv,
				      struct ispv3_reg_array *reg_setting,
				      unsigned int size)
{
	struct ispv3_data *data = priv->pdata;
	unsigned int index;
	uint32_t offset;

	for (index = 0; index < size; index++) {
		offset = reg_setting[index].reg_addr;
		if ((offset & MITOP_REG_MASK) != MITOP_REG_MASK) {
			CAM_ERR(CAM_ISPV3, "pci supports to write regs in noburst mode only\n");
			return -EIO;
		}
		writel_relaxed(reg_setting[index].reg_data, data->base +
			       (offset & ~MITOP_REG_MASK));
	}

	return 0;
}

static inline int cam_ispv3_spi_burst_read(struct ispv3_image_device *priv,
					   uint32_t reg_addr,
					   uint32_t *data_buf,
					   uint32_t size)
{
	return cam_ispv3_spi_read(priv, reg_addr, data_buf, size);
}

static inline int cam_ispv3_spi_burst_write(struct ispv3_image_device *priv,
					    uint32_t reg_addr,
					    uint32_t *data_buf,
					    uint32_t size)
{
	return cam_ispv3_spi_write(priv, reg_addr, data_buf, size);
}

static inline int cam_ispv3_spi_burst_set(struct ispv3_image_device *priv,
					  uint32_t reg_addr,
					  uint32_t reg_data,
					  uint32_t size)
{
	return cam_ispv3_spi_set(priv, reg_addr, reg_data, size);
}

static inline int cam_ispv3_pci_burst_read(struct ispv3_image_device *priv,
					   uint32_t reg_addr,
					   uint32_t *data_buf,
					   uint32_t size)
{
	struct ispv3_data *data = priv->pdata;

	if ((reg_addr & MITOP_REG_MASK) == MITOP_REG_MASK)
		memcpy_fromio(data_buf,
			      data->base + (reg_addr & ~MITOP_REG_MASK),
			      size);
	else if ((reg_addr & MITOP_REG_MASK) == MITOP_OCRAM_MASK)
		memcpy(data_buf, priv->base[0] +
		       (reg_addr-MITOP_OCRAM_MASK-OCRAM_OFFSET-RPMSG_SIZE),
		       size);
	else
		memcpy(data_buf, priv->base[1] + reg_addr, size);


	return 0;
}

static inline int cam_ispv3_pci_burst_write(struct ispv3_image_device *priv,
					    uint32_t reg_addr,
					    uint32_t *data_buf,
					    uint32_t size)
{
	struct ispv3_data *data = priv->pdata;

	if ((reg_addr & MITOP_REG_MASK) == MITOP_REG_MASK)
		memcpy_toio(data->base + (reg_addr & ~MITOP_REG_MASK),
			    data_buf, size);
	else if ((reg_addr & MITOP_REG_MASK) == MITOP_OCRAM_MASK)
		memcpy(priv->base[0] +
		       (reg_addr-MITOP_OCRAM_MASK-OCRAM_OFFSET-RPMSG_SIZE),
		       data_buf, size);
	else
		memcpy(priv->base[1] + reg_addr, data_buf, size);

	return 0;
}

static inline int cam_ispv3_pci_burst_set(struct ispv3_image_device *priv,
					  uint32_t reg_addr,
					  uint32_t reg_data,
					  uint32_t size)
{
	struct ispv3_data *data = priv->pdata;
	unsigned int index;

	if ((reg_addr & MITOP_REG_MASK) == MITOP_REG_MASK) {
		for (index = 0; index < size; index++) {
			writel_relaxed(reg_data, data->base +
				       (reg_addr & ~MITOP_REG_MASK));
			reg_addr += 4;
		}
	} else if ((reg_addr & MITOP_REG_MASK) == MITOP_OCRAM_MASK)
		memset(priv->base[0] +
		       (reg_addr-MITOP_OCRAM_MASK-OCRAM_OFFSET-RPMSG_SIZE),
		       reg_data, sizeof(uint32_t) * size);
	else
		memset(priv->base[1] + reg_addr, reg_data,
		       sizeof(uint32_t) * size);
	return 0;
}

static int cam_ispv3_single_read(struct ispv3_image_device *priv,
				 struct ispv3_reg_array *reg_setting,
				 uint32_t size,
				 enum isp_interface_type bus_type)
{
	int ret = -EIO;

	switch (bus_type) {
	case ISP_INTERFACE_SPI:
		ret = cam_ispv3_spi_single_read(priv, reg_setting, size);
		break;
	case ISP_INTERFACE_PCIE:
		ret = cam_ispv3_pci_check_link_status(priv);
		if (ret) {
			CAM_ERR(CAM_ISPV3, "The ISPv3 device link down!\n");
			break;
		}

		ret = cam_ispv3_pci_single_read(priv, reg_setting, size);
		break;
	case ISP_INTERFACE_IIC:
		CAM_ERR(CAM_ISPV3, "not support iic driver\n");
		break;
	default:
		break;
	}

	return ret;
}

static int cam_ispv3_single_write(struct ispv3_image_device *priv,
				  struct ispv3_reg_array *reg_setting,
				  uint32_t size,
				  enum isp_interface_type bus_type)
{
	int ret = -EIO;

	switch (bus_type) {
	case ISP_INTERFACE_SPI:
		ret = cam_ispv3_spi_single_write(priv, reg_setting, size);
		break;
	case ISP_INTERFACE_PCIE:
		ret = cam_ispv3_pci_check_link_status(priv);
		if (ret) {
			CAM_ERR(CAM_ISPV3, "The ISPv3 device link down!\n");
			break;
		}

		ret = cam_ispv3_pci_single_write(priv, reg_setting, size);
		break;
	case ISP_INTERFACE_IIC:
		CAM_ERR(CAM_ISPV3, "not support iic driver\n");
		break;
	default:
		break;
	}

	return ret;
}

static int cam_ispv3_burst_read(struct ispv3_image_device *priv,
				uint32_t reg_addr,
				uint32_t *data_buf,
				uint32_t size,
				enum isp_interface_type bus_type)
{
	int ret = -EIO;

	switch (bus_type) {
	case ISP_INTERFACE_SPI:
		if (reg_addr < MITOP_OCRAM_MAX_ADDR) {
			ret = cam_ispv3_spi_burst_read(priv, reg_addr,
						       data_buf,
						       size * sizeof(uint32_t));
		} else {
			CAM_ERR(CAM_ISPV3, "spi can not read regs in burst mode\n");
		}
		break;
	case ISP_INTERFACE_PCIE:
		ret = cam_ispv3_pci_check_link_status(priv);
		if (ret) {
			CAM_ERR(CAM_ISPV3, "The ISPv3 device link down!\n");
			break;
		}

		ret = cam_ispv3_pci_burst_read(priv, reg_addr,
					       data_buf,
					       size * sizeof(uint32_t));
		break;
	case ISP_INTERFACE_IIC:
		CAM_ERR(CAM_ISPV3, "not support iic driver\n");
		break;
	default:
		break;
	}

	return ret;
}

static int cam_ispv3_burst_write(struct ispv3_image_device *priv,
				 uint32_t reg_addr,
				 uint32_t *data_buf,
				 uint32_t size,
				 enum isp_interface_type bus_type)
{
	int ret = -EIO;

	switch (bus_type) {
	case ISP_INTERFACE_SPI:
		ret = cam_ispv3_spi_burst_write(priv, reg_addr,
						data_buf,
						size * sizeof(uint32_t));
		break;
	case ISP_INTERFACE_PCIE:
		ret = cam_ispv3_pci_check_link_status(priv);
		if (ret) {
			CAM_ERR(CAM_ISPV3, "The ISPv3 device link down!\n");
			break;
		}

		ret = cam_ispv3_pci_burst_write(priv, reg_addr,
						data_buf,
						size * sizeof(uint32_t));
		break;
	case ISP_INTERFACE_IIC:
		CAM_ERR(CAM_ISPV3, "not support iic driver\n");
		break;
	default:
		break;
	}

	return ret;
}

static int cam_ispv3_burst_set(struct ispv3_image_device *priv,
			       uint32_t reg_addr,
			       uint32_t reg_data,
			       uint32_t size,
			       enum isp_interface_type bus_type)
{
	int ret = -EIO;

	switch (bus_type) {
	case ISP_INTERFACE_SPI:
		ret = cam_ispv3_spi_burst_set(priv, reg_addr, reg_data,
					      size * sizeof(uint32_t));
		break;
	case ISP_INTERFACE_PCIE:
		ret = cam_ispv3_pci_check_link_status(priv);
		if (ret) {
			CAM_ERR(CAM_ISPV3, "The ISPv3 device link down!\n");
			break;
		}

		ret = cam_ispv3_pci_burst_set(priv, reg_addr,
					      reg_data,
					      size * sizeof(uint32_t));
		break;
	case ISP_INTERFACE_IIC:
		CAM_ERR(CAM_ISPV3, "not support iic driver\n");
		break;
	default:
		break;
	}

	return ret;
}

int cam_ispv3_read(struct ispv3_image_device *priv,
		   struct cam_control *cmd)
{
	struct ispv3_config_setting user_setting;
	struct ispv3_reg_array *reg_setting;
	uint32_t *databuf = NULL;
	int ret = 0;

	ret = copy_from_user(&user_setting, (void __user *)(cmd->handle),
			    sizeof(user_setting));
	if (ret < 0) {
		CAM_ERR(CAM_ISPV3, "Copy data from user space failed\n");
		return ret;
	}

	switch (user_setting.access_mode) {
	case ISP_SINGLECPY:
		reg_setting = kzalloc(sizeof(struct ispv3_reg_array) *
				      user_setting.data_num,
				      GFP_KERNEL);
		if (!reg_setting)
			return -ENOMEM;

		ret = copy_from_user(reg_setting,
				    (void __user *)(user_setting.reg_array_buf),
				    sizeof(struct ispv3_reg_array) *
				    user_setting.data_num);
		if (ret < 0) {
			kfree(reg_setting);
			return ret;
		}
		ret = cam_ispv3_single_read(priv, reg_setting,
					    user_setting.data_num,
					    user_setting.bus_type);
		if (!ret) {
			ret = copy_to_user(
				(void __user *)(user_setting.reg_array_buf),
				reg_setting,
				sizeof(struct ispv3_reg_array) *
				user_setting.data_num);
			if (ret) {
				CAM_ERR(CAM_ISPV3, "Copy data to user space failed\n");
				ret = -EFAULT;
			}
		}
		kfree(reg_setting);
		break;

	case ISP_BURSTCPY:
		databuf = kzalloc(sizeof(uint32_t) * user_setting.data_num,
				  GFP_KERNEL);
		if (!databuf)
			return -ENOMEM;

		ret = cam_ispv3_burst_read(priv,
					   user_setting.reg_burst_setting.reg_addr,
					   databuf,
					   user_setting.data_num,
					   user_setting.bus_type);
		if (!ret) {
			ret = copy_to_user(
			   (void __user *)(user_setting.reg_burst_setting.data_buf),
			   databuf, sizeof(uint32_t) * user_setting.data_num);
			if (ret) {
				CAM_ERR(CAM_ISPV3, "Copy data to user space failed\n");
				ret = -EFAULT;
			}
		}
		kfree(databuf);
		break;
	default:
		ret = -ENOIOCTLCMD;
		break;
	}

	return ret;
}

int cam_ispv3_write(struct ispv3_image_device *priv,
		    struct cam_control *cmd)
{
	struct ispv3_config_setting user_setting;
	struct ispv3_reg_array *reg_setting;
	uint32_t *databuf = NULL;
	int ret = 0;

	ret = copy_from_user(&user_setting, (void __user *)(cmd->handle),
			    sizeof(user_setting));
	if (ret < 0) {
		CAM_ERR(CAM_ISPV3, "get user cmd setting failed!\n");
		return ret;
	}

	switch (user_setting.access_mode) {
	case ISP_SINGLECPY:
		reg_setting = kzalloc(sizeof(struct ispv3_reg_array) *
				      user_setting.data_num, GFP_KERNEL);
		if (!reg_setting)
			return -ENOMEM;

		ret = copy_from_user(reg_setting,
			     (void __user *)(user_setting.reg_array_buf),
			     sizeof(struct ispv3_reg_array) *
			     user_setting.data_num);
		if (ret < 0) {
			CAM_ERR(CAM_ISPV3, "Copy cmd setting from user space failed!\n");
			kfree(reg_setting);
			return ret;
		}
		user_setting.reg_array_buf = reg_setting;

		ret = cam_ispv3_single_write(priv, reg_setting,
					     user_setting.data_num,
					     user_setting.bus_type);
		if (ret)
			CAM_ERR(CAM_ISPV3,
				"Write setting failed, ret = %d\n", ret);

		kfree(reg_setting);
		break;

	case ISP_BURSTCPY:
		databuf = vmalloc(sizeof(uint32_t) * user_setting.data_num);
		if (!databuf)
			return -ENOMEM;

		ret = copy_from_user(databuf,
		      (void __user *)(user_setting.reg_burst_setting.data_buf),
		      sizeof(uint32_t) * user_setting.data_num);
		if (ret < 0) {
			CAM_ERR(CAM_ISPV3,
				"Copy cmd setting from user space failed\n");
			kfree(databuf);
			return ret;
		}
		ret = cam_ispv3_burst_write(priv,
					    user_setting.reg_burst_setting.reg_addr,
					    databuf, user_setting.data_num,
					    user_setting.bus_type);
		if (ret)
			CAM_ERR(CAM_ISPV3,
				"Write setting failed, ret = %d\n", ret);

		vfree(databuf);
		break;

	case ISP_BURSTSET:
		ret = cam_ispv3_burst_set(priv,
					  user_setting.reg_array.reg_addr,
					  user_setting.reg_array.reg_data,
					  user_setting.data_num,
					  user_setting.bus_type);
		break;
	default:
		ret = -ENOIOCTLCMD;
		break;
	}

	return ret;
}


inline int cam_ispv3_reg_read(struct ispv3_image_device *priv,
			      uint32_t reg_addr, uint32_t *reg_data)
{
	struct ispv3_data *data = priv->pdata;

	if (data->interface_type == ISPV3_PCIE) {
		if (atomic_read(&data->pci_link_state) == ISPV3_PCI_LINK_UP) {
			*reg_data = readl_relaxed(data->base +
					(reg_addr & ~MITOP_REG_MASK));
			return 0;
		} else {
			return cam_ispv3_spi_read(priv, reg_addr,
					reg_data, sizeof(uint32_t));
		}
	} else {
		return cam_ispv3_spi_read(priv, reg_addr,
				reg_data, sizeof(uint32_t));
	}
}

inline int cam_ispv3_reg_write(struct ispv3_image_device *priv,
			       uint32_t reg_addr, uint32_t reg_data)
{
	struct ispv3_data *data = priv->pdata;

	if (data->interface_type == ISPV3_PCIE) {
		if (atomic_read(&data->pci_link_state) == ISPV3_PCI_LINK_UP) {
			writel_relaxed(reg_data,
					data->base + (reg_addr & ~MITOP_REG_MASK));
			return 0;
		} else {
			return cam_ispv3_spi_write(priv, reg_addr,
					&reg_data, sizeof(uint32_t));
		}
	} else {
		return cam_ispv3_spi_write(priv, reg_addr,
					 &reg_data, sizeof(uint32_t));
	}
}

static int cam_ispv3_turn_off_cpu(struct ispv3_image_device *priv)
{
	int ret = 0;

	/* Reset ispv3 CPU unit */
	ret = cam_ispv3_reg_write(priv, ISPV3_INTERNAL_SRAM_CTL_ADDR, 0x0308);
	if (ret)
		return ret;
	ret = cam_ispv3_reg_write(priv, ISPV3_CPU_CLK_CTL_ADDR, 0x03);
	if (ret)
		return ret;
	ret = cam_ispv3_reg_write(priv, ISPV3_CPU_RESET_CTL_ADDR, 0x03);

	return ret;
}

static int cam_ispv3_disable_L1state(struct ispv3_image_device *priv)
{
	uint32_t reg_data;
	int ret;

	ret = cam_ispv3_reg_read(priv, ISPV3_PCIE_L1_MODE_ADDR, &reg_data);
	if (ret)
		return ret;

	reg_data &= ~0x3;

	return cam_ispv3_reg_write(priv, ISPV3_PCIE_L1_MODE_ADDR, reg_data);
}

#ifdef CONFIG_ZISP_OCRAM_AON
static int cam_ispv3_turn_on_cpu(struct ispv3_image_device *priv)
{
	int ret = 0;

	/* Turn on ispv3 CPU unit */
	ret = cam_ispv3_reg_write(priv, ISPV3_CPU_PD_ADDR0, 0x7e);
	if (ret)
		return ret;
	ret = cam_ispv3_reg_write(priv, ISPV3_CPU_PD_ADDR1, 0x7e);
	if (ret)
		return ret;
	ret = cam_ispv3_reg_write(priv, ISPV3_CPU_PD_CTL_ADDR, 0xffffff01);
	if (ret)
		return ret;

	ret = cam_ispv3_reg_write(priv, ISPV3_INTERNAL_SRAM_CTL_ADDR, 0xfffff000);
	if (ret)
		return ret;
	ret = cam_ispv3_reg_write(priv, ISPV3_CPU_CLK_CTL_ADDR, 0xffffc001);
	if (ret)
		return ret;
	ret = cam_ispv3_reg_write(priv, ISPV3_CPU_RESET_CTL_ADDR, 0xffffc001);
	if (ret)
		return ret;


	return ret;
}
#endif

static int cam_ispv3_disable_sram_deep_sleep(struct ispv3_image_device *priv)
{
	int ret = 0;

	ret = cam_ispv3_reg_write(priv, ISPV3_SRAM_SLEEP_ADDR, 0x00);
	if (ret)
		return ret;
	ret = cam_ispv3_reg_write(priv, ISPV3_SRAM_DISABLE_ADDR, 0x00);
	if (ret)
		return ret;
	ret = cam_ispv3_reg_write(priv, ISPV3_CPU_PD_CTL_ADDR, 0x01);

	return ret;
}

static int cam_ispv3_enable_sram_deep_sleep(struct ispv3_image_device *priv)
{
	return cam_ispv3_reg_write(priv, ISPV3_SRAM_SLEEP_ADDR, 0x01);
}

static int cam_ispv3_interrupt_assign(struct ispv3_image_device *priv)
{
	uint32_t value = 0;
	int ret = 0;

	/*
	 * 1. ISPV3 ap_int_0 gpio pin and ISPV3 ap_int_1 gpio
	 * pin function select
	 * 2. ISPV3 internal software interrupt assign and pipe
	 * interrupt assign
	 */
	if (priv->irq_type == ISPV3_HW_IRQ) {
		ret = cam_ispv3_reg_write(priv, ISPV3_GPIO_AP_INT0_ADDR,
					  ISPV3_GPIO_AP_INT0_VAL);
		if (ret)
			return ret;

		ret = cam_ispv3_reg_write(priv, ISPV3_GPIO_AP_INT1_ADDR,
					  ISPV3_GPIO_AP_INT1_VAL);
		if (ret)
			return ret;
	}

	ret = cam_ispv3_reg_write(priv, ISPV3_INT_SOURCE_ASSIGN_ADDR,
				  ISPV3_INT_SW_SOURCE_ASSIGN(ISPV3_INT_SW_SOURCE1));
	if (ret)
		return ret;

	ret = cam_ispv3_reg_read(priv, ISPV3_INT_ENABLE_AP_ADDR,
				 &value);
	if (ret)
		return ret;

	value = value | ISPV3_INT_ENABLE_SW_TO_AP;
	ret = cam_ispv3_reg_write(priv, ISPV3_INT_ENABLE_AP_ADDR, value);

	return ret;

}

static int cam_ispv3_interrupt_ap_enable(struct ispv3_image_device *priv)
{
	uint32_t value = 0;
	int ret = 0;
#ifdef ISPV3_WHOLE_SESSION
	ret = cam_ispv3_reg_write(priv, ISPV3_AP_INT_ADDR,
				   ISPV3_AP_RLB_INT_EN |
				   ISPV3_AP_IDD_INT_EN |
				   ISPV3_AP_TXLM_MUX_INT_EN |
				   ISPV3_AP_TXLM_A0_INT_EN |
				   ISPV3_AP_TXLM_B0_INT_EN |
				   ISPV3_AP_TXLM_D0_INT_EN |
				   ISPV3_AP_SOF_INT_EN);
	if (ret)
		return ret;
#endif
	/* enable the MISN interrupt of int_17 for AP */
	ret = cam_ispv3_reg_write(priv, ISPV3_INT_MISN_EN_AP_ADDR,
				  ISPV3_INT_MISN_EN_TO_AP);
	if (ret)
		return ret;

	/* SWINT enable:
	 * AP write to ISPV3 register, and then the corresponding hardware will
	 * notify the D25F CPU by ISPV3 internal interrupt line.
	 * So we need enable SWINT register
	 */
	ret = cam_ispv3_reg_read(priv, ISPV3_SWINT_ENABLE_ADDR,
				 &value);
	if (ret)
		return ret;

	value = ISPV3_SWINT_ENABLE_VALUE(value);
	ret = cam_ispv3_reg_write(priv, ISPV3_SWINT_ENABLE_ADDR, value);

	return ret;

}

static int cam_ispv3_interrupt_ap_disable(struct ispv3_image_device *priv)
{
	int ret = 0;

	ret = cam_ispv3_reg_write(priv, ISPV3_AP_INT_ADDR, 0x00);
	if (ret)
		return ret;

	ret = cam_ispv3_reg_write(priv, ISPV3_INT_MISN_EN_AP_ADDR, 0x00);
	if (ret)
		return ret;

	ret = cam_ispv3_reg_write(priv, ISPV3_SWINT_ENABLE_ADDR, 0x00);

	return ret;
}

static int cam_ispv3_mode_select(struct ispv3_image_device *priv)
{
	int ret = 0;
	uint32_t reg_data = 0;

	ret = cam_ispv3_reg_read(priv, ISPV3_SYS_CONFIG_ADDR, &reg_data);
	if (ret)
		return ret;

	if (((reg_data & 0x07) == 2) || ((reg_data & 0x07) == 6))
		priv->irq_type = ISPV3_HW_IRQ;
	else
		priv->irq_type = ISPV3_SW_IRQ;

	return ret;
}

static int cam_ispv3_init_aio_timing(struct ispv3_image_device *priv)
{
	int ret = 0;

	ret = cam_ispv3_reg_write(priv, ISPV3_GEN_CLK_ADDR, 0x04);
	if (ret)
		return ret;
	ret = cam_ispv3_reg_write(priv, ISPV3_AIO_TIMING0_ADDR, 0x6632);
	if (ret)
		return ret;
	ret = cam_ispv3_reg_write(priv, ISPV3_AIO_TIMING1_ADDR, 0x6632);
	if (ret)
		return ret;
	ret = cam_ispv3_reg_write(priv, ISPV3_AIO_TIMING2_ADDR, 0x6632);
	if (ret)
		return ret;
	ret = cam_ispv3_reg_write(priv, ISPV3_AIO_TIMING3_ADDR, 0x6632);
	if (ret)
		return ret;
	ret = cam_ispv3_reg_write(priv, ISPV3_AIO_ENABLE_ADDR, 0x01);
	if (ret)
		return ret;

	return ret;
}

#ifdef CONFIG_ZISP_OCRAM_AON
static int cam_ispv3_resume_aio_boot_timing(struct ispv3_image_device *priv)
{
	uint32_t boot_addr = priv->boot_addr;
	int ret = 0;

	ret = cam_ispv3_reg_write(priv, ISPV3_AIO_BOOT_TIMING0_ADDR,
				  (boot_addr & 0xf000) | 0x137);
	if (ret)
		return ret;
	ret = cam_ispv3_reg_write(priv, ISPV3_AIO_BOOT_TIMING1_ADDR,
				  (boot_addr & 0xffff0000) >> 16);
	if (ret)
		return ret;
	ret = cam_ispv3_reg_write(priv, ISPV3_AIO_BOOT_TIMING2_ADDR, 0x0113);
	if (ret)
		return ret;
	ret = cam_ispv3_reg_write(priv, ISPV3_AIO_BOOT_TIMING3_ADDR,
				  ((boot_addr & 0xfff) << 4) | 0x01);
	if (ret)
		return ret;
	ret = cam_ispv3_reg_write(priv, ISPV3_AIO_BOOT_TIMING4_ADDR, 0x9102);

	return ret;
}
#endif

static void cam_ispv3_suspend_func(struct work_struct *work)
{
	struct ispv3_image_device *priv;
	struct ispv3_data *data;
	int ret;

	priv = container_of(work, struct ispv3_image_device,
			    work);

	data = priv->pdata;
	if (!data) {
		CAM_ERR(CAM_ISPV3, "The ispv3 data struct is NULL!");
		return;
	}

	ret = cam_ispv3_enable_sram_deep_sleep(priv);
	if (ret) {
		CAM_ERR(CAM_ISPV3, "SRAM enter deep sleep mode failed!");
		return;
	}

	ret = cam_ispv3_turn_off_cpu(priv);
	if (ret) {
		CAM_ERR(CAM_ISPV3, "reset cpu failed!");
		return;
	}

	if (data->interface_type == ISPV3_PCIE) {
		//udelay(9000);
		ret = ispv3_suspend_pci_link(data);
		if (ret) {
			CAM_ERR(CAM_ISPV3, "cannot suspend the ispv3 device");
			return;
		}
	}

	ret = ispv3_power_off(data);
	if (ret) {
		CAM_ERR(CAM_ISPV3, "cannot power off the ispv3 device");
		return;
	}

	CAM_INFO(CAM_ISPV3, "suspend ispv3 device");
}

int cam_ispv3_notify_sof(struct cam_ispv3_ctrl_t *s_ctrl,
			 int64_t frameId, int64_t reqId)
{
	int rc = 0;
	struct cam_req_mgr_trigger_notify  notify;

	if (s_ctrl->bridge_intf.crm_cb &&
	    s_ctrl->bridge_intf.crm_cb->notify_trigger) {
		notify.link_hdl = s_ctrl->bridge_intf.link_hdl;
		notify.dev_hdl = s_ctrl->bridge_intf.device_hdl;
		notify.frame_id = frameId;
		notify.trigger = CAM_TRIGGER_POINT_SOF;
		notify.req_id = reqId;
		notify.sof_timestamp_val = 0;
		notify.trigger_id = 0x1;
		notify.trigger_source = CAM_REQ_MGR_TRIG_SRC_EXTERNAL;
		s_ctrl->bridge_intf.crm_cb->notify_trigger(&notify);
		CAM_INFO(CAM_ISPV3, "frame %lld reqId %llu",
				notify.frame_id, notify.req_id);
    }

    return rc;
}

void cam_ispv3_notify_message(struct cam_ispv3_ctrl_t *s_ctrl,
			      uint32_t id, uint32_t type)
{
	struct v4l2_event event;

	event.id = id;
	event.type = type;
	v4l2_event_queue(s_ctrl->v4l2_dev_str.sd.devnode, &event);
}
#ifdef ISPV3_WHOLE_SESSION
#define MISN_INTERRUPT
#define SOF_INTERRUPT
static irqreturn_t cam_ispv3_irq_handler(int irq, void *dev_id)
{
	struct cam_ispv3_ctrl_t *s_ctrl = dev_id;
	struct ispv3_image_device *priv;
	uint32_t reg_val;
	int ret = 0;

	priv = s_ctrl->priv;
	if (!priv)
		return IRQ_NONE;

#ifdef BUG_SOF
	if (atomic_read(&priv->pdata->power_state)) {
		CAM_INFO(CAM_ISPV3, "ispv3 has already powered off");
		return IRQ_HANDLED;
	}
#endif

#ifdef MISN_INTERRUPT
	ret = cam_ispv3_reg_read(priv, ISPV3_INT_MISN_STATUS_ADDR, &reg_val);
	if (ret)
		return IRQ_NONE;

	if (reg_val) {
		ret = cam_ispv3_reg_read(priv, ISPV3_MISN_MOD_STATUS_ADDR,
					 &reg_val);
		if (ret)
			return IRQ_NONE;

		if (reg_val) {
			ret = cam_ispv3_reg_write(priv, ISPV3_MISN_MOD_STATUS_ADDR,
						  reg_val);
			if (ret)
				return IRQ_NONE;

			if ((reg_val & ISPV3_MISN_MOD_INT_LONG_EXP) ||
			    (reg_val & ISPV3_MISN_MOD_INT_MIDD_EXP))
				cam_ispv3_notify_message(s_ctrl,
					V4L_EVENT_CAM_REQ_MGR_ISPV3_MISN_SOF,
					V4L_EVENT_CAM_REQ_MGR_EVENT);
			if (reg_val & ISPV3_MISN_MOD_INT_LONG_LSC)
				cam_ispv3_notify_message(s_ctrl,
					V4L_EVENT_CAM_REQ_MGR_ISPV3_MISN_LSC,
					V4L_EVENT_CAM_REQ_MGR_EVENT);
			if (reg_val & 0xffffff0) {
				CAM_ERR(CAM_ISPV3, "MISN_INTERRUPT error: 0x%x\n", reg_val);
			}
		}
		ret = cam_ispv3_reg_write(priv, ISPV3_MITOP_MOD_STATUS_ADDR,
					  ISPV3_MITOP_MOD_INT_MISN);
		if (ret)
			return IRQ_NONE;

		ret = cam_ispv3_reg_write(priv, ISPV3_MITOP_MOD_STATUS_ADDR,
					  0x00);
		if (ret)
			return IRQ_NONE;

		ret = cam_ispv3_reg_write(priv, ISPV3_INT_MISN_STATUS_ADDR,
					  0x01);
		if (ret)
			return IRQ_NONE;
	}
#endif

#ifdef SOF_INTERRUPT
	ret = cam_ispv3_reg_read(priv, ISPV3_FRAME_INT_ST_ADDR, &reg_val);
	if (ret)
		return IRQ_NONE;

	if (reg_val & ISPV3_FRAME_INT_ST_CH1_SOF) {
		if (s_ctrl->stop_notify_crm == false && s_ctrl->trigger_source == CAM_REQ_MGR_TRIG_SRC_EXTERNAL) {
			CAM_DBG(CAM_ISPV3, "Frame count-1: %d, frame id:%d, is4k:%d\n",
				s_ctrl->frame_count - 1, s_ctrl->frame_id, s_ctrl->is4K);

			if ((s_ctrl->frame_count < s_ctrl->frame_id + 1) &&
			    (s_ctrl->frame_count >= 1)) {
				CAM_DBG(CAM_ISPV3, "Change frame_count for matching sof(%d->%d)\n",
					s_ctrl->frame_count, s_ctrl->frame_id + 1);

				s_ctrl->frame_count = s_ctrl->frame_id + 1;
			}

			s_ctrl->frame_count++;

			if (s_ctrl->frame_count == 2)
				cam_ispv3_notify_sof(s_ctrl, s_ctrl->frame_count - 1, 0);
			else if (s_ctrl->frame_count > 2)
				cam_ispv3_notify_sof(s_ctrl, s_ctrl->frame_count - 1,
						     s_ctrl->req_id);
		}

		ret = cam_ispv3_reg_write(priv, ISPV3_FRAME_INT_ST_ADDR,
					  reg_val);
		if (ret)
			return IRQ_NONE;

	}
#endif

#ifdef RLB_INTERRUPT
	ret = cam_ispv3_reg_read(priv, ISPV3_RLB_A0_INT_ST_ADDR, &reg_val);
	if (ret)
		return IRQ_NONE;

	if (reg_val & ISPV3_RLB_A0_INT_ST_SOF) {
		ret = cam_ispv3_reg_write(priv, ISPV3_RLB_A0_INT_ST_ADDR,
					  ISPV3_RLB_A0_INT_ST_SOF);
		if (ret)
			return IRQ_NONE;

		cam_ispv3_notify_message(s_ctrl,
					 V4L_EVENT_CAM_REQ_MGR_ISPV3_TXLM_SOF,
					 V4L_EVENT_CAM_REQ_MGR_EVENT);
	}

	if (reg_val & ISPV3_RLB_A0_INT_ST_EOF) {
		ret = cam_ispv3_reg_write(priv, ISPV3_RLB_A0_INT_ST_ADDR,
					  ISPV3_RLB_A0_INT_ST_EOF);
		if (ret)
			return IRQ_NONE;
	}
#endif

#ifdef IDD_INTERRUPT
	ret = cam_ispv3_reg_read(priv, ISPV3_IDD_INT_ST_ADDR, &reg_val);
	if (ret)
		return IRQ_NONE;

	if (reg_val & ISPV3_IDD_INT_ST_SOF) {
		ret = cam_ispv3_reg_write(priv, ISPV3_IDD_INT_ST_ADDR,
					  ISPV3_IDD_INT_ST_SOF);
		if (ret)
			return IRQ_NONE;

		cam_ispv3_notify_message(s_ctrl,
					 V4L_EVENT_CAM_REQ_MGR_ISPV3_TXLM_SOF,
					 V4L_EVENT_CAM_REQ_MGR_EVENT);
	}

	if (reg_val & ISPV3_IDD_INT_ST_EOF) {
		ret = cam_ispv3_reg_write(priv, ISPV3_IDD_INT_ST_ADDR,
					  ISPV3_IDD_INT_ST_EOF);
		if (ret)
			return IRQ_NONE;
	}
#endif

#ifdef TXLM_A0_INTERRUPT
	ret = cam_ispv3_reg_read(priv, ISPV3_TXLM_A0_INT_ST_ADDR, &reg_val);
	if (ret)
		return IRQ_NONE;

	if (reg_val & ISPV3_TXLM_INT_ST_SOF) {
		ret = cam_ispv3_reg_write(priv, ISPV3_TXLM_A0_INT_ST_ADDR,
					  ISPV3_TXLM_INT_ST_SOF);
		if (ret)
			return IRQ_NONE;

		cam_ispv3_notify_message(s_ctrl,
					 V4L_EVENT_CAM_REQ_MGR_ISPV3_TXLM_SOF,
					 V4L_EVENT_CAM_REQ_MGR_EVENT);
	}

	if (reg_val & ISPV3_TXLM_INT_ST_EOF_ALL) {
		ret = cam_ispv3_reg_write(priv, ISPV3_TXLM_A0_INT_ST_ADDR,
					  ISPV3_TXLM_INT_ST_EOF_ALL);
		if (ret)
			return IRQ_NONE;
	}
#endif

#ifdef TXLM_B0_INTERRUPT
	ret = cam_ispv3_reg_read(priv, ISPV3_TXLM_B0_INT_ST_ADDR, &reg_val);
	if (ret)
		return IRQ_NONE;

	if (reg_val & ISPV3_TXLM_INT_ST_SOF) {
		ret = cam_ispv3_reg_write(priv, ISPV3_TXLM_B0_INT_ST_ADDR,
					  ISPV3_TXLM_INT_ST_SOF);
		if (ret)
			return IRQ_NONE;

		cam_ispv3_notify_message(s_ctrl,
					 V4L_EVENT_CAM_REQ_MGR_ISPV3_TXLM_SOF,
					 V4L_EVENT_CAM_REQ_MGR_EVENT);
	}

	if (reg_val & ISPV3_TXLM_INT_ST_EOF_ALL) {
		ret = cam_ispv3_reg_write(priv, ISPV3_TXLM_B0_INT_ST_ADDR,
					  ISPV3_TXLM_INT_ST_EOF_ALL);
		if (ret)
			return IRQ_NONE;
	}
#endif

#ifdef TXLM_MUX_INTERRUPT
	ret = cam_ispv3_reg_read(priv, ISPV3_TXLM_MUX_INT_ST_ADDR, &reg_val);
	if (ret)
		return IRQ_NONE;

	if (reg_val & ISPV3_TXLM_MUX_INT_ST_SOF) {
		ret = cam_ispv3_reg_write(priv, ISPV3_TXLM_MUX_INT_ST_ADDR,
					  ISPV3_TXLM_MUX_INT_ST_SOF);
		if (ret)
			return IRQ_NONE;

		cam_ispv3_notify_message(s_ctrl,
					 V4L_EVENT_CAM_REQ_MGR_ISPV3_TXLM_SOF,
					 V4L_EVENT_CAM_REQ_MGR_EVENT);
	}
#endif

	return IRQ_HANDLED;
}
#endif

static irqreturn_t cam_ispv3_power_irq_handler(int irq, void *dev_id)
{
	struct ispv3_image_device *priv = dev_id;
	int ret = 0;
	uint32_t val = 0;
	uint32_t reg_val = 0;

	if (priv->irq_type == ISPV3_HW_IRQ) {
		ret = cam_ispv3_reg_read(priv, ISPV3_INT_STATUS_ADDR, &reg_val);
		if (ret || (reg_val != ISPV3_INT_STATUS_SW_INT))
			return IRQ_NONE;
	}

	ret = cam_ispv3_reg_read(priv, ISPV3_SW_DATA_AP_ADDR, &val);
	if (ret)
		return IRQ_NONE;

	if ((val >= ISPV3_FW_ADDR_START) &&
			(val < ISPV3_FW_ADDR_END)) {
		priv->boot_addr = val;
		schedule_work(&priv->work);
	} else {
		if (val == ISPV3_SW_DATA_TURN_ON_CPU)
			complete(&priv->comp_on);
		else if (val == ISPV3_SW_DATA_TURN_OFF_CPU)
			complete(&priv->comp_off);
		else if (val == ISPV3_SW_DATA_MISN_END)
			complete(&priv->comp_setup);
	}

	if (val > 0) {
		ret = cam_ispv3_reg_write(priv, ISPV3_SW_DATA_AP_ADDR, 0x00);
		if (ret)
			return IRQ_NONE;
	}

	if (priv->irq_type == ISPV3_SW_IRQ) {
		ret = cam_ispv3_reg_write(priv, ISPV3_GPIO_AP_INT1_ADDR, 0xa40);
		if (ret)
			return IRQ_NONE;
	}

	if (priv->irq_type == ISPV3_HW_IRQ) {
		ret = cam_ispv3_reg_write(priv, ISPV3_INT_STATUS_ADDR, reg_val);
		if (ret)
			return IRQ_NONE;
	}

	return IRQ_HANDLED;
}

static int cam_isp_subscribe_event(struct v4l2_subdev *sd,
	struct v4l2_fh *fh,
	struct v4l2_event_subscription *sub)
{
	return v4l2_event_subscribe(fh, sub, CAM_SUBDEVICE_EVENT_MAX, NULL);
}

static int cam_isp_unsubscribe_event(struct v4l2_subdev *sd,
	struct v4l2_fh *fh,
	struct v4l2_event_subscription *sub)
{
	return v4l2_event_unsubscribe(fh, sub);
}

int cam_ispv3_turn_off(struct ispv3_image_device *priv)
{
	struct ispv3_data *data;
	int ret = 0;

	data = priv->pdata;
	if (!data) {
		CAM_ERR(CAM_ISPV3, "The ispv3 data struct is NULL!");
		return -EINVAL;
	}

	/* Write register to power off mcu */
#ifdef BUG_SOF
	if (atomic_read(&data->power_state)) {
		CAM_INFO(CAM_ISPV3, "ispv3 has already powered down");
		return 0;
	}
#else
	if (data->power_state == ISPV3_POWER_OFF) {
		CAM_INFO(CAM_ISPV3, "ispv3 has already powered down");
		return 0;
	}
#endif

#ifdef CONFIG_ZISP_OCRAM_AON
	ret = cam_ispv3_reg_write(priv, ISPV3_SW_DATA_CPU_ADDR,
				  ISPV3_SW_DATA_CPU_TURN_OFF);
	if (ret)
		return ret;

	ret = cam_ispv3_reg_write(priv, ISPV3_SW_TRIGGER_CPU_ADDR,
				  ISPV3_SW_TRIGGER_CPU_VAL);
	if (ret)
		return ret;

	ret = wait_for_completion_timeout(&priv->comp_off,
					  msecs_to_jiffies(400));
	if (!ret) {
		CAM_ERR(CAM_ISPV3, "wait power off interrupt failed!");
		return -ETIMEDOUT;
	}
#endif

	if (data->interface_type == ISPV3_SPI) {
		data->spi->max_speed_hz = 1000000;
		ret = spi_setup(data->spi);
		if (ret) {
			CAM_ERR(CAM_ISPV3, "set speed of spi failed!");
			return ret;
		}
	}

#ifdef CONFIG_ZISP_OCRAM_AON
	ret = cam_ispv3_enable_sram_deep_sleep(priv);
	if (ret) {
		CAM_ERR(CAM_ISPV3, "SRAM enter deep sleep mode failed!");
		return ret;
	}
#else
	rproc_shutdown(data->rproc);
#endif

	ret = cam_ispv3_turn_off_cpu(priv);
	if (ret) {
		CAM_ERR(CAM_ISPV3, "reset cpu failed!");
		return ret;
	}
	//udelay(9000);
	if (data->interface_type == ISPV3_PCIE) {
		if (atomic_read(&data->pci_link_state) == ISPV3_PCI_LINK_UP) {
			ret = ispv3_suspend_pci_link(data);
			if (ret) {
				CAM_ERR(CAM_ISPV3, "cannot suspend the ispv3 device");
				return ret;
			}
		} else {
			CAM_INFO(CAM_ISPV3, "PCIe link is already suspended!");
		}
	}

	ret = ispv3_power_off(data);
	if (ret)
		CAM_ERR(CAM_ISPV3, "cannot power off the ispv3 device");

	return ret;
}

int cam_ispv3_turn_on(struct cam_ispv3_ctrl_t *s_ctrl, struct ispv3_image_device *priv,
		      uint64_t dfs_cmd_value)
{
	struct ispv3_data *data;
	uint32_t reg_data;
	int ret = 0;

	data = priv->pdata;
	if (!data) {
		CAM_ERR(CAM_ISPV3, "The ispv3 data struct is NULL!");
		return -EINVAL;
	}

	s_ctrl->frame_count = 0;
	s_ctrl->frame_id = 0;
	s_ctrl->stop_notify_crm = false;
	if (work_pending(&priv->work))
		cancel_work_sync(&priv->work);

#ifdef BUG_SOF
	if (atomic_read(&data->power_state)) {
		ret = ispv3_power_on(data);
		if (ret) {
			CAM_ERR(CAM_ISPV3, "cannot power on the ispv3 device");
			return ret;
		}
	} else {
		CAM_INFO(CAM_ISPV3, "ispv3 has already powered on");
		return 0;
	}
#else
	if (data->power_state != ISPV3_POWER_ON) {
		ret = ispv3_power_on(data);
		if (ret) {
			CAM_ERR(CAM_ISPV3, "cannot power on the ispv3 device");
			return ret;
		}
	} else {
		CAM_INFO(CAM_ISPV3, "ispv3 has already powered on");
		return 0;
	}
#endif

	if (data->interface_type == ISPV3_PCIE) {
		if (atomic_read(&data->pci_link_state) == ISPV3_PCI_LINK_DOWN) {
			ret = ispv3_resume_pci_link(data);
			if (ret) {
				CAM_ERR(CAM_ISPV3, "cannot resume the ispv3 device");
				return ret;
			}
		} else {
			CAM_INFO(CAM_ISPV3, "PCIe link is already resumed!");
		}
	}

	udelay(5000);

	ret = cam_ispv3_init_aio_timing(priv);
	if (ret) {
		CAM_ERR(CAM_ISPV3, "init aio timing of mcu failed");
		return ret;
	}

#ifdef CONFIG_ZISP_OCRAM_AON
	ret = cam_ispv3_resume_aio_boot_timing(priv);
	if (ret) {
		CAM_ERR(CAM_ISPV3, "resume aio boot timing failed!");
		return ret;
	}
#endif
	ret = cam_ispv3_disable_sram_deep_sleep(priv);
	if (ret) {
		CAM_ERR(CAM_ISPV3, "SRAM exit deep sleep mode failed!");
		return ret;
	}

	ret = cam_ispv3_interrupt_assign(priv);
	if (ret) {
		CAM_ERR(CAM_ISPV3, "interrupt assign failed!");
		return ret;
	}

	ret = cam_ispv3_interrupt_ap_enable(priv);
	if (ret) {
		CAM_ERR(CAM_ISPV3, "some interrupts to ap enable failed!");
		return ret;
	}

	if (data->interface_type == ISPV3_PCIE) {
		cam_ispv3_disable_L1state(priv);
	}

	cam_ispv3_reg_write(priv, ISPV3_SW_DATA_CPU_ADDR, (uint32_t)dfs_cmd_value);
	CAM_INFO(CAM_ISPV3, "Set the SWINT register to 0x%08x!", dfs_cmd_value);

#ifdef CONFIG_ZISP_OCRAM_AON
	ret = cam_ispv3_turn_on_cpu(priv);
	if (ret) {
		CAM_ERR(CAM_ISPV3, "turn on cpu failed!");
		return ret;
	}
#endif

	if (data->interface_type == ISPV3_SPI) {
		data->spi->max_speed_hz = 16000000;
		ret = spi_setup(data->spi);
		if (ret) {
			CAM_ERR(CAM_ISPV3, "set speed of spi failed!");
			return ret;
		}
	}

#ifndef CONFIG_ZISP_OCRAM_AON
	rproc_boot(data->rproc);
#endif

	ret = wait_for_completion_timeout(&priv->comp_on,
			msecs_to_jiffies(400));
	if (!ret) {
		if (!cam_ispv3_reg_read(priv, ISPV3_SW_DATA_AP_ADDR, &reg_data)) {
			CAM_ERR(CAM_ISPV3, "swint cause return %x!", reg_data);
		}
		CAM_ERR(CAM_ISPV3, "wait for completion timeout!");
		ret = -ETIMEDOUT;
	}

	return ret;
}

static long cam_ispv3_subdev_ioctl(struct v4l2_subdev *sd,
	unsigned int cmd, void *arg)
{
	int ret = 0;
	struct cam_ispv3_ctrl_t *s_ctrl =
		v4l2_get_subdevdata(sd);

	switch (cmd) {
	case VIDIOC_CAM_CONTROL:
		ret = cam_ispv3_driver_cmd(s_ctrl, arg);
		break;
	case CAM_SD_SHUTDOWN:
		ret = cam_ispv3_turn_off(s_ctrl->priv);
		cam_ispv3_notify_message(s_ctrl,
					 V4L_EVENT_CAM_REQ_MGR_POLL_EXIT,
					 V4L_EVENT_CAM_REQ_MGR_EVENT);
		break;
	default:
		CAM_ERR(CAM_ISPV3, "Invalid ioctl cmd: %d", cmd);
		ret = -EINVAL;
		break;
	}
	return ret;
}

static int cam_ispv3_subdev_close(struct v4l2_subdev *sd,
	struct v4l2_subdev_fh *fh)
{
	struct cam_ispv3_ctrl_t *s_ctrl =
		v4l2_get_subdevdata(sd);

	if (!s_ctrl) {
		CAM_ERR(CAM_ISPV3, "s_ctrl ptr is NULL");
		return -EINVAL;
	}

	mutex_lock(&(s_ctrl->cam_ispv3_mutex));
	cam_ispv3_shutdown(s_ctrl);
	mutex_unlock(&(s_ctrl->cam_ispv3_mutex));

	return 0;
}

#ifdef CONFIG_COMPAT
static long cam_ispv3_init_subdev_do_ioctl(struct v4l2_subdev *sd,
					   unsigned int cmd,
					   unsigned long arg)
{
	struct cam_control cmd_data;
	int32_t ret = 0;

	if (copy_from_user(&cmd_data, (void __user *)arg,
		sizeof(cmd_data))) {
		CAM_ERR(CAM_ISPV3, "Failed to copy from user_ptr=%pK size=%zu",
			(void __user *)arg, sizeof(cmd_data));
		return -EFAULT;
	}

	switch (cmd) {
	case VIDIOC_CAM_CONTROL:
		ret = cam_ispv3_subdev_ioctl(sd, cmd, &cmd_data);
		if (ret < 0)
			CAM_ERR(CAM_ISPV3, "cam_ispv3_subdev_ioctl failed");
		break;
	default:
		CAM_ERR(CAM_ISPV3, "Invalid compat ioctl cmd_type: %d", cmd);
		ret = -EINVAL;
	}

	if (!ret) {
		if (copy_to_user((void __user *)arg, &cmd_data,
			sizeof(cmd_data))) {
			CAM_ERR(CAM_ISPV3,
				"Failed to copy to user_ptr=%pK size=%zu",
				(void __user *)arg, sizeof(cmd_data));
			ret = -EFAULT;
		}
	}

	return ret;
}

#endif
static struct v4l2_subdev_core_ops cam_ispv3_subdev_core_ops = {
	.ioctl = cam_ispv3_subdev_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl32 = cam_ispv3_init_subdev_do_ioctl,
#endif
	.subscribe_event = cam_isp_subscribe_event,
	.unsubscribe_event = cam_isp_unsubscribe_event,

	.s_power = cam_ispv3_power,
};

static struct v4l2_subdev_ops cam_ispv3_subdev_ops = {
	.core = &cam_ispv3_subdev_core_ops,
};

static const struct v4l2_subdev_internal_ops cam_ispv3_internal_ops = {
	.close = cam_ispv3_subdev_close,
};

static int cam_ispv3_init_subdev_params(struct cam_ispv3_ctrl_t *s_ctrl)
{
	int ret = 0;

	s_ctrl->v4l2_dev_str.internal_ops =
		&cam_ispv3_internal_ops;
	s_ctrl->v4l2_dev_str.ops =
		&cam_ispv3_subdev_ops;
	strlcpy(s_ctrl->device_name, CAMX_ISPV3_DEV_NAME,
		sizeof(s_ctrl->device_name));
	s_ctrl->v4l2_dev_str.name =
		s_ctrl->device_name;
	s_ctrl->v4l2_dev_str.sd_flags =
		(V4L2_SUBDEV_FL_HAS_DEVNODE | V4L2_SUBDEV_FL_HAS_EVENTS);
	s_ctrl->v4l2_dev_str.ent_function =
		CAM_ISPV3_DEVICE_TYPE;
	s_ctrl->v4l2_dev_str.token = s_ctrl;

	s_ctrl->v4l2_dev_str.msg_cb = cam_ispv3_subdev_handle_message;

	ret = cam_register_subdev(&(s_ctrl->v4l2_dev_str));
	if (ret)
		CAM_ERR(CAM_ISPV3, "Fail with cam_register_subdev ret: %d", ret);

	return ret;
}


static void cam_ispv3_component_unbind(struct device *dev,
	struct device *master_dev, void *data)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct cam_ispv3_ctrl_t *s_ctrl;
	struct cam_hw_soc_info *soc_info;
	struct ispv3_image_device *priv;
	int ret = 0;
	int i;

	s_ctrl = platform_get_drvdata(pdev);
	if (!s_ctrl) {
		CAM_ERR(CAM_ISPV3, "ISPV3 device is NULL");
		return;
	}

	priv = s_ctrl->priv;
	if (!priv) {
		CAM_ERR(CAM_ISPV3, "priv device is NULL");
		return;
	}

	ret = cam_ispv3_interrupt_ap_disable(priv);
	if (ret) {
		CAM_ERR(CAM_ISPV3, "interrupt disable failed");
		return;
	}

	CAM_INFO(CAM_ISPV3, "platform remove invoked");
	mutex_lock(&(s_ctrl->cam_ispv3_mutex));
	cam_ispv3_shutdown(s_ctrl);
	mutex_unlock(&(s_ctrl->cam_ispv3_mutex));
	cam_unregister_subdev(&(s_ctrl->v4l2_dev_str));
	soc_info = &s_ctrl->soc_info;
	for (i = 0; i < soc_info->num_clk; i++)
		devm_clk_put(soc_info->dev, soc_info->clk[i]);

	platform_set_drvdata(pdev, NULL);
	v4l2_set_subdevdata(&(s_ctrl->v4l2_dev_str.sd), NULL);
}

static int cam_ispv3_component_bind(struct device *dev,
	struct device *master_dev, void *data_t)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct cam_ispv3_ctrl_t *s_ctrl = NULL;
	struct cam_hw_soc_info *soc_info = NULL;
	struct ispv3_image_device *priv;
	struct ispv3_data *data;
	int ret = -EIO;
	struct resource *res[CAM_RES_NUM];
	int res_idx;
	unsigned long irqflags;

	/* Create ISPV3 control structure */
	s_ctrl = devm_kzalloc(&pdev->dev,
			      sizeof(struct cam_ispv3_ctrl_t), GFP_KERNEL);
	if (!s_ctrl)
		return -ENOMEM;

	mutex_init(&s_ctrl->cam_ispv3_mutex);

	priv = devm_kzalloc(&pdev->dev,
			    sizeof(struct ispv3_image_device), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	data = dev_get_drvdata(pdev->dev.parent);
	if (!data) {
		CAM_ERR(CAM_ISPV3, "The ispv3 data struct is NULL!");
		return -EINVAL;
	}

	soc_info = &s_ctrl->soc_info;
	soc_info->pdev = pdev;
	soc_info->dev = &pdev->dev;
	soc_info->dev_name = pdev->name;

	s_ctrl->of_node = pdev->dev.of_node;

	/* fill in platform device */
	s_ctrl->pdev = pdev;
	s_ctrl->frame_count = 0;
	s_ctrl->frame_id = 0;
	s_ctrl->stop_notify_crm = false;

	/* Fill platform device id */
	pdev->id = soc_info->index;

	ret = cam_ispv3_init_subdev_params(s_ctrl);
	if (ret)
		return -EINVAL;

	s_ctrl->bridge_intf.device_hdl = -1;
	s_ctrl->bridge_intf.link_hdl = -1;
	s_ctrl->bridge_intf.ops.get_dev_info = cam_ispv3_publish_dev_info;
	s_ctrl->bridge_intf.ops.link_setup = cam_ispv3_establish_link;
	s_ctrl->bridge_intf.ops.apply_req = cam_ispv3_apply_request;
	s_ctrl->bridge_intf.ops.flush_req = cam_ispv3_flush_request;

	s_ctrl->ispv3_state = CAM_ISPV3_INIT;

	priv->pdata = data;
	priv->dev = &pdev->dev;
	init_completion(&priv->comp_on);
	init_completion(&priv->comp_off);
	init_completion(&priv->comp_setup);
	INIT_WORK(&priv->work, cam_ispv3_suspend_func);

	s_ctrl->priv = priv;
	platform_set_drvdata(pdev, s_ctrl);

	CAM_INFO(CAM_ISPV3, "interface type is:%s\n", data->interface_type ? "PCIE" : "SPI");
	if (data->interface_type == ISPV3_PCIE) {
		for (res_idx = 0; res_idx < CAM_RES_NUM; res_idx++) {
			res[res_idx] = platform_get_resource(pdev, IORESOURCE_MEM,
							     res_idx);
			if (!res[res_idx]) {
				CAM_ERR(CAM_ISPV3, "ispv3 get resource %d info failed!",
					res_idx);
				return ret;
			}

			priv->base[res_idx] = devm_ioremap_resource(&pdev->dev,
								    res[res_idx]);
			if (IS_ERR(priv->base[res_idx]))
				return PTR_ERR(priv->base[res_idx]);
		}
	}

	ret = cam_ispv3_mode_select(priv);
	if (ret) {
		CAM_ERR(CAM_ISPV3, "mode select failed!");
		return ret;
	}
	CAM_INFO(CAM_ISPV3, "irq type is:%s\n", priv->irq_type ? "MODE 6" : "MODE 4");

	ret = cam_ispv3_init_aio_timing(priv);
	if (ret) {
		CAM_ERR(CAM_ISPV3, "init aio timing failed!");
		return ret;
	}

	ret = cam_ispv3_disable_sram_deep_sleep(priv);
	if (ret) {
		CAM_ERR(CAM_ISPV3, "SRAM exit deep sleep mode failed!");
		return ret;
	}

	ret = cam_ispv3_interrupt_assign(priv);
	if (ret) {
		CAM_ERR(CAM_ISPV3, "interrupt assign failed!");
		return ret;
	}

	ret = cam_ispv3_interrupt_ap_enable(priv);
	if (ret) {
		CAM_ERR(CAM_ISPV3, "some interrupts to ap enable failed!");
		return ret;
	}

	irqflags = (priv->irq_type == ISPV3_SW_IRQ) ?
		(IRQF_TRIGGER_RISING | IRQF_ONESHOT) : (IRQF_TRIGGER_HIGH | IRQF_ONESHOT);
#ifdef ISPV3_WHOLE_SESSION
	if (data->gpio_irq_cam) {
		ret = devm_request_threaded_irq(&pdev->dev, data->gpio_irq_cam,
						NULL, cam_ispv3_irq_handler,
						irqflags,
						"ispv3-cam",
						s_ctrl);
		if (ret) {
			CAM_ERR(CAM_ISPV3, "request cam irq failed");
			return ret;
		}
	}
#endif
	if (data->gpio_irq_power) {
		ret = devm_request_threaded_irq(&pdev->dev, data->gpio_irq_power,
						NULL, cam_ispv3_power_irq_handler,
						irqflags,
						"ispv3-power",
						priv);
		if (ret) {
			CAM_ERR(CAM_ISPV3, "request power irq failed");
			return ret;
		}
	}

	if (data->remote_callback != NULL) {
		ret = (data->remote_callback)(data);
		if (ret) {
			CAM_ERR(CAM_ISPV3, "remote callback failed");
			return ret;
		}
	} else {
		CAM_ERR(CAM_ISPV3, "remote callback is not registered");
		return -EINVAL;
	}

#ifndef CONFIG_ZISP_OCRAM_AON
	ret = cam_ispv3_enable_sram_deep_sleep(priv);
	if (ret) {
		CAM_ERR(CAM_ISPV3, "SRAM enter deep sleep mode failed!");
		return ret;
	}

	if (data->interface_type == ISPV3_PCIE) {
		ret = ispv3_suspend_pci_link(data);
		if (ret) {
			CAM_ERR(CAM_ISPV3, "cannot suspend the ispv3 device");
			return ret;
		}
	}

	ret = ispv3_power_off(data);
	if (ret) {
		CAM_ERR(CAM_ISPV3, "cannot power off the ispv3 device");
		return ret;
	}
#endif

	CAM_INFO(CAM_ISPV3, "camera probe succesed !");

	return ret;
}

const static struct component_ops cam_ispv3_component_ops = {
	.bind = cam_ispv3_component_bind,
	.unbind = cam_ispv3_component_unbind,
};

static int cam_ispv3_platform_remove(struct platform_device *pdev)
{
	component_del(&pdev->dev, &cam_ispv3_component_ops);

	return 0;
}

static int32_t cam_ispv3_driver_platform_probe(struct platform_device *pdev)
{
	int ret = 0;

	ret = component_add(&pdev->dev, &cam_ispv3_component_ops);
	if (ret)
		CAM_ERR(CAM_ISPV3, "failed to add component rc: %d", ret);

	return ret;
}

struct platform_driver cam_ispv3_platform_driver = {
	.probe = cam_ispv3_driver_platform_probe,
	.driver = {
		.name = "ispv3-cam",
		.owner = THIS_MODULE,
		.suppress_bind_attrs = true,
	},
	.remove = cam_ispv3_platform_remove,
};

int cam_ispv3_driver_init(void)
{
	int32_t ret = 0;

	ret = platform_driver_register(&cam_ispv3_platform_driver);
	if (ret < 0) {
		CAM_ERR(CAM_ISPV3, "platform_driver_register Failed: ret = %d",
			ret);
		return ret;
	}

	return ret;
}

void cam_ispv3_driver_exit(void)
{
	platform_driver_unregister(&cam_ispv3_platform_driver);
}

MODULE_DESCRIPTION("cam_ispv3_driver");
MODULE_LICENSE("GPL v2");
