/*
 *Copyright 2014 NXP Semiconductors
 *
 *Licensed under the Apache License, Version 2.0 (the "License");
 *you may not use this file except in compliance with the License.
 *You may obtain a copy of the License at
 *
 *http://www.apache.org/licenses/LICENSE-2.0
 *
 *Unless required by applicable law or agreed to in writing, software
 *distributed under the License is distributed on an "AS IS" BASIS,
 *WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *See the License for the specific language governing permissions and
 *limitations under the License.
 */

#include "tfa_service.h"
#include "tfa_container.h"
#include "config.h"
#include "tfa.h"
#include "tfa_dsp_fw.h"
#include "tfa98xx_tfafieldnames.h"

/* module globals */
static int tfa98xx_cnt_verbose;

static nxpTfaContainer_t *g_cont; /* container file */
static int g_devs = -1; /* nr of devices TODO use direct access to cont? */
static nxpTfaDeviceList_t *g_dev[TFACONT_MAXDEVS];
static int g_profs[TFACONT_MAXDEVS];
static int g_liveds[TFACONT_MAXDEVS];
static nxpTfaProfileList_t  *g_prof[TFACONT_MAXDEVS][TFACONT_MAXPROFS];
static nxpTfaLiveDataList_t  *g_lived[TFACONT_MAXDEVS][TFACONT_MAXPROFS];
static int nxp_tfa_vstep[TFACONT_MAXDEVS];
static char errorname[] = "!ERROR!";
static char nonename[] = "NONE";
static int partial_enable = 1;

static void cont_get_devs(nxpTfaContainer_t *cont);

static int float_to_int(uint32_t x)
{
	unsigned e = (0x7F + 31) - ((*(unsigned *) &x & 0x7F800000) >> 23);
	unsigned m = 0x80000000 | (*(unsigned *) &x << 8);
	return -(int)((m >> e) & -(e < 32));
}

extern struct tfa98xx *getHandle(int dev);
extern void tfa98xx_dump_register(struct tfa98xx *tfa98xx, int level, char *msg);

void tfa_set_partial_update(int enp)
{
	partial_enable = enp;
}

/*
 * check the container file and set module global
*/
enum tfa_error tfa_load_cnt(void *cnt, int length)
{
	nxpTfaContainer_t  *cntbuf = (nxpTfaContainer_t  *)cnt;

	g_cont = NULL;

	if (length > TFA_MAX_CNT_LENGTH) {
		pr_err("incorrect length\n");
		return tfa_error_container;
	}

	if (HDR(cntbuf->id[0], cntbuf->id[1]) == 0) {
		pr_err("header is 0\n");
		return tfa_error_container;
	}

	if ((HDR(cntbuf->id[0], cntbuf->id[1])) != paramsHdr) {
		pr_err("wrong header type: 0x%02x 0x%02x\n", cntbuf->id[0], cntbuf->id[1]);
		return tfa_error_container;
	}

	if (cntbuf->size == 0) {
		pr_err("data size is 0\n");
		return tfa_error_container;
	}

	/* check CRC */
	if (tfaContCrcCheckContainer(cntbuf)) {
		pr_err("CRC error\n");
		return tfa_error_container;
	}

	/* check sub version level */
	if ((cntbuf->subversion[1] == NXPTFA_PM_SUBVERSION) &&
		 (cntbuf->subversion[0] == '0')) {
		g_cont = cntbuf;
		cont_get_devs(g_cont);
	} else {
		pr_err("container sub-version not supported: %c%c\n",
				cntbuf->subversion[0], cntbuf->subversion[1]);
		return tfa_error_container;
	}

	return tfa_error_ok;
}

void tfa_deinit(void)
{
	g_cont = NULL;
	g_devs = -1;
}

/*
 * Set the debug option
 */
void tfa_cnt_verbose(int level)
{
	tfa98xx_cnt_verbose = level;
}

/* start count from 1, 0 is invalid */
void tfaContSetCurrentVstep(int channel, int vstep_idx)
{
	if (channel < TFACONT_MAXDEVS)
		nxp_tfa_vstep[channel] = vstep_idx + 1;
	else
		pr_err("channel nr %d>%d\n", channel, TFACONT_MAXDEVS - 1);
}

/* start count from 1, 0 is invalid */
int tfaContGetCurrentVstep(int channel)
{
	if (channel < TFACONT_MAXDEVS)
		return nxp_tfa_vstep[channel] - 1;

	pr_err("channel nr %d>%d\n", channel, TFACONT_MAXDEVS - 1);
	return -EPERM;
}

nxpTfaContainer_t *tfa98xx_get_cnt(void)
{
	return g_cont;
}

/*
 * Dump the contents of the file header
 */
void tfaContShowHeader(nxpTfaHeader_t *hdr)
{
	char _id[2];

	pr_debug("File header\n");

	_id[1] = hdr->id >> 8;
	_id[0] = hdr->id & 0xff;
	pr_debug("\tid:%.2s version:%.2s subversion:%.2s\n", _id,
				hdr->version, hdr->subversion);
	pr_debug("\tsize:%d CRC:0x%08x\n", hdr->size, hdr->CRC);
	pr_debug("\tcustomer:%.8s application:%.8s type:%.8s\n",
				hdr->customer, hdr->application, hdr->type);
}

/*
 * return device list dsc from index
 */
nxpTfaDeviceList_t *tfaContGetDevList(nxpTfaContainer_t *cont, int dev_idx)
{
	uint8_t *base = (uint8_t *) cont;

	if ((dev_idx < 0) || (dev_idx >= cont->ndev))
		return NULL;

	if (cont->index[dev_idx].type != dscDevice)
		return NULL;

	base += cont->index[dev_idx].offset;
	return (nxpTfaDeviceList_t *) base;
}

/*
 * get the Nth profile for the Nth device
 */
nxpTfaProfileList_t *tfaContGetDevProfList(nxpTfaContainer_t *cont, int devIdx,
						int profIdx)
{
	nxpTfaDeviceList_t *dev;
	int idx, hit;
	uint8_t *base = (uint8_t *) cont;

	dev = tfaContGetDevList(cont, devIdx);
	if (dev) {
		for (idx = 0, hit = 0; idx < dev->length; idx++) {
			if (dev->list[idx].type == dscProfile) {
				if (profIdx == hit++)
					return (nxpTfaProfileList_t *) (dev->
									list
									[idx].
									offset +
									base);
			}
		}
	}

	return NULL;
}

/*
 * get the Nth lifedata for the Nth device
 */
nxpTfaLiveDataList_t *tfaContGetDevLiveDataList(nxpTfaContainer_t *cont, int devIdx,
							int lifeDataIdx)
{
	nxpTfaDeviceList_t *dev;
	int idx, hit;
	uint8_t *base = (uint8_t *) cont;

	dev = tfaContGetDevList(cont, devIdx);
	if (dev) {
		for (idx = 0, hit = 0; idx < dev->length; idx++) {
			if (dev->list[idx].type == dscLiveData) {
				if (lifeDataIdx == hit++)
					return (nxpTfaLiveDataList_t *)(dev->list[idx].offset + base);
			}
		}
	}

	return NULL;
}

/*
 * Get the max volume step associated with Nth profile for the Nth device
 */
int tfacont_get_max_vstep(int dev_idx, int prof_idx)
{
	nxpTfaVolumeStep2File_t *vp;
	struct nxpTfaVolumeStepMax2File *vp3;
	int vstep_count = 0;
	vp = (nxpTfaVolumeStep2File_t *) tfacont_getfiledata(dev_idx, prof_idx, volstepHdr);
	if (vp == NULL)
		return 0;
	/* check the header type to load different NrOfVStep appropriately */
	if (tfa98xx_dev_family(dev_idx) == 2) {
		/* this is actually tfa2, so re-read the buffer*/
		vp3 = (struct nxpTfaVolumeStepMax2File *)
		tfacont_getfiledata(dev_idx, prof_idx, volstepHdr);
		if (vp3)
			vstep_count = vp3->NrOfVsteps;
	} else {
		/* this is max1*/
		if (vp)
			vstep_count = vp->vsteps;
	}
	return vstep_count;
}

/**
 * Get the file contents associated with the device or profile
 * Search within the device tree, if not found, search within the profile
 * tree. There can only be one type of file within profile or device.
  */
nxpTfaFileDsc_t *tfacont_getfiledata(int dev_idx, int prof_idx, enum nxpTfaHeaderType type)
{
	nxpTfaDeviceList_t *dev;
	nxpTfaProfileList_t *prof;
	nxpTfaFileDsc_t *file;
	nxpTfaHeader_t *hdr;
	unsigned int i;

	if (g_cont == NULL) {
		pr_err("invalid pointer to container file\n");
		return NULL;
	}

	dev = tfaContGetDevList(g_cont, dev_idx);
	if (dev == NULL) {
		pr_err("invalid pointer to container file device list\n");
		return NULL;
	}

	/* process the device list until a file type is encountered */
	for (i = 0; i < dev->length; i++) {
		if (dev->list[i].type == dscFile) {
			file = (nxpTfaFileDsc_t *)(dev->list[i].offset+(uint8_t *)g_cont);
			if (file != NULL) {
				hdr = (nxpTfaHeader_t *)file->data;
				/* check for file type */
				if (hdr->id == type) {
					/* pr_debug("%s: file found of type %d in device %s\n", __FUNCTION__, type, tfaContDeviceName(devIdx));*/
					return (nxpTfaFileDsc_t *)&file->data;
				}
			}
		}
	}

	/* File not found in device tree.
	 * So, look in the profile list until the file type is encountered
	 */
	prof = tfaContGetDevProfList(g_cont, dev_idx, prof_idx);
	if (prof == NULL) {
		pr_err("invalid pointer to container file profile list\n");
		return NULL;
	}

	for (i = 0; i < prof->length; i++) {
		if (prof->list[i].type == dscFile) {
			file = (nxpTfaFileDsc_t *)(prof->list[i].offset+(uint8_t *)g_cont);
			if (file != NULL) {
				hdr = (nxpTfaHeader_t *)file->data;
				if (hdr != NULL) {
					/* check for file type */
					if (hdr->id == type) {
						/* pr_debug("%s: file found of type %d in profile %s\n", __FUNCTION__, type, tfaContProfileName(devIdx, profIdx)); */
						return (nxpTfaFileDsc_t *)&file->data;
					}
				}
			}
		}
	}

	if (tfa98xx_cnt_verbose)
		pr_debug("%s: no file found of type %d\n", __FUNCTION__, type);

	return NULL;
}

/*
 * fill globals
 */
static void cont_get_devs(nxpTfaContainer_t *cont)
{
	nxpTfaProfileList_t *prof;
	nxpTfaLiveDataList_t *liveD;
	int i, j;
	int count;

	/* get nr of devlists+1 */
	for (i = 0; i < cont->ndev; i++)
		g_dev[i] = tfaContGetDevList(cont, i); /* cache it */

	g_devs = cont->ndev;
	/* walk through devices and get the profile lists */
	for (i = 0; i < g_devs; i++) {
		j = 0;
		count = 0;
		while ((prof = tfaContGetDevProfList(cont, i, j)) != NULL) {
			count++;
			g_prof[i][j++] = prof;
		}
		g_profs[i] = count;    /* count the nr of profiles per device */
	}

	g_devs = cont->ndev;
	/* walk through devices and get the livedata lists */
	for (i = 0; i < g_devs; i++) {
		j = 0;
		count = 0;
		while ((liveD = tfaContGetDevLiveDataList(cont, i, j)) != NULL) {
			count++;
			g_lived[i][j++] = liveD;
		}
		g_liveds[i] = count;    /* count the nr of livedata per device */
	}
}

