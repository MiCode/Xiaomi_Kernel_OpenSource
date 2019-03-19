/*
 * Copyright 2014-2017 NXP Semiconductors
 * Copyright (C) 2019 XiaoMi, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "dbgprint.h"
#include "tfa_container.h"
#include "tfa.h"
#include "tfa98xx_tfafieldnames.h"
#include "tfa_internal.h"

/* defines */
#define MODULE_BIQUADFILTERBANK 2
#define BIQUAD_COEFF_SIZE       6

/* module globals */
static uint8_t gslave_address = 0; /* This is used to SET the slave with the --slave option */

static int float_to_int(uint32_t x)
{
	unsigned e = (0x7F + 31) - ((*(unsigned *) &x & 0x7F800000) >> 23);
	unsigned m = 0x80000000 | (*(unsigned *) &x << 8);
	return -(int)((m >> e) & -(e < 32));
}

/*
 * check the container file
*/
enum tfa_error tfa_load_cnt(void *cnt, int length)
{
	nxpTfaContainer_t  *cntbuf = (nxpTfaContainer_t  *)cnt;

	if (length > TFA_MAX_CNT_LENGTH) {
		pr_err("incorrect length\n");
		return tfa_error_container;
	}

	if (HDR(cntbuf->id[0], cntbuf->id[1]) == 0) {
		pr_err("header is 0\n");
		return tfa_error_container;
	}

	if ( (HDR(cntbuf->id[0], cntbuf->id[1])) != paramsHdr ) {
		pr_err("wrong header type: 0x%02x 0x%02x\n", cntbuf->id[0], cntbuf->id[1]);
		return tfa_error_container;
	}

	if (cntbuf->size == 0) {
		pr_err("data size is 0\n");
		return tfa_error_container;
	}

	/* check CRC */
	if ( tfaContCrcCheckContainer(cntbuf)) {
		pr_err("CRC error\n");
		return tfa_error_container;
	}

	/* check sub version level */
	if ((cntbuf->subversion[1] != NXPTFA_PM_SUBVERSION) &&
							(cntbuf->subversion[0] != '0')) {
		pr_err("container sub-version not supported: %c%c\n",
				cntbuf->subversion[0], cntbuf->subversion[1]);
		return tfa_error_container;
	}

	return tfa_error_ok;
}

/*
 * Dump the contents of the file header
 */
void tfaContShowHeader(nxpTfaHeader_t *hdr) {
	char _id[2];

	pr_debug("File header\n");

	_id[1] = hdr->id >> 8;
	_id[0] = hdr->id & 0xff;
	pr_debug("\tid:%.2s version:%.2s subversion:%.2s\n", _id,
		   hdr->version, hdr->subversion);
	pr_debug("\tsize:%d CRC:0x%08x \n", hdr->size, hdr->CRC);
	pr_debug( "\tcustomer:%.8s application:%.8s type:%.8s\n", hdr->customer,
			   hdr->application, hdr->type);
}

/*
 * return device list dsc from index
 */
nxpTfaDeviceList_t *tfaContGetDevList(nxpTfaContainer_t *cont, int dev_idx)
{
	uint8_t *base = (uint8_t *) cont;

	if (cont == NULL)
		return NULL;

	if ( (dev_idx < 0) || (dev_idx >= cont->ndev))
		return NULL;

	if (cont->index[dev_idx].type != dscDevice)
		return NULL;

	base += cont->index[dev_idx].offset;
	return (nxpTfaDeviceList_t *) base;
}

/*
 * get the Nth profile for the Nth device
 */
nxpTfaProfileList_t *tfaContGetDevProfList(nxpTfaContainer_t * cont, int devIdx, int profIdx)
{
	nxpTfaDeviceList_t *dev;
	int idx, hit;
	uint8_t *base = (uint8_t *) cont;

	dev = tfaContGetDevList(cont, devIdx);
	if (dev) {
		for (idx = 0, hit = 0; idx < dev->length; idx++) {
			if (dev->list[idx].type == dscProfile) {
				if (profIdx == hit++)
					return (nxpTfaProfileList_t *) (dev->list[idx].offset+base);
			}
		}
	}

	return NULL;
}

/*
 * get the number of profiles for the Nth device
 */
int tfa_cnt_get_dev_nprof(struct tfa_device *tfa)
{
	nxpTfaDeviceList_t *dev;
	int idx, nprof = 0;

	if (tfa->cnt == NULL)
		return 0;

	if ((tfa->dev_idx < 0) || (tfa->dev_idx >= tfa->cnt->ndev))
		return 0;

	dev = tfaContGetDevList(tfa->cnt, tfa->dev_idx);
	if (dev) {
		for (idx = 0; idx < dev->length; idx++) {
			if (dev->list[idx].type == dscProfile) {
				nprof++;
			}
		}
	}

	return nprof;
}

/*
 * get the Nth lifedata for the Nth device
 */
nxpTfaLiveDataList_t *tfaContGetDevLiveDataList(nxpTfaContainer_t * cont, int devIdx,
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
                                        return (nxpTfaLiveDataList_t *)
                                        (dev->list[idx].offset + base);
			}
		}
	}

	return NULL;
}

/*
 * Get the max volume step associated with Nth profile for the Nth device
 */
int tfacont_get_max_vstep(struct tfa_device *tfa, int prof_idx) {
	nxpTfaVolumeStep2File_t *vp;
	struct nxpTfaVolumeStepMax2File *vp3;
	int vstep_count = 0;
	vp = (nxpTfaVolumeStep2File_t *) tfacont_getfiledata(tfa, prof_idx, volstepHdr);
	if (vp == NULL)
		return 0;
	/* check the header type to load different NrOfVStep appropriately */
	if (tfa->tfa_family == 2) {
		/* this is actually tfa2, so re-read the buffer*/
		vp3 = (struct nxpTfaVolumeStepMax2File *)
		tfacont_getfiledata(tfa, prof_idx, volstepHdr);
		if ( vp3 ) {
			vstep_count = vp3->NrOfVsteps;
		}
	} else {
		/* this is max1*/
		if ( vp ) {
			vstep_count = vp->vsteps;
		}
	}
	return vstep_count;
}

/**
 * Get the file contents associated with the device or profile
 * Search within the device tree, if not found, search within the profile
 * tree. There can only be one type of file within profile or device.
  */
nxpTfaFileDsc_t *tfacont_getfiledata(struct tfa_device *tfa, int prof_idx, enum nxpTfaHeaderType type)
{
	nxpTfaDeviceList_t *dev;
	nxpTfaProfileList_t *prof;
	nxpTfaFileDsc_t *file;
	nxpTfaHeader_t *hdr;
	unsigned int i;

	if (tfa->cnt == NULL) {
		pr_err("invalid pointer to container file\n");
		return NULL;
	}

	dev = tfaContGetDevList(tfa->cnt, tfa->dev_idx);
	if (dev == NULL) {
		pr_err("invalid pointer to container file device list\n");
		return NULL;
	}

	/* process the device list until a file type is encountered */
	for (i=0;i<dev->length;i++) {
		if ( dev->list[i].type == dscFile ) {
			file = (nxpTfaFileDsc_t *)(dev->list[i].offset+(uint8_t *)tfa->cnt);
			if (file != NULL) {
				hdr = (nxpTfaHeader_t *)file->data;
				/* check for file type */
				if ( hdr->id == type) {
					return (nxpTfaFileDsc_t *)&file->data;
				}
			}
		}
	}

	/* File not found in device tree.
	 * So, look in the profile list until the file type is encountered
	 */
	prof = tfaContGetDevProfList(tfa->cnt, tfa->dev_idx, prof_idx);
	if (prof == NULL) {
		pr_err("invalid pointer to container file profile list\n");
		return NULL;
	}

	for (i=0;i<prof->length;i++) {
		if (prof->list[i].type == dscFile) {
			file = (nxpTfaFileDsc_t *)(prof->list[i].offset+(uint8_t *)tfa->cnt);
			if (file != NULL) {
				hdr= (nxpTfaHeader_t *)file->data;
				if (hdr != NULL) {
					/* check for file type */
					if ( hdr->id == type) {
						return (nxpTfaFileDsc_t *)&file->data;
					}
				}
			}
		}
	}

	if (tfa->verbose)
		pr_debug("%s: no file found of type %d\n", __FUNCTION__, type);

	return NULL;
}

/*
 * write a parameter file to the device
 */
static enum Tfa98xx_Error tfaContWriteVstep(struct tfa_device *tfa,  nxpTfaVolumeStep2File_t *vp, int vstep)
{
	enum Tfa98xx_Error err;
	unsigned short vol;

	if (vstep < vp->vsteps) {
		/* vol = (unsigned short)(voldB / (-0.5f)); */
		vol = (unsigned short)(-2 * float_to_int(*((uint32_t *)&vp->vstep[vstep].attenuation)));
		if (vol > 255)	/* restricted to 8 bits */
			vol = 255;

		err = tfa98xx_set_volume_level(tfa, vol);
		if (err != Tfa98xx_Error_Ok)
			return err;

		err = tfa98xx_dsp_write_preset(tfa, sizeof(vp->vstep[0].preset), vp->vstep[vstep].preset);
		if (err != Tfa98xx_Error_Ok)
			return err;
		err = tfa_cont_write_filterbank(tfa, vp->vstep[vstep].filter);

	} else {
		pr_err("Incorrect volume given. The value vstep[%d] >= %d\n", vstep , vp->vsteps);
		err = Tfa98xx_Error_Bad_Parameter;
	}

