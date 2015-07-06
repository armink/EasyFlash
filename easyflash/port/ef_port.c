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

#include "easyflash.h"

/* environment variables start address */
#define ENV_START_ADDR               /* @note you must define it for a value */
/* the minimum size of flash erasure */
#define ERASE_MIN_SIZE               /* @note you must define it for a value */
#ifdef EF_ENV_USING_WL_MODE
/* ENV section total bytes size in wear leveling mode. */
#define ENV_SECTION_SIZE             /* @note you must define it for a value */
#else
/* ENV section total bytes size in normal mode. It's equal with FLASH_USER_SETTING_ENV_SIZE */
#define ENV_SECTION_SIZE          (EF_USER_SETTING_ENV_SIZE)
#endif
/* saved log section size */
#define LOG_AREA_SIZE               /* @note you must define it for a value */
/* print debug information of flash */
#define PRINT_DEBUG

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
EfErrCode ef_port_init(uint32_t *env_addr, size_t *env_total_size, size_t *erase_min_size,
        ef_env const **default_env, size_t *default_env_size, size_t *log_size) {
    EfErrCode result = EF_NO_ERR;

    EF_ASSERT(EF_USER_SETTING_ENV_SIZE % 4 == 0);
    EF_ASSERT(ENV_SECTION_SIZE % 4 == 0);

    *env_addr = ENV_START_ADDR;
    *env_total_size = ENV_SECTION_SIZE;
    *erase_min_size = ERASE_MIN_SIZE;
    *default_env = default_env_set;
    *default_env_size = sizeof(default_env_set) / sizeof(default_env_set[0]);
    *log_size = LOG_AREA_SIZE;

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

    EF_ASSERT(size % 4 == 0);

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
EfErrCode ef_port_erase(uint32_t addr, size_t size) {
    EfErrCode result = EF_NO_ERR;

    /* make sure the start address is a multiple of ERASE_MIN_SIZE */
    EF_ASSERT(addr % ERASE_MIN_SIZE == 0);

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
EfErrCode ef_port_write(uint32_t addr, const uint32_t *buf, size_t size) {
    EfErrCode result = EF_NO_ERR;

    EF_ASSERT(size % 4 == 0);
	
	/* You can add your code under here. */

    return result;
}

/**
 * lock the ENV ram cache
 */
void ef_port_env_lock(void) {
	
    /* You can add your code under here. */
	
}

/**
 * unlock the ENV ram cache
 */
void ef_port_env_unlock(void) {
	
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
void ef_log_debug(const char *file, const long line, const char *format, ...) {

#ifdef PRINT_DEBUG

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
void ef_log_info(const char *format, ...) {
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
void ef_print(const char *format, ...) {
    va_list args;

    /* args point to the first variable parameter */
    va_start(args, format);

	/* You can add your code under here. */
	
    va_end(args);
}