static char nostring[] = "Undefined string";

/* TODO add to API */
#define MODULE_BIQUADFILTERBANK 2
#define BIQUAD_COEFF_SIZE       6
/*
 * write a parameter file to the device
 */
static enum Tfa98xx_Error tfaContWriteVstep(int dev_idx,  nxpTfaVolumeStep2File_t *vp, int vstep)
{
	enum Tfa98xx_Error err;
	unsigned short vol;

	if (vstep < vp->vsteps) {
		/* vol = (unsigned short)(voldB / (-0.5f)); */
		vol = (unsigned short)(-2 * float_to_int(*((uint32_t *)&vp->vstep[vstep].attenuation)));
		if (vol > 255)	/* restricted to 8 bits */
			vol = 255;

		err = tfa98xx_set_volume_level(dev_idx, vol);
		if (err != Tfa98xx_Error_Ok)
			return err;

		err = tfa98xx_dsp_write_preset(dev_idx, sizeof(vp->vstep[0].preset), vp->vstep[vstep].preset);
		if (err != Tfa98xx_Error_Ok)
			return err;
		err = tfa_cont_write_filterbank(dev_idx, vp->vstep[vstep].filter);

	} else {
		pr_err("Incorrect volume given. The value vstep[%d] >= %d\n", nxp_tfa_vstep[dev_idx], vp->vsteps);
		err = Tfa98xx_Error_Bad_Parameter;
	}

	if (tfa98xx_cnt_verbose)
		pr_debug("vstep[%d][%d]\n", dev_idx, vstep);

	return err;
}

static struct nxpTfaVolumeStepMessageInfo *tfaContGetmsgInfoFromReg(struct nxpTfaVolumeStepRegisterInfo *regInfo)
{
	char *p = (char *)regInfo;
	p += sizeof(regInfo->NrOfRegisters) + (regInfo->NrOfRegisters * sizeof(uint32_t));
	return (struct nxpTfaVolumeStepMessageInfo *)p;
}

static int tfaContGetmsgLen(struct  nxpTfaVolumeStepMessageInfo *msgInfo)
{
	return (msgInfo->MessageLength.b[0] << 16) + (msgInfo->MessageLength.b[1] << 8) + msgInfo->MessageLength.b[2];
}

static struct nxpTfaVolumeStepMessageInfo *tfaContGetNextmsgInfo(struct  nxpTfaVolumeStepMessageInfo *msgInfo)
{
	char *p = (char *)msgInfo;
	int msgLen = tfaContGetmsgLen(msgInfo);
	int type = msgInfo->MessageType;

	p += sizeof(msgInfo->MessageType) + sizeof(msgInfo->MessageLength);
	if (type == 3)
		p += msgLen;
	else
		p += msgLen * 3;

	return (struct nxpTfaVolumeStepMessageInfo *)p;
}

static struct  nxpTfaVolumeStepRegisterInfo *tfaContGetNextRegFromEndInfo(struct  nxpTfaVolumeStepMessageInfo *msgInfo)
{
	char *p = (char *)msgInfo;
	p += sizeof(msgInfo->NrOfMessages);
	return (struct nxpTfaVolumeStepRegisterInfo *)p;

}

static struct nxpTfaVolumeStepRegisterInfo *tfaContGetRegForVstep(nxpTfaVolumeStepMax2File_t *vp, int idx)
{
	int i, j, nrMessage;

	struct nxpTfaVolumeStepRegisterInfo *regInfo
		= (struct nxpTfaVolumeStepRegisterInfo *)vp->vstepsBin;
	struct nxpTfaVolumeStepMessageInfo *msgInfo = NULL;

	for (i = 0; i < idx; i++) {
		msgInfo = tfaContGetmsgInfoFromReg(regInfo);
		nrMessage = msgInfo->NrOfMessages;

		for (j = 0; j < nrMessage; j++) {
			msgInfo = tfaContGetNextmsgInfo(msgInfo);
		}
		regInfo = tfaContGetNextRegFromEndInfo(msgInfo);
	}

	return regInfo;

}

struct tfa_partial_msg_block {
	uint8_t offset;
	uint16_t change;
	uint8_t update[16][3];
} __packed;

static enum Tfa98xx_Error tfaContWriteVstepMax2_One(int dev_idx, struct nxpTfaVolumeStepMessageInfo *new_msg,
						    struct nxpTfaVolumeStepMessageInfo *old_msg, int enable_partial_update)
{
	enum Tfa98xx_Error err = Tfa98xx_Error_Ok;
	int len = (tfaContGetmsgLen(new_msg) - 1) * 3;
	char *buf = (char *)new_msg->ParameterData;
	uint8_t *partial = NULL;
	uint8_t cmdid[3];

	if (enable_partial_update) {
		if (new_msg->MessageType != old_msg->MessageType) {
			pr_debug("Message type differ - Disable Partial Update\n");
			enable_partial_update = 0;
		} else if (tfaContGetmsgLen(new_msg) != tfaContGetmsgLen(old_msg)) {
			pr_debug("Message Length differ - Disable Partial Update\n");
			enable_partial_update = 0;
		}
	}

	if ((enable_partial_update) && (new_msg->MessageType == 1)) {
		/*
		 * No patial updates yet for message type 1 (Coefficients),
		 * for now send all or nothing
		 */
		enable_partial_update = 0;
		if (memcmp(old_msg->ParameterData, new_msg->ParameterData, len) == 0) {
			pr_debug("No change in message type 1 - discarding %d bytes\n", len);
			len = 0;
		} else {
			pr_debug("Disable Partial Update for message type 1\n");
		}
	}

	/* Change Message Len to the actual buffer len */
	memcpy(cmdid, new_msg->CmdId, sizeof(cmdid));

	/* ONLY FOR 88!
	 * The algoparams and mbdrc msg id will be changed to the reset type when SBSL=0
	 * if SBSL=1 the msg will remain unchanged. It's up to the tuning engineer to choose the 'without_reset'
	 * types inside the vstep. In other words: the reset msg is applied during SBSL==0 else it remains unchanged.
	 */
	if ((tfa98xx_dev_revision(dev_idx) & 0xff) == 0x88) {
		if (TFA_GET_BF(dev_idx, SBSL) == 0) {
			if ((new_msg->MessageType == 0) && (cmdid[2] != SB_PARAM_SET_ALGO_PARAMS)) {
				cmdid[2] = SB_PARAM_SET_ALGO_PARAMS;
				pr_debug("P-ID for SetAlgoParams modified!\n");
			} else if ((new_msg->MessageType == 2) && (cmdid[2] != SB_PARAM_SET_MBDRC)) {
				cmdid[2] = SB_PARAM_SET_MBDRC;
				pr_debug("P-ID for SetMBDrc modified!\n");
			}
		}
	}

	/*
	 * +sizeof(struct tfa_partial_msg_block) will allow to fit one
	 * additonnal partial block If the partial update goes over the len of
	 * a regular message ,we can safely write our block and check afterward
	 * that we are over the size of a usual update
	 */
	if (enable_partial_update) {
		partial = kmalloc((sizeof(uint8_t) * len) + sizeof(struct tfa_partial_msg_block),
				  GFP_KERNEL);
		if (!partial)
			pr_debug("Partial update memory error - Disabling\n");
	}

	if (partial) {
		uint8_t offset = 0, i = 0;
		uint16_t *change;
		uint8_t *n = new_msg->ParameterData;
		uint8_t *o = old_msg->ParameterData;
		uint8_t *p = partial;
		uint8_t *trim = partial;

		/* set dspFiltersReset */
		*p++ = 0x02;
		*p++ = 0x00;
		*p++ = 0x00;

		while ((o < (old_msg->ParameterData + len)) && (p < (partial + len - 3))) {
			if ((offset == 0xff) ||
			    (memcmp(n, o, 3 * sizeof(uint8_t)))) {
				*p++ = offset;
				change = (uint16_t *)p;
				*change = 0;
				p += 2;

				for (i = 0;
				    (i < 16) && (o < (old_msg->ParameterData + len));
				    i++, n += 3, o += 3) {
					if (memcmp(n, o, 3 * sizeof(uint8_t))) {
						*change |= BIT(i);
						memcpy(p, n, 3);
						p += 3;
						trim = p;
					}
				}

				offset = 0;
				*change = cpu_to_be16(*change);

			} else {
				n += 3;
				o += 3;
				offset++;
			}
		}

		if (trim == partial) {
			pr_debug("No Change in message - discarding %d bytes\n", len);
			len = 0;
		} else if (trim < (partial + len - 3)) {
			pr_debug("Using partial update: %d -> %d bytes\n", len, (int)(trim-partial+3));

			/* Add the termination marker */
			memset(trim, 0x00, 3);
			trim += 3;

			/* Signal This will be a partial update */
			cmdid[2] |= BIT(6);
			buf = (char *) partial;
			len = (int)(trim - partial);
		} else {
			pr_debug("Partial too big - use regular update\n");
		}
	}

	if (len) {
		uint8_t *buffer;

		pr_debug("Command-ID used: 0x%02x%02x%02x \n",
			 cmdid[0], cmdid[1], cmdid[2]);

		buffer = kmalloc(3 + len, GFP_KERNEL);
		if (buffer == NULL) {
			err = Tfa98xx_Error_Fail;
		} else {
			memcpy(&buffer[0], cmdid, 3);
			memcpy(&buffer[3], buf, len);
			err = tfa_dsp_msg(dev_idx, 3 + len, (char *)buffer);
			kfree(buffer);
		}
	}

	if (partial)
		kfree(partial);

	return err;
}

static enum Tfa98xx_Error tfaContWriteVstepMax2(int dev_idx, nxpTfaVolumeStepMax2File_t *vp, int vstep_idx, int vstep_msg_idx)
{
	enum Tfa98xx_Error err = Tfa98xx_Error_Ok;
	static struct nxpTfaVolumeStepRegisterInfo *p_regInfo;
	struct nxpTfaVolumeStepRegisterInfo *regInfo = NULL;
	struct nxpTfaVolumeStepMessageInfo *msgInfo = NULL, *p_msgInfo = NULL;
	nxpTfaBitfield_t bitF;
	int i, nrMessages, enp = partial_enable;

	if (vstep_idx >= vp->NrOfVsteps) {
		pr_debug("Volumestep %d is not available \n", vstep_idx);
		return Tfa98xx_Error_Bad_Parameter;
	}

	if (p_regInfo == NULL) {
		pr_debug("Inital vstep write\n");
		enp = 0;
	}

	regInfo = tfaContGetRegForVstep(vp, vstep_idx);

	msgInfo = tfaContGetmsgInfoFromReg(regInfo);
	nrMessages = msgInfo->NrOfMessages;

	if (enp) {
		p_msgInfo = tfaContGetmsgInfoFromReg(p_regInfo);
		if (nrMessages != p_msgInfo->NrOfMessages) {
			pr_debug("Message different - Disable partial update\n");
			enp = 0;
		}
	}

