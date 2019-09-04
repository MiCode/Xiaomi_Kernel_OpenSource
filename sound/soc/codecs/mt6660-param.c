/*
 *  sound/soc/codecs/mt6660-param.c
 *  Driver to Mediatek MT6660 SPKAMP Param
 *
 *  Copyright (C) 2018 Mediatek Inc.
 *  cy_huang <cy_huang@richtek.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *  See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/version.h>
#include <sound/soc.h>
/* param security purpose */
#include <linux/crypto.h>
#include <linux/scatterlist.h>
#include <crypto/akcipher.h>
#include <crypto/algapi.h>
#include <linux/crc32.h>
#include <crypto/hash.h>

#include "mt6660.h"

struct mt6660_param_drvdata {
	struct device *dev;
	struct mt6660_chip *chip;
	void *param;
	int param_size;
};

struct sec_header {
	char tag[8];
	u16 BHV;
	u16 MSHV;
	u32 TBS;
	char ICTagString[16];
	char BinDesc[96];
	char date[16];
	u32 EHO;
	u32 EHS;
	u32 EDS;
	u32 CRC;
	char AuthorInfo[80];
	u8 upub_key[144];
	u8 uinfo_sig[128];
	u8 hinfo_sig[128];
} __packed;

/* rsa 1024 sig */
#define SIG_SIZE	(128)
/* rsa 1024 pub key */
#define PUBKEY_SIZE	(140)
/* SHA256 Digest */
#define DIG_SIZE	(32)
/* Current Support Header version ver 1.0 */
#define MIN_HEADER_VER	(0x0100)

static const uint8_t def_pub_key[PUBKEY_SIZE] = {
	0x30, 0x81, 0x89, 0x02, 0x81, 0x81, 0x00, 0xca, 0x46, 0x23, 0xf8, 0xac,
	0x77, 0xcd, 0x0c, 0xa7, 0x3d, 0xbe, 0x27, 0xd7, 0x1d, 0xdc, 0x48, 0x57,
	0x12, 0xfc, 0x39, 0x78, 0xf5, 0x49, 0x99, 0x4e, 0x03, 0x90, 0x3e, 0x6d,
	0xa7, 0xdd, 0x9d, 0x66, 0x78, 0x3d, 0xe8, 0x83, 0x16, 0x15, 0x15, 0x03,
	0x4a, 0x20, 0x99, 0xc1, 0x75, 0x6a, 0xfe, 0x37, 0xae, 0x89, 0x8d, 0xb7,
	0xf5, 0x51, 0xb6, 0xf0, 0xca, 0xd5, 0x9b, 0xd2, 0x91, 0xdb, 0xf9, 0x01,
	0x78, 0x02, 0x8e, 0xdf, 0x23, 0xcb, 0x52, 0x43, 0xb1, 0x8d, 0xde, 0x1a,
	0x7d, 0x0f, 0xce, 0xd8, 0x65, 0xc7, 0x57, 0x04, 0xd8, 0xf6, 0x54, 0xee,
	0x15, 0x62, 0xb1, 0x07, 0x81, 0xd4, 0xf6, 0x08, 0xe7, 0x53, 0xa7, 0xad,
	0x7f, 0x5f, 0x58, 0x62, 0xd2, 0xee, 0xb5, 0x40, 0x9d, 0x0b, 0x83, 0x07,
	0x30, 0xd1, 0x13, 0xba, 0x43, 0x25, 0x56, 0xc7, 0x1f, 0x34, 0xed, 0x80,
	0x6f, 0x50, 0xe7, 0x02, 0x03, 0x01, 0x00, 0x01,
};

