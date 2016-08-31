/*
 * mods_acpi.c - This file is part of NVIDIA MODS kernel driver.
 *
 * Copyright (c) 2008-2014, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA MODS kernel driver is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * NVIDIA MODS kernel driver is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NVIDIA MODS kernel driver.
 * If not, see <http://www.gnu.org/licenses/>.
 */

#include "mods_internal.h"

#include <acpi/acpi.h>
#include <acpi/acpi_bus.h>

static acpi_status mods_acpi_find_acpi_handler(acpi_handle,
					       u32,
					       void *,
					       void **);

/*********************
 * PRIVATE FUNCTIONS *
 *********************/

/* store handle if found. */
static void mods_acpi_handle_init(char *method_name, acpi_handle *handler)
{
	MODS_ACPI_WALK_NAMESPACE(ACPI_TYPE_ANY,
				 ACPI_ROOT_OBJECT,
				 ACPI_UINT32_MAX,
				 mods_acpi_find_acpi_handler,
				 method_name,
				 handler);

	if (!(*handler)) {
		mods_debug_printk(DEBUG_ACPI, "ACPI method %s not found\n",
				  method_name);
		return;
	}
}

static acpi_status mods_acpi_find_acpi_handler(
	acpi_handle handle,
	u32 nest_level,
	void *dummy1,
	void **dummy2
)
{
	acpi_handle acpi_method_handler_temp;

	if (!acpi_get_handle(handle, dummy1, &acpi_method_handler_temp))
		*dummy2 = acpi_method_handler_temp;

	return OK;
}

static int mods_extract_acpi_object(
	char *method,
	union acpi_object *obj,
	NvU8 **buf,
	NvU8 *buf_end
)
{
	int ret = OK;
	switch (obj->type) {

	case ACPI_TYPE_BUFFER:
		if (obj->buffer.length == 0) {
			mods_error_printk(
			    "empty ACPI output buffer from ACPI method %s\n",
			    method);
			ret = -EINVAL;
		} else if (obj->buffer.length <= buf_end-*buf) {
			u32 size = obj->buffer.length;
			memcpy(*buf, obj->buffer.pointer, size);
			*buf += size;
		} else {
			mods_error_printk(
			    "output buffer too small for ACPI method %s\n",
			    method);
			ret = -EINVAL;
		}
		break;

	case ACPI_TYPE_INTEGER:
		if (4 <= buf_end-*buf) {
			if (obj->integer.value > 0xFFFFFFFFU) {
				mods_error_printk(
			    "integer value from ACPI method %s out of range\n",
					  method);
				ret = -EINVAL;
			} else {
				memcpy(*buf, &obj->integer.value, 4);
				*buf += 4;
			}
		} else {
			mods_error_printk(
				"output buffer too small for ACPI method %s\n",
				method);
			ret = -EINVAL;
		}
		break;

	case ACPI_TYPE_PACKAGE:
		if (obj->package.count == 0) {
			mods_error_printk(
			    "empty ACPI output package from ACPI method %s\n",
			    method);
			ret = -EINVAL;
		} else {
			union acpi_object *elements = obj->package.elements;
			u32 size = 0;
			u32 i;
			for (i = 0; i < obj->package.count; i++) {
				NvU8 *old_buf = *buf;
				ret = mods_extract_acpi_object(method,
							       &elements[i],
							       buf,
							       buf_end);
				if (ret == OK) {
					u32 new_size = *buf - old_buf;
					if (size == 0) {
						size = new_size;
					} else if (size != new_size) {
						mods_error_printk(
			 "ambiguous package element size from ACPI method %s\n",
						  method);
						ret = -EINVAL;
					}
				} else
					break;
			}
		}
		break;

	default:
		mods_error_printk(
			"unsupported ACPI output type 0x%02x from method %s\n",
			(unsigned)obj->type, method);
		ret = -EINVAL;
		break;

	}
	return ret;
}

static int mods_eval_acpi_method(struct file		      *pfile,
				 struct MODS_EVAL_ACPI_METHOD *p,
				 struct mods_pci_dev	      *pdevice)
{
	int ret = OK;
	int i;
	acpi_status status;
	struct acpi_object_list input;
	struct acpi_buffer output = { ACPI_ALLOCATE_BUFFER, NULL };
	union acpi_object *acpi_method = NULL;
	union acpi_object acpi_params[ACPI_MAX_ARGUMENT_NUMBER];
	acpi_handle acpi_method_handler = NULL;