	for (i = 0; i < nrMessages; i++) {
		/* Messagetype(3) is Smartstudio Info! Dont send this! */
		if (msgInfo->MessageType == 3) {
			pr_debug("Skipping Message Type 3\n");
			/* MessageLength is in bytes */
			msgInfo = tfaContGetNextmsgInfo(msgInfo);
			if (enp)
				p_msgInfo = tfaContGetNextmsgInfo(p_msgInfo);
			continue;
		}

		/* If no vstepMsgIndex is passed on, all message needs to be send */
		if ((vstep_msg_idx >= TFA_MAX_VSTEP_MSG_MARKER) || (vstep_msg_idx == i)) {
			err = tfaContWriteVstepMax2_One(dev_idx, msgInfo, p_msgInfo, enp);
			if (err != Tfa98xx_Error_Ok) {
				/*
				 * Force a full update for the next write
				 * As the current status of the DSP is unknown
				 */
				p_regInfo = NULL;
				return err;
			}
		}

		msgInfo = tfaContGetNextmsgInfo(msgInfo);
		if (enp)
			p_msgInfo = tfaContGetNextmsgInfo(p_msgInfo);
	}

	p_regInfo = regInfo;

	for (i = 0; i < regInfo->NrOfRegisters * 2; i++) {
		/* Byte swap the datasheetname */
		bitF.field = (uint16_t)(regInfo->registerInfo[i]>>8) | (regInfo->registerInfo[i]<<8);
		i++;
		bitF.value = (uint16_t)regInfo->registerInfo[i]>>8;
		err = tfaRunWriteBitfield(dev_idx, bitF);
		if (err != Tfa98xx_Error_Ok)
			return err;
	}

	/* Save the current vstep */
	tfa_set_swvstep(dev_idx, (unsigned short)vstep_idx);

	return err;
}

/*
 * Write DRC message to the dsp
 * If needed modify the cmd-id
 */

enum Tfa98xx_Error tfaContWriteDrcFile(int dev_idx, int size, uint8_t data[])
{
	enum Tfa98xx_Error err = Tfa98xx_Error_Ok;
	uint8_t cmdid_changed[3], modified = 0;

	if (TFA_GET_BF(dev_idx, SBSL) == 0) {
		/* Only do this when not set already */
		if (data[2] != SB_PARAM_SET_MBDRC) {
			cmdid_changed[0] = data[0];
			cmdid_changed[1] = data[1];
			cmdid_changed[2] = SB_PARAM_SET_MBDRC;
			modified = 1;

			if (tfa98xx_cnt_verbose) {
				pr_debug("P-ID for SetMBDrc modified!: ");
				pr_debug("Command-ID used: 0x%02x%02x%02x\n",
				cmdid_changed[0], cmdid_changed[1], cmdid_changed[2]);
			}
		}
	}

	if (modified == 1) {
		/* Send payload to dsp (Remove 3 from the length for cmdid) */
		err = tfa_dsp_msg_id(dev_idx, size-3, (const char *)data, cmdid_changed);
	} else {
		/* Send cmdId + payload to dsp */
		err = tfa_dsp_msg(dev_idx, size, (const char *)data);
	}

	return err;
}


/*
 * write a parameter file to the device
 * The VstepIndex and VstepMsgIndex are only used to write a specific msg from the vstep file.
 */
enum Tfa98xx_Error tfaContWriteFile(int dev_idx,  nxpTfaFileDsc_t *file, int vstep_idx, int vstep_msg_idx)
{
	enum Tfa98xx_Error err = Tfa98xx_Error_Ok;
	nxpTfaHeader_t *hdr = (nxpTfaHeader_t *)file->data;
	nxpTfaHeaderType_t type;
	int size;

	if (tfa98xx_cnt_verbose)
		tfaContShowHeader(hdr);

	type = (nxpTfaHeaderType_t) hdr->id;

	switch (type) {
	case msgHdr: /* generic DSP message */
		size = hdr->size - sizeof(nxpTfaMsgFile_t);
		err = tfa_dsp_msg(dev_idx, size, (const char *)((nxpTfaMsgFile_t *)hdr)->data);
		break;
	case volstepHdr:
		if (tfa98xx_dev_family(dev_idx) == 2) {
			err = tfaContWriteVstepMax2(dev_idx, (nxpTfaVolumeStepMax2File_t *)hdr, vstep_idx, vstep_msg_idx);
		} else {
			err = tfaContWriteVstep(dev_idx, (nxpTfaVolumeStep2File_t *)hdr, vstep_idx);
		}

		/* If writing the vstep was succesfull, set new current vstep */
		if (err == Tfa98xx_Error_Ok)
			tfaContSetCurrentVstep(dev_idx, vstep_idx);

		break;
	case speakerHdr:
		if (tfa98xx_dev_family(dev_idx) == 2) {
			/* Remove header and xml_id */
			size = hdr->size - sizeof(struct nxpTfaSpkHeader) - sizeof(struct nxpTfaFWVer);

			err = tfa_dsp_msg(dev_idx, size,
					(const char *)(((nxpTfaSpeakerFile_t *)hdr)->data + (sizeof(struct nxpTfaFWVer))));
		} else {
			size = hdr->size - sizeof(nxpTfaSpeakerFile_t);
			err = tfa98xx_dsp_write_speaker_parameters(dev_idx, size,
					(const unsigned char *)((nxpTfaSpeakerFile_t *)hdr)->data);
		}
		break;
	case presetHdr:
		size = hdr->size - sizeof(nxpTfaPreset_t);
		err = tfa98xx_dsp_write_preset(dev_idx, size, (const unsigned char *)((nxpTfaPreset_t *)hdr)->data);
		break;
	case equalizerHdr:
		err = tfa_cont_write_filterbank(dev_idx, ((nxpTfaEqualizerFile_t *)hdr)->filter);
		break;
	case patchHdr:
		size = hdr->size - sizeof(nxpTfaPatch_t); /* size is total length */
		err = tfa_dsp_patch(dev_idx,  size, (const unsigned char *) ((nxpTfaPatch_t *)hdr)->data);
		break;
	case configHdr:
		size = hdr->size - sizeof(nxpTfaConfig_t);
		err = tfa98xx_dsp_write_config(dev_idx, size, (const unsigned char *)((nxpTfaConfig_t *)hdr)->data);
		break;
	case drcHdr:
		if (hdr->version[0] == NXPTFA_DR3_VERSION) {
			/* Size is total size - hdrsize(36) - xmlversion(3) */
			size = hdr->size - sizeof(nxpTfaDrc2_t);
			err = tfaContWriteDrcFile(dev_idx, size, ((nxpTfaDrc2_t *)hdr)->data);
		} else {
			/*
			 * The DRC file is split as:
			 * 36 bytes for generic header (customer, application, and type)
			 * 127x3 (381) bytes first block contains the device and sample rate
			 * 				independent settings
			 * 127x3 (381) bytes block the device and sample rate specific values.
			 * The second block can always be recalculated from the first block,
			 * if vlsCal and the sample rate are known.
			 */
			/* size = hdr->size - sizeof(nxpTfaDrc_t); */
			size = 381; /* fixed size for first block */

			/* +381 is done to only send the second part of the drc block */
			err = tfa98xx_dsp_write_drc(dev_idx, size, ((const unsigned char *)((nxpTfaDrc_t *)hdr)->data+381));
		}
		break;
	case infoHdr:
		/* Ignore */
		break;
	default:
		pr_err("Header is of unknown type: 0x%x\n", type);
		return Tfa98xx_Error_Bad_Parameter;
	}

	return err;
}

/**
 * get the 1st of this dsc type this devicelist
 */
nxpTfaDescPtr_t *tfa_cnt_get_dsc(nxpTfaContainer_t *cnt, nxpTfaDescriptorType_t type, int dev_idx)
{
	nxpTfaDeviceList_t *dev = tfaContDevice (dev_idx);
	nxpTfaDescPtr_t *this;
	int i;

	if (!dev) {
		return NULL;
	}
	/* process the list until a the type is encountered */
	for (i = 0; i < dev->length; i++) {
		if (dev->list[i].type == (uint32_t)type) {
			this = (nxpTfaDescPtr_t *)(dev->list[i].offset+(uint8_t *)cnt);
			return this;
		}

	}

	return NULL;
}

/**
 * get the device type from the patch in this devicelist
 *  - find the patch file for this devidx
 *  - return the devid from the patch or 0 if not found
 */
int tfa_cnt_get_devid(nxpTfaContainer_t *cnt, int dev_idx)
{
	nxpTfaPatch_t *patchfile;
	nxpTfaDescPtr_t *patchdsc;
	uint8_t *patchheader;
	unsigned short devid, checkaddress;
	int checkvalue;

	patchdsc = tfa_cnt_get_dsc(cnt, dscPatch, dev_idx);
	patchdsc += 2; /* first the filename dsc and filesize, so skip them */
	patchfile = (nxpTfaPatch_t *)patchdsc;

	if (patchfile == NULL)
		return 0;

	patchheader = patchfile->data;

	checkaddress = (patchheader[1] << 8) + patchheader[2];
	checkvalue =
		(patchheader[3] << 16) + (patchheader[4] << 8) + patchheader[5];

	devid = patchheader[0];

	if (checkaddress == 0xFFFF && checkvalue != 0xFFFFFF && checkvalue != 0)
		devid = patchheader[5]<<8 | patchheader[0]; /* full revid */

	return devid;
}

/*
 * get the slave for the device if it exists
 */
enum Tfa98xx_Error tfaContGetSlave(int dev_idx, uint8_t *slave_addr)
{
	nxpTfaDeviceList_t *dev = tfaContDevice (dev_idx);

	if (dev == 0)
		return Tfa98xx_Error_Bad_Parameter;

	*slave_addr = dev->dev;
	return Tfa98xx_Error_Ok;
}

/*
 * write a bit field
 */
enum Tfa98xx_Error tfaRunWriteBitfield(Tfa98xx_handle_t dev_idx,  nxpTfaBitfield_t bf)
{
	enum Tfa98xx_Error error;
	uint16_t value;
	union {
		uint16_t field;
		nxpTfaBfEnum_t Enum;
	} bfUni;

	value = bf.value;
	bfUni.field = bf.field;
#ifdef TFA_DEBUG
	if (tfa98xx_cnt_verbose)
		pr_debug("bitfield: %s=%d (0x%x[%d..%d]=0x%x)\n", tfaContBfName(bfUni.field, tfa98xx_dev_revision(dev_idx)), value,
			bfUni.Enum.address, bfUni.Enum.pos, bfUni.Enum.pos+bfUni.Enum.len, value);
#endif
	error = tfa_set_bf(dev_idx, bfUni.field, value);

	return error;
}

/*
 * read a bit field
 */
enum Tfa98xx_Error tfaRunReadBitfield(Tfa98xx_handle_t dev_idx,  nxpTfaBitfield_t *bf)
{
	enum Tfa98xx_Error error;
	union {
		uint16_t field;
		nxpTfaBfEnum_t Enum;
	} bfUni;
	uint16_t regvalue, msk;

	bfUni.field = bf->field;

	error = tfa98xx_read_register16(dev_idx, (unsigned char)(bfUni.Enum.address), &regvalue);
	if (error)
		return error;

	msk = ((1 << (bfUni.Enum.len+1)) - 1) << bfUni.Enum.pos;

