/*
 * f_audio.c -- USB Audio class function driver
 *
 * Copyright (c) 2012-2015, The Linux Foundation. All rights reserved.
 * Copyright (C) 2008 Bryan Wu <cooloney@kernel.org>
 * Copyright (C) 2008 Analog Devices, Inc
 *
 * Enter bugs at http://blackfin.uclinux.org/
 *
 * Licensed under the GPL-2 or later.
 */

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "%s: " fmt, __func__

#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/atomic.h>

#include <sound/core.h>
#include <sound/initval.h>

#include "u_uac1.h"

/*
 * DESCRIPTORS ... most are static, but strings and full
 * configuration descriptors are built on demand.
 */

/*
 * We have two interfaces- AudioControl and AudioStreaming
 */
#define PLAYBACK_EP_MAX_PACKET_SIZE	32
static int req_playback_buf_size = PLAYBACK_EP_MAX_PACKET_SIZE;
module_param(req_playback_buf_size, int, S_IRUGO);
MODULE_PARM_DESC(req_playback_buf_size, "ISO OUT endpoint (playback) request buffer size");

static int req_playback_count = 48;
module_param(req_playback_count, int, S_IRUGO);
MODULE_PARM_DESC(req_playback_count, "ISO OUT endpoint (playback) request count");

static int audio_playback_buf_size = 256*32;
module_param(audio_playback_buf_size, int, S_IRUGO);
MODULE_PARM_DESC(audio_playback_buf_size, "Audio buffer size");

#define CAPTURE_EP_MAX_PACKET_SIZE	32
static int req_capture_buf_size = CAPTURE_EP_MAX_PACKET_SIZE;
module_param(req_capture_buf_size, int, S_IRUGO);
MODULE_PARM_DESC(req_capture_buf_size, "ISO IN endpoint (capture) request buffer size");

static int req_capture_count = 48;
module_param(req_capture_count, int, S_IRUGO);
MODULE_PARM_DESC(req_capture_count, "ISO IN endpoint (capture) request count");

static int audio_capture_buf_size = 256*32;
module_param(audio_capture_buf_size, int, S_IRUGO);
MODULE_PARM_DESC(audio_capture_buf_size, "Microphone Audio buffer size");

static int generic_set_cmd(struct usb_audio_control *con, u8 cmd, int value);
static int generic_get_cmd(struct usb_audio_control *con, u8 cmd);

#define SPEAKER_INPUT_TERMINAL_ID	3
#define SPEAKER_OUTPUT_TERMINAL_ID	4

#define MICROPHONE_INPUT_TERMINAL_ID	1
#define MICROPHONE_OUTPUT_TERMINAL_ID	2


 /*
  * We have two interfaces- AudioControl and AudioStreaming
  */

#define F_AUDIO_INTERFACE_MICROPHONE	2
#define F_AUDIO_INTERFACE_SPEAKER	3
#define F_AUDIO_NUM_INTERFACES		2

 /* B.3.1  Standard AC Interface Descriptor */
struct usb_interface_descriptor uac1_ac_interface_desc = {
	.bLength		= USB_DT_INTERFACE_SIZE,
	.bDescriptorType	= USB_DT_INTERFACE,
	.bNumEndpoints		= 0,
	.bInterfaceClass	= USB_CLASS_AUDIO,
	.bInterfaceSubClass	= USB_SUBCLASS_AUDIOCONTROL,
};

#define TOTAL_LENGTH (                  \
	UAC_DT_AC_HEADER_SIZE(2)      + \
	UAC_DT_INPUT_TERMINAL_SIZE    + \
	UAC_DT_OUTPUT_TERMINAL_SIZE   + \
	UAC_DT_INPUT_TERMINAL_SIZE    + \
	UAC_DT_OUTPUT_TERMINAL_SIZE     \
	)

 /* B.3.2  Class-Specific AC Interface Descriptor */
struct uac1_ac_header_descriptor_2 uac1_ac_header_desc = {
	.bLength		= UAC_DT_AC_HEADER_SIZE(2),
	.bDescriptorType	= USB_DT_CS_INTERFACE,
	.bDescriptorSubtype	= UAC_HEADER,
	.bcdADC			= __constant_cpu_to_le16(0x0100),
	.wTotalLength		= __constant_cpu_to_le16(TOTAL_LENGTH),
	.bInCollection		= F_AUDIO_NUM_INTERFACES,
	/*.baInterfaceNr	= {
					[0] = F_AUDIO_INTERFACE_MICROPHONE,
					[1] = F_AUDIO_INTERFACE_SPEAKER,
				  }
	*/
};

/*---------------------------------*/

struct uac_input_terminal_descriptor speaker_input_terminal_desc = {
	.bLength		= UAC_DT_INPUT_TERMINAL_SIZE,
	.bDescriptorType	= USB_DT_CS_INTERFACE,
	.bDescriptorSubtype	= UAC_INPUT_TERMINAL,
	.bTerminalID		= SPEAKER_INPUT_TERMINAL_ID,
	.wTerminalType		= UAC_TERMINAL_STREAMING,
	.bAssocTerminal		= SPEAKER_OUTPUT_TERMINAL_ID,
	.wChannelConfig		= 0x3,
};

struct uac1_output_terminal_descriptor speaker_output_terminal_desc = {
	.bLength		= UAC_DT_OUTPUT_TERMINAL_SIZE,
	.bDescriptorType	= USB_DT_CS_INTERFACE,
	.bDescriptorSubtype	= UAC_OUTPUT_TERMINAL,
	.bTerminalID		= SPEAKER_OUTPUT_TERMINAL_ID,
	.wTerminalType		= UAC_OUTPUT_TERMINAL_SPEAKER,
	.bAssocTerminal		= SPEAKER_INPUT_TERMINAL_ID,
	.bSourceID		= SPEAKER_INPUT_TERMINAL_ID,
};

static struct usb_audio_control speaker_mute_control = {
	.list = LIST_HEAD_INIT(speaker_mute_control.list),
	.name = "Speaker Mute Control",
	.type = UAC_FU_MUTE,
	/* Todo: add real Mute control code */
	.set = generic_set_cmd,
	.get = generic_get_cmd,
};

