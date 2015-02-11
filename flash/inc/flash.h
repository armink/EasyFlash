/*
 * This file is part of the EasyFlash Library.
 *
 * Copyright (C) 2013 by Armink <armink.ztl@gmail.com>
 *
 * Function: Is is an head file in this library. You can see all be called functions.
 * Created on: 2014-09-10
 */


#ifndef FLASH_H_
#define FLASH_H_

#include "types.h"

/* using CRC32 check when load environment variable from Flash */
#define FLASH_ENV_USING_CRC_CHECK

/* Flash debug print function. Must be implement by user. */
#define FLASH_DEBUG(...) flash_log_debug(__FILE__, __LINE__, __VA_ARGS__)
/* Flash routine print function. Must be implement by user. */
#define FLASH_INFO(...) flash_log_info(__VA_ARGS__)
/* Flash assert for developer. */
#define FLASH_ASSERT(EXPR)                                                    \
if (!(EXPR))                                                                  \
{                                                                             \
    FLASH_DEBUG("(%s) has assert failed at %s.\n", #EXPR, __FUNCTION__);     \
    while (1);                                                                \
}
/* EasyFlash software version number */
#define FLASH_SW_VERSION                "1.02.11"

typedef struct _flash_env{
    char *key;
    char *value;
}flash_env, *flash_env_t;

/* Flash error code */
typedef enum {
    FLASH_NO_ERR,
    FLASH_ERASE_ERR,
    FLASH_WRITE_ERR,
    FLASH_ENV_NAME_ERR,
    FLASH_ENV_NAME_EXIST,
    FLASH_ENV_FULL,
} FlashErrCode;

/* flash.c */
FlashErrCode flash_init(void);

/* flash_env.c */
void flash_load_env(void);
void flash_print_env(void);
char *flash_get_env(const char *key);
FlashErrCode flash_set_env(const char *key, const char *value);
FlashErrCode flash_save_env(void);
FlashErrCode flash_env_set_default(void);
uint32_t flash_get_env_total_size(void);
uint32_t flash_get_env_used_size(void);

/* flash_iap.c */
FlashErrCode flash_erase_bak_app(size_t app_size);
FlashErrCode flash_erase_user_app(uint32_t user_app_addr, size_t user_app_size);
FlashErrCode flash_erase_bl(uint32_t bl_addr, size_t bl_size);
FlashErrCode flash_write_data_to_bak(uint8_t *data, size_t size, size_t *cur_size,
        size_t total_size);
FlashErrCode flash_copy_app_from_bak(uint32_t user_app_addr, size_t app_size);
FlashErrCode flash_copy_bl_from_bak(uint32_t bl_addr, size_t bl_size);

/* flash_port.c */
FlashErrCode flash_read(uint32_t addr, uint32_t *buf, size_t size);
FlashErrCode flash_erase(uint32_t addr, size_t size);
FlashErrCode flash_write(uint32_t addr, const uint32_t *buf, size_t size);
void *flash_malloc(size_t size);
void flash_free(void *p);
void flash_log_debug(const char *file, const long line, const char *format, ...);
void flash_log_info(const char *format, ...);
void flash_print(const char *format, ...);

#endif /* FLASH_H_ */