	regvalue &= msk;
	bf->value = regvalue >> bfUni.Enum.pos;

	return error;
}

/*
 dsp mem direct write
 */
enum Tfa98xx_Error tfaRunWriteDspMem(Tfa98xx_handle_t dev, nxpTfaDspMem_t *cfmem)
{
	enum Tfa98xx_Error error = Tfa98xx_Error_Ok;
	int i;

	for (i = 0; i < cfmem->size; i++) {
		if (tfa98xx_cnt_verbose)
			pr_debug("dsp mem (%d): 0x%02x=0x%04x\n", cfmem->type, (cfmem->address+i), cfmem->words[i]);

		error = tfa98xx_dsp_write_mem_word(dev, (cfmem->address+i), cfmem->words[i], cfmem->type);
		if (error)
			return error;
	}

	return error;
}

/*
 * write filter payload to DSP
 *  note that the data is in an aligned union for all filter variants
 *  the aa data is used but it's the same for all of them
 */
enum Tfa98xx_Error tfaRunWriteFilter(Tfa98xx_handle_t dev, nxpTfaContBiquad_t *bq)
{
	enum Tfa98xx_Error error = Tfa98xx_Error_Ok;
	enum Tfa98xx_DMEM dmem;
	uint16_t address;
	uint8_t data[3 * 3 + sizeof(bq->aa.bytes)];
	int i, channel = 0, runs = 1;
	int8_t saved_index = bq->aa.index; /* This is used to set back the index */

	/* Channel=1 is primary, Channel=2 is secondary*/
	if (bq->aa.index > 100) {
		bq->aa.index -= 100;
		channel = 2;
	} else if (bq->aa.index > 50) {
		bq->aa.index -= 50;
		channel = 1;
	} else if (tfa98xx_dev_family(dev) == 2) {
		runs = 2;
	}

	if (tfa98xx_cnt_verbose) {
		if (channel == 2)
			pr_debug("filter[%d,S]", bq->aa.index);
		else if (channel == 1)
			pr_debug("filter[%d,P]", bq->aa.index);
		else
			pr_debug("filter[%d]", bq->aa.index);
	}

	for (i = 0; i < runs; i++) {
		if (runs == 2)
			channel++;

		/* get the target address for the filter on this device */
		dmem = tfa98xx_filter_mem(dev, bq->aa.index, &address, channel);
		if (dmem < 0)
			return Tfa98xx_Error_Bad_Parameter;

		/* send a DSP memory message that targets the devices specific memory for the filter
		 * msg params: which_mem, start_offset, num_words
		 */
		memset(data, 0, 3 * 3);
		data[2] = dmem; /* output[0] = which_mem */
		data[4] = address >> 8; /* output[1] = start_offset */
		data[5] = address & 0xff;
		data[8] = sizeof(bq->aa.bytes) / 3; /*output[2] = num_words */
		memcpy(&data[9], bq->aa.bytes, sizeof(bq->aa.bytes)); /* payload */

		if (tfa98xx_dev_family(dev) == 2) {
			error = tfa_dsp_cmd_id_write(dev, MODULE_FRAMEWORK, FW_PAR_ID_SET_MEMORY, sizeof(data), data);
		} else {
			error = tfa_dsp_cmd_id_write(dev, MODULE_FRAMEWORK, 4 /* param */, sizeof(data), data);
		}
	}

#ifdef TFA_DEBUG
	if (tfa98xx_cnt_verbose) {
		if (bq->aa.index == 13) {
			pr_debug("=%d,%.0f,%.2f\n",
				bq->in.type, bq->in.cutOffFreq, bq->in.leakage);
		} else if (bq->aa.index >= 10 && bq->aa.index <= 12) {
			pr_debug("=%d,%.0f,%.1f,%.1f\n", bq->aa.type,
				bq->aa.cutOffFreq, bq->aa.rippleDb, bq->aa.rolloff);
		} else {
			pr_debug("= unsupported filter index \n");
		}
	}
#endif

	/* Because we can load the same filters multiple times
	 * For example: When we switch profile we re-write in operating mode.
	 * We then need to remember the index (primary, secondary or both)
	 */
	bq->aa.index = saved_index;

	return error;
}

/*
 * write the register based on the input address, value and mask
 *  only the part that is masked will be updated
 */
enum Tfa98xx_Error tfaRunWriteRegister(Tfa98xx_handle_t handle, nxpTfaRegpatch_t *reg)
{
	enum Tfa98xx_Error error;
	uint16_t value, newvalue;

	if (tfa98xx_cnt_verbose)
		pr_debug("register: 0x%02x=0x%04x (msk=0x%04x)\n", reg->address, reg->value, reg->mask);

	error = tfa98xx_read_register16(handle, reg->address, &value);
	if (error)
		return error;

	value &= ~reg->mask;
	newvalue = reg->value & reg->mask;

	value |= newvalue;
	error = tfa98xx_write_register16(handle, reg->address, value);

	return error;

}

/*
 * return the bitfield
 */
nxpTfaBitfield_t tfaContDsc2Bf(nxpTfaDescPtr_t dsc)
{
	uint32_t *ptr = (uint32_t *)(&dsc);
	union {
		nxpTfaBitfield_t bf;
		uint32_t num;
	} num_bf;

	num_bf.num = *ptr; /* & TFA_BITFIELDDSCMSK; */

	return num_bf.bf;
}

/* write  reg  and bitfield items in the devicelist to the target */
enum Tfa98xx_Error tfaContWriteRegsDev(int dev_idx)
{
	nxpTfaDeviceList_t *dev = tfaContDevice (dev_idx);
	nxpTfaBitfield_t *bitF;
	int i;
	enum Tfa98xx_Error err = Tfa98xx_Error_Ok;

	if (!dev) {
		return Tfa98xx_Error_Bad_Parameter;
	}

	/* process the list until a patch, file of profile is encountered */
	for (i = 0; i < dev->length; i++) {
		if (dev->list[i].type == dscPatch ||
			dev->list[i].type == dscFile  ||
			dev->list[i].type == dscProfile)
			break;

		if (dev->list[i].type == dscBitfield) {
			bitF = (nxpTfaBitfield_t *)(dev->list[i].offset+(uint8_t *)g_cont);
			err = tfaRunWriteBitfield(dev_idx, *bitF);
		}
		if (dev->list[i].type == dscRegister) {
			err = tfaRunWriteRegister(dev_idx, (nxpTfaRegpatch_t *)
						(dev->list[i].offset+(char *)g_cont));
		}

		if (err)
			break;
	}
	return err;
}

/* write reg and bitfield items in the profilelist the target */
enum Tfa98xx_Error tfaContWriteRegsProf(int dev_idx, int prof_idx)
{
	nxpTfaProfileList_t *prof = tfaContProfile(dev_idx, prof_idx);
	nxpTfaBitfield_t *bitf;
	unsigned int i;
	enum Tfa98xx_Error err = Tfa98xx_Error_Ok;

	if (!prof)
		return Tfa98xx_Error_Bad_Parameter;

	if (tfa98xx_cnt_verbose)
		pr_debug("----- profile: %s (%d) -----\n", tfaContGetString(&prof->name), prof_idx);

	/* process the list until the end of the profile or the default section */
	for (i = 0; i < prof->length; i++) {
		/* We only want to write the values before the default section when we switch profile */
		if (prof->list[i].type == dscDefault)
			break;

		if (prof->list[i].type == dscBitfield) {
			bitf = (nxpTfaBitfield_t *)(prof->list[i].offset+(uint8_t *)g_cont);
			err = tfaRunWriteBitfield(dev_idx, *bitf);
		}
		if (prof->list[i].type == dscRegister)
			err = tfaRunWriteRegister(dev_idx, (nxpTfaRegpatch_t *)(!prof->list[i].offset+g_cont));
		if (err)
			break;
	}
	return err;
}

/* write  patchfile in the devicelist to the target */
enum Tfa98xx_Error tfaContWritePatch(int dev_idx)
{
	enum Tfa98xx_Error err = Tfa98xx_Error_Ok;
	nxpTfaDeviceList_t *dev = tfaContDevice(dev_idx);
	nxpTfaFileDsc_t *file;
	nxpTfaPatch_t *patchfile;
	int size, i;

	if (!dev) {
		return Tfa98xx_Error_Bad_Parameter;
	}
	/* process the list until a patch  is encountered */
	for (i = 0; i < dev->length; i++) {
		if (dev->list[i].type == dscPatch) {
			file = (nxpTfaFileDsc_t *)(dev->list[i].offset+(uint8_t *)g_cont);
			patchfile = (nxpTfaPatch_t *)&file->data;
			if (tfa98xx_cnt_verbose)
				tfaContShowHeader(&patchfile->hdr);
			size = patchfile->hdr.size - sizeof(nxpTfaPatch_t); /* size is total length */
			err = tfa_dsp_patch(dev_idx,  size, (const unsigned char *) patchfile->data);
			if (err)
				return err;
		}

	}

	return Tfa98xx_Error_Ok;
}

/* write all  param files in the devicelist to the target */
enum Tfa98xx_Error tfaContWriteFiles(int dev_idx)
{
	nxpTfaDeviceList_t *dev = tfaContDevice(dev_idx);
	nxpTfaFileDsc_t *file;
	nxpTfaCmd_t *cmd;
	enum Tfa98xx_Error err = Tfa98xx_Error_Ok;
	char buffer[(MEMTRACK_MAX_WORDS * 3) + 3] = {0}; /* every word requires 3 bytes, and 3 is the msg */
	int i, size = 0;

	if (!dev) {
		return Tfa98xx_Error_Bad_Parameter;
	}

	/* process the list and write all files  */
	for (i = 0; i < dev->length; i++) {
		if (dev->list[i].type == dscFile) {
			file = (nxpTfaFileDsc_t *)(dev->list[i].offset+(uint8_t *)g_cont);
			if (tfaContWriteFile(dev_idx,  file, 0, TFA_MAX_VSTEP_MSG_MARKER))
				return Tfa98xx_Error_Bad_Parameter;
		}

		if (dev->list[i].type == dscSetInputSelect ||
			dev->list[i].type == dscSetOutputSelect ||
			dev->list[i].type == dscSetProgramConfig ||
			dev->list[i].type == dscSetLagW ||
			dev->list[i].type == dscSetGains ||
			dev->list[i].type == dscSetvBatFactors ||
			dev->list[i].type == dscSetSensesCal ||
			dev->list[i].type == dscSetSensesDelay ||
			dev->list[i].type == dscSetMBDrc) {
			create_dsp_buffer_msg((nxpTfaMsg_t *)
						(dev->list[i].offset+(char *)g_cont), buffer, &size);
			err = tfa_dsp_msg(dev_idx, size, buffer);
			if (tfa98xx_cnt_verbose) {
				pr_debug("command: %s=0x%02x%02x%02x\n",
							tfaContGetCommandString(dev->list[i].type),
							(unsigned char)buffer[0],
							(unsigned char)buffer[1],
							(unsigned char)buffer[2]);
			}
		}

		if (dev->list[i].type == dscCmd) {
			size = *(uint16_t *)(dev->list[i].offset+(char *)g_cont);
			err = tfa_dsp_msg(dev_idx, size,  dev->list[i].offset+2+(char *)g_cont);
			if (tfa98xx_cnt_verbose) {
				cmd = (nxpTfaCmd_t *)(dev->list[i].offset+(uint8_t *)g_cont);
				pr_debug("Writing cmd=0x%02x%02x%02x \n", cmd->value[0], cmd->value[1], cmd->value[2]);
			}
		}
		if (err != Tfa98xx_Error_Ok)
			break;

		if (dev->list[i].type == dscCfMem)
			err = tfaRunWriteDspMem(dev_idx, (nxpTfaDspMem_t *)(dev->list[i].offset+(uint8_t *)g_cont));

		if (err != Tfa98xx_Error_Ok)
			break;
	}