static struct usb_audio_control speaker_volume_control = {
	.list = LIST_HEAD_INIT(speaker_volume_control.list),
	.name = "Speaker Volume Control",
	.type = UAC_FU_VOLUME,
	/* Todo: add real Volume control code */
	.set = generic_set_cmd,
	.get = generic_get_cmd,
};

static struct usb_audio_control_selector speaker_fu_controls = {
	.list = LIST_HEAD_INIT(speaker_fu_controls.list),
	.name = "Speaker Function Unit Controls",
};

static struct uac_input_terminal_descriptor microphone_input_terminal_desc = {
	.bLength		= UAC_DT_INPUT_TERMINAL_SIZE,
	.bDescriptorType	= USB_DT_CS_INTERFACE,
	.bDescriptorSubtype	= UAC_INPUT_TERMINAL,
	.bTerminalID		= MICROPHONE_INPUT_TERMINAL_ID,
	.wTerminalType		= UAC_INPUT_TERMINAL_MICROPHONE,
	.bAssocTerminal		= MICROPHONE_OUTPUT_TERMINAL_ID,
	.bNrChannels		= 1,
	.wChannelConfig		= 0x3,
};

static struct
uac1_output_terminal_descriptor microphone_output_terminal_desc = {
	.bLength		= UAC_DT_OUTPUT_TERMINAL_SIZE,
	.bDescriptorType	= USB_DT_CS_INTERFACE,
	.bDescriptorSubtype	= UAC_OUTPUT_TERMINAL,
	.bTerminalID		= MICROPHONE_OUTPUT_TERMINAL_ID,
	.wTerminalType		= UAC_TERMINAL_STREAMING,
	.bAssocTerminal		= MICROPHONE_INPUT_TERMINAL_ID,
	.bSourceID		= MICROPHONE_INPUT_TERMINAL_ID,
};

static struct usb_audio_control microphone_mute_control = {
	.list = LIST_HEAD_INIT(microphone_mute_control.list),
	.name = "Microphone Mute Control",
	.type = UAC_FU_MUTE,
	/* Todo: add real Mute control code */
	.set = generic_set_cmd,
	.get = generic_get_cmd,
};

static struct usb_audio_control microphone_volume_control = {
	.list = LIST_HEAD_INIT(microphone_volume_control.list),
	.name = "Microphone Volume Control",
	.type = UAC_FU_VOLUME,
	/* Todo: add real Volume control code */
	.set = generic_set_cmd,
	.get = generic_get_cmd,
};

static struct usb_audio_control_selector microphone_fu_controls = {
	.list = LIST_HEAD_INIT(microphone_fu_controls.list),
	.name = "Microphone Feature Unit Controls",
};

/*---------------------------------*/

 /* B.4.1  Standard AS Interface Descriptor */
static struct usb_interface_descriptor speaker_as_interface_alt_0_desc = {
	.bLength		= USB_DT_INTERFACE_SIZE,
	.bDescriptorType	= USB_DT_INTERFACE,
	.bAlternateSetting	= 0,
	.bNumEndpoints		= 0,
	.bInterfaceClass	= USB_CLASS_AUDIO,
	.bInterfaceSubClass	= USB_SUBCLASS_AUDIOSTREAMING,
};

static struct usb_interface_descriptor speaker_as_interface_alt_1_desc = {
	.bLength		= USB_DT_INTERFACE_SIZE,
	.bDescriptorType	= USB_DT_INTERFACE,
	.bAlternateSetting	= 1,
	.bNumEndpoints		= 1,
	.bInterfaceClass	= USB_CLASS_AUDIO,
	.bInterfaceSubClass	= USB_SUBCLASS_AUDIOSTREAMING,
};

 /* B.4.2  Class-Specific AS Interface Descriptor */
static struct uac1_as_header_descriptor speaker_as_header_desc = {
	.bLength		= UAC_DT_AS_HEADER_SIZE,
	.bDescriptorType	= USB_DT_CS_INTERFACE,
	.bDescriptorSubtype	= UAC_AS_GENERAL,
	.bTerminalLink		= SPEAKER_INPUT_TERMINAL_ID,
	.bDelay			= 1,
	.wFormatTag		= UAC_FORMAT_TYPE_I_PCM,
};

static struct uac_format_type_i_discrete_descriptor_1 speaker_as_type_i_desc = {
	.bLength		= UAC_FORMAT_TYPE_I_DISCRETE_DESC_SIZE(1),
	.bDescriptorType	= USB_DT_CS_INTERFACE,
	.bDescriptorSubtype	= UAC_FORMAT_TYPE,
	.bFormatType		= UAC_FORMAT_TYPE_I,
	.bSubframeSize		= 2,
	.bBitResolution		= 16,
	.bSamFreqType		= 1,
};

 /* Standard ISO OUT Endpoint Descriptor */
static struct usb_endpoint_descriptor speaker_as_ep_out_desc = {
	.bLength		= USB_DT_ENDPOINT_AUDIO_SIZE,
	.bDescriptorType	= USB_DT_ENDPOINT,
	.bEndpointAddress	= USB_DIR_OUT,
	.bmAttributes		= USB_ENDPOINT_SYNC_ADAPTIVE |
				  USB_ENDPOINT_XFER_ISOC,
	.wMaxPacketSize		=
		__constant_cpu_to_le16(PLAYBACK_EP_MAX_PACKET_SIZE),
	.bInterval		= 4,
};

static struct usb_ss_ep_comp_descriptor speaker_as_ep_out_comp_desc = {
	 .bLength =		 sizeof(speaker_as_ep_out_comp_desc),
	 .bDescriptorType =	 USB_DT_SS_ENDPOINT_COMP,

	 .wBytesPerInterval =	cpu_to_le16(1024),
};

/* Class-specific AS ISO OUT Endpoint Descriptor */
static struct uac_iso_endpoint_descriptor speaker_as_iso_out_desc  = {
	.bLength		= UAC_ISO_ENDPOINT_DESC_SIZE,
	.bDescriptorType	= USB_DT_CS_ENDPOINT,
	.bDescriptorSubtype	= UAC_EP_GENERAL,
	.bmAttributes		= 1,
	.bLockDelayUnits	= 1,
	.wLockDelay		= __constant_cpu_to_le16(1),
};

