
#include <mach/mt_typedefs.h>
#include <mach/mt_sec_hal.h>
#include <mach/sec_osal.h>

#include "hacc_mach.h"
#include "sec_error.h"

extern int open_sdriver_connection(void);
extern int tee_secure_request(unsigned int user, unsigned char *data, unsigned int data_size, 
    unsigned int direction, unsigned char *seed, unsigned int seed_size);
extern int close_sdriver_connection(void);

/* To turn on HACC module clock if required */
unsigned char masp_hal_secure_algo_init(void)
{
    bool ret = TRUE;

    return ret;
}

/* To turn off HACC module clock if required */
unsigned char masp_hal_secure_algo_deinit(void)
{
    bool ret = TRUE;

    return ret;
}

/* This function will not work in TEE case */
unsigned int masp_hal_sp_hacc_init (unsigned char *sec_seed, unsigned int size)
{
    /* No implemtation is required in TEE's case */
    return 0;
}

unsigned int masp_hal_sp_hacc_blk_sz (void)
{
    return AES_BLK_SZ;
}

static char* hacc_secure_request(HACC_USER user, unsigned char *buf, unsigned int buf_size, 
    BOOL bEncrypt, BOOL bDoLock, unsigned char *sec_seed, unsigned int seed_size)
{
    unsigned int ret = SEC_OK;

    /* get hacc lock */
    if(TRUE == bDoLock)
    {
        /* If the semaphore is successfully acquired, this function returns 0.*/            
        ret = osal_hacc_lock();

        if(ret)
        {
            ret = ERR_SBOOT_HACC_LOCK_FAIL;
            goto _exit;        
        }
    }
    /* turn on clock */
    masp_hal_secure_algo_init();


    if(buf_size != 0)
    {
        /* try to open connection to TEE */
        if(open_sdriver_connection() < 0)
        {
            ret = ERR_HACC_OPEN_SECURE_CONNECTION_FAIL;
            goto _exit;
        }

        /* send request to TEE */
        if( (ret = tee_secure_request((unsigned int)user, buf, buf_size, (unsigned int)bEncrypt, sec_seed, seed_size)) != SEC_OK)
        {
            ret = ERR_HACC_REQUEST_SECURE_SERVICE_FAIL;
            goto _exit;
        }

        if(close_sdriver_connection() < 0)
        {
            ret = ERR_HACC_CLOSE_SECURE_CONNECTION_FAIL;
            goto _exit;
        }
    }
    else
    {
        printk("[HACC] hacc_secure_request - buffer size is 0, no encryption or decyrption is performed\n");
    }


_exit:
    /* turn off clock */
    masp_hal_secure_algo_deinit();
    /* release hacc lock */
    if(TRUE == bDoLock)
    {    
        osal_hacc_unlock();
    }

    if(ret)
    {
        printk("[HACC] hacc_secure_request fail (0x%x) (don't ASSERT)\n", ret);   
            
        //ASSERT(0);
    }

    return buf;
}

void masp_hal_secure_algo(unsigned char Direction, unsigned char *ContentAddr, unsigned int ContentLen, unsigned char *CustomSeed, unsigned char *ResText)
{    
    unsigned int err = 0;
    unsigned char *src, *dst;
    unsigned int i = 0;

    /* try to get hacc lock */         
    do
    {
        /* If the semaphore is successfully acquired, this function returns 0.*/       
        err = osal_hacc_lock();
    }while( 0 != err );

    /* initialize source and destination address */
    src = (unsigned char *)ContentAddr;
    dst = (unsigned char *)ResText;  
    
    /* according to input parameter to encrypt or decrypt */
    switch (Direction)
    {
        case TRUE:     
            dst = hacc_secure_request(HACC_USER3, (unsigned char*)src, ContentLen, TRUE, FALSE, CustomSeed, _CRYPTO_SEED_LEN);//encrypt
            break;

        case FALSE:
            dst = hacc_secure_request(HACC_USER3, (unsigned char*)src, ContentLen, FALSE, FALSE, CustomSeed, _CRYPTO_SEED_LEN);//decrypt
            break;

        default:
            err = ERR_KER_CRYPTO_INVALID_MODE;
            goto _wrong_direction;
    }

    /* copy result */
    for (i=0; i < ContentLen; i++)
    {   
        *(ResText+i) = *(dst+i);
    }

_wrong_direction:
    /* try to release hacc lock */
    osal_hacc_unlock();

    if(err)
    {
        printk("[HACC] masp_hal_secure_algo error (0x%x) (don't ASSERT)\n", err);   
        //ASSERT(0);
    }
}

/*
 * For SECRO (user1), this function will help to get hacc lock
 * For SECCFG (user1-sbchk), it should get hacc lock via ioctl command before using this function
 * For MD NVRAM (user3), it should get hacc lock before using this function
 * For AP NVRAM (user2), it should get hacc lock via ioctl command before using this function
 */
unsigned char* masp_hal_sp_hacc_enc(unsigned char *buf, unsigned int size, unsigned char bAC, HACC_USER user, unsigned char bDoLock)
{
    return hacc_secure_request(user, buf, size, TRUE, bDoLock, NULL, 0);
}

unsigned char* masp_hal_sp_hacc_dec(unsigned char *buf, unsigned int size, unsigned char bAC, HACC_USER user, unsigned char bDoLock)
{
    return hacc_secure_request(user, buf, size, FALSE, bDoLock, NULL, 0);
}