	if (pdevice) {
#ifdef DEVICE_ACPI_HANDLE
		unsigned int devfn;
		struct pci_dev *dev;

		mods_debug_printk(DEBUG_ACPI, "ACPI %s for device %x:%02x.%x\n",
				  p->method_name,
				  (unsigned)pdevice->bus,
				  (unsigned)pdevice->device,
				  (unsigned)pdevice->function);

		devfn = PCI_DEVFN(pdevice->device, pdevice->function);
		dev = MODS_PCI_GET_SLOT(pdevice->bus, devfn);
		if (!dev) {
			mods_error_printk("ACPI: PCI device not found\n");
			return -EINVAL;
		}
		acpi_method_handler = DEVICE_ACPI_HANDLE(&dev->dev);
#else
		mods_error_printk(
			"this kernel does not support per-device ACPI calls\n");
		return -EINVAL;
#endif
	} else {
		mods_debug_printk(DEBUG_ACPI, "ACPI %s\n", p->method_name);
		mods_acpi_handle_init(p->method_name, &acpi_method_handler);
	}

	if (!acpi_method_handler) {
		mods_debug_printk(DEBUG_ACPI, "ACPI: handle for %s not found\n",
				  p->method_name);
		return -EINVAL;
	}

	if (p->argument_count >= ACPI_MAX_ARGUMENT_NUMBER) {
		mods_error_printk("invalid argument count for ACPI call\n");
		return -EINVAL;
	}

	for (i = 0; i < p->argument_count; i++) {
		switch (p->argument[i].type) {
		case ACPI_MODS_TYPE_INTEGER: {
			acpi_params[i].integer.type = ACPI_TYPE_INTEGER;
			acpi_params[i].integer.value
				= p->argument[i].integer.value;
			break;
		}
		case ACPI_MODS_TYPE_BUFFER: {
			acpi_params[i].buffer.type = ACPI_TYPE_BUFFER;
			acpi_params[i].buffer.length
				= p->argument[i].buffer.length;
			acpi_params[i].buffer.pointer
				= p->in_buffer + p->argument[i].buffer.offset;
			break;
		}
		default: {
			mods_error_printk("unsupported ACPI argument type\n");
			return -EINVAL;
		}
		}
	}

	input.count   = p->argument_count;
	input.pointer = acpi_params;

	status = acpi_evaluate_object(acpi_method_handler,
				      pdevice ? p->method_name : NULL,
				      &input,
				      &output);

	if (ACPI_FAILURE(status)) {
		mods_error_printk("ACPI method %s failed\n", p->method_name);
		return -EINVAL;
	}

	acpi_method = output.pointer;
	if (!acpi_method) {
		mods_error_printk("missing output from ACPI method %s\n",
				  p->method_name);
		ret = -EINVAL;
	} else {
		NvU8 *buf = p->out_buffer;
		ret = mods_extract_acpi_object(p->method_name,
					       acpi_method,
					       &buf,
					       buf+sizeof(p->out_buffer));
		p->out_data_size = (ret == OK) ? (buf - p->out_buffer) : 0;
	}

	kfree(output.pointer);
	return ret;
}

/*************************
 * ESCAPE CALL FUNCTIONS *
 *************************/

int esc_mods_eval_acpi_method(struct file *pfile,
			      struct MODS_EVAL_ACPI_METHOD *p)
{
	return mods_eval_acpi_method(pfile, p, 0);
}

int esc_mods_eval_dev_acpi_method(struct file *pfile,
				  struct MODS_EVAL_DEV_ACPI_METHOD *p)
{
	return mods_eval_acpi_method(pfile, &p->method, &p->device);
}

