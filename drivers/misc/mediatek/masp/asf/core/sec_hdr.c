#include "sec_hdr.h"

/**************************************************************************
 *  INTERNAL VARIABLES
 **************************************************************************/
static SEC_IMG_HEADER_VER sec_ver = UNSET;

/**************************************************************************
 *  GET VALUE
 **************************************************************************/
uint32 shdr_magic (SEC_IMG_HEADER_U* sec_hdr)
{
    switch(sec_ver)
    {
        case SEC_HDR_V1:
            return sec_hdr->v1.magic_number;       
        case SEC_HDR_V2:
            return sec_hdr->v2.magic_number;
        case SEC_HDR_V3:
            return sec_hdr->v3.magic_number;
        default:
            SEC_ASSERT(0);
            return 0;
    }
}

uchar* shdr_cust_name (SEC_IMG_HEADER_U* sec_hdr)
{
    switch(sec_ver)
    {
        case SEC_HDR_V1:
            return sec_hdr->v1.cust_name;       
        case SEC_HDR_V2:
            return sec_hdr->v2.cust_name;       
        case SEC_HDR_V3:
            return sec_hdr->v3.cust_name;      
        default:
            SEC_ASSERT(0);
            return 0;
    }
}

uint32 shdr_cust_name_len (SEC_IMG_HEADER_U* sec_hdr)
{
    switch(sec_ver)
    {
        case SEC_HDR_V1:
            return sizeof(sec_hdr->v1.cust_name);
        case SEC_HDR_V2:
            return sizeof(sec_hdr->v2.cust_name);       
        case SEC_HDR_V3:
            return sizeof(sec_hdr->v3.cust_name);      
        default:
            SEC_ASSERT(0);
            return 0;
    }
}

uint32 shdr_img_ver (SEC_IMG_HEADER_U* sec_hdr)
{
    switch(sec_ver)
    {
        case SEC_HDR_V1:
            return sec_hdr->v1.image_version;
        case SEC_HDR_V2:
            return sec_hdr->v2.image_version;
        case SEC_HDR_V3:
            return sec_hdr->v3.image_version;
        default:
            SEC_ASSERT(0);
            return 0;
    }
}

uint32 shdr_img_len (SEC_IMG_HEADER_U* sec_hdr)
{
    switch(sec_ver)
    {
        case SEC_HDR_V1:
            return sec_hdr->v1.image_length;
        case SEC_HDR_V2:
            return sec_hdr->v2.image_length;
        case SEC_HDR_V3:
            return sec_hdr->v3.image_length;
        default:
            SEC_ASSERT(0);
            return 0;            
    }
}

uint32 shdr_img_offset (SEC_IMG_HEADER_U* sec_hdr)
{
    switch(sec_ver)
    {
        case SEC_HDR_V1:
            return sec_hdr->v1.image_offset;
        case SEC_HDR_V2:
            return sec_hdr->v2.image_offset;
        case SEC_HDR_V3:
            return sec_hdr->v3.image_offset;
        default:
            SEC_ASSERT(0);
            return 0;            
    }
}

uint32 shdr_sign_len (SEC_IMG_HEADER_U* sec_hdr)
{
    switch(sec_ver)
    {
        case SEC_HDR_V1:
            return sec_hdr->v1.sign_length;
        case SEC_HDR_V2:
            return sec_hdr->v2.sign_length;
        case SEC_HDR_V3:
            return sec_hdr->v3.sign_length;
        default:
            SEC_ASSERT(0);
            return 0;            
    }
}

uint32 shdr_sign_offset (SEC_IMG_HEADER_U* sec_hdr)
{
    switch(sec_ver)
    {
        case SEC_HDR_V1:
            return sec_hdr->v1.sign_offset;
        case SEC_HDR_V2:
            return sec_hdr->v2.sign_offset;
        case SEC_HDR_V3:
            return sec_hdr->v3.sign_offset;
        default:
            SEC_ASSERT(0);
            return 0;            
    }
}

uint32 shdr_sig_len (SEC_IMG_HEADER_U* sec_hdr)
{
    switch(sec_ver)
    {
        case SEC_HDR_V1:
            return sec_hdr->v1.signature_length;
        case SEC_HDR_V2:
            return sec_hdr->v2.signature_length;
        case SEC_HDR_V3:
            return sec_hdr->v3.signature_length;
        default:
            SEC_ASSERT(0);
            return 0;            
    }
}