static struct usb_audio_control speaker_sample_freq_control = {
	.list = LIST_HEAD_INIT(speaker_sample_freq_control.list),
	.name = "Speaker Sampling Frequency Control",
	.type = UAC_EP_CS_ATTR_SAMPLE_RATE,
	.set  = generic_set_cmd,
	.get  = generic_get_cmd,
};

static struct usb_audio_control_selector speaker_as_iso_out = {
	.list = LIST_HEAD_INIT(speaker_as_iso_out.list),
	.name = "Speaker Iso-out Endpoint Control",
	.type = UAC_EP_GENERAL,
	.desc = (struct usb_descriptor_header *)&speaker_as_iso_out_desc,
};

/*---------------------------------*/

/* B.4.1  Standard AS Interface Descriptor */
static struct usb_interface_descriptor microphone_as_interface_alt_0_desc = {
	.bLength		= USB_DT_INTERFACE_SIZE,
	.bDescriptorType	= USB_DT_INTERFACE,
	.bAlternateSetting	= 0,
	.bNumEndpoints		= 0,
	.bInterfaceClass	= USB_CLASS_AUDIO,
	.bInterfaceSubClass	= USB_SUBCLASS_AUDIOSTREAMING,
};

static struct usb_interface_descriptor microphone_as_interface_alt_1_desc = {
	.bLength		= USB_DT_INTERFACE_SIZE,
	.bDescriptorType	= USB_DT_INTERFACE,
	.bAlternateSetting	= 1,
	.bNumEndpoints		= 1,
	.bInterfaceClass	= USB_CLASS_AUDIO,
	.bInterfaceSubClass	= USB_SUBCLASS_AUDIOSTREAMING,
};

/* B.4.2  Class-Specific AS Interface Descriptor */
static struct uac1_as_header_descriptor microphone_as_header_desc = {
	.bLength		= UAC_DT_AS_HEADER_SIZE,
	.bDescriptorType	= USB_DT_CS_INTERFACE,
	.bDescriptorSubtype	= UAC_AS_GENERAL,
	.bTerminalLink		= MICROPHONE_OUTPUT_TERMINAL_ID,
	.bDelay			= 1,
	.wFormatTag		= UAC_FORMAT_TYPE_I_PCM,
};

static struct
uac_format_type_i_discrete_descriptor_1 microphone_as_type_i_desc = {
	.bLength		= UAC_FORMAT_TYPE_I_DISCRETE_DESC_SIZE(1),
	.bDescriptorType	= USB_DT_CS_INTERFACE,
	.bDescriptorSubtype	= UAC_FORMAT_TYPE,
	.bFormatType		= UAC_FORMAT_TYPE_I,
	.bNrChannels		= 1,
	.bSubframeSize		= 2,
	.bBitResolution		= 16,
	.bSamFreqType		= 1,
};

/* Standard ISO IN Endpoint Descriptor */
static struct usb_endpoint_descriptor microphone_as_ep_in_desc = {
	.bLength		= USB_DT_ENDPOINT_AUDIO_SIZE,
	.bDescriptorType	= USB_DT_ENDPOINT,
	.bEndpointAddress	= USB_DIR_IN,
	.bmAttributes		=
		USB_ENDPOINT_XFER_ISOC | USB_ENDPOINT_SYNC_ASYNC,
	.wMaxPacketSize		=
		__constant_cpu_to_le16(CAPTURE_EP_MAX_PACKET_SIZE),
	.bInterval		= 4,
};

static struct usb_ss_ep_comp_descriptor microphone_as_ep_in_comp_desc = {
	 .bLength =		 sizeof(microphone_as_ep_in_comp_desc),
	 .bDescriptorType =	 USB_DT_SS_ENDPOINT_COMP,

	 .wBytesPerInterval =	cpu_to_le16(1024),
};

 /* Class-specific AS ISO OUT Endpoint Descriptor */
static struct uac_iso_endpoint_descriptor microphone_as_iso_in_desc  = {
	.bLength		= UAC_ISO_ENDPOINT_DESC_SIZE,
	.bDescriptorType	= USB_DT_CS_ENDPOINT,
	.bDescriptorSubtype	= UAC_EP_GENERAL,
	.bmAttributes		= 1,
	.bLockDelayUnits	= 1,
	.wLockDelay		= __constant_cpu_to_le16(1),
};

static struct usb_audio_control microphone_sample_freq_control = {
	.list = LIST_HEAD_INIT(microphone_sample_freq_control.list),
	.name = "Microphone Sampling Frequency Control",
	.type = UAC_EP_CS_ATTR_SAMPLE_RATE,
	.set  = generic_set_cmd,
	.get  = generic_get_cmd,
};

static struct usb_audio_control_selector microphone_as_iso_in = {
	.list = LIST_HEAD_INIT(microphone_as_iso_in.list),
	.name = "Microphone Iso-IN Endpoint Control",
	.type = UAC_EP_GENERAL,
	.desc = (struct usb_descriptor_header *)&microphone_as_iso_in_desc,
};

/*--------------------------------- */

static struct usb_descriptor_header *f_audio_desc[]  = {
	(struct usb_descriptor_header *)&uac1_ac_interface_desc,
	(struct usb_descriptor_header *)&uac1_ac_header_desc,

	(struct usb_descriptor_header *)&microphone_input_terminal_desc,
	(struct usb_descriptor_header *)&microphone_output_terminal_desc,

	(struct usb_descriptor_header *)&speaker_input_terminal_desc,
	(struct usb_descriptor_header *)&speaker_output_terminal_desc,

	(struct usb_descriptor_header *)&microphone_as_interface_alt_0_desc,
	(struct usb_descriptor_header *)&microphone_as_interface_alt_1_desc,
	(struct usb_descriptor_header *)&microphone_as_header_desc,
	(struct usb_descriptor_header *)&microphone_as_type_i_desc,
	(struct usb_descriptor_header *)&microphone_as_ep_in_desc,
	(struct usb_descriptor_header *)&microphone_as_iso_in_desc,

	(struct usb_descriptor_header *)&speaker_as_interface_alt_0_desc,
	(struct usb_descriptor_header *)&speaker_as_interface_alt_1_desc,
	(struct usb_descriptor_header *)&speaker_as_header_desc,
	(struct usb_descriptor_header *)&speaker_as_type_i_desc,
	(struct usb_descriptor_header *)&speaker_as_ep_out_desc,
	(struct usb_descriptor_header *)&speaker_as_iso_out_desc,

