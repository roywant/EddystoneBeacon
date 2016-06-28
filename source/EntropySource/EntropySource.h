#ifndef __ENTROPY_SOURCE_H__
#define __ENTROPY_SOURCE_H__
#include "stddef.h"

    int eddystoneEntropyPoll( void *data,
                        unsigned char *output, size_t len, size_t *olen );
                        
#endif /* #ifndef __BLE_CONFIG_PARAMS_PERSISTENCE_H__*/