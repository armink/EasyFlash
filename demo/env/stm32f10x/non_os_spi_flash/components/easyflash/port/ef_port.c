/*
 * This file is part of the EasyFlash Library.
 *
 * Copyright (c) 2015, Armink, <armink.ztl@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * 'Software'), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED 'AS IS', WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * Function: Portable interface for stm32f10x platform.
 * Created on: 2015-01-16
 */

#include <easyflash.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stm32f10x_conf.h>
#include <sfud.h>

/* default environment variables set for user */
static const ef_env default_env_set[] = {
        {"iap_need_copy_app","0"},
        {"iap_copy_app_size","0"},
        {"stop_in_bootloader","0"},
        {"device_id","1"},
        {"boot_times","0"},
};

static char log_buf[128];

/**
 * Flash port for hardware initialize.
 *
 * @param default_env default ENV set for user
 * @param default_env_size default ENV size
 *
 * @return result
 */
EfErrCode ef_port_init(ef_env const **default_env, size_t *default_env_size) {
    EfErrCode result = EF_NO_ERR;

    *default_env = default_env_set;
    *default_env_size = sizeof(default_env_set) / sizeof(default_env_set[0]);
    /* initialize SFUD library for SPI Flash */
    sfud_init();

    return result;
}

/**
 * Read data from flash.
 * @note This operation's units is word.
 *
 * @param addr flash address
 * @param buf buffer to store read data
 * @param size read bytes size
 *
 * @return result
 */
EfErrCode ef_port_read(uint32_t addr, uint32_t *buf, size_t size) {
    EfErrCode result = EF_NO_ERR;
    const sfud_flash *flash = sfud_get_device_table() + SFUD_SST25_DEVICE_INDEX;

    EF_ASSERT(size % 4 == 0);

    sfud_read(flash, addr, size, (uint8_t *)buf);

    return result;
}

/**
 * Erase data on flash.
 * @note This operation is irreversible.
 * @note This operation's units is different which on many chips.
 *
 * @param addr flash address
 * @param size erase bytes size
 *
 * @return result
 */
EfErrCode ef_port_erase(uint32_t addr, size_t size) {
    EfErrCode result = EF_NO_ERR;
    sfud_err sfud_result = SFUD_SUCCESS;
    const sfud_flash *flash = sfud_get_device_table() + SFUD_SST25_DEVICE_INDEX;
    
    /* make sure the start address is a multiple of FLASH_ERASE_MIN_SIZE */
    EF_ASSERT(addr % EF_ERASE_MIN_SIZE == 0);
    
    sfud_result = sfud_erase(flash, addr, size);

    if(sfud_result != SFUD_SUCCESS) {
        result = EF_ERASE_ERR;
    }

    return result;
}
/**
 * Write data to flash.
 * @note This operation's units is word.
 * @note This operation must after erase. @see flash_erase.
 *
 * @param addr flash address
 * @param buf the write data buffer
 * @param size write bytes size
 *
 * @return result
 */
EfErrCode ef_port_write(uint32_t addr, const uint32_t *buf, size_t size) {
    EfErrCode result = EF_NO_ERR;
    sfud_err sfud_result = SFUD_SUCCESS;
    const sfud_flash *flash = sfud_get_device_table() + SFUD_SST25_DEVICE_INDEX;

    EF_ASSERT(size % 4 == 0);

    sfud_result = sfud_write(flash, addr, size, (const uint8_t *)buf);

    if(sfud_result != SFUD_SUCCESS) {
        result = EF_WRITE_ERR;
    }

    return result;
}

/**
 * lock the ENV ram cache
 */
void ef_port_env_lock(void) {
    __disable_irq();
}

/**
 * unlock the ENV ram cache
 */
void ef_port_env_unlock(void) {
    __enable_irq();
}


/**
 * This function is print flash debug info.
 *
 * @param file the file which has call this function
 * @param line the line number which has call this function
 * @param format output format
 * @param ... args
 *
 */
void ef_log_debug(const char *file, const long line, const char *format, ...) {

#ifdef PRINT_DEBUG

    va_list args;

    /* args point to the first variable parameter */
    va_start(args, format);
    ef_print("[Flash](%s:%ld) ", file, line);
    /* must use vprintf to print */
    vsprintf(log_buf, format, args);
    ef_print("%s", log_buf);
    printf("\r");
    va_end(args);

#endif

}

/**
 * This function is print flash routine info.
 *
 * @param format output format
 * @param ... args
 */
void ef_log_info(const char *format, ...) {
    va_list args;

    /* args point to the first variable parameter */
    va_start(args, format);
    ef_print("[Flash]");
    /* must use vprintf to print */
    vsprintf(log_buf, format, args);
    ef_print("%s", log_buf);
    printf("\r");
    va_end(args);
}
/**
 * This function is print flash non-package info.
 *
 * @param format output format
 * @param ... args
 */
void ef_print(const char *format, ...) {
    va_list args;

    /* args point to the first variable parameter */
    va_start(args, format);
    /* must use vprintf to print */
    vsprintf(log_buf, format, args);
    printf("%s", log_buf);
    va_end(args);
}