	NULL,
};

static struct usb_descriptor_header *f_audio_ss_desc[]  = {
	(struct usb_descriptor_header *)&uac1_ac_interface_desc,
	(struct usb_descriptor_header *)&uac1_ac_header_desc,

	(struct usb_descriptor_header *)&microphone_input_terminal_desc,
	(struct usb_descriptor_header *)&microphone_output_terminal_desc,

	(struct usb_descriptor_header *)&speaker_input_terminal_desc,
	(struct usb_descriptor_header *)&speaker_output_terminal_desc,

	(struct usb_descriptor_header *)&microphone_as_interface_alt_0_desc,
	(struct usb_descriptor_header *)&microphone_as_interface_alt_1_desc,
	(struct usb_descriptor_header *)&microphone_as_header_desc,
	(struct usb_descriptor_header *)&microphone_as_type_i_desc,
	(struct usb_descriptor_header *)&microphone_as_ep_in_desc,
	(struct usb_descriptor_header *)&microphone_as_ep_in_comp_desc,
	(struct usb_descriptor_header *)&microphone_as_iso_in_desc,

	(struct usb_descriptor_header *)&speaker_as_interface_alt_0_desc,
	(struct usb_descriptor_header *)&speaker_as_interface_alt_1_desc,
	(struct usb_descriptor_header *)&speaker_as_header_desc,
	(struct usb_descriptor_header *)&speaker_as_type_i_desc,
	(struct usb_descriptor_header *)&speaker_as_ep_out_desc,
	(struct usb_descriptor_header *)&speaker_as_ep_out_comp_desc,
	(struct usb_descriptor_header *)&speaker_as_iso_out_desc,

	NULL,
};

/* string IDs are assigned dynamically */

static struct usb_string audio_string_defs[] = {
	{  } /* end of list */
};

static struct usb_gadget_strings audio_stringtab_dev = {
	.language	= 0x0409,	/* en-us */
	.strings	= audio_string_defs,
};

static struct usb_gadget_strings *audio_strings[] = {
	&audio_stringtab_dev,
	NULL,
};

/*
 * This function is an ALSA sound card following USB Audio Class Spec 1.0.
 */

/*-------------------------------------------------------------------------*/
struct f_audio_buf {
	u8 *buf;
	int actual;
	struct list_head list;
};

static struct f_audio_buf *f_audio_buffer_alloc(int buf_size)
{
	struct f_audio_buf *playback_copy_buf;

	playback_copy_buf = kzalloc(sizeof *playback_copy_buf, GFP_ATOMIC);
	if (!playback_copy_buf) {
		pr_err("Failed to allocate playback_copy_buf");
		return ERR_PTR(-ENOMEM);
	}

	playback_copy_buf->buf = kzalloc(buf_size, GFP_ATOMIC);
	if (!playback_copy_buf->buf) {
		pr_err("Failed to allocate playback_copy_buf buffer");
		kfree(playback_copy_buf);
		return ERR_PTR(-ENOMEM);
	}

	return playback_copy_buf;
}

static void f_audio_buffer_free(struct f_audio_buf *audio_buf)
{
	if (audio_buf) {
		kfree(audio_buf->buf);
		audio_buf->buf = NULL;
		kfree(audio_buf);
		audio_buf = NULL;
	}
}
/*-------------------------------------------------------------------------*/

struct f_audio {
	struct gaudio			card;

	/* endpoints handle full and/or high speeds */
	struct usb_ep			*out_ep;
	struct usb_ep			*in_ep;

	spinlock_t			playback_lock;
	struct f_audio_buf		*playback_copy_buf;
	struct work_struct		playback_work;
	struct list_head		play_queue;

	spinlock_t			capture_lock;
	struct f_audio_buf		*capture_copy_buf;
	struct work_struct		capture_work;
	struct list_head		capture_queue;
	struct usb_request		*capture_req;

	u8				alt_intf[F_AUDIO_NUM_INTERFACES];

	/* Control Set command */
	struct list_head		fu_cs;
	struct list_head		ep_cs;
	u8				set_cmd;
	struct usb_audio_control	*set_con;
};

static inline struct f_audio *func_to_audio(struct usb_function *f)
{
	return container_of(f, struct f_audio, card.func);
}

/*-------------------------------------------------------------------------*/

static void f_audio_playback_work(struct work_struct *data)
{
	struct f_audio *audio = container_of(data, struct f_audio,
					playback_work);
	struct f_audio_buf *play_buf;
	unsigned long flags;
	int res = 0;

	spin_lock_irqsave(&audio->playback_lock, flags);
	if (list_empty(&audio->play_queue)) {
		pr_err("playback_buf is empty");
		spin_unlock_irqrestore(&audio->playback_lock, flags);
		return;
	}
	play_buf = list_first_entry(&audio->play_queue,
			struct f_audio_buf, list);
	list_del(&play_buf->list);
	spin_unlock_irqrestore(&audio->playback_lock, flags);

	pr_debug("play_buf->actual = %d", play_buf->actual);

	res = u_audio_playback(&audio->card, play_buf->buf, play_buf->actual);
	if (res)
		pr_err("copying failed");

	f_audio_buffer_free(play_buf);
}

static int
f_audio_playback_ep_complete(struct usb_ep *ep, struct usb_request *req)
{
	struct f_audio *audio = req->context;
	struct f_audio_buf *copy_buf = audio->playback_copy_buf;
	int err;

	if (!copy_buf)
		return -EINVAL;

	/* Copy buffer is full, add it to the play_queue */
	if (audio_playback_buf_size - copy_buf->actual < req->actual) {
		pr_debug("audio_playback_buf_size %d - copy_buf->actual %d, req->actual %d",
			audio_playback_buf_size, copy_buf->actual, req->actual);
		list_add_tail(&copy_buf->list, &audio->play_queue);
		schedule_work(&audio->playback_work);
		copy_buf = f_audio_buffer_alloc(audio_playback_buf_size);
		if (IS_ERR(copy_buf)) {
			pr_err("Failed to allocate playback_copy_buf");
			return -ENOMEM;
		}
	}

	memcpy(copy_buf->buf + copy_buf->actual, req->buf, req->actual);
	copy_buf->actual += req->actual;
	audio->playback_copy_buf = copy_buf;

	err = usb_ep_queue(ep, req, GFP_ATOMIC);
	if (err)
		pr_err("Failed to queue %s req: err - %d\n", ep->name, err);

	return err;
}

