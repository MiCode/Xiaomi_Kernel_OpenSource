#include "sec_boot_lib.h"

/**************************************************************************
 *  MODULE NAME
 **************************************************************************/
#define MOD                         "CFMT_CORE"

/******************************************************************************
 *  IMAGE VERIFICATION MEMORY DUMP FUNCTIONS
 ******************************************************************************/
#define DUMP_MORE_FOR_DEBUG 0

#if DUMP_MORE_FOR_DEBUG
static void sec_cipherfmt_dump_buffer(uchar* buf, uint32 len)
{
    uint32 i = 0;

    for (i =1; i <len+1; i++)
    {                
        SMSG(true,"0x%x,",buf[i-1]);
        
        if(0 == (i%8))
            SMSG(true,"\n");
    }

    if(0 != (len%8))
        SMSG(true,"\n");    
}
#endif  


/**************************************************************************
 *  FUNCTION To check if the image is encrypted
 **************************************************************************/
int sec_cipherfmt_check_cipher(ASF_FILE fp, unsigned int start_off, unsigned int *img_len)
{
    CIPHER_HEADER cipher_header;
    uint32 read_sz = 0;
    unsigned int ret = SEC_OK;

    ASF_GET_DS

    memset(&cipher_header, 0x00, CIPHER_IMG_HEADER_SIZE);
    
    if( !ASF_FILE_ERROR(fp) )
    {
        ASF_SEEK_SET(fp, start_off);
        /* get header */
        if (CIPHER_IMG_HEADER_SIZE != (read_sz = ASF_READ(fp, (char*)&cipher_header, CIPHER_IMG_HEADER_SIZE)))
        {
            SMSG(true,"[%s] read size '%d' != '%d'\n",MOD,read_sz,CIPHER_IMG_HEADER_SIZE);
            ret = ERR_IMAGE_CIPHER_READ_FAIL;
            goto _read_hdr_fail;
        }
    }
    else
    {
        SMSG(true,"[%s] file pointer is NULL\n",MOD);
        ret = ERR_IMAGE_CIPHER_IMG_NOT_FOUND;
        goto _img_hdr_not_found;
    }

#if DUMP_MORE_FOR_DEBUG
    SMSG(true,"[%s] sec_cipherfmt_check_cipher - dump header\n",MOD);
    sec_cipherfmt_dump_buffer((char*)&cipher_header, CIPHER_IMG_HEADER_SIZE);
#endif

    if( CIPHER_IMG_MAGIC != cipher_header.magic_number ) 
    {
        SMSG(true,"[%s] magic number is wrong\n",MOD);
        ret = ERR_IMAGE_CIPHER_HEADER_NOT_FOUND;
        goto _img_hdr_not_found;
    }

    *img_len = cipher_header.image_length;

_img_hdr_not_found:
_read_hdr_fail:   
    
    ASF_PUT_DS


    return ret;
}

/**************************************************************************
 *  FUNCTION To decrypt the cipher content
 **************************************************************************/
