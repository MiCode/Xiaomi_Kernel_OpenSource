#include "sec_boot_lib.h"
#include "sec_sign_extension.h"
#include "sec_signfmt_v2.h"
#include "sec_signfmt_util.h"
#include "sec_log.h"
#include "sec_error.h"
#include "sec_mtd_util.h"
#include "sec_wrapper.h"
#include <mach/sec_osal.h>  

/**************************************************************************
 *  MODULE NAME
 **************************************************************************/
#define MOD                         "SFMT_V2"

/**************************************************************************
 *  FUNCTION To Get Hash Length
 **************************************************************************/
unsigned int sec_signfmt_get_hash_length_v2()
{
    return get_hash_size(SEC_CRYPTO_HASH_SHA1);
}

/**************************************************************************
 *  FUNCTION To Get Signature Length
 **************************************************************************/
unsigned int sec_signfmt_get_signature_length_v2()
{
    return get_signature_size(SEC_CRYPTO_SIG_RSA1024);
}

/**************************************************************************
 *  FUNCTION To Get Extension Length
 **************************************************************************/
unsigned int sec_signfmt_get_extension_length_v2(ASF_FILE fp)
{
    /* the extension include signature + hash + extension header */
    if(ASF_FILE_NULL == fp)  
    {
        return 0;
    }
    else
    {
        return get_hash_size(SEC_CRYPTO_HASH_SHA1)+get_signature_size(SEC_CRYPTO_SIG_RSA1024);
    }
}

/**************************************************************************
 *  FUNCTION To Get Image Hash
 **************************************************************************/
int sec_signfmt_calculate_image_hash_v2(char* part_name, SEC_IMG_HEADER *img_hdr, 
    unsigned int image_type, char *hash_buf, unsigned int hash_len)
{
    uint32 ret = SEC_OK; 
    uint32 verify_len = 0;
    uchar *verify_buf = NULL;
    uint32 img_sign_off = 0;
    uint32 img_sign_len = 0;

    /* reset buffer */
    memset(hash_buf, 0x00, hash_len);

    if(hash_len != sec_signfmt_get_hash_length_v2())
    {
        SMSG(true,"[%s] hash buffer size is invalid '%d'\n",MOD,hash_len);
        ret = ERR_SIGN_FORMAT_HASH_SIZE_WRONG;
        goto _end;
    }

    /* init check */
    if(SEC_IMG_MAGIC != img_hdr->magic_number)
    {
        SMSG(true,"[%s] magic number is invalid '0x%x'\n",MOD,img_hdr->magic_number);
        ret = ERR_SIGN_FORMAT_MAGIC_WRONG;
        goto _end;
    }

    /* check length */
    verify_len = SEC_IMG_HEADER_SIZE;
    img_sign_off = 0;
    if (is_signfmt_v2(img_hdr))
    {
        img_sign_len = img_hdr->sign_length;
        img_sign_off += img_hdr->sign_offset;
    }
    else
    {
        /* workaround for v1 */
        img_sign_len = ((SEC_IMG_HEADER_U*)img_hdr)->v1.sign_length;
        img_sign_off += ((SEC_IMG_HEADER_U*)img_hdr)->v1.sign_offset;
    }
    verify_len += img_sign_len;

    /* prepare verify buffer */
    verify_buf = ASF_MALLOC(verify_len);
    if(NULL == verify_buf)
    {   
        ret = ERR_FS_READ_BUF_ALLOCATE_FAIL;
        goto _malloc_verify_fail;
    }
    memset(verify_buf, 0x00, verify_len);

    /* fill in verify buffer */
    mcpy(verify_buf, img_hdr, SEC_IMG_HEADER_SIZE);

    if(SEC_OK != (ret = sec_dev_read_image ( pl2part(part_name), 
                                                (char*)verify_buf+SEC_IMG_HEADER_SIZE, 
                                                img_sign_off,
                                                img_sign_len,
                                                image_type)))
    {
        SMSG(true,"[%s] read mtd '%s' fail at image offset 0x%x with length 0x%x\n",MOD,(char*)pl2part(part_name),img_sign_off,img_sign_len);
        goto _read_mtd_fail;
    }
    
    /* ================== */    
    /* dump sign header   */
    /* ================== */        
    SMSG(sec_info.bMsg,"[%s] dump sign header\n",MOD);
    dump_buf((uchar*)img_hdr,SEC_IMG_HEADER_SIZE);

    /* ================== */    
    /* dump file data     */
    /* ================== */        
    SMSG(sec_info.bMsg,"[%s] dump file data\n",MOD);
    dump_buf(verify_buf+SEC_IMG_HEADER_SIZE,8);    

    /* hash */
    SMSG(sec_info.bMsg,"[%s] generate hash ... \n",MOD);    
    if(SEC_OK != (ret = sec_hash(verify_buf, verify_len, (uchar*)hash_buf, hash_len )))
    {
        SMSG(true,"[%s] generate hash fail\n\n",MOD);
        ret = ERR_SIGN_FORMAT_GENERATE_HASH_FAIL;
        goto _hash_fail;
    }        

    /* ================== */    
    /* dump hash data     */
    /* ================== */        
    SMSG(sec_info.bMsg,"[%s] dump hash data\n",MOD);
    dump_buf((uchar*)hash_buf,hash_len);  


    SMSG(sec_info.bMsg,"[%s] generate hash pass\n\n",MOD);

_hash_fail:
_read_mtd_fail:
    ASF_FREE(verify_buf);
_malloc_verify_fail:
_end:

    return ret;
}