static void f_audio_capture_work(struct work_struct *data)
{
	struct f_audio *audio =
			container_of(data, struct f_audio, capture_work);
	struct f_audio_buf *capture_buf;
	unsigned long flags;
	int res = 0;

	capture_buf = f_audio_buffer_alloc(audio_capture_buf_size);
	if (capture_buf <= 0) {
		pr_err("%s: buffer alloc failed\n", __func__);
		return;
	}

	res = u_audio_capture(&audio->card, capture_buf->buf,
			audio_capture_buf_size);
	if (res)
		pr_err("copying failed");

	pr_debug("Queue capture packet: size %d", audio_capture_buf_size);
	spin_lock_irqsave(&audio->capture_lock, flags);
	list_add_tail(&capture_buf->list, &audio->capture_queue);
	spin_unlock_irqrestore(&audio->capture_lock, flags);
}

static int
f_audio_capture_ep_complete(struct usb_ep *ep, struct usb_request *req)
{
	struct f_audio *audio = req->context;
	struct f_audio_buf *copy_buf = audio->capture_copy_buf;
	unsigned long flags;
	int err = 0;

	if (copy_buf == 0) {
		pr_debug("copy_buf == 0");
		spin_lock_irqsave(&audio->capture_lock, flags);
		if (list_empty(&audio->capture_queue)) {
			spin_unlock_irqrestore(&audio->capture_lock, flags);
			schedule_work(&audio->capture_work);
			goto done;
		}
		copy_buf = list_first_entry(&audio->capture_queue,
						struct f_audio_buf, list);
		list_del(&copy_buf->list);
		audio->capture_copy_buf = copy_buf;
		spin_unlock_irqrestore(&audio->capture_lock, flags);
	}

	pr_debug("Copy %d bytes", req->actual);
	memcpy(req->buf, copy_buf->buf + copy_buf->actual, req->actual);
	copy_buf->actual += req->actual;

	if (audio_capture_buf_size - copy_buf->actual < req->actual) {
		f_audio_buffer_free(copy_buf);
		audio->capture_copy_buf = 0;
		schedule_work(&audio->capture_work);
	}
done:
	err = usb_ep_queue(ep, req, GFP_ATOMIC);
	if (err)
		pr_err("Failed to queue %s req: err - %d\n", ep->name, err);

	return err;
}

static void f_audio_complete(struct usb_ep *ep, struct usb_request *req)
{
	struct f_audio *audio = req->context;
	int status = req->status;
	u32 data = 0;

	switch (status) {
	case 0:	/* normal completion? */
		if (ep == audio->out_ep) {
			f_audio_playback_ep_complete(ep, req);
		} else if (ep == audio->in_ep) {
			f_audio_capture_ep_complete(ep, req);
		} else if (audio->set_con) {
			memcpy(&data, req->buf, req->length);
			audio->set_con->set(audio->set_con, audio->set_cmd,
					le16_to_cpu(data));
			audio->set_con = NULL;
		}
		break;
	default:
		pr_err("Failed completion: status %d", status);
		break;
	}
}

static int audio_set_intf_req(struct usb_function *f,
		const struct usb_ctrlrequest *ctrl)
{
	struct f_audio		*audio = func_to_audio(f);
	struct usb_composite_dev *cdev = f->config->cdev;
	struct usb_request	*req = cdev->req;
	u8			id = ((le16_to_cpu(ctrl->wIndex) >> 8) & 0xFF);
	u16			len = le16_to_cpu(ctrl->wLength);
	u16			w_value = le16_to_cpu(ctrl->wValue);
	u8			con_sel = (w_value >> 8) & 0xFF;
	u8			cmd = (ctrl->bRequest & 0x0F);
	struct usb_audio_control_selector *cs;
	struct usb_audio_control *con;

	pr_debug("bRequest 0x%x, w_value 0x%04x, len %d, entity %d\n",
			ctrl->bRequest, w_value, len, id);

	list_for_each_entry(cs, &audio->fu_cs, list) {
		if (cs->id == id) {
			list_for_each_entry(con, &cs->control, list) {
				if (con->type == con_sel) {
					audio->set_con = con;
					break;
				}
			}
			break;
		}
	}

	audio->set_cmd = cmd;
	req->context = audio;
	req->complete = f_audio_complete;

	return len;
}

static int audio_get_intf_req(struct usb_function *f,
		const struct usb_ctrlrequest *ctrl)
{
	struct f_audio		*audio = func_to_audio(f);
	struct usb_composite_dev *cdev = f->config->cdev;
	struct usb_request	*req = cdev->req;
	int			value = -EOPNOTSUPP;
	u8			id = ((le16_to_cpu(ctrl->wIndex) >> 8) & 0xFF);
	u16			len = le16_to_cpu(ctrl->wLength);
	u16			w_value = le16_to_cpu(ctrl->wValue);
	u8			con_sel = (w_value >> 8) & 0xFF;
	u8			cmd = (ctrl->bRequest & 0x0F);
	struct usb_audio_control_selector *cs;
	struct usb_audio_control *con;

	pr_debug("bRequest 0x%x, w_value 0x%04x, len %d, entity %d\n",
			ctrl->bRequest, w_value, len, id);

	list_for_each_entry(cs, &audio->fu_cs, list) {
		if (cs->id == id) {
			list_for_each_entry(con, &cs->control, list) {
				if (con->type == con_sel && con->get) {
					value = con->get(con, cmd);
					break;
				}
			}
			break;
		}
	}

	req->context = audio;
	req->complete = f_audio_complete;
	len = min_t(size_t, sizeof(value), len);
	memcpy(req->buf, &value, len);

	return len;
}

static void audio_set_endpoint_complete(struct usb_ep *ep,
					struct usb_request *req)
{
	struct f_audio *audio = req->context;
	u32 data = 0;

	if (req->status == 0 && audio->set_con) {
		memcpy(&data, req->buf, req->length);
		audio->set_con->set(audio->set_con, audio->set_cmd,
					le32_to_cpu(data));
		audio->set_con = NULL;
	}
}

