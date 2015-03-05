/*
 * This file is part of the EasyFlash Library.
 *
 * Copyright (c) 2015, Armink, <armink.ztl@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Function: Portable interface for each platform.
 * Created on: 2015-01-16
 */

#include "flash.h"

/* environment variables start address */
#define FLASH_ENV_START_ADDR               /* @note you must define it for a value */
/* the minimum size of flash erasure */
#define #define FLASH_ERASE_MIN_SIZE       /* @note you must define it for a value */
/* environment variables bytes size */
#define FLASH_ENV_SECTION_SIZE             /* @note you must define it for a value */
/* print debug information of flash */
#define FLASH_PRINT_DEBUG

/* default environment variables set for user */
static const flash_env default_env_set[] = {

};

/**
 * Flash port for hardware initialize.
 *
 * @param env_addr environment variables start address
 * @param env_size environment variables bytes size (@note must be word alignment)
 * @param erase_min_size the minimum size of Flash erasure
 * @param default_env default environment variables set for user
 * @param default_env_size default environment variables size
 *
 * @return result
 */
FlashErrCode flash_port_init(uint32_t *env_addr, size_t *env_size, size_t *erase_min_size,
        flash_env const **default_env, size_t *default_env_size) {
    FlashErrCode result = FLASH_NO_ERR;

    FLASH_ASSERT(FLASH_ENV_SECTION_SIZE % 4 == 0);

    *env_addr = FLASH_ENV_START_ADDR;
    *env_size = FLASH_ENV_SECTION_SIZE;
    *erase_min_size = FLASH_ERASE_MIN_SIZE;
    *default_env = default_env_set;
    *default_env_size = sizeof(default_env_set)/sizeof(default_env_set[0]);

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

	/* You can add your code under here. */

    return result;
}

/**
 * Allocate a block of memory with a minimum of 'size' bytes.
 *
 * @param size is the minimum size of the requested block in bytes.
 *
 * @return pointer to allocated memory or NULL if no free memory was found.
 */
void *flash_malloc(size_t size) {
    return rt_malloc(size);
}

/**
 * This function will release the previously allocated memory block by
 * flash_malloc. The released memory block is taken back to system heap.
 *
 * @param p the pointer to allocated memory which will be released
 */
void flash_free(void *p) {
	
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