static int mt6660_param_header_digest(struct device *dev, const void *in,
				      int in_cnt, void *out, int out_cnt)
{
	struct crypto_shash *tfm;
	int ret;

	dev_dbg(dev, "%s ++\n", __func__);
	if (!dev || !in || !out || !in_cnt || out_cnt != DIG_SIZE)
		return -EINVAL;
	/* sha256 digest gen */
	tfm = crypto_alloc_shash("sha256", 0, 0);
	if (IS_ERR(tfm))
		return PTR_ERR(tfm);
	{
		SHASH_DESC_ON_STACK(desc, tfm);
		desc->tfm = tfm;
		desc->flags = CRYPTO_TFM_REQ_MAY_SLEEP;
		ret = crypto_shash_digest(desc, in, in_cnt, out);
		if (ret < 0)
			dev_err(dev, "%s: gen digest fail\n", __func__);
		shash_desc_zero(desc);
	}
	crypto_free_shash(tfm);
	dev_dbg(dev, "%s --\n", __func__);
	return ret;
}

struct crypto_result {
	struct completion completion;
	int err;
};

static const u8 RSA_digest_info_SHA256[] = {
	0x30, 0x31, 0x30, 0x0d, 0x06, 0x09, 0x60, 0x86,
	0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x01, 0x05,
	0x00, 0x04, 0x20
};

static void mt6660_param_key_verify_done(struct crypto_async_request *req,
					 int err)
{
	struct crypto_result *compl = req->data;

	if (err == -EINPROGRESS)
		return;
	compl->err = err;
	complete(&compl->completion);
}

static int pkcs_1_v1_5_decode_emsa(struct device *dev, const u8 *in, int inlen)
{
	const u8 *asn1_template = RSA_digest_info_SHA256;
	int asn1_size = ARRAY_SIZE(RSA_digest_info_SHA256);
	int hash_size = 32;
	unsigned int t_offset, ps_end, ps_start, i;

	if (!dev || !in || !inlen)
		return -EINVAL;
	if (inlen < 2 + 1 + asn1_size + hash_size)
		return -EINVAL;

	ps_start = 2;
	if (in[0] != 0x00 && in[1] != 0x01) {
		dev_err(dev, "[in[0] == %02u], [in[1] = %02u]\n", in[0], in[1]);
		return -EBADMSG;
	}
	t_offset = inlen - (asn1_size + hash_size);
	ps_end = t_offset - 1;
	if (in[ps_end] != 0x00) {
		dev_err(dev, "[in[T-1] == %02u]\n", in[ps_end]);
		return -EBADMSG;
	}
	for (i = ps_start; i < ps_end; i++) {
		if (in[i] != 0xff) {
			dev_err(dev, "[in[PS%x] == %02u]\n", i - 2, in[i]);
			return -EBADMSG;
		}
	}
	if (crypto_memneq(asn1_template, in + t_offset, asn1_size) != 0) {
		dev_err(dev, "[in[T] ASN.1 mismatch]\n");
		return -EBADMSG;
	}
	return t_offset + asn1_size;
}

static int mt6660_param_sig_output(struct device *dev,
				   const void *pubkey, int pubkey_size,
				   const void *sig, int sig_size,
				   const void *dig, int dig_size)
{
	struct crypto_result compl;
	struct crypto_akcipher *tfm;
	struct akcipher_request *req;
	struct scatterlist sig_sg, digest_sg;
	void *output;
	unsigned int outlen;
	int ret = 0;

	dev_dbg(dev, "%s ++\n", __func__);
	if (!dev || !pubkey || pubkey_size < PUBKEY_SIZE ||
		!sig || sig_size < SIG_SIZE || !dig || dig_size < DIG_SIZE)
		return -EINVAL;
	tfm = crypto_alloc_akcipher("rsa", 0, 0);
	if (IS_ERR(tfm))
		return PTR_ERR(tfm);
	req = akcipher_request_alloc(tfm, GFP_KERNEL);
	if (!req)
		goto err_free_tfm;
	ret = crypto_akcipher_set_pub_key(tfm, pubkey, pubkey_size);
	if (ret < 0)
		goto err_free_req;
	outlen = crypto_akcipher_maxsize(tfm);
	output = devm_kzalloc(dev, outlen, GFP_KERNEL);
	if (!output)
		goto err_free_req;
	sg_init_one(&sig_sg, sig, sig_size);
	sg_init_one(&digest_sg, output, outlen);
	akcipher_request_set_crypt(req, &sig_sg, &digest_sg, sig_size, outlen);
	init_completion(&compl.completion);
	akcipher_request_set_callback(req, CRYPTO_TFM_REQ_MAY_BACKLOG |
				      CRYPTO_TFM_REQ_MAY_SLEEP,
				      mt6660_param_key_verify_done, &compl);
	ret = crypto_akcipher_verify(req);
	if (ret == -EINPROGRESS) {
		wait_for_completion(&compl.completion);
		ret = compl.err;
	}
	if (ret < 0)
		goto out_free_output;
	ret = pkcs_1_v1_5_decode_emsa(dev, output, req->dst_len);
	if (ret < 0)
		goto out_free_output;
	if (memcmp(dig, output + ret, DIG_SIZE) != 0)
		ret = -EKEYREJECTED;
	else
		ret = 0;
	dev_dbg(dev, "%s result [%d] --\n", __func__, ret);
out_free_output:
	devm_kfree(dev, output);
err_free_req:
	akcipher_request_free(req);
err_free_tfm:
	crypto_free_akcipher(tfm);
	return ret;
}