static int audio_set_endpoint_req(struct usb_function *f,
		const struct usb_ctrlrequest *ctrl)
{
	int	value = -EOPNOTSUPP;
	u16	ep = le16_to_cpu(ctrl->wIndex);
	u16	len = le16_to_cpu(ctrl->wLength);
	u16	w_value = le16_to_cpu(ctrl->wValue);

	struct f_audio *audio = func_to_audio(f);
	struct usb_composite_dev *cdev = f->config->cdev;
	struct usb_request *req = cdev->req;
	struct usb_audio_control_selector *cs;
	struct usb_audio_control *con;

	u8	epnum   = ep & ~0x80;
	u8	con_sel = (w_value >> 8) & 0xFF;
	u8	cmd     = (ctrl->bRequest & 0x0F);

	pr_debug("bRequest 0x%x, w_value 0x%04x, len %d, endp %d, epnum %d\n",
			ctrl->bRequest, w_value, len, ep, epnum);

	list_for_each_entry(cs, &audio->ep_cs, list) {
		if (cs->id != epnum)
			continue;

		list_for_each_entry(con, &cs->control, list) {
			if (con->type != con_sel)
				continue;

			switch (cmd) {
			case UAC__CUR:
			case UAC__MIN:
			case UAC__MAX:
			case UAC__RES:
				audio->set_con = con;
				audio->set_cmd = cmd;
				req->context   = audio;
				req->complete  = audio_set_endpoint_complete;
				value = len;
				break;
			case UAC__MEM:
				break;
			default:
				pr_err("Unknown command");
				break;
			}
			break;
		}
		break;
	}

	return value;
}

static int audio_get_endpoint_req(struct usb_function *f,
		const struct usb_ctrlrequest *ctrl)
{
	struct f_audio *audio = func_to_audio(f);
	struct usb_composite_dev *cdev = f->config->cdev;
	struct usb_request *req = cdev->req;
	struct usb_audio_control_selector *cs;
	struct usb_audio_control *con;
	int data;

	int value   = -EOPNOTSUPP;
	u8  ep      = (le16_to_cpu(ctrl->wIndex) & 0x7F);
	u8  epnum   = ep & ~0x80;
	u16 len     = le16_to_cpu(ctrl->wLength);
	u16 w_value = le16_to_cpu(ctrl->wValue);
	u8  con_sel = (w_value >> 8) & 0xFF;
	u8  cmd     = (ctrl->bRequest & 0x0F);

	pr_debug("bRequest 0x%x, w_value 0x%04x, len %d, ep %d\n",
			ctrl->bRequest, w_value, len, ep);

	list_for_each_entry(cs, &audio->ep_cs, list) {
		if (cs->id != epnum)
			continue;

		list_for_each_entry(con, &cs->control, list) {
			if (con->type != con_sel)
				continue;

			switch (cmd) {
			case UAC__CUR:
			case UAC__MIN:
			case UAC__MAX:
			case UAC__RES:
				data = cpu_to_le32(generic_get_cmd(con, cmd));
				memcpy(req->buf, &data, len);
				value = len;
				break;
			case UAC__MEM:
				break;
			default:
				break;
			}
			break;
		}
		break;
	}

	return value;
}

static int
f_audio_setup(struct usb_function *f, const struct usb_ctrlrequest *ctrl)
{
	struct usb_composite_dev *cdev = f->config->cdev;
	struct usb_request	*req = cdev->req;
	int			value = -EOPNOTSUPP;
	u16			w_index = le16_to_cpu(ctrl->wIndex);
	u16			w_value = le16_to_cpu(ctrl->wValue);
	u16			w_length = le16_to_cpu(ctrl->wLength);

	/* composite driver infrastructure handles everything; interface
	 * activation uses set_alt().
	 */

	switch (ctrl->bRequestType) {
	case USB_DIR_OUT | USB_TYPE_CLASS | USB_RECIP_INTERFACE:
		pr_debug("USB_DIR_OUT | USB_TYPE_CLASS | USB_RECIP_INTERFACE");
		value = audio_set_intf_req(f, ctrl);
		break;

	case USB_DIR_IN | USB_TYPE_CLASS | USB_RECIP_INTERFACE:
		pr_debug("USB_DIR_IN | USB_TYPE_CLASS | USB_RECIP_INTERFACE");
		value = audio_get_intf_req(f, ctrl);
		break;

	case USB_DIR_OUT | USB_TYPE_CLASS | USB_RECIP_ENDPOINT:
		pr_debug("USB_DIR_OUT | USB_TYPE_CLASS | USB_RECIP_ENDPOINT");
		value = audio_set_endpoint_req(f, ctrl);
		break;

	case USB_DIR_IN | USB_TYPE_CLASS | USB_RECIP_ENDPOINT:
		pr_debug("USB_DIR_IN | USB_TYPE_CLASS | USB_RECIP_ENDPOINT");
		value = audio_get_endpoint_req(f, ctrl);
		break;

	default:
		pr_err("Unknown control request %02x.%02x v%04x i%04x l%d\n",
			ctrl->bRequestType, ctrl->bRequest,
			w_value, w_index, w_length);

	}

	/* respond with data transfer or status phase? */
	if (value >= 0) {
		pr_debug("audio req %02x.%02x v%04x i%04x l%d\n",
			ctrl->bRequestType, ctrl->bRequest,
			w_value, w_index, w_length);
		req->zero = 1;
		req->length = value;
		value = usb_ep_queue(cdev->gadget->ep0, req, GFP_ATOMIC);
		if (value < 0)
			pr_err("audio response failed on err %d\n", value);
	} else {
		pr_err("STALL\n");
	}

	/* device either stalls (value < 0) or reports success */
	return value;
}

static int f_audio_get_alt(struct usb_function *f, unsigned intf)
{
	struct f_audio	*audio = func_to_audio(f);

	if (intf == uac1_ac_header_desc.baInterfaceNr[0])
		return audio->alt_intf[0];
	if (intf == uac1_ac_header_desc.baInterfaceNr[1])
		return audio->alt_intf[1];

	return 0;
}