	if (tfa->verbose) pr_debug("vstep[%d][%d]\n", tfa->dev_idx, vstep);

	return err;
}

static struct nxpTfaVolumeStepMessageInfo *
tfaContGetmsgInfoFromReg(struct nxpTfaVolumeStepRegisterInfo *regInfo)
{
	char *p = (char*) regInfo;
	p += sizeof(regInfo->NrOfRegisters) + (regInfo->NrOfRegisters * sizeof(uint32_t));
	return (struct nxpTfaVolumeStepMessageInfo*) p;
}

static int
tfaContGetmsgLen(struct  nxpTfaVolumeStepMessageInfo *msgInfo)
{
	return (msgInfo->MessageLength.b[0] << 16) + (msgInfo->MessageLength.b[1] << 8) + msgInfo->MessageLength.b[2];
}

static struct nxpTfaVolumeStepMessageInfo *
tfaContGetNextmsgInfo(struct  nxpTfaVolumeStepMessageInfo *msgInfo)
{
	char *p = (char*) msgInfo;
	int msgLen = tfaContGetmsgLen(msgInfo);
	int type = msgInfo->MessageType;

	p += sizeof(msgInfo->MessageType) + sizeof(msgInfo->MessageLength);
	if (type == 3)
		p += msgLen;
	else
		p += msgLen * 3;

	return (struct nxpTfaVolumeStepMessageInfo*) p;
}

static struct  nxpTfaVolumeStepRegisterInfo*
tfaContGetNextRegFromEndInfo(struct  nxpTfaVolumeStepMessageInfo *msgInfo)
{
	char *p = (char*) msgInfo;
	p += sizeof(msgInfo->NrOfMessages);
	return (struct nxpTfaVolumeStepRegisterInfo*) p;

}