	return err;
}

/*
 *  write all  param files in the profilelist to the target
 *   this is used during startup when maybe ACS is set
 */
enum Tfa98xx_Error tfaContWriteFilesProf(int dev_idx, int prof_idx, int vstep_idx)
{
	enum Tfa98xx_Error err = Tfa98xx_Error_Ok;
	nxpTfaProfileList_t *prof = tfaContProfile(dev_idx, prof_idx);
	unsigned int i;
	nxpTfaFileDsc_t *file;
	nxpTfaPatch_t *patchfile;
	int size;

	if (!prof)
		return Tfa98xx_Error_Bad_Parameter;

	/* process the list and write all files  */
	for (i = 0; i < prof->length; i++) {
		switch (prof->list[i].type) {
		case dscFile:
			file = (nxpTfaFileDsc_t *)(prof->list[i].offset+(uint8_t *)g_cont);
			err = tfaContWriteFile(dev_idx,  file, vstep_idx, TFA_MAX_VSTEP_MSG_MARKER);
			break;
		case dscPatch:
			file = (nxpTfaFileDsc_t *)(prof->list[i].offset+(uint8_t *)g_cont);
			patchfile = (nxpTfaPatch_t *)&file->data;
			if (tfa98xx_cnt_verbose)
				tfaContShowHeader(&patchfile->hdr);
			size = patchfile->hdr.size - sizeof(nxpTfaPatch_t); /* size is total length */
			err = tfa_dsp_patch(dev_idx, size, (const unsigned char *)patchfile->data);
			break;
		case dscCfMem:
			err = tfaRunWriteDspMem(dev_idx, (nxpTfaDspMem_t *)(prof->list[i].offset + (uint8_t *)g_cont));
			break;
		default:
			/* ignore any other type */
			break;
		}
	}

	return err;
}

enum Tfa98xx_Error tfaContWriteItem(int dev_idx, nxpTfaDescPtr_t *dsc)
{
	enum Tfa98xx_Error err = Tfa98xx_Error_Ok;
	nxpTfaRegpatch_t *reg;
	nxpTfaMode_t *cas;
	nxpTfaBitfield_t *bitf;
	/*pr_info("[nxp]  tfaContWriteItem  dsc->type=%d    offset=0x%04x\n", dsc->type, dsc->offset);*/

	switch (dsc->type) {
	case dscDefault:
	case dscDevice:
	case dscProfile:
		break;
	case dscRegister:   /* register patch */
		reg = (nxpTfaRegpatch_t *)(dsc->offset+(uint8_t *)g_cont);
		err = tfaRunWriteRegister(dev_idx, reg);
		/* pr_debug("$0x%2x=0x%02x,0x%02x\n", reg->address, reg->mask, reg->value); */
		break;
	case dscString: /* ascii: zero terminated string */
		pr_debug(";string: %s\n", tfaContGetString(dsc));
		break;
	case dscFile:       /* filename + file contents */
	case dscPatch:
		break;
	case dscMode:
		cas = (nxpTfaMode_t *)(dsc->offset+(uint8_t *)g_cont);
		if (cas->value == Tfa98xx_Mode_RCV)
			tfa98xx_select_mode(dev_idx, Tfa98xx_Mode_RCV);
		else
			tfa98xx_select_mode(dev_idx, Tfa98xx_Mode_Normal);
		break;
	case dscCfMem:
		err = tfaRunWriteDspMem(dev_idx, (nxpTfaDspMem_t *)(dsc->offset+(uint8_t *)g_cont));
		break;
	case dscBitfield:
		bitf = (nxpTfaBitfield_t *)(dsc->offset+(uint8_t *)g_cont);
		err = tfaRunWriteBitfield(dev_idx, *bitf);
		/*pr_info("[nxp] tfaRunWriteBitfield    field=%d, value=%d\n", bitf->field, bitf->value);*/
		break;
	case dscFilter:
		err = tfaRunWriteFilter(dev_idx, (nxpTfaContBiquad_t *)(dsc->offset+(uint8_t *)g_cont));
		break;
	}
	/*tfa98xx_dump_register(getHandle(dev_idx), 2, " in tfaContWriteItem()");*/

	return err;
}

static unsigned int tfa98xx_sr_from_field(unsigned int field)
{
	switch (field) {
	case 0:
		return 8000;
	case 1:
		return 11025;
	case 2:
		return 12000;
	case 3:
		return 16000;
	case 4:
		return 22050;
	case 5:
		return 24000;
	case 6:
		return 32000;
	case 7:
		return 44100;
	case 8:
		return 48000;
	default:
		return 0;
	}
}

enum Tfa98xx_Error tfa_write_filters(int dev_idx, int prof_idx)
{
	enum Tfa98xx_Error err = Tfa98xx_Error_Ok;
	nxpTfaProfileList_t *prof = tfaContProfile(dev_idx, prof_idx);
	unsigned int i;
	int status;

	if (!prof)
		return Tfa98xx_Error_Bad_Parameter;

	if (tfa98xx_cnt_verbose) {
		pr_debug("----- profile: %s (%d) -----\n", tfaContGetString(&prof->name), prof_idx);
		pr_debug("Waiting for CLKS... \n");
	}

	for (i = 10; i > 0; i--) {
		err = tfa98xx_dsp_system_stable(dev_idx, &status);
		if (status)
			break;
		else
			msleep_interruptible(10);
	}

	if (i == 0) {
		if (tfa98xx_cnt_verbose)
			pr_err("Unable to write filters, CLKS=0 \n");
		return Tfa98xx_Error_StateTimedOut;
	}

	/* process the list until the end of the profile or the default section */
	for (i = 0; i < prof->length; i++) {
		if (prof->list[i].type == dscFilter) {
			if (tfaContWriteItem(dev_idx, &prof->list[i]) != Tfa98xx_Error_Ok)
				return Tfa98xx_Error_Bad_Parameter;
		}
	}

	return err;
}

unsigned int tfa98xx_get_profile_sr(int dev_idx, unsigned int prof_idx)
{
	nxpTfaBitfield_t *bitf;
	unsigned int i;
	nxpTfaDeviceList_t *dev;
	nxpTfaProfileList_t *prof;
	int fs_profile = -1;

	dev = tfaContDevice (dev_idx);
	if (!dev)
		return 0;

	prof = tfaContProfile(dev_idx, prof_idx);
	if (!prof)
		return 0;

	/* Check profile fields first */
	for (i = 0; i < prof->length; i++) {
		if (prof->list[i].type == dscDefault)
			break;

		/* check for profile settingd (AUDFS) */
		if (prof->list[i].type == dscBitfield) {
			bitf = (nxpTfaBitfield_t *)(prof->list[i].offset+(uint8_t *)g_cont);
			if (bitf->field == TFA_FAM(dev_idx, AUDFS)) {
				fs_profile = bitf->value;
				break;
			}
		}
	}

	pr_debug("%s - profile fs: 0x%x = %dHz (%d - %d)\n", __FUNCTION__, fs_profile,
					tfa98xx_sr_from_field(fs_profile),
					dev_idx, prof_idx);
	if (fs_profile != -1)
		return tfa98xx_sr_from_field(fs_profile);

	/* Check for container default setting */
	/* process the list until a patch, file of profile is encountered */
	for (i = 0; i < dev->length; i++) {
		if (dev->list[i].type == dscPatch ||
			dev->list[i].type == dscFile  ||
			dev->list[i].type == dscProfile)
			break;

		if (dev->list[i].type == dscBitfield) {
			bitf = (nxpTfaBitfield_t *)(dev->list[i].offset + (uint8_t *)g_cont);
			if (bitf->field == TFA_FAM(dev_idx, AUDFS)) {
				fs_profile = bitf->value;
				break;
			}
		}
		/* Ignore register case */
	}

	pr_debug("%s - default fs: 0x%x = %dHz (%d - %d)\n", __FUNCTION__, fs_profile,
					tfa98xx_sr_from_field(fs_profile),
					dev_idx, prof_idx);
	if (fs_profile != -1)
		return tfa98xx_sr_from_field(fs_profile);

	return 48000;
}

enum Tfa98xx_Error get_sample_rate_info(int dev_idx, nxpTfaProfileList_t *prof, nxpTfaProfileList_t *previous_prof, int fs_previous_profile)
{
	enum Tfa98xx_Error err = Tfa98xx_Error_Ok;
	nxpTfaBitfield_t *bitf;
	unsigned int i;
	int fs_default_profile = 8;	/* default is 48kHz */
	int fs_next_profile = 8;	/* default is 48kHz */

	/* ---------- default settings previous profile ---------- */
	for (i = 0; i < previous_prof->length; i++) {
		/* Search for the default section */
		if (i == 0) {
			while (previous_prof->list[i].type != dscDefault && i < previous_prof->length)
				i++;
			i++;
		}

		/* Only if we found the default section search for AUDFS */
		if (i < previous_prof->length) {
			if (previous_prof->list[i].type == dscBitfield) {
				bitf = (nxpTfaBitfield_t *)(previous_prof->list[i].offset + (uint8_t *)g_cont);
				if (bitf->field == TFA_FAM(dev_idx, AUDFS)) {
					fs_default_profile = bitf->value;
					break;
				}
			}
		}
	}

	/* ---------- settings next profile ---------- */
	for (i = 0; i < prof->length; i++) {
		/* We only want to write the values before the default section */
		if (prof->list[i].type == dscDefault)
			break;
		/* search for AUDFS */
		if (prof->list[i].type == dscBitfield) {
			bitf = (nxpTfaBitfield_t *)(prof->list[i].offset+(uint8_t *)g_cont);
			if (bitf->field == TFA_FAM(dev_idx, AUDFS)) {
				fs_next_profile = bitf->value;
				break;
			}
		}
	}

	/* Enable if needed for debugging!
	if (tfa98xx_cnt_verbose) {
		pr_debug("sample rate from the previous profile: %d \n", fs_previous_profile);
		pr_debug("sample rate in the default section: %d \n", fs_default_profile);
		pr_debug("sample rate for the next profile: %d \n", fs_next_profile);
	}
	*/

	if (fs_next_profile != fs_default_profile) {
		if (tfa98xx_cnt_verbose)
			pr_debug("Writing delay tables for AUDFS=%d \n", fs_next_profile);

		/* If the AUDFS from the next profile is not the same as
		 * the AUDFS from the default we need to write new delay tables
		 */
		err = tfa98xx_dsp_write_tables(dev_idx, fs_next_profile);
	} else if (fs_default_profile != fs_previous_profile) {
		if (tfa98xx_cnt_verbose)
			pr_debug("Writing delay tables for AUDFS=%d \n", fs_default_profile);

		/* But if we do not have a new AUDFS in the next profile and
		 * the AUDFS from the default profile is not the same as the AUDFS
		 * from the previous profile we also need to write new delay tables
		 */
		err = tfa98xx_dsp_write_tables(dev_idx, fs_default_profile);
	}

