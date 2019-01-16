#ifndef SEC_WRAPPER_H
#define SEC_WRAPPER_H

int sec_init_key (unsigned char *nKey, unsigned int nKey_len, 
    unsigned char *eKey, unsigned int eKey_len);
int sec_hash(unsigned char *data_buf,  unsigned int data_len, 
    unsigned char *hash_buf, unsigned int hash_len);
int sec_verify (unsigned char* data_buf,  unsigned int data_len, 
    unsigned char* sig_buf, unsigned int sig_len);

#endif