uint32 shdr_sig_offset (SEC_IMG_HEADER_U* sec_hdr)
{
    switch(sec_ver)
    {
        case SEC_HDR_V1:
            return sec_hdr->v1.signature_offset;
        case SEC_HDR_V2:
            return sec_hdr->v2.signature_offset;
        case SEC_HDR_V3:
            return sec_hdr->v3.signature_offset;
        default:
            SEC_ASSERT(0);
            return 0;            
    }
}

/**************************************************************************
 *  SET VALUE
 **************************************************************************/
void set_shdr_magic (SEC_IMG_HEADER_U* sec_hdr, uint32 val)
{
    switch(sec_ver)
    {
        case SEC_HDR_V1:
            sec_hdr->v1.magic_number= val;
            break; 
        case SEC_HDR_V2:
            sec_hdr->v2.magic_number = val;
            break;
        case SEC_HDR_V3:
            sec_hdr->v3.magic_number = val;
        default:
            SEC_ASSERT(0);
    }
}

void set_shdr_img_ver (SEC_IMG_HEADER_U* sec_hdr, uint32 ver)
{
    switch(sec_ver)
    {
        case SEC_HDR_V1:
            sec_hdr->v1.image_version = ver;
            break; 
        case SEC_HDR_V2:
            sec_hdr->v2.image_version = ver;
            break;
        case SEC_HDR_V3:
            sec_hdr->v3.image_version = ver;
        default:
            SEC_ASSERT(0);
    }
}

void set_shdr_cust_name (SEC_IMG_HEADER_U* sec_hdr, uchar* name, uint32 len)
{
    switch(sec_ver)
    {
        case SEC_HDR_V1:
            memset(sec_hdr->v1.cust_name,0,sizeof(sec_hdr->v1.cust_name));
            mcpy(sec_hdr->v1.cust_name,name,len);
            break;            
        case SEC_HDR_V2:
            memset(sec_hdr->v2.cust_name,0,sizeof(sec_hdr->v2.cust_name));
            mcpy(sec_hdr->v2.cust_name,name,len);
            break;
        case SEC_HDR_V3:
            memset(sec_hdr->v3.cust_name,0,sizeof(sec_hdr->v3.cust_name));
            mcpy(sec_hdr->v3.cust_name,name,len);
            break;
        default:
            SEC_ASSERT(0);
    }
}
 
void set_shdr_sign_len (SEC_IMG_HEADER_U* sec_hdr, uint32 val)
{
    switch(sec_ver)
    {
        case SEC_HDR_V1:
            sec_hdr->v1.sign_length = val;
            break;            
        case SEC_HDR_V2:
            sec_hdr->v2.sign_length = val;
            break;
        case SEC_HDR_V3:
            sec_hdr->v3.sign_length = val;
            break;
        default:
            SEC_ASSERT(0);
    }
}

void set_shdr_sign_offset (SEC_IMG_HEADER_U* sec_hdr, uint32 val)
{
    switch(sec_ver)
    {
        case SEC_HDR_V1:
            sec_hdr->v1.sign_offset = val;
            break;
        case SEC_HDR_V2:
            sec_hdr->v2.sign_offset = val;
            break;
        case SEC_HDR_V3:
            sec_hdr->v3.sign_offset = val;
            break;
        default:
            SEC_ASSERT(0);
    }
}

/**************************************************************************
 *  VERSION
 **************************************************************************/

SEC_IMG_HEADER_VER get_shdr_ver (void)
{
    switch(sec_ver)
    {
        case SEC_HDR_V1:
        case SEC_HDR_V2:
        case SEC_HDR_V3:
            return sec_ver;
        default:
            SEC_ASSERT(0);
            return 0;            
    }
}

void set_shdr_ver (SEC_IMG_HEADER_VER ver)
{
    switch(ver)
    {
        case SEC_HDR_V1:
        case SEC_HDR_V2:
        case SEC_HDR_V3:
            sec_ver = ver;       
            break;
        default:
            SEC_ASSERT(0);
    }
}