int sec_cipherfmt_decrypted(ASF_FILE fp, unsigned int start_off, char *buf, unsigned int buf_len, unsigned int *data_offset)
{
    unsigned int ret = SEC_OK;
    CIPHER_HEADER cipher_header;
    uint32 read_sz = 0;
    uint32 read_offset = start_off;
    char *buf_ptr = buf;
    uint32 total_len = 0;
    uint32 end_pos;
    char tmp_buf[CIPHER_BLOCK_SIZE];
    char tmp_buf2[CIPHER_BLOCK_SIZE];
    uint32 try_read_len = 0;
    
    ASF_GET_DS

    SMSG(true,"[%s] sec_cipherfmt_decrypted (DS lock) - start offset is %d (0x%x)\n",MOD, start_off, start_off);
    SMSG(true,"[%s] sec_cipherfmt_decrypted (DS lock) - total buffer length %d (0x%x)\n",MOD, buf_len, buf_len);

    memset(&cipher_header, 0x00, CIPHER_IMG_HEADER_SIZE);
    
    if( !ASF_FILE_ERROR(fp) )
    {
        ASF_SEEK_SET(fp, read_offset);
        /* get header */
        if (CIPHER_IMG_HEADER_SIZE != (read_sz = ASF_READ(fp, (char*)&cipher_header, CIPHER_IMG_HEADER_SIZE)))
        {
            SMSG(true,"[%s] read size '%d' != '%d'\n",MOD,read_sz,CIPHER_IMG_HEADER_SIZE);
            ret = ERR_IMAGE_CIPHER_READ_FAIL;
            goto _read_hdr_fail;
        }
    }
    else
    {
        SMSG(true,"[%s] file pointer is NULL\n",MOD);
        ret = ERR_IMAGE_CIPHER_IMG_NOT_FOUND;
        goto _read_hdr_fail;
    }

    if( CIPHER_IMG_MAGIC != cipher_header.magic_number ) 
    {
        SMSG(true,"[%s] file pointer is NULL\n",MOD);
        ret = ERR_IMAGE_CIPHER_HEADER_NOT_FOUND;
        goto _hdr_not_found;
    }

#if DUMP_MORE_FOR_DEBUG
        SMSG(true,"[%s] sec_cipherfmt_decrypted - dump header\n",MOD);
        sec_cipherfmt_dump_buffer((char*)&cipher_header, CIPHER_IMG_HEADER_SIZE);
#endif

    read_offset += CIPHER_IMG_HEADER_SIZE;
    ASF_SEEK_SET(fp, read_offset);
    total_len = cipher_header.image_length;
    
    SMSG(true,"[%s] sec_cipherfmt_decrypted - cipher_offset is %d (0x%x)\n",MOD, cipher_header.cipher_offset, cipher_header.cipher_offset);
    SMSG(true,"[%s] sec_cipherfmt_decrypted - cipher_length %d (0x%x)\n",MOD, cipher_header.cipher_length, cipher_header.cipher_length);

    /* by pass cipher offset, cause this part is not encrypted */        
    if( cipher_header.cipher_offset )
    {
        /* get header */
        if (cipher_header.cipher_offset != (read_sz = ASF_READ(fp, (char*)buf_ptr, cipher_header.cipher_offset)))
        {
            SMSG(true,"[%s] read start size '%d' != '%d'\n",MOD,read_sz,cipher_header.cipher_offset);
            ret = ERR_IMAGE_CIPHER_READ_FAIL;
            goto _read_cipher_offset_fail;
        }   
#if DUMP_MORE_FOR_DEBUG
        SMSG(true,"[%s] sec_cipherfmt_decrypted - dump head part: 0x%x\n",MOD, read_offset);
        sec_cipherfmt_dump_buffer(buf_ptr, cipher_header.cipher_offset);
#endif        
        read_offset += cipher_header.cipher_offset;
        buf_ptr += cipher_header.cipher_offset;
        total_len -= cipher_header.cipher_offset;
    }

    ASF_SEEK_SET(fp, read_offset);

    /* decrypted the cipher content */
    if( cipher_header.cipher_length )
    {
        end_pos = read_offset + cipher_header.cipher_length;

        /* init the key */
        ret = sec_aes_init();	
		if (ret) {
            SMSG(true,"[%s] key init failed!\n",MOD);
			ret = ERR_IMAGE_CIPHER_KEY_ERR;
			goto _key_init_fail;
		}

        /* read with fixed block size */
        while( read_offset < end_pos )
        {
            if( (end_pos - read_offset) < CIPHER_BLOCK_SIZE )
            {
                SMSG(true,"[%s] cipher block size is not aligned (warning!)\n",MOD);
                try_read_len = end_pos - read_offset;
                break;
            }
            memset(tmp_buf, 0x00, CIPHER_BLOCK_SIZE);
            if (CIPHER_BLOCK_SIZE != (read_sz = ASF_READ(fp, (char*)tmp_buf, CIPHER_BLOCK_SIZE)))
            {
                SMSG(true,"[%s] read cipher size '%d' != '%d'(try)\n",MOD,read_sz,CIPHER_BLOCK_SIZE);
                ret = ERR_IMAGE_CIPHER_READ_FAIL;
                goto _read_cipher_size_wrong;
            }
              
#if DUMP_MORE_FOR_DEBUG
            SMSG(true,"[%s] sec_cipherfmt_decrypted - dump cipher part: 0x%x\n",MOD, read_offset);
            sec_cipherfmt_dump_buffer(tmp_buf, CIPHER_BLOCK_SIZE);
#endif      

            ret = lib_aes_dec(tmp_buf, CIPHER_BLOCK_SIZE, buf_ptr, CIPHER_BLOCK_SIZE);	
			if (ret) {
                SMSG(true,"[%s] dec cipher block fail\n",MOD);
				ret = ERR_IMAGE_CIPHER_DEC_Fail;
				goto _decrypt_fail;
			}

#if DUMP_MORE_FOR_DEBUG
            SMSG(true,"[%s] sec_cipherfmt_decrypted - dump decipher part: 0x%x\n",MOD, read_offset);
            sec_cipherfmt_dump_buffer(buf_ptr, CIPHER_BLOCK_SIZE);
#endif      

            read_offset += CIPHER_BLOCK_SIZE;
            buf_ptr += CIPHER_BLOCK_SIZE;
            total_len -= CIPHER_BLOCK_SIZE;
        }

        /* read with remain block size */
        if( try_read_len )
        {
            memset(tmp_buf, 0x00, CIPHER_BLOCK_SIZE);
            if (try_read_len != (read_sz = ASF_READ(fp, (char*)tmp_buf, try_read_len)))
            {
                SMSG(true,"[%s] read cipher size '%d' != '%d'(remain)\n",MOD,read_sz,try_read_len);
                ret = ERR_IMAGE_CIPHER_READ_FAIL;
                goto _read_cipher_size_wrong;
            }      

#if DUMP_MORE_FOR_DEBUG
            SMSG(true,"[%s] sec_cipherfmt_decrypted - dump cipher part: 0x%x (try)\n",MOD, read_offset);
            sec_cipherfmt_dump_buffer(tmp_buf, try_read_len);
#endif  

            ret = lib_aes_dec(tmp_buf, CIPHER_BLOCK_SIZE, tmp_buf2, CIPHER_BLOCK_SIZE);	
			if (ret) {
                SMSG(true,"[%s] dec cipher block fail\n",MOD);
				ret = ERR_IMAGE_CIPHER_DEC_Fail;
				goto _decrypt_fail;
			}
            memcpy(buf_ptr, tmp_buf2, try_read_len);
            
#if DUMP_MORE_FOR_DEBUG
            SMSG(true,"[%s] sec_cipherfmt_decrypted - dump decipher part: 0x%x (try)\n",MOD, read_offset);
            sec_cipherfmt_dump_buffer(buf_ptr, try_read_len);
#endif  
            read_offset += try_read_len;
            buf_ptr += try_read_len;
            total_len -= try_read_len;
        }
    }

    /* read final plain text part, cause this part is not encrypted */
    if( total_len > 0 )
    {
        if (total_len != (read_sz = ASF_READ(fp, (char*)buf_ptr, total_len)))
        {
            SMSG(true,"[%s] read tail size '%d' != '%d'\n",MOD,read_sz,total_len);
            ret = ERR_IMAGE_CIPHER_READ_FAIL;
            goto _read_tail_fail;
        }

#if DUMP_MORE_FOR_DEBUG
        SMSG(true,"[%s] sec_cipherfmt_decrypted - dump tail part: 0x%x\n",MOD, read_offset);
        sec_cipherfmt_dump_buffer(buf_ptr, total_len);
#endif                
        read_offset += total_len;
        buf_ptr += total_len;
        total_len -= total_len;
    }

    if( total_len != 0 )
    {
        SMSG(true,"[%s] image size is not correct\n",MOD);
        ret = ERR_IMAGE_CIPHER_WRONG_OPERATION;
        goto _image_size_wrong;
    }

    *data_offset = CIPHER_IMG_HEADER_SIZE;
    SMSG(true,"[%s] image descrypted successful (DS unlock)\n",MOD);

_image_size_wrong:
_read_tail_fail:
_decrypt_fail:
_read_cipher_size_wrong:
_key_init_fail:
_read_cipher_offset_fail:
_hdr_not_found:
_read_hdr_fail:
        
    ASF_PUT_DS

    return ret;
}