static int f_audio_set_alt(struct usb_function *f, unsigned intf, unsigned alt)
{
	struct f_audio		*audio = func_to_audio(f);
	struct usb_ep		*out_ep = audio->out_ep;
	struct usb_ep		*in_ep = audio->in_ep;
	struct usb_request	*req;
	unsigned long flags;
	int i = 0, err = 0;

	pr_debug("intf %d, alt %d\n", intf, alt);

	if (intf == uac1_ac_header_desc.baInterfaceNr[0]) {
		if (audio->alt_intf[0] == alt) {
			pr_debug("Alt interface is already set to %d. Do nothing.\n",
				alt);

			return 0;
		}

		if (alt == 1) {
			err = usb_ep_enable(in_ep);
			if (err) {
				pr_err("Failed to enable capture ep");
				return err;
			}
			in_ep->driver_data = audio;
			audio->capture_copy_buf = 0;

			/* Allocate a write buffer */
			req = usb_ep_alloc_request(in_ep, GFP_ATOMIC);
			if (!req) {
				pr_err("request allocation failed\n");
				return -ENOMEM;
			}
			req->buf = kzalloc(req_capture_buf_size,
						GFP_ATOMIC);
			if (!req->buf) {
				pr_err("request buffer allocation failed\n");
				return -ENOMEM;
			}

			req->length = req_capture_buf_size;
			req->context = audio;
			req->complete =	f_audio_complete;
			audio->capture_req = req;
			err = usb_ep_queue(in_ep, req, GFP_ATOMIC);
			if (err)
				pr_err("Failed to queue %s req: err %d\n",
				 in_ep->name, err);
			schedule_work(&audio->capture_work);
		} else {
			struct f_audio_buf *capture_buf;
			spin_lock_irqsave(&audio->capture_lock, flags);
			while (!list_empty(&audio->capture_queue)) {
				capture_buf =
					list_first_entry(
						&audio->capture_queue,
						 struct f_audio_buf,
						 list);
				list_del(&capture_buf->list);
				f_audio_buffer_free(capture_buf);
			}
			spin_unlock_irqrestore(&audio->capture_lock, flags);
		}
		audio->alt_intf[0] = alt;
	} else if (intf == uac1_ac_header_desc.baInterfaceNr[1]) {
		if (audio->alt_intf[1] == alt) {
			pr_debug("Alt interface is already set to %d. Do nothing.\n",
				alt);

			return 0;
		}

		if (alt == 1) {
			err = usb_ep_enable(out_ep);
			if (err) {
				pr_err("Failed to enable playback ep");
				return err;
			}
			out_ep->driver_data = audio;
			audio->playback_copy_buf =
				f_audio_buffer_alloc(audio_playback_buf_size);
			if (IS_ERR(audio->playback_copy_buf)) {
				pr_err("Failed to allocate playback_copy_buf");
				return -ENOMEM;
			}

			/*
			 * allocate a bunch of read buffers
			 * and queue them all at once.
			 */
			for (i = 0; i < req_playback_count && err == 0; i++) {
				req = usb_ep_alloc_request(out_ep, GFP_ATOMIC);
				if (!req) {
					pr_err("request allocation failed\n");
					return -ENOMEM;
				}
				req->buf = kzalloc(req_playback_buf_size,
						GFP_ATOMIC);
				if (!req->buf) {
					pr_err("request buffer allocation failed\n");
					return -ENOMEM;
				}
				req->length = req_playback_buf_size;
				req->context = audio;
				req->complete =	f_audio_complete;
				err = usb_ep_queue(out_ep, req, GFP_ATOMIC);
				if (err)
					pr_err("Failed to queue %s queue req: err %d\n",
						out_ep->name, err);
			}
			pr_debug("Allocated %d requests\n", req_playback_count);
		} else {
			struct f_audio_buf *playback_copy_buf =
				audio->playback_copy_buf;
			if (playback_copy_buf) {
				pr_err("Schedule playback_work");
				list_add_tail(&playback_copy_buf->list,
						&audio->play_queue);
				schedule_work(&audio->playback_work);
				audio->playback_copy_buf = NULL;
			} else {
				pr_err("playback_buf is empty. Stop.");
			}
		}
		audio->alt_intf[1] = alt;
	} else {
		pr_err("Interface %d. Do nothing. Return %d\n", intf, err);
	}

	return err;
}

static void f_audio_disable(struct usb_function *f)
{
	u_audio_clear();
}

/*-------------------------------------------------------------------------*/

static void f_audio_build_desc(struct f_audio *audio)
{
	struct gaudio *card = &audio->card;
	u8 *sam_freq;
	int rate;

	/* Set channel numbers */
	speaker_input_terminal_desc.bNrChannels =
			u_audio_get_playback_channels(card);
	speaker_as_type_i_desc.bNrChannels =
			u_audio_get_playback_channels(card);

	microphone_input_terminal_desc.bNrChannels =
			u_audio_get_capture_channels(card);
	microphone_as_type_i_desc.bNrChannels =
			u_audio_get_capture_channels(card);

	/* Set sample rates */
	rate = u_audio_get_playback_rate(card);
	sam_freq = speaker_as_type_i_desc.tSamFreq[0];
	memcpy(sam_freq, &rate, 3);

	rate = u_audio_get_capture_rate(card);
	sam_freq = microphone_as_type_i_desc.tSamFreq[0];
	memcpy(sam_freq, &rate, 3);

	/* Todo: Set Sample bits and other parameters */

	return;
}