static int mt6660_param_header_check_signature(struct device *dev,
					       const char *buf, size_t cnt)
{
	const struct sec_header *header = (const struct sec_header *)buf;
	u8 digest[DIG_SIZE];
	int ret;

	/* verify user info sig AuthorInfo to UserPubkey first */
	memset(digest, 0, DIG_SIZE);
	ret = mt6660_param_header_digest(dev, (void *)header + 160,
					 224, digest, DIG_SIZE);
	if (ret < 0) {
		dev_err(dev, "digest fail\n");
		return ret;
	}
	ret = mt6660_param_sig_output(dev, def_pub_key, PUBKEY_SIZE,
				      header->uinfo_sig, SIG_SIZE, digest,
				      DIG_SIZE);
	if (ret < 0) {
		dev_err(dev, "user info sig fail\n");
		return ret;
	}
	/* verify header info sig tag to crc first */
	memset(digest, 0, DIG_SIZE);
	ret = mt6660_param_header_digest(dev, (void *)header, 160,
					 digest, DIG_SIZE);
	if (ret < 0) {
		dev_err(dev, "digest fail\n");
		return ret;
	}
	ret = mt6660_param_sig_output(dev, header->upub_key, PUBKEY_SIZE,
				      header->hinfo_sig, SIG_SIZE, digest,
				      DIG_SIZE);
	if (ret < 0) {
		dev_err(dev, "header info sig fail\n");
		return ret;
	}
	return 0;
}

static int mt6660_param_header_validate(struct device *dev,
					const char *buf, size_t cnt)
{
	const struct sec_header *header = (const struct sec_header *)buf;
	u32 size;
	int ret;

	if (header->MSHV > MIN_HEADER_VER) {
		dev_err(dev, "not support header version 0x%04x", header->MSHV);
		return -ENOTSUPP;
	}
	/* check signature pub_key digest */
	ret = mt6660_param_header_check_signature(dev, buf, cnt);
	if (ret < 0) {
		dev_err(dev, "check header signature fail\n");
		return ret;
	}
	/* check bin size match */
	size = sizeof(*header) + header->EHO + header->EHS + header->EDS;
	if (size != header->TBS || size != cnt) {
		dev_err(dev, "check size fail\n");
		return -EINVAL;
	}
	return 0;
}

static int mt6660_param_data_validate(struct device *dev,
				      const char *buf, size_t cnt)
{
	const struct sec_header *header = (const struct sec_header *)buf;
	u8 *data;
	int offset;
	u32 crc;

	offset = sizeof(*header) + header->EHO + header->EHS;
	data = (u8 *)(buf + offset);
	crc = crc32_be(-1, data, header->EDS);
	if (crc != header->CRC) {
		dev_err(dev, "crc 0x%08x, h_crc = 0x%08x not match\n",
			crc, header->CRC);
		return -EINVAL;
	}
	return 0;
}