static struct nxpTfaVolumeStepRegisterInfo*
tfaContGetRegForVstep(nxpTfaVolumeStepMax2File_t *vp, int idx)
{
	int i, j, nrMessage;

	struct nxpTfaVolumeStepRegisterInfo *regInfo
		= (struct nxpTfaVolumeStepRegisterInfo*) vp->vstepsBin;
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

#pragma pack (push, 1)
struct tfa_partial_msg_block {
	uint8_t offset;
	uint16_t change;
	uint8_t update[16][3];
};
#pragma pack (pop)

static enum Tfa98xx_Error tfaContWriteVstepMax2_One(struct tfa_device *tfa, struct nxpTfaVolumeStepMessageInfo *new_msg,
						    struct nxpTfaVolumeStepMessageInfo *old_msg, int enable_partial_update)
{
	enum Tfa98xx_Error err = Tfa98xx_Error_Ok;
	int len = (tfaContGetmsgLen(new_msg) - 1) * 3;
	char *buf = (char*)new_msg->ParameterData;
	uint8_t *partial = NULL;
	uint8_t cmdid[3];
	int use_partial_coeff = 0;

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
		/* No patial updates for message type 1 (Coefficients) */
		enable_partial_update = 0;
		if ((tfa->rev & 0xff) == 0x88) {
			use_partial_coeff = 1;
		} else if ((tfa->rev & 0xff) == 0x13) {
			use_partial_coeff = 1;
		}
	}

	/* Change Message Len to the actual buffer len */
	memcpy(cmdid, new_msg->CmdId, sizeof(cmdid));

	/* The algoparams and mbdrc msg id will be changed to the reset type when SBSL=0
	 * if SBSL=1 the msg will remain unchanged. It's up to the tuning engineer to choose the 'without_reset'
	 * types inside the vstep. In other words: the reset msg is applied during SBSL==0 else it remains unchanged.
	 */
	if (tfa_needs_reset(tfa) == 1) {
		if (new_msg->MessageType == 0) {
			cmdid[2] = SB_PARAM_SET_ALGO_PARAMS;
			if (tfa->verbose)
				pr_debug("P-ID for SetAlgoParams modified!\n");
		} else if (new_msg->MessageType == 2) {
			cmdid[2] = SB_PARAM_SET_MBDRC;
			if (tfa->verbose)
				pr_debug("P-ID for SetMBDrc modified!\n");
		}
	}

	/*
	 * +sizeof(struct tfa_partial_msg_block) will allow to fit one
	 * additonnal partial block If the partial update goes over the len of
	 * a regular message ,we can safely write our block and check afterward
	 * that we are over the size of a usual update
	 */
	if (enable_partial_update) {
		partial = kmem_cache_alloc(tfa->cachep, GFP_KERNEL);
		if (!partial)
			pr_debug("Partial update memory error - Disabling\n");
	}

	if (partial) {
		uint8_t offset = 0, i = 0;
		uint16_t *change;
		uint8_t *n = new_msg->ParameterData;
		uint8_t *o = old_msg->ParameterData;
		uint8_t *p = partial;
		uint8_t* trim = partial;

		/* set dspFiltersReset */
		*p++ = 0x02;
		*p++ = 0x00;
		*p++ = 0x00;

		while ((o < (old_msg->ParameterData + len)) &&
		      (p < (partial + len - 3))) {
			if ((offset == 0xff) ||
			    (memcmp(n, o, 3 * sizeof(uint8_t)))) {
				*p++ = offset;
				change = (uint16_t*) p;
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
			pr_debug("Using partial update: %d -> %d bytes\n", len , (int)(trim-partial+3));

			/* Add the termination marker */
			memset(trim, 0x00, 3);
			trim += 3;

			/* Signal This will be a partial update */
			cmdid[2] |= BIT(6);
			buf = (char*) partial;
			len = (int)(trim - partial);
		} else {
			pr_debug("Partial too big - use regular update\n");
		}
	}

	if (use_partial_coeff) {
		err = dsp_partial_coefficients(tfa, old_msg->ParameterData, new_msg->ParameterData);
	} else if (len) {
		uint8_t *buffer;

		if (tfa->verbose)
			pr_debug("Command-ID used: 0x%02x%02x%02x \n", cmdid[0], cmdid[1], cmdid[2]);

		buffer = kmem_cache_alloc(tfa->cachep, GFP_KERNEL);
		if (buffer == NULL) {
			err = Tfa98xx_Error_Fail;
		} else {
			memcpy(&buffer[0], cmdid, 3);
			memcpy(&buffer[3], buf, len);
			err = dsp_msg(tfa, 3 + len, (char *)buffer);
			kmem_cache_free(tfa->cachep, buffer);
		}
	}

	if (partial)
		kmem_cache_free(tfa->cachep, partial);

	return err;
}

static enum Tfa98xx_Error tfaContWriteVstepMax2(struct tfa_device *tfa, nxpTfaVolumeStepMax2File_t *vp, int vstep_idx, int vstep_msg_idx)
{
	enum Tfa98xx_Error err = Tfa98xx_Error_Ok;
	struct nxpTfaVolumeStepRegisterInfo *regInfo = NULL;
	struct nxpTfaVolumeStepMessageInfo *msgInfo = NULL, *p_msgInfo = NULL;
	nxpTfaBitfield_t bitF;
	int i, nrMessages, enp = tfa->partial_enable;

	if (vstep_idx >= vp->NrOfVsteps) {
		pr_debug("Volumestep %d is not available \n", vstep_idx);
		return Tfa98xx_Error_Bad_Parameter;
	}

	if (tfa->p_regInfo == NULL) {
		if (tfa->verbose)
			pr_debug("Inital vstep write\n");
		enp = 0;
 	}

	regInfo = tfaContGetRegForVstep(vp, vstep_idx);

	msgInfo = tfaContGetmsgInfoFromReg(regInfo);
	nrMessages = msgInfo->NrOfMessages;

	if (enp) {
		p_msgInfo = tfaContGetmsgInfoFromReg(tfa->p_regInfo);
		if (nrMessages != p_msgInfo->NrOfMessages) {
			pr_debug("Message different - Disable partial update\n");
			enp = 0;
		}
	}

	for (i = 0; i < nrMessages; i++) {
		/* Messagetype(3) is Smartstudio Info! Dont send this! */
		if(msgInfo->MessageType == 3) {
			/* MessageLength is in bytes */
			msgInfo = tfaContGetNextmsgInfo(msgInfo);
			if(enp)
				p_msgInfo = tfaContGetNextmsgInfo(p_msgInfo);
			continue;
		}

		/* If no vstepMsgIndex is passed on, all message needs to be send */
		if ((vstep_msg_idx >= TFA_MAX_VSTEP_MSG_MARKER) || (vstep_msg_idx == i)) {
			err = tfaContWriteVstepMax2_One(tfa, msgInfo, p_msgInfo, enp);
			if (err != Tfa98xx_Error_Ok) {
				/*
				 * Force a full update for the next write
				 * As the current status of the DSP is unknown
				 */
				tfa->p_regInfo = NULL;
				return err;
			}
		}

		msgInfo = tfaContGetNextmsgInfo(msgInfo);
		if(enp)
			p_msgInfo = tfaContGetNextmsgInfo(p_msgInfo);
	}

	tfa->p_regInfo = regInfo;

	for(i=0; i<regInfo->NrOfRegisters*2; i++) {
		/* Byte swap the datasheetname */
		bitF.field = (uint16_t)(regInfo->registerInfo[i]>>8) | (regInfo->registerInfo[i]<<8);
		i++;
		bitF.value = (uint16_t)regInfo->registerInfo[i]>>8;
		err = tfaRunWriteBitfield(tfa , bitF);
		if (err != Tfa98xx_Error_Ok)
			return err;
	}

	/* Save the current vstep */
	tfa_dev_set_swvstep(tfa, (unsigned short)vstep_idx);

	return err;
}

/*
 * Write DRC message to the dsp
 * If needed modify the cmd-id
 */

enum Tfa98xx_Error tfaContWriteDrcFile(struct tfa_device *tfa, int size, uint8_t data[])
{
	enum Tfa98xx_Error err = Tfa98xx_Error_Ok;
	uint8_t *msg = NULL;

	msg = kmem_cache_alloc(tfa->cachep, GFP_KERNEL);
	if (msg == NULL)
		return Tfa98xx_Error_Fail;
	memcpy(msg, data, size);

	if (TFA_GET_BF(tfa, SBSL) == 0) {
		/* Only do this when not set already */
		if (msg[2] != SB_PARAM_SET_MBDRC) {
			msg[2] = SB_PARAM_SET_MBDRC;

			if (tfa->verbose) {
				pr_debug("P-ID for SetMBDrc modified!: ");
				pr_debug("Command-ID used: 0x%02x%02x%02x \n",
				         msg[0], msg[1], msg[2]);
			}
		}
	}

	/* Send cmdId + payload to dsp */
	err = dsp_msg(tfa, size, (const char *)msg);

	kmem_cache_free(tfa->cachep, msg);

	return err;
}


/*
 * write a parameter file to the device
 * The VstepIndex and VstepMsgIndex are only used to write a specific msg from the vstep file.
 */
enum Tfa98xx_Error tfaContWriteFile(struct tfa_device *tfa,  nxpTfaFileDsc_t *file, int vstep_idx, int vstep_msg_idx)
{
	enum Tfa98xx_Error err = Tfa98xx_Error_Ok;
	nxpTfaHeader_t *hdr = (nxpTfaHeader_t *)file->data;
	nxpTfaHeaderType_t type;
	int size;

	if (tfa->verbose) {
		tfaContShowHeader(hdr);
	}

	type = (nxpTfaHeaderType_t) hdr->id;

	switch (type) {
	case msgHdr: /* generic DSP message */
		size = hdr->size - sizeof(nxpTfaMsgFile_t);
		err = dsp_msg(tfa, size, (const char *)((nxpTfaMsgFile_t *)hdr)->data);
		break;
	case volstepHdr:
		if (tfa->tfa_family == 2) {
			err = tfaContWriteVstepMax2(tfa, (nxpTfaVolumeStepMax2File_t *)hdr, vstep_idx, vstep_msg_idx);
		} else {
			err = tfaContWriteVstep(tfa, (nxpTfaVolumeStep2File_t *)hdr, vstep_idx);
		}

		break;
	case speakerHdr:
		if (tfa->tfa_family == 2) {
			/* Remove header and xml_id */
			size = hdr->size - sizeof(struct nxpTfaSpkHeader) - sizeof(struct nxpTfaFWVer);

			err = dsp_msg(tfa, size,
					(const char *)(((nxpTfaSpeakerFile_t *)hdr)->data + (sizeof(struct nxpTfaFWVer))));
		} else {
			size = hdr->size - sizeof(nxpTfaSpeakerFile_t);
			err = tfa98xx_dsp_write_speaker_parameters(tfa, size,
					(const unsigned char *)((nxpTfaSpeakerFile_t *)hdr)->data);
		}
		break;
	case presetHdr:
		size = hdr->size - sizeof(nxpTfaPreset_t);
		err = tfa98xx_dsp_write_preset(tfa, size, (const unsigned char *)((nxpTfaPreset_t *)hdr)->data);
		break;
	case equalizerHdr:
		err = tfa_cont_write_filterbank(tfa, ((nxpTfaEqualizerFile_t *)hdr)->filter);
		break;
	case patchHdr:
		size = hdr->size - sizeof(nxpTfaPatch_t );
		err = tfa_dsp_patch(tfa,  size, (const unsigned char *) ((nxpTfaPatch_t *)hdr)->data);
		break;
	case configHdr:
		size = hdr->size - sizeof(nxpTfaConfig_t);
		err = tfa98xx_dsp_write_config(tfa, size, (const unsigned char *)((nxpTfaConfig_t *)hdr)->data);
		break;
	case drcHdr:
		if(hdr->version[0] == NXPTFA_DR3_VERSION) {
			/* Size is total size - hdrsize(36) - xmlversion(3) */
			size = hdr->size - sizeof(nxpTfaDrc2_t);
			err = tfaContWriteDrcFile(tfa, size, ((nxpTfaDrc2_t *)hdr)->data);
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

			size = 381; /* fixed size for first block */


			err = tfa98xx_dsp_write_drc(tfa, size, ((const unsigned char *)((nxpTfaDrc_t *)hdr)->data+381));
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
static nxpTfaDescPtr_t *tfa_cnt_get_dsc(nxpTfaContainer_t *cnt, nxpTfaDescriptorType_t type, int dev_idx)
{
	nxpTfaDeviceList_t *dev = tfaContDevice(cnt, dev_idx);
	nxpTfaDescPtr_t *_this;
	int i;

	if ( !dev ) {
		return NULL;
	}
	/* process the list until a the type is encountered */
	for(i=0;i<dev->length;i++) {
		if ( dev->list[i].type == (uint32_t)type ) {
			_this = (nxpTfaDescPtr_t *)(dev->list[i].offset+(uint8_t *)cnt);
			return _this;
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
	if ( !patchdsc ) /* no patch for this device, assume non-i2c */
		return 0;
	patchdsc += 2; /* first the filename dsc and filesize, so skip them */
	patchfile = (nxpTfaPatch_t *)patchdsc;

	patchheader = patchfile->data;

	checkaddress = (patchheader[1] << 8) + patchheader[2];
	checkvalue =
		(patchheader[3] << 16) + (patchheader[4] << 8) + patchheader[5];

	devid = patchheader[0];

	if(checkaddress == 0xFFFF && checkvalue != 0xFFFFFF && checkvalue != 0) {
		devid = patchheader[5]<<8 | patchheader[0]; /* full revid */
	}

	return devid;
}

/**
 * get the firmware version from the patch in this devicelist
 */
int tfa_cnt_get_patch_version(struct tfa_device *tfa)
{
	nxpTfaPatch_t *patchfile;
	nxpTfaDescPtr_t *patchdsc;
	uint8_t *data;
	int size, version;

	if (tfa->cnt == NULL)
		return -1;

	patchdsc = tfa_cnt_get_dsc(tfa->cnt, dscPatch, tfa->dev_idx);
	patchdsc += 2; /* first the filename dsc and filesize, so skip them */
	patchfile = (nxpTfaPatch_t *)patchdsc;

	size = patchfile->hdr.size - sizeof(nxpTfaPatch_t);
	data = patchfile->data;

	version = (data[size-3] << 16) + (data[size-2] << 8) + data[size-1];

	return version;
}


/*
 * get the slave for the device if it exists
 */
enum Tfa98xx_Error tfaContGetSlave(struct tfa_device *tfa, uint8_t *slave_addr)
{
	nxpTfaDeviceList_t *dev = NULL;

	/* Make sure the cnt file is loaded */
	if (tfa->cnt != NULL) {
		dev = tfaContDevice(tfa->cnt, tfa->dev_idx);
	}

	if (dev == NULL) {
		/* Check if slave argument is used! */
		if(gslave_address == 0) {
			return Tfa98xx_Error_Bad_Parameter;
		} else {
			*slave_addr = gslave_address;
			return Tfa98xx_Error_Ok;
		}
	}

	*slave_addr = dev->dev;
	return Tfa98xx_Error_Ok;
}

/* If no container file is given, we can always have used the slave argument */
void tfaContSetSlave(uint8_t slave_addr)
{
	gslave_address = slave_addr;
}

/*
 * lookup slave and return device index
 */
int tfa_cont_get_idx(struct tfa_device *tfa)
{
	nxpTfaDeviceList_t *dev = NULL;
	int i;

	for (i=0; i<tfa->cnt->ndev; i++) {
		dev = tfaContDevice(tfa->cnt, i);
		if (dev->dev == tfa->slave_address)
			break;

	}
	if (i == tfa->cnt->ndev)
		return -1;

	return i;
}

/*
 * write a bit field
 */
enum Tfa98xx_Error tfaRunWriteBitfield(struct tfa_device *tfa,  nxpTfaBitfield_t bf)
{
	enum Tfa98xx_Error error;
        uint16_t value;
	union {
		uint16_t field;
		nxpTfaBfEnum_t Enum;
	} bfUni;

	value=bf.value;
	bfUni.field = bf.field;
#ifdef TFA_DEBUG
	if (tfa->verbose)
		pr_debug("bitfield: %s=0x%x (0x%x[%d..%d]=0x%x)\n", tfaContBfName(bfUni.field, tfa->rev), value,
			bfUni.Enum.address, bfUni.Enum.pos, bfUni.Enum.pos+bfUni.Enum.len, value);
#endif
        error = tfa_set_bf(tfa, bfUni.field, value);

	return error;
}

/*
 * read a bit field
 */
enum Tfa98xx_Error tfaRunReadBitfield(struct tfa_device *tfa,  nxpTfaBitfield_t *bf)
{
	enum Tfa98xx_Error error;
	union {
		uint16_t field;
		nxpTfaBfEnum_t Enum;
	} bfUni;
	uint16_t regvalue, msk;

	bfUni.field = bf->field;

	error = reg_read(tfa, (unsigned char)(bfUni.Enum.address), &regvalue);
	if (error) return error;

	msk = ((1<<(bfUni.Enum.len+1))-1)<<bfUni.Enum.pos;

	regvalue &= msk;
	bf->value = regvalue>>bfUni.Enum.pos;

	return error;
}

/*
 dsp mem direct write
 */
static enum Tfa98xx_Error tfaRunWriteDspMem(struct tfa_device *tfa, nxpTfaDspMem_t *cfmem)
{
	enum Tfa98xx_Error error = Tfa98xx_Error_Ok;
	int i;

	for(i=0;i<cfmem->size;i++) {
		if (tfa->verbose)
			pr_debug("dsp mem (%d): 0x%02x=0x%04x\n", cfmem->type, cfmem->address, cfmem->words[i]);

		error = mem_write(tfa, cfmem->address++, cfmem->words[i], cfmem->type);
		if (error) return error;
	}

	return error;
}

/*
 * write filter payload to DSP
 *  note that the data is in an aligned union for all filter variants
 *  the aa data is used but it's the same for all of them
 */
static enum Tfa98xx_Error tfaRunWriteFilter(struct tfa_device *tfa, nxpTfaContBiquad_t *bq)
{
	enum Tfa98xx_Error error = Tfa98xx_Error_Ok;
	enum Tfa98xx_DMEM dmem;
	uint16_t address;
	uint8_t data[3*3+sizeof(bq->aa.bytes)];
	int i, channel=0, runs=1;
	int8_t saved_index=bq->aa.index; /* This is used to set back the index */

	/* Channel=1 is primary, Channel=2 is secondary*/
	if (bq->aa.index > 100) {
		bq->aa.index -= 100;
		channel = 2;
	} else if (bq->aa.index > 50) {
		bq->aa.index -= 50;
		channel = 1;
	} else if((tfa->rev & 0xff) == 0x88) {
		runs=2;
	}

	if (tfa->verbose) {
		if(channel == 2)
			pr_debug("filter[%d,S]", bq->aa.index);
		else if(channel == 1)
			pr_debug("filter[%d,P]", bq->aa.index);
		else
			pr_debug("filter[%d]", bq->aa.index);
	}

	for(i=0; i<runs; i++) {
		if(runs==2)
			channel++;

		/* get the target address for the filter on this device */
		dmem = tfa98xx_filter_mem(tfa, bq->aa.index, &address, channel);
		if(dmem == Tfa98xx_DMEM_ERR) {
			if (tfa->verbose) {
				pr_debug("Warning: XFilter settings are applied via msg file (ini filter[x] format is skipped).\n");
			}
			/* Dont exit with an error here, We could continue without problems */
			return Tfa98xx_Error_Ok;
		}

		/* send a DSP memory message that targets the devices specific memory for the filter
		 * msg params: which_mem, start_offset, num_words
		 */
		memset(data, 0, 3*3);
		data[2] = dmem; /* output[0] = which_mem */
		data[4] = address >> 8; /* output[1] = start_offset */
		data[5] = address & 0xff;
		data[8] = sizeof(bq->aa.bytes)/3; /*output[2] = num_words */
		memcpy( &data[9], bq->aa.bytes, sizeof(bq->aa.bytes)); /* payload */

		if(tfa->tfa_family == 2) {
			error = tfa_dsp_cmd_id_write(tfa, MODULE_FRAMEWORK, FW_PAR_ID_SET_MEMORY, sizeof(data), data);
		} else {
			error = tfa_dsp_cmd_id_write(tfa, MODULE_FRAMEWORK, 4 /* param */ , sizeof(data), data);
		}
	}

#ifdef TFA_DEBUG
	if (tfa->verbose) {
		if (bq->aa.index==13) {
			pr_debug("=%d,%.0f,%.2f \n",
				bq->in.type, bq->in.cutOffFreq, bq->in.leakage);
		} else if(bq->aa.index >= 10 && bq->aa.index <= 12) {
			pr_debug("=%d,%.0f,%.1f,%.1f \n", bq->aa.type,
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
static enum Tfa98xx_Error tfaRunWriteRegister(struct tfa_device *tfa, nxpTfaRegpatch_t *reg)
{
	enum Tfa98xx_Error error;
	uint16_t value, newvalue;

	if (tfa->verbose)
		pr_debug("register: 0x%02x=0x%04x (msk=0x%04x)\n", reg->address, reg->value, reg->mask);

	error = reg_read(tfa, reg->address, &value);
	if (error) return error;

	value &= ~reg->mask;
	newvalue = reg->value & reg->mask;

	value |= newvalue;
	error = reg_write(tfa,  reg->address, value);

	return error;

}


enum Tfa98xx_Error tfaContWriteRegsDev(struct tfa_device *tfa)
{
	nxpTfaDeviceList_t *dev = tfaContDevice(tfa->cnt, tfa->dev_idx);
	nxpTfaBitfield_t *bitF;
	int i;
	enum Tfa98xx_Error err = Tfa98xx_Error_Ok;

	if ( !dev ) {
		return Tfa98xx_Error_Bad_Parameter;
	}

	/* process the list until a patch, file of profile is encountered */
	for(i=0;i<dev->length;i++) {
		if ( dev->list[i].type == dscPatch ||
			  dev->list[i].type ==dscFile  ||
			  dev->list[i].type ==dscProfile ) break;

		if  ( dev->list[i].type == dscBitfield) {
			bitF = (nxpTfaBitfield_t *)( dev->list[i].offset+(uint8_t *)tfa->cnt);
			err = tfaRunWriteBitfield(tfa , *bitF);
		}
		if  ( dev->list[i].type == dscRegister ) {
			err = tfaRunWriteRegister(tfa, (nxpTfaRegpatch_t *)( dev->list[i].offset+(char*)tfa->cnt));
		}

		if ( err ) break;
	}

	return err;
}


enum Tfa98xx_Error tfaContWriteRegsProf(struct tfa_device *tfa, int prof_idx)
{
	nxpTfaProfileList_t *prof = tfaContGetDevProfList(tfa->cnt, tfa->dev_idx, prof_idx);
	nxpTfaBitfield_t *bitf;
	unsigned int i;
	enum Tfa98xx_Error err = Tfa98xx_Error_Ok;

	if ( !prof ) {
		return Tfa98xx_Error_Bad_Parameter;
	}

	if (tfa->verbose)
		pr_debug("----- profile: %s (%d) -----\n", tfaContGetString(tfa->cnt, &prof->name), prof_idx);

	/* process the list until the end of the profile or the default section */
	for(i=0;i<prof->length;i++) {
		/* We only want to write the values before the default section when we switch profile */
		if(prof->list[i].type == dscDefault)
			break;

		if  ( prof->list[i].type == dscBitfield) {
			bitf = (nxpTfaBitfield_t *)( prof->list[i].offset+(uint8_t *)tfa->cnt);
			err = tfaRunWriteBitfield(tfa , *bitf);
		}
		if  ( prof->list[i].type == dscRegister ) {
			err = tfaRunWriteRegister(tfa, (nxpTfaRegpatch_t *)( prof->list[i].offset+(char*)tfa->cnt));
		}
		if ( err ) break;
	}
	return err;
}


enum Tfa98xx_Error tfaContWritePatch(struct tfa_device *tfa)
{
	enum Tfa98xx_Error err = Tfa98xx_Error_Ok;
	nxpTfaDeviceList_t *dev = tfaContDevice(tfa->cnt, tfa->dev_idx);
	nxpTfaFileDsc_t *file;
	nxpTfaPatch_t *patchfile;
	int size, i;

	if ( !dev ) {
		return Tfa98xx_Error_Bad_Parameter;
	}
	/* process the list until a patch  is encountered */
	for(i=0;i<dev->length;i++) {
		if ( dev->list[i].type == dscPatch ) {
			file = (nxpTfaFileDsc_t *)(dev->list[i].offset+(uint8_t *)tfa->cnt);
			patchfile =(nxpTfaPatch_t *)&file->data;
			if (tfa->verbose) tfaContShowHeader(&patchfile->hdr);
			size = patchfile->hdr.size - sizeof(nxpTfaPatch_t );
			err = tfa_dsp_patch(tfa, size, (const unsigned char *) patchfile->data);
			if ( err ) return err;
		}
	}

	return Tfa98xx_Error_Ok;
}

/**
 * Create a buffer which can be used to send to the dsp.
 */
static void create_dsp_buffer_msg(struct tfa_device *tfa, nxpTfaMsg_t *msg, char *buffer, int *size)
{
        int i, nr = 0;

	(void)tfa;

        /* Copy cmdId. Remember that the cmdId is reversed */
        buffer[nr++] = msg->cmdId[2];
        buffer[nr++] = msg->cmdId[1];
        buffer[nr++] = msg->cmdId[0];

        /* Copy the data to the buffer */
        for(i=0; i<msg->msg_size; i++) {
                buffer[nr++] = (uint8_t) ((msg->data[i] >> 16) & 0xffff);
                buffer[nr++] = (uint8_t) ((msg->data[i] >> 8) & 0xff);
                buffer[nr++] = (uint8_t) (msg->data[i] & 0xff);
        }

        *size = nr;
}


enum Tfa98xx_Error tfaContWriteFiles(struct tfa_device *tfa)
{
	nxpTfaDeviceList_t *dev = tfaContDevice(tfa->cnt, tfa->dev_idx);
	nxpTfaFileDsc_t *file;
	enum Tfa98xx_Error err = Tfa98xx_Error_Ok;
	char buffer[(MEMTRACK_MAX_WORDS * 3) + 3] = {0};
	int i, size = 0;

	if ( !dev ) {
		return Tfa98xx_Error_Bad_Parameter;
	}
	/* process the list and write all files  */
	for(i=0;i<dev->length;i++) {
		if ( dev->list[i].type == dscFile ) {
			file = (nxpTfaFileDsc_t *)(dev->list[i].offset+(uint8_t *)tfa->cnt);
			if ( tfaContWriteFile(tfa,  file, 0 , TFA_MAX_VSTEP_MSG_MARKER) ){
				return Tfa98xx_Error_Bad_Parameter;
			}
		}

		if  ( dev->list[i].type == dscSetInputSelect ||
		      dev->list[i].type == dscSetOutputSelect ||
		      dev->list[i].type == dscSetProgramConfig ||
		      dev->list[i].type == dscSetLagW ||
		      dev->list[i].type == dscSetGains ||
		      dev->list[i].type == dscSetvBatFactors ||
		      dev->list[i].type == dscSetSensesCal ||
		      dev->list[i].type == dscSetSensesDelay ||
		      dev->list[i].type == dscSetMBDrc ||
		      dev->list[i].type == dscSetFwkUseCase ||
		      dev->list[i].type == dscSetVddpConfig ) {
			create_dsp_buffer_msg(tfa, (nxpTfaMsg_t *)
			                      ( dev->list[i].offset+(char*)tfa->cnt), buffer, &size);
			if (tfa->verbose) {
				pr_debug("command: %s=0x%02x%02x%02x \n",
				tfaContGetCommandString(dev->list[i].type),
				(unsigned char)buffer[0], (unsigned char)buffer[1], (unsigned char)buffer[2]);
			}

			err = dsp_msg(tfa, size, buffer);
		}

		if  ( dev->list[i].type == dscCmd ) {
			size = *(uint16_t *)(dev->list[i].offset+(char*)tfa->cnt);

			err = dsp_msg(tfa, size, dev->list[i].offset+2+(char*)tfa->cnt);
			if (tfa->verbose) {
				const char *cmd_id = dev->list[i].offset+2+(char*)tfa->cnt;
				pr_debug("Writing cmd=0x%02x%02x%02x \n", (uint8_t)cmd_id[0], (uint8_t)cmd_id[1], (uint8_t)cmd_id[2]);
			}
		}
		if (err != Tfa98xx_Error_Ok)
			break;

		if  ( dev->list[i].type == dscCfMem ) {
			err = tfaRunWriteDspMem(tfa, (nxpTfaDspMem_t *)(dev->list[i].offset+(uint8_t *)tfa->cnt));
		}

		if (err != Tfa98xx_Error_Ok)
			break;
	}

	return err;
}

/*
 *  write all  param files in the profilelist to the target
 *   this is used during startup when maybe ACS is set
 */
enum Tfa98xx_Error tfaContWriteFilesProf(struct tfa_device *tfa, int prof_idx, int vstep_idx)
{
	enum Tfa98xx_Error err = Tfa98xx_Error_Ok;
	nxpTfaProfileList_t *prof = tfaContGetDevProfList(tfa->cnt, tfa->dev_idx, prof_idx);
	char buffer[(MEMTRACK_MAX_WORDS * 3) + 3] = {0};
	unsigned int i;
	nxpTfaFileDsc_t *file;
	nxpTfaPatch_t *patchfile;
	int size;

	if ( !prof ) {
		return Tfa98xx_Error_Bad_Parameter;
	}

	/* process the list and write all files  */
	for(i=0;i<prof->length;i++) {
		switch (prof->list[i].type) {
			case dscFile:
				file = (nxpTfaFileDsc_t *)(prof->list[i].offset+(uint8_t *)tfa->cnt);
				err = tfaContWriteFile(tfa,  file, vstep_idx, TFA_MAX_VSTEP_MSG_MARKER);
				break;
			case dscPatch:
				file = (nxpTfaFileDsc_t *)(prof->list[i].offset+(uint8_t *)tfa->cnt);
				patchfile =(nxpTfaPatch_t *)&file->data;
				if (tfa->verbose) tfaContShowHeader(&patchfile->hdr);
				size = patchfile->hdr.size - sizeof(nxpTfaPatch_t );
				err = tfa_dsp_patch(tfa,  size, (const unsigned char *) patchfile->data);
				break;
			case dscCfMem:
				err = tfaRunWriteDspMem(tfa, (nxpTfaDspMem_t *)(prof->list[i].offset+(uint8_t *)tfa->cnt));
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
			case dscSetFwkUseCase:
			case dscSetVddpConfig:
					create_dsp_buffer_msg(tfa, (nxpTfaMsg_t *)
			                     (prof->list[i].offset+(uint8_t *)tfa->cnt), buffer, &size);
					if (tfa->verbose) {
						pr_debug("command: %s=0x%02x%02x%02x \n",
						tfaContGetCommandString(prof->list[i].type),
						(unsigned char)buffer[0], (unsigned char)buffer[1], (unsigned char)buffer[2]);
					}

					err = dsp_msg(tfa, size, buffer);
					break;
			default:
				/* ignore any other type */
				break;
		}
	}

	return err;
}

static enum Tfa98xx_Error tfaContWriteItem(struct tfa_device *tfa, nxpTfaDescPtr_t * dsc)
{
	enum Tfa98xx_Error err = Tfa98xx_Error_Ok;
	nxpTfaRegpatch_t *reg;
	nxpTfaMode_t *cas;
	nxpTfaBitfield_t *bitf;


	if(tfa->ext_dsp == 0 && !(dsc->type == dscBitfield || dsc->type == dscRegister)) {
		return Tfa98xx_Error_Ok;
	}

	switch (dsc->type) {
        case dscDefault:
	case dscDevice:
	case dscProfile:
		break;
	case dscRegister:
		reg = (nxpTfaRegpatch_t *)(dsc->offset+(uint8_t *)tfa->cnt);
		return tfaRunWriteRegister(tfa, reg);

		break;
	case dscString:
		pr_debug(";string: %s\n", tfaContGetString(tfa->cnt, dsc));
		break;
	case dscFile:
	case dscPatch:
		break;
	case dscMode:
		cas = (nxpTfaMode_t *)(dsc->offset+(uint8_t *)tfa->cnt);
		if(cas->value == Tfa98xx_Mode_RCV)
			tfa98xx_select_mode(tfa, Tfa98xx_Mode_RCV);
		else
			tfa98xx_select_mode(tfa, Tfa98xx_Mode_Normal);
		break;
	case dscCfMem:
		err = tfaRunWriteDspMem(tfa, (nxpTfaDspMem_t *)(dsc->offset+(uint8_t *)tfa->cnt));
		break;
	case dscBitfield:
		bitf = (nxpTfaBitfield_t *)(dsc->offset+(uint8_t *)tfa->cnt);
		return tfaRunWriteBitfield(tfa , *bitf);
		break;
	case dscFilter:
		return tfaRunWriteFilter(tfa, (nxpTfaContBiquad_t *)(dsc->offset+(uint8_t *)tfa->cnt));
		break;
	}

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

enum Tfa98xx_Error tfa_write_filters(struct tfa_device *tfa, int prof_idx)
{
	enum Tfa98xx_Error err = Tfa98xx_Error_Ok;
	nxpTfaProfileList_t *prof = tfaContGetDevProfList(tfa->cnt, tfa->dev_idx, prof_idx);
	unsigned int i;
	int status;

	if ( !prof ) {
		return Tfa98xx_Error_Bad_Parameter;
	}

	if (tfa->verbose) {
		pr_debug("----- profile: %s (%d) -----\n", tfaContGetString(tfa->cnt, &prof->name), prof_idx);
		pr_debug("Waiting for CLKS... \n");
	}

	for(i=10; i>0; i--) {
		err = tfa98xx_dsp_system_stable(tfa, &status);
		if(status)
			break;
		else
			msleep_interruptible(10);
	}

	if(i==0) {
		if (tfa->verbose)
			pr_err("Unable to write filters, CLKS=0 \n");

		return Tfa98xx_Error_StateTimedOut;
	}

	/* process the list until the end of the profile or the default section */
	for(i=0;i<prof->length;i++) {
		if  ( prof->list[i].type == dscFilter ) {
			if (tfaContWriteItem(tfa, &prof->list[i]) != Tfa98xx_Error_Ok)
				return Tfa98xx_Error_Bad_Parameter;
		}
	}

	return err;
}

unsigned int tfa98xx_get_profile_sr(struct tfa_device *tfa, unsigned int prof_idx)
{
	nxpTfaBitfield_t *bitf;
	unsigned int i;
	nxpTfaDeviceList_t *dev;
	nxpTfaProfileList_t *prof;
	int fs_profile = -1;

	dev = tfaContDevice(tfa->cnt, tfa->dev_idx);
	if (!dev)
		return 0;

	prof = tfaContGetDevProfList(tfa->cnt, tfa->dev_idx, prof_idx);
	if (!prof)
		return 0;

	/* Check profile fields first */
	for(i = 0; i < prof->length; i++) {
		if(prof->list[i].type == dscDefault)
			break;

		/* check for profile settingd (AUDFS) */
		if (prof->list[i].type == dscBitfield) {
			bitf = (nxpTfaBitfield_t *)(prof->list[i].offset+(uint8_t *)tfa->cnt);
			if (bitf->field == TFA_FAM(tfa, AUDFS)) {
				fs_profile = bitf->value;
				break;
			}
		}
	}

	if (tfa->verbose)
		pr_debug("%s - profile fs: 0x%x = %dHz (%d - %d)\n",
		         __FUNCTION__, fs_profile,
		         tfa98xx_sr_from_field(fs_profile),
		         tfa->dev_idx, prof_idx);

	if (fs_profile != -1)
		return tfa98xx_sr_from_field(fs_profile);

	/* Check for container default setting */
	/* process the list until a patch, file of profile is encountered */
	for(i = 0; i < dev->length; i++) {
		if (dev->list[i].type == dscPatch ||
			  dev->list[i].type ==dscFile  ||
			  dev->list[i].type ==dscProfile)
			break;

		if  (dev->list[i].type == dscBitfield) {
			bitf = (nxpTfaBitfield_t *)( dev->list[i].offset+(uint8_t *)tfa->cnt);
			if (bitf->field == TFA_FAM(tfa, AUDFS)) {
				fs_profile = bitf->value;
				break;
			}
		}
		/* Ignore register case */
	}

	if (tfa->verbose)
		pr_debug("%s - default fs: 0x%x = %dHz (%d - %d)\n",
		         __FUNCTION__, fs_profile,
		         tfa98xx_sr_from_field(fs_profile),
			tfa->dev_idx, prof_idx);

	if (fs_profile != -1)
		return tfa98xx_sr_from_field(fs_profile);

	return 48000; /* default of HW */
}

static enum Tfa98xx_Error get_sample_rate_info(struct tfa_device *tfa, nxpTfaProfileList_t *prof, nxpTfaProfileList_t *previous_prof, int fs_previous_profile)
{
	enum Tfa98xx_Error err = Tfa98xx_Error_Ok;
	nxpTfaBitfield_t *bitf;
	unsigned int i;
	int fs_default_profile=8;	/* default is 48kHz */
	int fs_next_profile=8;		/* default is 48kHz */


	/* ---------- default settings previous profile ---------- */
	for(i=0;i<previous_prof->length;i++) {
		/* Search for the default section */
		if(i == 0) {
			while(previous_prof->list[i].type != dscDefault && i < previous_prof->length) {
				i++;
			}
			i++;
		}

		/* Only if we found the default section search for AUDFS */
		if(i < previous_prof->length) {
			if ( previous_prof->list[i].type == dscBitfield ) {
				bitf = (nxpTfaBitfield_t *)(previous_prof->list[i].offset+(uint8_t *)tfa->cnt);
				if(bitf->field == TFA_FAM(tfa, AUDFS)) {
					fs_default_profile = bitf->value;
					break;
				}
			}
		}
	}

	/* ---------- settings next profile ---------- */
	for(i=0;i<prof->length;i++) {
		/* We only want to write the values before the default section */
		if(prof->list[i].type == dscDefault)
			break;
		/* search for AUDFS */
		if ( prof->list[i].type == dscBitfield ) {
			bitf = (nxpTfaBitfield_t *)(prof->list[i].offset+(uint8_t *)tfa->cnt);
			if(bitf->field == TFA_FAM(tfa, AUDFS)) {
				fs_next_profile = bitf->value;
				break;
			}
		}
	}

	/* Enable if needed for debugging!
	if (tfa->verbose) {
		pr_debug("sample rate from the previous profile: %d \n", fs_previous_profile);
		pr_debug("sample rate in the default section: %d \n", fs_default_profile);
		pr_debug("sample rate for the next profile: %d \n", fs_next_profile);
	}
	*/

	if(fs_next_profile != fs_default_profile) {
		if (tfa->verbose)
			pr_debug("Writing delay tables for AUDFS=%d \n", fs_next_profile);

		/* If the AUDFS from the next profile is not the same as
		 * the AUDFS from the default we need to write new delay tables
		 */
		err = tfa98xx_dsp_write_tables(tfa, fs_next_profile);
	} else if(fs_default_profile != fs_previous_profile) {
		if (tfa->verbose)
			pr_debug("Writing delay tables for AUDFS=%d \n", fs_default_profile);

		/* But if we do not have a new AUDFS in the next profile and
		 * the AUDFS from the default profile is not the same as the AUDFS
		 * from the previous profile we also need to write new delay tables
		 */
		err = tfa98xx_dsp_write_tables(tfa, fs_default_profile);
	}

	return err;
}

/*
 *  process all items in the profilelist
 *   NOTE an error return during processing will leave the device muted
 *
 */
enum Tfa98xx_Error tfaContWriteProfile(struct tfa_device *tfa, int prof_idx, int vstep_idx)
{
	enum Tfa98xx_Error err = Tfa98xx_Error_Ok;
	nxpTfaProfileList_t *prof = tfaContGetDevProfList(tfa->cnt, tfa->dev_idx, prof_idx);
	nxpTfaProfileList_t *previous_prof = tfaContGetDevProfList(tfa->cnt, tfa->dev_idx, tfa_dev_get_swprof(tfa));
	char buffer[(MEMTRACK_MAX_WORDS * 4) + 4] = {0};
	unsigned int i, k=0, j=0, tries=0;
	nxpTfaFileDsc_t *file;
	int size = 0, ready, fs_previous_profile = 8; /* default fs is 48kHz*/

	if ( !prof || !previous_prof ) {
		pr_err("Error trying to get the (previous) swprofile \n");
		return Tfa98xx_Error_Bad_Parameter;
	}

	if (tfa->verbose) {
		tfa98xx_trace_printk("device:%s profile:%s vstep:%d\n", tfaContDeviceName(tfa->cnt, tfa->dev_idx),
					tfaContProfileName(tfa->cnt, tfa->dev_idx, prof_idx), vstep_idx);
	}

	/* We only make a power cycle when the profiles are not in the same group */
	if (prof->group == previous_prof->group && prof->group != 0) {
		if (tfa->verbose) {
			pr_debug("The new profile (%s) is in the same group as the current profile (%s) \n",
				tfaContGetString(tfa->cnt, &prof->name), tfaContGetString(tfa->cnt, &previous_prof->name));
		}
	} else {
		/* mute */
		err = tfaRunMute(tfa);
		if (err) return err;

		/* Get current sample rate before we start switching */
		fs_previous_profile = TFA_GET_BF(tfa, AUDFS);

		/* clear SBSL to make sure we stay in initCF state */
		if (tfa->tfa_family == 2) {
			TFA_SET_BF_VOLATILE(tfa, SBSL, 0);
		}

		/* When we switch profile we first power down the subsystem
		 * This should only be done when we are in operating mode
		 */
		if (((tfa->tfa_family == 2) && (TFA_GET_BF(tfa, MANSTATE) >= 6)) || (tfa->tfa_family != 2)) {
			err = tfa98xx_powerdown(tfa, 1);
			if (err) return err;

			/* Wait until we are in PLL powerdown */
			do {
				err = tfa98xx_dsp_system_stable(tfa, &ready);
				if (!ready)
					break;
				else
					msleep_interruptible(10); /* wait 10ms to avoid busload */
				tries++;
			} while (tries <= 100);

			if (tries > 100) {
				pr_debug("Wait for PLL powerdown timed out!\n");
				return Tfa98xx_Error_StateTimedOut;
			}
		} else {
			pr_debug("No need to go to powerdown now \n");
		}
	}

	/* set all bitfield settings */
	/* First set all default settings */
	if (tfa->verbose) {
		pr_debug("---------- default settings profile: %s (%d) ---------- \n",
				tfaContGetString(tfa->cnt, &previous_prof->name), tfa_dev_get_swprof(tfa));
	}

	err = show_current_state(tfa);

	/* Loop profile length */
	for(i=0;i<previous_prof->length;i++) {
		/* Search for the default section */
		if(i == 0) {
			while(previous_prof->list[i].type != dscDefault && i < previous_prof->length) {
				i++;
			}
			i++;
		}

		/* Only if we found the default section try writing the items */
		if(i < previous_prof->length) {
			if ( tfaContWriteItem(tfa,  &previous_prof->list[i]) != Tfa98xx_Error_Ok )
				return Tfa98xx_Error_Bad_Parameter;
		}
	}

	if (tfa->verbose)
		pr_debug("---------- new settings profile: %s (%d) ---------- \n",
				tfaContGetString(tfa->cnt, &prof->name), prof_idx);

	/* set new settings */
	for(i=0;i<prof->length;i++) {
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
			case dscSetFwkUseCase:
			case dscSetVddpConfig:
			case dscCmd:
			case dscFilter:
			case dscDefault:
				/* When one of these files are found, we exit */
				i = prof->length;
				break;
			default:
				err = tfaContWriteItem(tfa, &prof->list[i]);
				if ( err != Tfa98xx_Error_Ok )
					return Tfa98xx_Error_Bad_Parameter;
				break;
		}
	}

	if (prof->group != previous_prof->group || prof->group == 0) {
		if (tfa->tfa_family == 2)
			TFA_SET_BF_VOLATILE(tfa, MANSCONF, 1);

		/* Leave powerdown state */
		err = tfa_cf_powerup(tfa);
		if (err) return err;

		err = show_current_state(tfa);

		if (tfa->tfa_family == 2) {
			/* Reset SBSL to 0 (workaround of enbl_powerswitch=0) */
			TFA_SET_BF_VOLATILE(tfa, SBSL, 0);
			/* Sending commands to DSP we need to make sure RST is 0 (otherwise we get no response)*/
			TFA_SET_BF(tfa, RST, 0);
		}
	}

	/* Check if there are sample rate changes */
	err = get_sample_rate_info(tfa, prof, previous_prof, fs_previous_profile);
	if (err) return err;


		/* Write files from previous profile (default section)
		 * Should only be used for the patch&trap patch (file)
		 */
	if(tfa->ext_dsp != 0) {
		if (tfa->tfa_family == 2) {
			for(i=0;i<previous_prof->length;i++) {
				/* Search for the default section */
				if(i == 0) {
					while(previous_prof->list[i].type != dscDefault && i < previous_prof->length) {
						i++;
					}
					i++;
				}

				/* Only if we found the default section try writing the file */
				if(i < previous_prof->length) {
					if(previous_prof->list[i].type == dscFile || previous_prof->list[i].type == dscPatch) {
						/* Only write this once */
						if ( tfa->verbose && k==0) {
							pr_debug("---------- files default profile: %s (%d) ---------- \n",
									tfaContGetString(tfa->cnt, &previous_prof->name), prof_idx);
							k++;
						}
						file = (nxpTfaFileDsc_t *)(previous_prof->list[i].offset+(uint8_t *)tfa->cnt);
						err = tfaContWriteFile(tfa,  file, vstep_idx, TFA_MAX_VSTEP_MSG_MARKER);
					}
				}
			}
		}

		if ( tfa->verbose) {
			pr_debug("---------- files new profile: %s (%d) ---------- \n",
					tfaContGetString(tfa->cnt, &prof->name), prof_idx);
		}
	}

		/* write everything until end or the default section starts
		 * Start where we currenly left */
		for(i=j;i<prof->length;i++) {
			/* We only want to write the values before the default section when we switch profile */

		if(prof->list[i].type == dscDefault) {
				break;
		}

		switch (prof->list[i].type) {
			case dscFile:
			case dscPatch:
				/* For tiberius stereo 1 device does not have a dsp! */
				if(tfa->ext_dsp != 0){
					file = (nxpTfaFileDsc_t *)(prof->list[i].offset+(uint8_t *)tfa->cnt);
					err = tfaContWriteFile(tfa, file, vstep_idx, TFA_MAX_VSTEP_MSG_MARKER);
				}
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
			case dscSetFwkUseCase:
			case dscSetVddpConfig:
				/* For tiberius stereo 1 device does not have a dsp! */
				if(tfa->ext_dsp != 0) {
					create_dsp_buffer_msg(tfa, (nxpTfaMsg_t *)
						( prof->list[i].offset+(char*)tfa->cnt), buffer, &size);
					err = dsp_msg(tfa, size, buffer);

					if (tfa->verbose) {
						pr_debug("command: %s=0x%02x%02x%02x \n",
							tfaContGetCommandString(prof->list[i].type),
						(unsigned char)buffer[0], (unsigned char)buffer[1], (unsigned char)buffer[2]);
					}
				}
				break;
			case dscCmd:
				/* For tiberius stereo 1 device does not have a dsp! */
				if(tfa->ext_dsp != 0) {
					size = *(uint16_t *)(prof->list[i].offset+(char*)tfa->cnt);
					err = dsp_msg(tfa, size, prof->list[i].offset+2+(char*)tfa->cnt);
					if (tfa->verbose) {
						const char *cmd_id = prof->list[i].offset+2+(char*)tfa->cnt;
						pr_debug("Writing cmd=0x%02x%02x%02x \n", (uint8_t)cmd_id[0], (uint8_t)cmd_id[1], (uint8_t)cmd_id[2]);
					}
				}
				break;
			default:
				/* This allows us to write bitfield, registers or xmem after files */
				if (tfaContWriteItem(tfa, &prof->list[i]) != Tfa98xx_Error_Ok) {
					return Tfa98xx_Error_Bad_Parameter;
				}
				break;
		}

		if (err != Tfa98xx_Error_Ok) {
				return err;
		}
	}

	if ((prof->group != previous_prof->group || prof->group == 0) && (tfa->tfa_family == 2)) {
		if (TFA_GET_BF(tfa, REFCKSEL) == 0) {
			/* set SBSL to go to operation mode */
			TFA_SET_BF_VOLATILE(tfa, SBSL, 1);
		}
	}

	return err;
}

/*
 *  process only vstep in the profilelist
 *
 */
enum Tfa98xx_Error tfaContWriteFilesVstep(struct tfa_device *tfa, int prof_idx, int vstep_idx)
{
	nxpTfaProfileList_t *prof = tfaContGetDevProfList(tfa->cnt, tfa->dev_idx, prof_idx);
	unsigned int i;
	nxpTfaFileDsc_t *file;
	nxpTfaHeader_t *hdr;
	nxpTfaHeaderType_t type;
	enum Tfa98xx_Error err = Tfa98xx_Error_Ok;

	if ( !prof )
		return Tfa98xx_Error_Bad_Parameter;

	if (tfa->verbose)
		tfa98xx_trace_printk("device:%s profile:%s vstep:%d\n", tfaContDeviceName(tfa->cnt, tfa->dev_idx),
					tfaContProfileName(tfa->cnt, tfa->dev_idx, prof_idx), vstep_idx);

	/* write vstep file only! */
	for(i=0;i<prof->length;i++) {
		if ( prof->list[i].type == dscFile ) {
			file = (nxpTfaFileDsc_t *)(prof->list[i].offset+(uint8_t *)tfa->cnt);
			hdr = (nxpTfaHeader_t *)file->data;
			type = (nxpTfaHeaderType_t) hdr->id;

			switch (type) {
			case volstepHdr:
				if ( tfaContWriteFile(tfa, file, vstep_idx, TFA_MAX_VSTEP_MSG_MARKER) )
					return Tfa98xx_Error_Bad_Parameter;
				break;
			default:
				break;
			}
		}
	}

	return err;
}

char *tfaContGetString(nxpTfaContainer_t *cnt, nxpTfaDescPtr_t *dsc)
{
	if ( dsc->type != dscString)
		return "Undefined string";

	return dsc->offset+(char*)cnt;
}

char *tfaContGetCommandString(uint32_t type)
{
	if(type == dscSetInputSelect)
		return "SetInputSelector";
	else if(type == dscSetOutputSelect)
		return "SetOutputSelector";
	else if(type == dscSetProgramConfig)
		return "SetProgramConfig";
	else if(type == dscSetLagW)
		return "SetLagW";
	else if(type == dscSetGains)
		return "SetGains";
	else if(type == dscSetvBatFactors)
		return "SetvBatFactors";
	else if(type == dscSetSensesCal)
		return "SetSensesCal";
	else if(type == dscSetSensesDelay)
		return "SetSensesDelay";
	else if(type == dscSetMBDrc)
		return "SetMBDrc";
	else if(type == dscSetFwkUseCase)
		return "SetFwkUseCase";
	else if(type == dscSetVddpConfig)
		return "SetVddpConfig";
	else if(type == dscFilter)
		return "filter";
	else
		return "Undefined string";
}

/*
 * Get the name of the device at a certain index in the container file
 *  return device name
 */
char  *tfaContDeviceName(nxpTfaContainer_t *cnt, int dev_idx)
{
	nxpTfaDeviceList_t *dev;

	dev = tfaContDevice(cnt, dev_idx);
	if (dev == NULL)
		return "!ERROR!";

	return tfaContGetString(cnt, &dev->name);
}

/*
 * Get the application name from the container file application field
 * note that the input stringbuffer should be sizeof(application field)+1
 *
 */
int tfa_cnt_get_app_name(struct tfa_device *tfa, char *name)
{
	unsigned int i;
	int len = 0;

	for(i=0; i<sizeof(tfa->cnt->application); i++) {
		if (isalnum(tfa->cnt->application[i])) /* copy char if valid */
			name[len++] = tfa->cnt->application[i];
		if (tfa->cnt->application[i]=='\0')
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
int tfaContGetCalProfile(struct tfa_device *tfa)
{
	int prof, cal_idx = -2;

	if ( (tfa->dev_idx < 0) || (tfa->dev_idx >= tfa->cnt->ndev) )
		return -1;

	/* search for the calibration profile in the list of profiles */
	for (prof = 0; prof < tfa->cnt->nprof; prof++) {
		if(strstr(tfaContProfileName(tfa->cnt, tfa->dev_idx, prof), ".cal") != NULL) {
			cal_idx = prof;
			pr_debug("Using calibration profile: '%s'\n", tfaContProfileName(tfa->cnt, tfa->dev_idx, prof));
			break;
		}
	}

	return cal_idx;
}

/**
 * Is the profile a tap profile
 */
int tfaContIsTapProfile(struct tfa_device *tfa, int prof_idx)
{
	if ((tfa->dev_idx < 0) || (tfa->dev_idx >= tfa->cnt->ndev))
		return -1;

	/* Check if next profile is tap profile */
	if (strstr(tfaContProfileName(tfa->cnt, tfa->dev_idx, prof_idx), ".tap") != NULL) {
		pr_debug("Using Tap profile: '%s'\n", tfaContProfileName(tfa->cnt, tfa->dev_idx, prof_idx));
		return 1;
	}

	return 0;
}

/*
 * Get the name of the profile at certain index for a device in the container file
 *  return profile name
 */
char *tfaContProfileName(nxpTfaContainer_t *cnt, int dev_idx, int prof_idx)
{
	nxpTfaProfileList_t *prof = NULL;

	/* the Nth profiles for this device */
	prof = tfaContGetDevProfList(cnt, dev_idx, prof_idx);

	/* If the index is out of bound */
	if (prof == NULL)
		return "NONE";

	return tfaContGetString(cnt, &prof->name);
}

/*
 * return 1st profile list
 */
nxpTfaProfileList_t *tfaContGet1stProfList(nxpTfaContainer_t * cont)
{
	nxpTfaProfileList_t *prof;
	uint8_t *b = (uint8_t *) cont;

	int maxdev = 0;
	nxpTfaDeviceList_t *dev;


	maxdev = cont->ndev;

	dev = tfaContGetDevList(cont, maxdev - 1);
        if(dev == NULL)
                return NULL;

	b = (uint8_t *) dev + sizeof(nxpTfaDeviceList_t) + dev->length * (sizeof(nxpTfaDescPtr_t));
	prof = (nxpTfaProfileList_t *) b;
	return prof;
}

/*
 * return 1st livedata list
 */
nxpTfaLiveDataList_t *tfaContGet1stLiveDataList(nxpTfaContainer_t * cont)
{
        nxpTfaLiveDataList_t *ldata;
        nxpTfaProfileList_t *prof;
        nxpTfaDeviceList_t *dev;
	uint8_t *b = (uint8_t *) cont;
	int maxdev, maxprof;


	maxdev = cont->ndev;

	maxprof = cont->nprof;


	dev = tfaContGetDevList(cont, maxdev - 1);

	b = (uint8_t *) dev + sizeof(nxpTfaDeviceList_t) +
	    dev->length * (sizeof(nxpTfaDescPtr_t));

	while(maxprof != 0) {

		prof = (nxpTfaProfileList_t *) b;
		b += sizeof(nxpTfaProfileList_t) +
		     ((prof->length-1) * (sizeof(nxpTfaDescPtr_t)));
		maxprof--;
	}

	/* Else the marker falls off */
	b += 4;

	ldata = (nxpTfaLiveDataList_t *) b;
	return ldata;
}

/*
 * return the device list pointer
 */
nxpTfaDeviceList_t *tfaContDevice(nxpTfaContainer_t *cnt, int dev_idx)
{
	return tfaContGetDevList(cnt, dev_idx);
}

/*
 * return the next profile:
 *  - assume that all profiles are adjacent
 *  - calculate the total length of the input
 *  - the input profile + its length is the next profile
 */
nxpTfaProfileList_t* tfaContNextProfile(nxpTfaProfileList_t* prof) {
	uint8_t *this, *next; /* byte pointers for byte pointer arithmetic */
	nxpTfaProfileList_t* nextprof;
	int listlength; /* total length of list in bytes */

        if(prof == NULL)
                return NULL;

	if (prof->ID != TFA_PROFID)
		return NULL;	/* invalid input */

	this = (uint8_t *)prof;
	/* nr of items in the list, length includes name dsc so - 1*/
	listlength = (prof->length - 1)*sizeof(nxpTfaDescPtr_t);
	/* the sizeof(nxpTfaProfileList_t) includes the list[0] length */
	next = this + listlength + sizeof(nxpTfaProfileList_t);
	nextprof = (nxpTfaProfileList_t *)next;

	if (nextprof->ID != TFA_PROFID)
		return NULL;

	return nextprof;
}

/*
 * return the next livedata
 */
nxpTfaLiveDataList_t* tfaContNextLiveData(nxpTfaLiveDataList_t* livedata) {
	nxpTfaLiveDataList_t* nextlivedata = (nxpTfaLiveDataList_t *)( (char*)livedata + (livedata->length*4) +
                                                                        sizeof(nxpTfaLiveDataList_t) -4);

	if (nextlivedata->ID == TFA_LIVEDATAID)
		return nextlivedata;

	return NULL;
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

	base = (uint8_t *)&cont->CRC + 4;
	size = (size_t)(cont->size - (base - (uint8_t *)cont));
	crc = ~crc32_le(~0u, base, size);

	return crc != cont->CRC;
}

static void get_all_features_from_cnt(struct tfa_device *tfa, int *hw_feature_register, int sw_feature_register[2])
{
	nxpTfaFeatures_t *features;
	int i;

	nxpTfaDeviceList_t *dev = tfaContDevice(tfa->cnt, tfa->dev_idx);

	/* Init values in case no keyword is defined in cnt file: */
	*hw_feature_register = -1;
	sw_feature_register[0] = -1;
	sw_feature_register[1] = -1;

	if(dev == NULL)
		return;


	for(i=0;i<dev->length;i++) {
		if (dev->list[i].type == dscFeatures) {
			features = (nxpTfaFeatures_t *)(dev->list[i].offset+(uint8_t *)tfa->cnt);
			*hw_feature_register = features->value[0];
			sw_feature_register[0] = features->value[1];
			sw_feature_register[1] = features->value[2];
			break;
		}
	}
}

/* wrapper function */
void get_hw_features_from_cnt(struct tfa_device *tfa, int *hw_feature_register)
{
        int sw_feature_register[2];
        get_all_features_from_cnt(tfa, hw_feature_register, sw_feature_register);
}

/* wrapper function */
void get_sw_features_from_cnt(struct tfa_device *tfa, int sw_feature_register[2])
{
        int hw_feature_register;
        get_all_features_from_cnt(tfa, &hw_feature_register, sw_feature_register);
}

enum Tfa98xx_Error tfa98xx_factory_trimmer(struct tfa_device *tfa)
{
	return (tfa->dev_ops.factory_trimmer)(tfa);
}

enum Tfa98xx_Error tfa_set_filters(struct tfa_device *tfa, int prof_idx)
{
	enum Tfa98xx_Error err = Tfa98xx_Error_Ok;
	nxpTfaProfileList_t *prof = tfaContGetDevProfList(tfa->cnt, tfa->dev_idx, prof_idx);
	unsigned int i;

	if ( !prof )
		return Tfa98xx_Error_Bad_Parameter;

	/* If we are in powerdown there is no need to set filters */
	if (TFA_GET_BF(tfa, PWDN) == 1)
		return Tfa98xx_Error_Ok;

	/* loop the profile to find filter settings */
	for(i=0;i<prof->length;i++) {
		/* We only want to write the values before the default section */
		if(prof->list[i].type == dscDefault)
			break;

		/* write all filter settings */
		if ( prof->list[i].type == dscFilter) {
			if (tfaContWriteItem(tfa,  &prof->list[i]) != Tfa98xx_Error_Ok)
				return err;
		}
	}

	return err;
}

int tfa_tib_dsp_msgmulti(struct tfa_device *tfa, int length, const char *buffer)
{
	uint8_t *buf = (uint8_t *)buffer;
	static uint8_t *blob = NULL, *blobptr; /* TODO: not multi-thread safe */
	static int total = 0; /* TODO: not multi-thread safe */
	int post_len = 0;

	/* checks for 24b_BE or 32_LE */
	int len_word_in_bytes = (tfa->convert_dsp32) ? 4 : 3;
	/* TODO: get rid of these magic constants max size should depend on the tfa device type */
	int tfadsp_max_msg_size = (tfa->convert_dsp32) ? 5336 : 4000;

	/* No data found*/
	if(length == -1 && blob == NULL) {
		return -1;
	}

	if (length == -1) {
		int i;
		/* set last length field to zero */
		for (i = total; i < (total + len_word_in_bytes); i++) {
			blob[i] = 0;
		}
		total += len_word_in_bytes;
		memcpy(buf, blob, total);

		kfree(blob);
		blob = NULL; /* Set to NULL pointer, otherwise no new malloc is done! */
		return total;
	}

	if (blob == NULL) {
		if (tfa->verbose)
				pr_debug("%s, Creating the multi-message \n\n", __FUNCTION__);

		blob = kmalloc(tfadsp_max_msg_size, GFP_KERNEL);
		/* add command ID for multi-msg = 0x008015 */
		if (tfa->convert_dsp32) {
			blob[0] = 0x15;
			blob[1] = 0x80;
			blob[2] = 0x0;
			blob[3] = 0x0;
		}
		else {
			blob[0] = 0x0;
			blob[1] = 0x80;
			blob[2] = 0x15;
		}
		blobptr = blob;
		blobptr += len_word_in_bytes;
		total = len_word_in_bytes;
	}

	if (tfa->verbose) {
		pr_debug("%s, id:0x%02x%02x%02x, length:%d \n", __FUNCTION__, buf[0], buf[1], buf[2], length);
	}

	/* check total message size after concatination */
	post_len = total+length+(2*len_word_in_bytes);
	if (post_len > tfadsp_max_msg_size) {

		return Tfa98xx_Error_Buffer_too_small;
	}

	/* add length field (length in words) to the multi message */
	if (tfa->convert_dsp32) {
		*blobptr++ = (uint8_t)((length/len_word_in_bytes) & 0xff);          /* lsb */
		*blobptr++ = (uint8_t)(((length/len_word_in_bytes) & 0xff00) >> 8); /* msb */
		*blobptr++ = 0x0;
		*blobptr++ = 0x0;
	}
	else {
		*blobptr++ = 0x0;
		*blobptr++ = (uint8_t)(((length/len_word_in_bytes) & 0xff00) >> 8); /* msb */
		*blobptr++ = (uint8_t)((length/len_word_in_bytes) & 0xff);          /* lsb */
	}
	memcpy(blobptr, buf, length);
	blobptr += length;
	total += (length + len_word_in_bytes);

	/* SetRe25 message is always the last message of the multi-msg */
	if (tfa->convert_dsp32) {
		if (buf[1] == 0x81 && buf[0] == SB_PARAM_SET_RE25C) {
			return 1; /* 1 means last message is done! */
		}
	} else {
		if (buf[1] == 0x81 && buf[2] == SB_PARAM_SET_RE25C) {
			return 1; /* 1 means last message is done! */
		}
	}

	return 0;
}