/* audio function driver setup/binding */
static int
f_audio_bind(struct usb_configuration *c, struct usb_function *f)
{
	struct usb_composite_dev *cdev = c->cdev;
	struct f_audio		*audio = func_to_audio(f);
	int			status;
	struct usb_ep		*ep = NULL;
	u8			epaddr;


	f_audio_build_desc(audio);

	/* allocate instance-specific interface IDs, and patch descriptors */
	status = usb_interface_id(c, f);
	if (status < 0) {
		pr_err("%s: failed to allocate desc interface", __func__);
		goto fail;
	}
	uac1_ac_interface_desc.bInterfaceNumber = status;

	status = -ENOMEM;

	status = usb_interface_id(c, f);
	if (status < 0) {
		pr_err("%s: failed to allocate alt interface", __func__);
		goto fail;
	}
	microphone_as_interface_alt_0_desc.bInterfaceNumber = status;
	microphone_as_interface_alt_1_desc.bInterfaceNumber = status;
	uac1_ac_header_desc.baInterfaceNr[0] = status;
	audio->alt_intf[0] = 0;

	status = -ENODEV;

	status = usb_interface_id(c, f);
	if (status < 0) {
		pr_err("%s: failed to allocate alt interface", __func__);
		goto fail;
	}
	speaker_as_interface_alt_0_desc.bInterfaceNumber = status;
	speaker_as_interface_alt_1_desc.bInterfaceNumber = status;
	uac1_ac_header_desc.baInterfaceNr[1] = status;
	audio->alt_intf[1] = 0;

	status = -ENODEV;

	/* allocate instance-specific endpoints */
	ep = usb_ep_autoconfig(cdev->gadget, &microphone_as_ep_in_desc);
	if (!ep) {
		pr_err("%s: failed to autoconfig in endpoint", __func__);
		goto fail;
	}
	audio->in_ep = ep;
	ep->desc = &microphone_as_ep_in_desc;
	ep->driver_data = cdev;

	status = -ENOMEM;

	ep = usb_ep_autoconfig(cdev->gadget, &speaker_as_ep_out_desc);
	if (!ep) {
		pr_err("%s: failed to autoconfig out endpoint", __func__);
		goto fail;
	}
	audio->out_ep = ep;
	ep->desc = &speaker_as_ep_out_desc;
	ep->driver_data = cdev;

	/* associate bEndpointAddress with usb_function */
	epaddr = microphone_as_ep_in_desc.bEndpointAddress & ~USB_DIR_IN;
	microphone_as_iso_in.id = epaddr;

	epaddr = speaker_as_ep_out_desc.bEndpointAddress & ~USB_DIR_IN;
	speaker_as_iso_out.id = epaddr;

	/* copy descriptors, and track endpoint copies */
	status = usb_assign_descriptors(f, f_audio_desc, f_audio_desc,
					f_audio_ss_desc);
	if (status)
		goto fail;
	return 0;

fail:
	if (ep)
		ep->driver_data = NULL;
	return status;
}

static void
f_audio_unbind(struct usb_configuration *c, struct usb_function *f)
{
	struct f_audio *audio = func_to_audio(f);

	usb_free_all_descriptors(f);
	kfree(audio);
}

/*-------------------------------------------------------------------------*/

static int generic_set_cmd(struct usb_audio_control *con, u8 cmd, int value)
{
	con->data[cmd] = value;

	return 0;
}

static int generic_get_cmd(struct usb_audio_control *con, u8 cmd)
{
	return con->data[cmd];
}

/* Todo: add more control selecotor dynamically */
int  control_selector_init(struct f_audio *audio)
{
	INIT_LIST_HEAD(&audio->fu_cs);
	list_add(&microphone_fu_controls.list, &audio->fu_cs);
	list_add(&speaker_fu_controls.list, &audio->fu_cs);

	INIT_LIST_HEAD(&microphone_fu_controls.control);
	list_add(&microphone_mute_control.list,
		 &microphone_fu_controls.control);
	list_add(&microphone_volume_control.list,
		 &microphone_fu_controls.control);

	INIT_LIST_HEAD(&speaker_fu_controls.control);
	list_add(&speaker_mute_control.list,
		 &speaker_fu_controls.control);
	list_add(&speaker_volume_control.list,
		 &speaker_fu_controls.control);

	microphone_volume_control.data[UAC__CUR] = 0xffc0;
	microphone_volume_control.data[UAC__MIN] = 0xe3a0;
	microphone_volume_control.data[UAC__MAX] = 0xfff0;
	microphone_volume_control.data[UAC__RES] = 0x0030;

	speaker_volume_control.data[UAC__CUR] = 0xffc0;
	speaker_volume_control.data[UAC__MIN] = 0xe3a0;
	speaker_volume_control.data[UAC__MAX] = 0xfff0;
	speaker_volume_control.data[UAC__RES] = 0x0030;

	INIT_LIST_HEAD(&audio->ep_cs);
	list_add(&speaker_as_iso_out.list, &audio->ep_cs);
	list_add(&microphone_as_iso_in.list, &audio->ep_cs);

	INIT_LIST_HEAD(&microphone_as_iso_in.control);
	list_add(&microphone_sample_freq_control.list,
		 &microphone_as_iso_in.control);

	INIT_LIST_HEAD(&speaker_as_iso_out.control);
	list_add(&speaker_sample_freq_control.list,
		 &speaker_as_iso_out.control);

	return 0;

}

/**
 * audio_bind_config - add USB audio function to a configuration
 * @c: the configuration to support the USB audio function
 * Context: single threaded during gadget setup
 *
 * Returns zero on success, else negative errno.
 */
int audio_bind_config(struct usb_configuration *c)
{
	struct f_audio *audio;
	int status;

	/* allocate and initialize one new instance */
	audio = kzalloc(sizeof *audio, GFP_KERNEL);
	if (!audio)
		return -ENOMEM;

	audio->card.gadget = c->cdev->gadget;

	INIT_LIST_HEAD(&audio->play_queue);
	spin_lock_init(&audio->playback_lock);

	INIT_LIST_HEAD(&audio->capture_queue);
	spin_lock_init(&audio->capture_lock);

	audio->card.func.name = "audio";
	audio->card.func.strings = audio_strings;
	audio->card.func.bind = f_audio_bind;
	audio->card.func.unbind = f_audio_unbind;
	audio->card.func.get_alt = f_audio_get_alt;
	audio->card.func.set_alt = f_audio_set_alt;
	audio->card.func.setup = f_audio_setup;
	audio->card.func.disable = f_audio_disable;

	control_selector_init(audio);
	INIT_WORK(&audio->playback_work, f_audio_playback_work);
	INIT_WORK(&audio->capture_work, f_audio_capture_work);

	/* set up ASLA audio devices */
	status = gaudio_setup(&audio->card);
	if (status < 0)
		goto add_fail;

	status = usb_add_function(c, &audio->card.func);
	if (status) {
		pr_err("%s: Failed to add usb audio function, err = %d",
			__func__, status);
		goto setup_fail;
	}

	return status;

add_fail:
	gaudio_cleanup();
setup_fail:
	kfree(audio);
	return status;
}