static int mt6660_param_data_trigger(struct device *dev,
				     const char *buf, size_t count)
{
	struct mt6660_param_drvdata *p_drvdata = dev_get_drvdata(dev);
	const struct sec_header *header = (const struct sec_header *)buf;
	void *data;
	u32 data_len;
	int ret;

	/* copy buf to private variable */
	if (p_drvdata->param) {
		devm_kfree(dev, p_drvdata->param);
		p_drvdata->param = NULL;
		p_drvdata->param_size = 0;
	}
	if (!p_drvdata->param) {
		p_drvdata->param = devm_kzalloc(dev, count, GFP_KERNEL);
		if (!p_drvdata->param)
			return -ENOMEM;
		p_drvdata->param_size = count;
		memcpy(p_drvdata->param, buf, count);
	}
	data = (void *)(buf + sizeof(*header) + header->EHO + header->EHS);
	data_len = header->EDS;
	ret = mt6660_codec_trigger_param_write(p_drvdata->chip, data, data_len);
	if (ret < 0) {
		dev_err(dev, "trigger parameter write fail\n");
		return ret;
	}
	return 0;
}

static ssize_t mt6660_param_file_show(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	struct mt6660_param_drvdata *p_drvdata = dev_get_drvdata(dev);

	if (!p_drvdata->param)
		return scnprintf(buf, PAGE_SIZE, "no proprietary param\n");
	memcpy(buf, p_drvdata->param, p_drvdata->param_size);
	return p_drvdata->param_size;
}

static ssize_t mt6660_param_file_store(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf, size_t count)
{
	int ret;

	if (count < sizeof(struct sec_header))
		return -EINVAL;
	ret = mt6660_param_header_validate(dev, buf, count);
	if (ret < 0) {
		dev_err(dev, "parameter check header fail\n");
		return ret;
	}
	ret = mt6660_param_data_validate(dev, buf, count);
	if (ret < 0) {
		dev_err(dev, "parameter check data fail\n");
		return ret;
	}
	ret = mt6660_param_data_trigger(dev, buf, count);
	if (ret < 0) {
		dev_err(dev, "parameter trigger data fail\n");
		return ret;
	}
	return count;
}

static const DEVICE_ATTR(prop_params, 0644, mt6660_param_file_show,
			 mt6660_param_file_store);

static int mt6660_param_probe(struct platform_device *pdev)
{
	struct mt6660_chip *chip = dev_get_drvdata(pdev->dev.parent);
	struct mt6660_param_drvdata *p_drvdata = NULL;
	int ret = 0;

	dev_info(&pdev->dev, "%s: ++\n", __func__);
	p_drvdata = devm_kzalloc(&pdev->dev, sizeof(*p_drvdata), GFP_KERNEL);
	if (!p_drvdata)
		return -ENOMEM;
	/* drvdata initialize */
	p_drvdata->dev = &pdev->dev;
	p_drvdata->chip = chip;
	platform_set_drvdata(pdev, p_drvdata);
	ret = device_create_file(&pdev->dev, &dev_attr_prop_params);
	if (ret < 0)
		goto cfile_fail;
	dev_info(&pdev->dev, "%s: --\n", __func__);
	return 0;
cfile_fail:
	devm_kfree(&pdev->dev, p_drvdata);
	platform_set_drvdata(pdev, NULL);
	return ret;
}

static int mt6660_param_remove(struct platform_device *pdev)
{
	struct mt6660_param_drvdata *p_drvdata = platform_get_drvdata(pdev);

	dev_dbg(p_drvdata->dev, "%s: ++\n", __func__);
	dev_dbg(p_drvdata->dev, "%s: --\n", __func__);
	return 0;
}

static const struct platform_device_id mt6660_param_pdev_id[] = {
	{ "mt6660-param", 0},
	{ },
};
MODULE_DEVICE_TABLE(platform, mt6660_param_pdev_id);

static struct platform_driver mt6660_param_driver = {
	.driver = {
		.name = "mt6660-param",
		.owner = THIS_MODULE,
	},
	.probe = mt6660_param_probe,
	.remove = mt6660_param_remove,
	.id_table = mt6660_param_pdev_id,
};
module_platform_driver(mt6660_param_driver);