	return err;
}

/*
 *  process all items in the profilelist
 *   NOTE an error return during processing will leave the device muted
 *
 */
enum Tfa98xx_Error tfaContWriteProfile(int dev_idx, int prof_idx, int vstep_idx)
{
	enum Tfa98xx_Error err = Tfa98xx_Error_Ok;
	nxpTfaProfileList_t *prof = tfaContProfile(dev_idx, prof_idx);
	nxpTfaProfileList_t *previous_prof = tfaContProfile(dev_idx, tfa_get_swprof(dev_idx));
	char buffer[(MEMTRACK_MAX_WORDS * 3) + 3] = {0}; /* every word requires 3 bytes, and 3 is the msg */
	unsigned int i, k = 0, j = 0, tries = 0;
	nxpTfaFileDsc_t *file;
	nxpTfaCmd_t *cmd;
	int size = 0, ready, fs_previous_profile = 8; /* default fs is 48kHz*/
	int dev_family = tfa98xx_dev_family(dev_idx);

	tfa98xx_dump_register(getHandle(dev_idx), 2, "tfaContWriteProfile  begin");
	if (!prof || !previous_prof) {
		pr_err("Error trying to get the (previous) swprofile \n");
		return Tfa98xx_Error_Bad_Parameter;
	}

	if (tfa98xx_cnt_verbose) {
		tfa98xx_trace_printk("device:%s profile:%s vstep:%d\n", tfaContDeviceName(dev_idx),
					tfaContProfileName(dev_idx, prof_idx), vstep_idx);
	}

	/* We only make a power cycle when the profiles are not in the same group */
	if (prof->group == previous_prof->group && prof->group != 0) {
		if (tfa98xx_cnt_verbose) {
			pr_debug("The new profile (%s) is in the same group as the current profile (%s)\n",
				tfaContGetString(&prof->name), tfaContGetString(&previous_prof->name));
		}
	} else {
		/* mute */
		/*pr_info("tfaContWriteProfile()     will be calling tfaRunMute()\n");*/
		err = tfaRunMute(dev_idx);
		tfa98xx_dump_register(getHandle(dev_idx), 2, "tfaContWriteProfile  after tfaRunMute");
		if (Tfa98xx_Error_Ok != err)
			pr_err("tfaContWriteProfile()	  tfaRunMute	error(%d)\n", err);
		/* Get current sample rate before we start switching */
		fs_previous_profile = TFA_GET_BF(dev_idx, AUDFS);

		tfa98xx_dump_register(getHandle(dev_idx), 2, "tfaContWriteProfile  after performed TFA_SET_BF_VOLATILE(dev_idx, SBSL, 0);");
		/* When we switch profile we first power down the subsystem
		 * This should only be done when we are in operating mode
		 * */
		if (((dev_family == 2) && (TFA_GET_BF(dev_idx, MANSTATE) >= 6)) || (dev_family != 2)) {
			err = tfa98xx_powerdown(dev_idx, 1);
			tfa98xx_dump_register(getHandle(dev_idx), 2, "tfaContWriteProfile  after tfa98xx_powerdown");
			if (Tfa98xx_Error_Ok != err) {
				pr_err("tfaContWriteProfile()	  tfa98xx_powerdown  error(%d)\n", err);
				return err;
			}

			/* Wait until we are in PLL powerdown */
			do {
				if (dev_family == 2) {
					/* Added to skip the state of init_cf,
					* otherwise chip cannot be powered down
					*/
					if (TFA_GET_BF(dev_idx, MANSTATE) == 6) {
						TFA_SET_BF_VOLATILE(dev_idx, SBSL, 1);
						TFA_SET_BF_VOLATILE(dev_idx, MANSCONF, 0);
					}
					if (TFA_GET_BF(dev_idx, MANSTATE) == 0) {
						break;
					} else {
						msleep_interruptible(5); /* wait 5ms to avoid busload */
					}
				} else {
					err = tfa98xx_dsp_system_stable(dev_idx, &ready);
					if (Tfa98xx_Error_Ok != err)
						pr_err("tfaContWriteProfile()	  tfa98xx_dsp_system_stable  error(%d)\n", err);

					if (!ready)
						break;
					else
						msleep_interruptible(5); /* wait 10ms to avoid busload */
				}
				tries++;
			} while (tries <= 100);

			if (tries > 100) {
				pr_err("Wait for PLL powerdown timed out!\n");
				return Tfa98xx_Error_StateTimedOut;
			}
		} else {
			pr_debug("No need to go to powerdown now \n");
		}
	}

	/* set all bitfield settings */
	/* First set all default settings */
	if (tfa98xx_cnt_verbose) {
		pr_debug("---------- default settings profile: %s (%d) ----------\n",
				tfaContGetString(&previous_prof->name), tfa_get_swprof(dev_idx));

		if (dev_family == 2) {
			pr_info("---------- default settings profile: %s (%d) ----------\n",
					tfaContGetString(&previous_prof->name), tfa_get_swprof(dev_idx));
			err = show_current_state(dev_idx);
		}
	}

	/* Loop profile length */
	for (i = 0; i < previous_prof->length; i++) {
		tfa98xx_dump_register(getHandle(dev_idx), 2, "tfaContWriteProfile  before tfaContWriteItem");

		/* Search for the default section */
		if (i == 0) {
			while (previous_prof->list[i].type != dscDefault && i < previous_prof->length)
				i++;
			i++;
		}

		/* Only if we found the default section try writing the items */
		if (i < previous_prof->length) {
			if (tfaContWriteItem(dev_idx,  &previous_prof->list[i]) != Tfa98xx_Error_Ok) {
				pr_err("tfaContWriteProfile()	  tfaContWriteItem	error(%d)\n", Tfa98xx_Error_Bad_Parameter);
				return Tfa98xx_Error_Bad_Parameter;
			}
			tfa98xx_dump_register(getHandle(dev_idx), 2, "tfaContWriteProfile  after tfaContWriteItem");
		}
	}

	if (tfa98xx_cnt_verbose)
		pr_debug("---------- new settings profile: %s (%d) ----------\n",
				tfaContGetString(&prof->name), prof_idx);

	/* set new settings */
	for (i = 0; i < prof->length; i++) {
		/* Remember where we currently are with writing items*/
		j = i;

		/* We only want to write the values before the default section when we switch profile */
		/* process and write all non-file items */
		switch (prof->list[i].type) {
		case dscFile:
		case dscPatch:
		case dscSetInputSelect:
		case dscSetOutputSelect:
		case dscSetProgramConfig:
		case dscSetLagW:
		case dscSetGains:
		case dscSetvBatFactors:
		case dscSetSensesCal:
		case dscSetSensesDelay:
		case dscSetMBDrc:
		case dscCmd:
		case dscFilter:
		case dscDefault:
			/* When one of these files are found, we exit */
			i = prof->length;
			break;
		default:
			tfa98xx_dump_register(getHandle(dev_idx), 2, "tfaContWriteProfile  before tfaContWriteItem in switching....");
			err = tfaContWriteItem(dev_idx,  &prof->list[i]);
			tfa98xx_dump_register(getHandle(dev_idx), 2, "tfaContWriteProfile  after  tfaContWriteItem in switching....");
			if (err != Tfa98xx_Error_Ok) {
				pr_err("tfaContWriteProfile()	 default:  tfaContWriteItem	error(%d)\n", err);
				return Tfa98xx_Error_Bad_Parameter;
			}
			break;
		}
	}

	if (prof->group != previous_prof->group || prof->group == 0) {
		tfa98xx_dump_register(getHandle(dev_idx), 2, "tfaContWriteProfile  in profile group");

		/* SL: Make sure device stay in init_CF state */
		if (dev_family == 2) {
			TFA_SET_BF_VOLATILE(dev_idx, MANSCONF, 1);

			/* Reset SBSL to 0 (workaround of enbl_powerswitch=0) */
			TFA_SET_BF_VOLATILE(dev_idx, SBSL, 0);
			/* Sending commands to DSP we need to make sure RST is 0 (otherwise we get no response)*/
			TFA_SET_BF(dev_idx, RST, 1);

			err = show_current_state(dev_idx);

			if (strnstr(tfaContProfileName(dev_idx, prof_idx), ".standby", strlen(tfaContProfileName(dev_idx, prof_idx))) != NULL) {
				tries = 0;
				err = tfa98xx_powerdown(dev_idx, 1);
				TFA_SET_BF_VOLATILE(dev_idx, AMPE, 0);
				pr_info("tfaContWriteProfile   after performed tfa98xx_powerdown\n");

				do {
					int manstate = TFA_GET_BF(dev_idx, MANSTATE);

					pr_info("tfaContWriteProfile   manstate=%d\n", manstate);
					if (manstate == 0x0a) {
						msleep_interruptible(5);
					} else if (manstate == 6) {
						TFA_SET_BF_VOLATILE(dev_idx, SBSL, 1);
					} else if ((manstate == 0) || (manstate == 1)) {
						/* SL: TFA powered down and already in stanby profile */
						TFA_SET_BF_VOLATILE(dev_idx, SBSL, 0);
						pr_info("tfaContWriteProfile   after SBSL set to 0 in do...while()\n");
						return err;
					}
					tries++;
				} while (tries < 50);
			} else {/* SL: If not .standby profile then power up device*/
				if (TFA_GET_BF(dev_idx, CFE) != 0) {
					if (TFA_GET_BF(dev_idx, REFCKSEL) == 0) {
						pr_info("tfaContWriteProfile   will be performed TFA_SET_BF(dev_idx, REFCKSEL, 1)\n");
						TFA_SET_BF(dev_idx, REFCKSEL, 1);
						TFA_SET_BF(dev_idx, AMPE, 0);
					}
				} else {/*SL: if CFE is not enabled then only power up then return*/
					/* power on the sub system */
					TFA_SET_BF_VOLATILE(dev_idx, PWDN, 0);
					return err;
				}
			}
		}
		/* Leave powerdown state */
		err = tfa_cf_powerup(dev_idx);
		if (Tfa98xx_Error_Ok != err) {
			pr_err("tfaContWriteProfile()	  tfa_cf_powerup  error(%d)\n", err);
			return err;
		}

		if (tfa98xx_cnt_verbose && (2 == dev_family))
			err = show_current_state(dev_idx);

		tries = 0;

		/* SL: Fix the powerswitch issue to be able to load the setting */
		if (dev_family == 2) {
			do {
				if ((TFA_GET_BF(dev_idx, MANSTATE) == 6) &&
					(TFA_GET_BF(dev_idx, RST) == 1) &&
					(TFA_GET_BF(dev_idx, SBSL) == 0)) {
					/* Reset SBSL to 0 (workaround of enbl_powerswitch=0) */
					TFA_SET_BF_VOLATILE(dev_idx, SBSL, 0);
					/* Sending commands to DSP we need to make sure RST is 1 (otherwise we get no response)*/
					TFA_SET_BF(dev_idx, RST, 0);
					break;
				} else {
					msleep_interruptible(1);
				}
				tries++;
			} while (tries < 30);
		}
	}