int esc_mods_acpi_get_ddc(struct file *pfile, struct MODS_ACPI_GET_DDC *p)
{
#if !defined(DEVICE_ACPI_HANDLE)
	mods_error_printk(
		"this kernel does not support per-device ACPI calls\n");
	return -EINVAL;
#else

	acpi_status status;
	struct acpi_device *device = NULL;
	struct acpi_buffer output = { ACPI_ALLOCATE_BUFFER, NULL };
	union acpi_object *ddc;
	union acpi_object ddc_arg0 = { ACPI_TYPE_INTEGER };
	struct acpi_object_list input = { 1, &ddc_arg0 };
	struct list_head *node, *next;
	NvU32 i;
	acpi_handle dev_handle	= NULL;
	acpi_handle lcd_dev_handle	= NULL;

	mods_debug_printk(DEBUG_ACPI,
			  "ACPI _DDC (EDID) for device %x:%02x.%x\n",
			  (unsigned)p->device.bus,
			  (unsigned)p->device.device,
			  (unsigned)p->device.function);

	{
		unsigned int devfn = PCI_DEVFN(p->device.device,
					       p->device.function);
		struct pci_dev *dev = MODS_PCI_GET_SLOT(p->device.bus, devfn);
		if (!dev) {
			mods_error_printk("ACPI: PCI device not found\n");
			return -EINVAL;
		}
		dev_handle = DEVICE_ACPI_HANDLE(&dev->dev);
	}
	if (!dev_handle) {
		mods_debug_printk(DEBUG_ACPI,
				  "ACPI: handle for _DDC not found\n");
		return -EINVAL;
	}
	status = acpi_bus_get_device(dev_handle, &device);

	if (ACPI_FAILURE(status) || !device) {
		mods_error_printk("ACPI: device for _DDC not found\n");
		return -EINVAL;
	}

	list_for_each_safe(node, next, &device->children) {
#ifdef MODS_ACPI_DEVID_64
		unsigned long long
#else
		unsigned long
#endif
			device_id = 0;

		struct acpi_device *dev =
			list_entry(node, struct acpi_device, node);

		if (!dev)
			continue;

		status = acpi_evaluate_integer(dev->handle,
					       "_ADR",
					       NULL,
					       &device_id);
		if (ACPI_FAILURE(status))
			/* Couldnt query device_id for this device */
			continue;

		device_id = (device_id & 0xffff);

		if ((device_id == 0x0110) || /* Only for an LCD*/
		    (device_id == 0x0118) ||
		    (device_id == 0x0400)) {

			lcd_dev_handle = dev->handle;
			mods_debug_printk(DEBUG_ACPI,
				"ACPI: Found LCD 0x%x on device %x:%02x.%x\n",
				(unsigned)device_id,
				(unsigned)p->device.bus,
				(unsigned)p->device.device,
				(unsigned)p->device.function);
			break;
		}

	}

	if (lcd_dev_handle == NULL) {
		mods_error_printk("ACPI: LCD not found for device %x:%02x.%x\n",
				  (unsigned)p->device.bus,
				  (unsigned)p->device.device,
				  (unsigned)p->device.function);
		return -EINVAL;
	}

	/*
	 * As per ACPI Spec 3.0:
	 * ARG0 = 0x1 for 128 bytes EDID buffer
	 * ARG0 = 0x2 for 256 bytes EDID buffer
	 */
	for (i = 1; i <= 2; i++) {
		ddc_arg0.integer.value = i;
		status = acpi_evaluate_object(lcd_dev_handle,
					      "_DDC",
					      &input,
					      &output);
		if (ACPI_SUCCESS(status))
			break;
	}

	if (ACPI_FAILURE(status)) {
		mods_error_printk("ACPI method _DDC (EDID) failed\n");
		return -EINVAL;
	}

	ddc = output.pointer;
	if (ddc && (ddc->type == ACPI_TYPE_BUFFER)
		&& (ddc->buffer.length > 0)) {

		if (ddc->buffer.length <= sizeof(p->out_buffer)) {
			p->out_data_size = ddc->buffer.length;
			memcpy(p->out_buffer,
			       ddc->buffer.pointer,
			       p->out_data_size);
		} else {
			mods_error_printk(
		       "output buffer too small for ACPI method _DDC (EDID)\n");
			kfree(output.pointer);
			return -EINVAL;
		}
	} else {
		mods_error_printk("unsupported ACPI output type\n");
		kfree(output.pointer);
		return -EINVAL;
	}

	kfree(output.pointer);
	return OK;
#endif
}
