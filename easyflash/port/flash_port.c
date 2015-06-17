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
 * Function: Portable interface for each platform.
 * Created on: 2015-01-16
 */

#include "flash.h"

/* environment variables start address */
#define FLASH_ENV_START_ADDR               /* @note you must define it for a value */
/* the minimum size of flash erasure */
#define FLASH_ERASE_MIN_SIZE               /* @note you must define it for a value */
#ifdef FLASH_ENV_USING_WEAR_LEVELING_MODE
/* ENV section total bytes size in wear leveling mode. */
#define FLASH_ENV_SECTION_SIZE             /* @note you must define it for a value */
#else
/* ENV section total bytes size in normal mode. It's equal with FLASH_USER_SETTING_ENV_SIZE */
#define FLASH_ENV_SECTION_SIZE          (FLASH_USER_SETTING_ENV_SIZE)
#endif
/* saved log section size */
#define FLASH_LOG_AREA_SIZE               /* @note you must define it for a value */
/* print debug information of flash */
#define FLASH_PRINT_DEBUG

/* default environment variables set for user */
static const flash_env default_env_set[] = {

};

/**
 * Flash port for hardware initialize.
 *
 * @param env_addr ENV start address
 * @param env_total_size ENV sector total bytes size (@note must be word alignment)
 * @param erase_min_size the minimum size of Flash erasure
 * @param default_env default ENV set for user
 * @param default_env_size default ENV size
 * @param log_total_size saved log area size
 *
 * @return result
 */
FlashErrCode flash_port_init(uint32_t *env_addr, size_t *env_total_size, size_t *erase_min_size,
        flash_env const **default_env, size_t *default_env_size, size_t *log_size) {
    FlashErrCode result = FLASH_NO_ERR;

    FLASH_ASSERT(FLASH_USER_SETTING_ENV_SIZE % 4 == 0);
    FLASH_ASSERT(FLASH_ENV_SECTION_SIZE % 4 == 0);

    *env_addr = FLASH_ENV_START_ADDR;
    *env_total_size = FLASH_ENV_SECTION_SIZE;
    *erase_min_size = FLASH_ERASE_MIN_SIZE;
    *default_env = default_env_set;
    *default_env_size = sizeof(default_env_set)/sizeof(default_env_set[0]);
    *log_size = FLASH_LOG_AREA_SIZE;

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
FlashErrCode flash_read(uint32_t addr, uint32_t *buf, size_t size) {
    FlashErrCode result = FLASH_NO_ERR;
	
    FLASH_ASSERT(size % 4 == 0);

    /* You can add your code under here. */

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
FlashErrCode flash_erase(uint32_t addr, size_t size) {
    FlashErrCode result = FLASH_NO_ERR;
	
	/* make sure the start address is a multiple of FLASH_ERASE_MIN_SIZE */
    FLASH_ASSERT(addr % FLASH_ERASE_MIN_SIZE == 0);

	/* You can add your code under here. */

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
FlashErrCode flash_write(uint32_t addr, const uint32_t *buf, size_t size) {
    FlashErrCode result = FLASH_NO_ERR;

	FLASH_ASSERT(size % 4 == 0);
	
	/* You can add your code under here. */

    return result;
}

/**
 * lock the ENV ram cache
 */
void flash_env_lock(void) {
	
    /* You can add your code under here. */
	
}

/**
 * unlock the ENV ram cache
 */
void flash_env_unlock(void) {
	
    /* You can add your code under here. */
	
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
void flash_log_debug(const char *file, const long line, const char *format, ...) {

#ifdef FLASH_PRINT_DEBUG

    va_list args;

    /* args point to the first variable parameter */
    va_start(args, format);

	/* You can add your code under here. */
	
    va_end(args);

#endif

}

/**
 * This function is print flash routine info.
 *
 * @param format output format
 * @param ... args
 */
void flash_log_info(const char *format, ...) {
    va_list args;

    /* args point to the first variable parameter */
    va_start(args, format);

	/* You can add your code under here. */
	
    va_end(args);
}
/**
 * This function is print flash non-package info.
 *
 * @param format output format
 * @param ... args
 */
void flash_print(const char *format, ...) {
    va_list args;

    /* args point to the first variable parameter */
    va_start(args, format);

	/* You can add your code under here. */
	
    va_end(args);
}