	/* Check if there are sample rate changes */
	err = get_sample_rate_info(dev_idx, prof, previous_prof, fs_previous_profile);
	if (Tfa98xx_Error_Ok != err) {
		pr_err("tfaContWriteProfile()	  get_sample_rate_info  error(%d)\n", err);
		return err;
	}

	/* Write files from previous profile (default section)
	 * Should only be used for the patch&trap patch (file)
	 */
	if (dev_family == 2) {
		tfa98xx_dump_register(getHandle(dev_idx), 2, "[nxp] tfaContWriteProfile()  before write");
		for (i = 0; i < previous_prof->length; i++) {
			/* Search for the default section */
			if (i == 0) {
				while (previous_prof->list[i].type != dscDefault && i < previous_prof->length)
					i++;
				i++;
			}

			/* Only if we found the default section try writing the file */
			if (i < previous_prof->length) {
				if (previous_prof->list[i].type == dscFile || previous_prof->list[i].type == dscPatch) {
					/* Only write this once */
					if (tfa98xx_cnt_verbose && k == 0) {
						pr_debug("---------- files default profile: %s (%d) ----------\n",
								tfaContGetString(&previous_prof->name), prof_idx);
						k++;
					}
					file = (nxpTfaFileDsc_t *)(previous_prof->list[i].offset+(uint8_t *)g_cont);
					pr_info("tfaContWriteProfile()	   will be calling tfaContWriteFile()\n");
					err = tfaContWriteFile(dev_idx,  file, vstep_idx, TFA_MAX_VSTEP_MSG_MARKER);
					if (Tfa98xx_Error_Ok != err)
						pr_err("tfaContWriteProfile()	  tfaContWriteFile	error(%d)\n", err);
				}
			}
		}
	}

	if (tfa98xx_cnt_verbose) {
		pr_debug("---------- files new profile: %s (%d) ----------\n",
				tfaContGetString(&prof->name), prof_idx);
	}
	pr_info("tfaContWriteProfile()	  start writing files");

	/* write everything until end or the default section starts
	 * Start where we currenly left */
	for (i = j; i < prof->length; i++) {
		/* We only want to write the values before the default section when we switch profile */
		if (prof->list[i].type == dscDefault)
			break;

		switch (prof->list[i].type) {
		case dscFile:
		case dscPatch:
			file = (nxpTfaFileDsc_t *)(prof->list[i].offset+(uint8_t *)g_cont);
			err = tfaContWriteFile(dev_idx, file, vstep_idx, TFA_MAX_VSTEP_MSG_MARKER);
			break;
		case dscSetInputSelect:
		case dscSetOutputSelect:
		case dscSetProgramConfig:
		case dscSetLagW:
		case dscSetGains:
		case dscSetvBatFactors:
		case dscSetSensesCal:
		case dscSetSensesDelay:
		case dscSetMBDrc:
			create_dsp_buffer_msg((nxpTfaMsg_t *)
				(prof->list[i].offset+(char *)g_cont), buffer, &size);
			err = tfa_dsp_msg(dev_idx, size, buffer);

			if (tfa98xx_cnt_verbose)
				pr_debug("command: %s=0x%02x%02x%02x\n",
					tfaContGetCommandString(prof->list[i].type),
				(unsigned char)buffer[0], (unsigned char)buffer[1], (unsigned char)buffer[2]);
			break;
		case dscCmd:
			size = *(uint16_t *)(prof->list[i].offset+(char *)g_cont);
			err = tfa_dsp_msg(dev_idx, size, prof->list[i].offset + 2 + (char *)g_cont);

			if (tfa98xx_cnt_verbose) {
				cmd = (nxpTfaCmd_t *)(prof->list[i].offset+(uint8_t *)g_cont);
				pr_debug("Writing cmd=0x%02x%02x%02x \n", cmd->value[0], cmd->value[1], cmd->value[2]);
			}
			break;
		default:
			/* This allows us to write bitfield, registers or xmem after files */
			if (tfaContWriteItem(dev_idx,  &prof->list[i]) != Tfa98xx_Error_Ok)
				return Tfa98xx_Error_Bad_Parameter;
			break;
		}

		if (err != Tfa98xx_Error_Ok) {
			pr_err("tfaContWriteProfile()	  prof->list[%d].type=%d	  >>> error(%d)\n", i,  prof->list[i].type, err);
			return err;
		}
	}

	if ((prof->group != previous_prof->group || prof->group == 0) && (dev_family == 2)) {
		if (TFA_GET_BF(dev_idx, REFCKSEL) == 0) {
			/* set SBSL to go to operation mode */
			TFA_SET_BF_VOLATILE(dev_idx, SBSL, 1);
		}
	}
	if (err != Tfa98xx_Error_Ok)
		pr_err("tfaContWriteProfile()	  end    error(%d)\n", err);

	return err;
}

/*
 *  process only vstep in the profilelist
 *
 */
enum Tfa98xx_Error tfaContWriteFilesVstep(int dev_idx, int prof_idx, int vstep_idx)
{
	nxpTfaProfileList_t *prof = tfaContProfile(dev_idx, prof_idx);
	unsigned int i;
	nxpTfaFileDsc_t *file;
	nxpTfaHeader_t *hdr;
	nxpTfaHeaderType_t type;
	enum Tfa98xx_Error err = Tfa98xx_Error_Ok;

	if (!prof)
		return Tfa98xx_Error_Bad_Parameter;

	if (tfa98xx_cnt_verbose)
		tfa98xx_trace_printk("device:%s profile:%s vstep:%d\n", tfaContDeviceName(dev_idx),
					tfaContProfileName(dev_idx, prof_idx), vstep_idx);

	/* write vstep file only! */
	for (i = 0; i < prof->length; i++) {
		if (prof->list[i].type == dscFile) {
			file = (nxpTfaFileDsc_t *)(prof->list[i].offset+(uint8_t *)g_cont);
			hdr = (nxpTfaHeader_t *)file->data;
			type = (nxpTfaHeaderType_t) hdr->id;

			switch (type) {
			case volstepHdr:
				if (tfaContWriteFile(dev_idx, file, vstep_idx, TFA_MAX_VSTEP_MSG_MARKER))
					return Tfa98xx_Error_Bad_Parameter;
				break;
			default:
				break;
			}
		}
	}

	return err;
}


char *tfaContGetString(nxpTfaDescPtr_t *dsc)
{
	if (dsc->type != dscString)
		return nostring;

	return dsc->offset+(char *)g_cont;
}

void individual_calibration_results(Tfa98xx_handle_t handle)
{
	int value_P, value_S;

	/* Read the calibration result in xmem (529=primary channel) (530=secondary channel) */
	tfa98xx_dsp_read_mem(handle, 529, 1, &value_P);
	tfa98xx_dsp_read_mem(handle, 530, 1, &value_S);

	if (value_P != 1 && value_S != 1) {
		pr_debug("Calibration failed on both channels! \n");
	} else if (value_P != 1) {
		pr_debug("Calibration failed on Primary (Left) channel! \n");
		TFA_SET_BF_VOLATILE(handle, SSLEFTE, 0); /* Disable the sound for the left speaker */
	} else if (value_S != 1) {
		pr_debug("Calibration failed on Secondary (Right) channel! \n");
		TFA_SET_BF_VOLATILE(handle, SSRIGHTE, 0); /* Disable the sound for the right speaker */
	}

	TFA_SET_BF_VOLATILE(handle, AMPINSEL, 0); /* Set amplifier input to TDM */
	TFA_SET_BF_VOLATILE(handle, SBSL, 1);
}

char *tfaContGetCommandString(uint32_t type)
{
	if (type == dscSetInputSelect)
		return "setInputSelector";
	else if (type == dscSetOutputSelect)
		return "setOutputSelector";
	else if (type == dscSetProgramConfig)
		return "setProgramConfig";
	else if (type == dscSetLagW)
		return "setLagW";
	else if (type == dscSetGains)
		return "setGains";
	else if (type == dscSetvBatFactors)
		return "setvBatFactors";
	else if (type == dscSetSensesCal)
		return "setSensesCal";
	else if (type == dscSetSensesDelay)
		return "setSensesDelay";
	else if (type == dscSetMBDrc)
		return "setMBDrc";
	else if (type == dscFilter)
		return "filter";
	else
		return nostring;
}

/*
 * Get the name of the device at a certain index in the container file
 *  return device name
 */
char  *tfaContDeviceName(int dev_idx)
{
	nxpTfaDeviceList_t *dev;

	if (dev_idx >= tfa98xx_cnt_max_device())
		return errorname;

	dev = tfaContDevice(dev_idx);
	if (dev == NULL)
		return errorname;

	return tfaContGetString(&dev->name);
}

/*
 * Get the application name from the container file application field
 * note that the input stringbuffer should be sizeof(application field)+1
 *
 */
int tfa_cnt_get_app_name(char *name)
{
	unsigned int i;
	int len = 0;

	for (i = 0; i < sizeof(g_cont->application); i++) {
		if (isalnum(g_cont->application[i])) /* copy char if valid */
			name[len++] = g_cont->application[i];
		if (g_cont->application[i] == '\0')
			break;
	}
	name[len++] = '\0';

	return len;
}

/*
 * Get profile index of the calibration profile.
 * Returns: (profile index) if found, (-2) if no
 * calibration profile is found or (-1) on error
 */
int tfaContGetCalProfile(int dev_idx)
{
	int prof, nprof, cal_idx = -2;

	if ((dev_idx < 0) || (dev_idx >= tfa98xx_cnt_max_device()))
		return -EPERM;

	nprof = tfaContMaxProfile(dev_idx);
	/* search for the calibration profile in the list of profiles */
	for (prof = 0; prof < nprof; prof++) {
		if (strnstr(tfaContProfileName(dev_idx, prof), ".cal", strlen(".cal")) != NULL) {
			cal_idx = prof;
			pr_debug("Using calibration profile: '%s'\n", tfaContProfileName(dev_idx, prof));
			break;
		}
	}
	return cal_idx;
}

/**
 * Is the profile a tap profile ?
 * @param dev_idx the index of the device
 * @param prof_idx the index of the profile
 * @return 1 if the profile is a tap profile or 0 if not
 */
int tfaContIsTapProfile(int dev_idx, int prof_idx)
{
	if ((dev_idx < 0) || (dev_idx >= tfa98xx_cnt_max_device()))
		return -EPERM;

	/* Check if next profile is tap profile */
	if (strstr(tfaContProfileName(dev_idx, prof_idx), ".tap") != NULL) {
		pr_debug("Using Tap profile: '%s'\n", tfaContProfileName(dev_idx, prof_idx));
		return 1;
	}

	return 0;
}

/*
 * Get the name of the profile at certain index for a device in the container file
 *  return profile name
 */
char *tfaContProfileName(int dev_idx, int prof_idx)
{
	nxpTfaProfileList_t *prof;

	if ((dev_idx < 0) || (dev_idx >= tfa98xx_cnt_max_device()))
		return errorname;
	if ((prof_idx < 0) || (prof_idx >= tfaContMaxProfile(dev_idx)))
		return nonename;

	/* the Nth profiles for this device */
	prof = tfaContGetDevProfList(g_cont, dev_idx, prof_idx);
	return tfaContGetString(&prof->name);
}

/*
 * return 1st profile list
 */
