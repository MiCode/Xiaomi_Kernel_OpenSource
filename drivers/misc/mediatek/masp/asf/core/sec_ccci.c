#include <mach/mt_sec_hal.h>
#include "sec_boot_lib.h"
#include "sec_ccci.h"

/**************************************************************************
 *  MODULE NAME
 **************************************************************************/
#define MOD                         "SEC_CCCI"

int masp_ccci_signfmt_verify_file(char *file_path, unsigned int *data_offset, unsigned int *data_sec_len)
{
    unsigned int ret = SEC_OK; 
    SEC_IMG_HEADER img_hdr;

    *data_offset = 0;
    *data_sec_len = 0;

    ret = sec_signfmt_verify_file(file_path, &img_hdr, data_offset, data_sec_len);

    /* image is not signed */
    if( ret == ERR_SIGN_FORMAT_MAGIC_WRONG )
    {
        if((sec_modem_auth_enabled() == 0) && (sec_schip_enabled() == 0)) 
        {
            SMSG(true,"[%s] image has no sec header\n",MOD);
			ret = SEC_OK;
			goto _out;
		}
		else 
        {
            SMSG(true,"[%s] (img not signed) sec_modem_auth_enabled() = %d\n",MOD,sec_modem_auth_enabled());
            SMSG(true,"[%s] (img not signed) sec_schip_enabled() = %d\n",MOD,sec_schip_enabled());
			ret = ERR_SIGN_FORMAT_MAGIC_WRONG;
			goto _out;
		}
    }

    if( ret != SEC_OK )
    {
        SMSG(true,"[%s] file '%s' verify failed\n",MOD,file_path);
        goto _out;
    }

    SMSG(true,"[%s] data_offset is %d\n",MOD,*data_offset);
    SMSG(true,"[%s] data_sec_len is %d\n",MOD,*data_sec_len);

_out:
    
    return ret;
}

int masp_ccci_version_info(void)
{
    return CCCI_VERSION;
}

int sec_ccci_file_open(char *file_path)
{
    int fp_id;

    fp_id = osal_filp_open_read_only(file_path);

    if(fp_id != OSAL_FILE_NULL)
    {
        return fp_id;
    }

    return -1;
}

int sec_ccci_file_close(int fp_id)
{
    return osal_filp_close(fp_id);
}


int masp_ccci_is_cipherfmt(int fp_id, unsigned int start_off, unsigned int *img_len)
{
    if( SEC_OK != sec_cipherfmt_check_cipher(fp_id, start_off, img_len) )
    {
        *img_len = 0;
        return 0;
    }

    return 1;
}

int masp_ccci_decrypt_cipherfmt(int fp_id, unsigned int start_off, char *buf, unsigned int buf_len, unsigned int *data_offset)
{
    return sec_cipherfmt_decrypted(fp_id, start_off, buf, buf_len, data_offset);
}

void masp_secure_algo(unsigned char Direction, unsigned char *ContentAddr, unsigned int ContentLen, unsigned char *CustomSeed, unsigned char *ResText)
{
    return masp_hal_secure_algo(Direction, ContentAddr, ContentLen, CustomSeed, ResText);
}

/* return the result of hwEnableClock ( )
   - TRUE  (1) means crypto engine init success
   - FALSE (0) means crypto engine init fail    */
unsigned char masp_secure_algo_init(void)
{
    return masp_hal_secure_algo_init();
}

/* return the result of hwDisableClock ( )
   - TRUE  (1) means crypto engine de-init success
   - FALSE (0) means crypto engine de-init fail    */
unsigned char masp_secure_algo_deinit(void)
{
    return masp_hal_secure_algo_deinit();
}