/**************************************************************************
 *  FUNCTIONS
 **************************************************************************/
int sec_signfmt_verify_file_v2(ASF_FILE fp, SEC_IMG_HEADER *img_hdr)
{
    uint32 ret = SEC_OK; 
    uint32 verify_len = 0;
    uchar *verify_buf = NULL;
    uint32 img_sign_off = 0;
    uint32 img_sign_len = 0;
    uint32 signature_off = 0;
    uint32 signature_len = 0;
    uchar *signature_buf = NULL;
    uint32 read_sz = 0;

    /* init check */
    if(SEC_IMG_MAGIC != img_hdr->magic_number)
    {
        SMSG(true,"[%s] magic number is invalid '0x%x'\n",MOD,img_hdr->magic_number);
        ret = ERR_SIGN_FORMAT_MAGIC_WRONG;
        goto _magic_wrong_err;
    }

    /* check length */
    verify_len = SEC_IMG_HEADER_SIZE;
    img_sign_off = SEC_IMG_HEADER_SIZE;
    signature_off = 0;
    if (is_signfmt_v2(img_hdr))
    {
        img_sign_len = img_hdr->sign_length;
        img_sign_off += img_hdr->sign_offset;
        signature_off += img_hdr->signature_offset;
    }
    else
    {
        /* workaround for v1 */
        img_sign_len = ((SEC_IMG_HEADER_U*)img_hdr)->v1.sign_length;
        img_sign_off += ((SEC_IMG_HEADER_U*)img_hdr)->v1.sign_offset;
        signature_off += ((SEC_IMG_HEADER_U*)img_hdr)->v1.signature_offset;
    }
    verify_len += img_sign_len;
    signature_len = sec_signfmt_get_signature_length_v2();

    /* prepare signature buffer */
    signature_buf= (uchar*)ASF_MALLOC(signature_len);
    if(NULL == signature_buf)
    {   
        ret = ERR_FS_READ_BUF_ALLOCATE_FAIL;
        goto _malloc_sig_fail;
    }
    memset(signature_buf, 0x00, signature_len);

    /* fill in signature buffer */
    ASF_SEEK_SET(fp, signature_off);

    if (signature_len != (read_sz = ASF_READ(fp, signature_buf, signature_len)))
    {
        SMSG(true,"[%s] read size '%d' != '%d'\n",MOD,read_sz,signature_len);
        ret = ERR_FS_READ_SIZE_FAIL;
        goto _read_sig_fail;
    }

    /* prepare verify buffer */
    verify_buf = ASF_MALLOC(verify_len);
    if(NULL == verify_buf)
    {   
        ret = ERR_FS_READ_BUF_ALLOCATE_FAIL;
        goto _malloc_verify_fail;
    }
    memset(verify_buf, 0x00, verify_len);

    /* fill in verify buffer */
    mcpy(verify_buf, img_hdr, SEC_IMG_HEADER_SIZE);
    ASF_SEEK_SET(fp, img_sign_off);

    if (img_sign_len != (read_sz = ASF_READ(fp, verify_buf+SEC_IMG_HEADER_SIZE, img_sign_len)))
    {
        SMSG(true,"[%s] read size '%d' != '%d'\n",MOD,read_sz,img_sign_len);
        ret = ERR_FS_READ_SIZE_FAIL;
        goto _read_verify_fail;
    }

    /* ================== */    
    /* dump sign header   */
    /* ================== */        
    SMSG(sec_info.bMsg,"[%s] dump sign header\n",MOD);
    dump_buf(verify_buf,SEC_IMG_HEADER_SIZE);

    /* ================== */    
    /* dump file data     */
    /* ================== */        
    SMSG(sec_info.bMsg,"[%s] dump file data\n",MOD);
    dump_buf(verify_buf+SEC_IMG_HEADER_SIZE,8);    

    /* ================== */    
    /* dump signature     */
    /* ================== */        
    SMSG(sec_info.bMsg,"[%s] dump signature\n",MOD);
    dump_buf(signature_buf,signature_len);

    /* verify */
    SMSG(sec_info.bMsg,"[%s] verify (lock)... \n",MOD);    

    osal_verify_lock();
    
    if(SEC_OK != (ret = sec_verify(verify_buf, verify_len, signature_buf, signature_len )))
    {
        osal_verify_unlock();    
        SMSG(true,"[%s] verify fail (unlock), ret is %d\n\n",MOD,ret);
        goto _verify_fail;
    }        

    osal_verify_unlock();    

    SMSG(sec_info.bMsg,"[%s] verify pass (unlock)\n\n",MOD);

_verify_fail:
_read_verify_fail:
    ASF_FREE(verify_buf);
_malloc_verify_fail:
_read_sig_fail:
    ASF_FREE(signature_buf);
_malloc_sig_fail:
_magic_wrong_err:

    return ret;
}