nxpTfaProfileList_t *tfaContGet1stProfList(nxpTfaContainer_t *cont)
{
	nxpTfaProfileList_t *prof;
	uint8_t *b = (uint8_t *) cont;

	int maxdev = 0;
	nxpTfaDeviceList_t *dev;

	/* get nr of devlists */
	maxdev = cont->ndev;
	/* get last devlist */
	dev = tfaContGetDevList(cont, maxdev - 1);
	if (dev == NULL)
		return NULL;
	/* the 1st profile starts after the last device list */
	b = (uint8_t *) dev + sizeof(nxpTfaDeviceList_t) + dev->length * (sizeof(nxpTfaDescPtr_t));
	prof = (nxpTfaProfileList_t *) b;
	return prof;
}

/*
 * return 1st livedata list
 */
nxpTfaLiveDataList_t *tfaContGet1stLiveDataList(nxpTfaContainer_t *cont)
{
	nxpTfaLiveDataList_t *ldata;
	nxpTfaProfileList_t *prof;
	nxpTfaDeviceList_t *dev;
	uint8_t *b = (uint8_t *) cont;
	int maxdev, maxprof;

	/* get nr of devlists+1 */
	maxdev = cont->ndev;
	/* get nr of proflists */
	maxprof = cont->nprof;

	/* get last devlist */
	dev = tfaContGetDevList(cont, maxdev - 1);
	/* the 1st livedata starts after the last device list */
	b = (uint8_t *) dev + sizeof(nxpTfaDeviceList_t) +
		dev->length * (sizeof(nxpTfaDescPtr_t));

	while (maxprof != 0) {
		/* get last proflist */
		prof = (nxpTfaProfileList_t *) b;
		b += sizeof(nxpTfaProfileList_t) +
			((prof->length-1) * (sizeof(nxpTfaDescPtr_t)));
		maxprof--;
	}

	/* Else the marker falls off */
	b += 4; /*bytes*/

	ldata = (nxpTfaLiveDataList_t *) b;
	return ldata;
}


enum Tfa98xx_Error tfaContOpen(int dev_idx)
{
	return tfa98xx_open((Tfa98xx_handle_t)dev_idx);
}

enum Tfa98xx_Error tfaContClose(int dev_idx)
{
	return tfa98xx_close(dev_idx);
}

/*
 * return the device count in the container file
 */
int tfa98xx_cnt_max_device(void)
{
	return g_cont != NULL ? g_cont->ndev : 0;
}

/*
 * lookup slave and return device index
 */
int tfa98xx_cnt_slave2idx(int slave_addr)
{
	int idx;

	for (idx = 0; idx < g_devs; idx++) {
		if (g_dev[idx]->dev == slave_addr)
			return idx;
	}

	return -EPERM;
}

/*
 * lookup slave and return device revid
 */
int tfa98xx_cnt_slave2revid(int slave_addr)
{
	int idx = tfa98xx_cnt_slave2idx(slave_addr);
	uint16_t revid;

	if (idx < 0)
		return idx;

	/* note that the device must have been opened before */
	revid = tfa98xx_get_device_revision(idx);

	/* quick check for valid contents */
	return (revid&0xFF) >= 0x12 ? revid : -1 ;
}

/*
 * return the device list pointer
 */
nxpTfaDeviceList_t *tfaContDevice(int dev_idx)
{
	if (dev_idx < g_devs)
		return g_dev[dev_idx];
	/* pr_err("Devlist index too high:%d!", idx); */
	return NULL;
}

/*
 * return the per device profile count
 */
int tfaContMaxProfile(int dev_idx)
{
	if (dev_idx >= g_devs) {
		/* pr_err("Devlist index too high:%d!", ndev); */
		return 0;
	}
	return g_profs[dev_idx];
}

/*
 * return the next profile:
 *  - assume that all profiles are adjacent
 *  - calculate the total length of the input
 *  - the input profile + its length is the next profile
 */
nxpTfaProfileList_t *tfaContNextProfile(nxpTfaProfileList_t *prof)
{
	uint8_t *this, *next; /* byte pointers for byte pointer arithmetic */
	nxpTfaProfileList_t *nextprof;
	int listlength; /* total length of list in bytes */

	if (prof == NULL)
		return NULL;

	if (prof->ID != TFA_PROFID)
		return NULL;	/* invalid input */

	this = (uint8_t *)prof;
	/* nr of items in the list, length includes name dsc so - 1*/
	listlength = (prof->length - 1)*sizeof(nxpTfaDescPtr_t);
	/* the sizeof(nxpTfaProfileList_t) includes the list[0] length */
	next = this + listlength + sizeof(nxpTfaProfileList_t); /* - sizeof(nxpTfaDescPtr_t); */
	nextprof = (nxpTfaProfileList_t *)next;

	if (nextprof->ID != TFA_PROFID)
		return NULL;

	return nextprof;
}

/*
 * return the next livedata
 */
nxpTfaLiveDataList_t *tfaContNextLiveData(nxpTfaLiveDataList_t *livedata)
{
	nxpTfaLiveDataList_t *nextlivedata = (nxpTfaLiveDataList_t *)((char *)livedata + (livedata->length * 4) +
							sizeof(nxpTfaLiveDataList_t) - 4);

	if (nextlivedata->ID == TFA_LIVEDATAID)
		return nextlivedata;

	return NULL;
}

/*
 * return the device list pointer
 */
nxpTfaProfileList_t *tfaContProfile(int dev_idx, int prof_ipx)
{
	if (dev_idx >= g_devs) {
		/* pr_err("Devlist index too high:%d!", ndev); */
		return NULL;
	}
	if (prof_ipx >= g_profs[dev_idx]) {
		/* pr_err("Proflist index too high:%d!", nprof); */
		return NULL;
	}

		return g_prof[dev_idx][prof_ipx];
}

/*
 * check CRC for container
 *   CRC is calculated over the bytes following the CRC field
 *
 *   return non zero value on error
 */
int tfaContCrcCheckContainer(nxpTfaContainer_t *cont)
{
	uint8_t *base;
	size_t size;
	uint32_t crc;

	base = (uint8_t *)&cont->CRC + 4; /* ptr to bytes following the CRC field */
	size = (size_t)(cont->size - (base - (uint8_t *)cont)); /* nr of bytes following the CRC field */
	if (size < 0)
		return 1;
	crc = ~crc32_le(~0u, base, size);

	return crc != cont->CRC;
}

/**
 * Create a buffer which can be used to send to the dsp.
 */
void create_dsp_buffer_msg(nxpTfaMsg_t *msg, char *buffer, int *size) /* TODO cleanup */
{
	int i, j = 0;

	/* Copy cmdId. Remember that the cmdId is reversed */
	buffer[0] = msg->cmdId[2];
	buffer[1] = msg->cmdId[1];
	buffer[2] = msg->cmdId[0];

	/* Copy the data to the buffer */
	for (i = 3; i < 3 + (msg->msg_size * 3); i++) {
		buffer[i] = (uint8_t) ((msg->data[j] >> 16) & 0xffff);
		i++;
		buffer[i] = (uint8_t) ((msg->data[j] >> 8) & 0xff);
		i++;
		buffer[i] = (uint8_t) (msg->data[j] & 0xff);
		j++;
	}

	*size = (3 + (msg->msg_size * 3)) * sizeof(char);
}

void get_all_features_from_cnt(Tfa98xx_handle_t dev_idx, int *hw_feature_register, int sw_feature_register[2])
{
	nxpTfaFeatures_t *features;
	int i;

	nxpTfaDeviceList_t *dev = tfaContDevice(dev_idx);

	/* Init values in case no keyword is defined in cnt file: */
	*hw_feature_register = -1;
	sw_feature_register[0] = -1;
	sw_feature_register[1] = -1;

	if (dev == NULL)
		return;

	/* process the device list */
	for (i = 0; i < dev->length; i++) {
		if (dev->list[i].type == dscFeatures) {
			features = (nxpTfaFeatures_t *)(dev->list[i].offset+(uint8_t *)g_cont);
			*hw_feature_register = features->value[0];
			sw_feature_register[0] = features->value[1];
			sw_feature_register[1] = features->value[2];
			break;
		}
	}
}

/* wrapper function */
void get_hw_features_from_cnt(Tfa98xx_handle_t dev_idx, int *hw_feature_register)
{
	int sw_feature_register[2];

	get_all_features_from_cnt(dev_idx, hw_feature_register, sw_feature_register);
}

/* wrapper function */
void get_sw_features_from_cnt(Tfa98xx_handle_t dev_idx, int sw_feature_register[2])
{
	int hw_feature_register;

	get_all_features_from_cnt(dev_idx, &hw_feature_register, sw_feature_register);
}

/* Factory trimming for the Boost converter */
void tfa_factory_trimmer(Tfa98xx_handle_t dev_idx)
{
	unsigned short currentValue, delta;
	int result;

	/* Factory trimming for the Boost converter */
	/* check if there is a correction needed */
	result = TFA_GET_BF(dev_idx, DCMCCAPI);
	if (result) {
		/* Get currentvalue of DCMCC and the Delta value */
		currentValue = (unsigned short)TFA_GET_BF(dev_idx, DCMCC);
		delta = (unsigned short)TFA_GET_BF(dev_idx, USERDEF);

		/* check the sign bit (+/-) */
		result = TFA_GET_BF(dev_idx, DCMCCSB);
		if (result == 0) {
			/* Do not exceed the maximum value of 15 */
			if (currentValue + delta < 15) {
				TFA_SET_BF_VOLATILE(dev_idx, DCMCC, currentValue + delta);
				if (tfa98xx_cnt_verbose)
					pr_debug("Max coil current is set to: %d\n", currentValue + delta);
			} else {
				TFA_SET_BF_VOLATILE(dev_idx, DCMCC, 15);
				if (tfa98xx_cnt_verbose)
					pr_debug("Max coil current is set to: 15\n");
			}
		} else if (result == 1) {
			/* Do not exceed the minimum value of 0 */
			if (currentValue - delta > 0) {
				TFA_SET_BF_VOLATILE(dev_idx, DCMCC, currentValue - delta);
				if (tfa98xx_cnt_verbose)
					pr_debug("Max coil current is set to: %d\n", currentValue - delta);
			} else {
				TFA_SET_BF_VOLATILE(dev_idx, DCMCC, 0);
				if (tfa98xx_cnt_verbose)
					pr_debug("Max coil current is set to: 0\n");
			}
		}
	}
}

enum Tfa98xx_Error tfa_set_filters(int dev_idx, int prof_idx)
{
	enum Tfa98xx_Error err = Tfa98xx_Error_Ok;
	nxpTfaProfileList_t *prof = tfaContProfile(dev_idx, prof_idx);
	unsigned int i;

	if (!prof)
		return Tfa98xx_Error_Bad_Parameter;

	/* If we are in powerdown there is no need to set filters */
	if (TFA_GET_BF(dev_idx, PWDN) == 1)
		return Tfa98xx_Error_Ok;

	/* loop the profile to find filter settings */
	for (i = 0; i < prof->length; i++) {
		/* We only want to write the values before the default section */
		if (prof->list[i].type == dscDefault)
			break;

		/* write all filter settings */
		if (prof->list[i].type == dscFilter) {
			if (tfaContWriteItem(dev_idx,  &prof->list[i]) != Tfa98xx_Error_Ok)
				return err;
		}
	}

	return err;
}
