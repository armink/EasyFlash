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
 * Function: Portable interface for stm32f10x platform.
 * Created on: 2015-01-16
 */

#include "flash.h"
#include <rthw.h>
#include <rtthread.h>
#include <stm32f10x_conf.h>

/* page size for stm32 flash */
#if defined(STM32F10X_LD) || defined(STM32F10X_LD_VL) || defined (STM32F10X_MD) || defined (STM32F10X_MD_VL)
#define PAGE_SIZE     1024
#else
#define PAGE_SIZE     2048
#endif

/* Environment variables start address */
#define FLASH_ENV_START_ADDR            (FLASH_BASE + 100 * 1024) /* from the chip position: 100KB */
/* the minimum size of flash erasure */
#define FLASH_ERASE_MIN_SIZE             PAGE_SIZE                /* it is one page for STM32 */
/* the user setting size of ENV */
#define FLASH_USER_SETTING_ENV_SIZE      PAGE_SIZE                /* a page size */
#ifdef FLASH_ENV_USING_WEAR_LEVELING_MODE
/* ENV section total bytes size in wear leveling mode. */
#define FLASH_ENV_SECTION_SIZE          (4 * FLASH_ERASE_MIN_SIZE)/* 8K */
#else
/* ENV section total bytes size in normal mode. It's equal with FLASH_USER_SETTING_ENV_SIZE */
#define FLASH_ENV_SECTION_SIZE          (FLASH_USER_SETTING_ENV_SIZE)
#endif
/* print debug information of flash */
#define FLASH_PRINT_DEBUG

/* default environment variables set for user */
static const flash_env default_env_set[] = {
        {"iap_need_copy_app","0"},
        {"iap_copy_app_size","0"},
        {"stop_in_bootloader","0"},
        {"device_id","1"},
        {"boot_times","0"},
};

static char log_buf[RT_CONSOLEBUF_SIZE];

/**
 * Flash port for hardware initialize.
 *
 * @param env_addr ENV start address
 * @param env_user_size user setting ENV bytes size (@note must be word alignment)
 * @param env_total_size ENV sector total bytes size (@note must be word alignment)
 * @param erase_min_size the minimum size of Flash erasure
 * @param default_env default ENV set for user
 * @param default_env_size default ENV size
 *
 * @return result
 */
FlashErrCode flash_port_init(uint32_t *env_addr, size_t *env_user_size, size_t *env_total_size,
        size_t *erase_min_size, flash_env const **default_env, size_t *default_env_size) {
    FlashErrCode result = FLASH_NO_ERR;

    FLASH_ASSERT(FLASH_USER_SETTING_ENV_SIZE % 4 == 0);
    FLASH_ASSERT(FLASH_ENV_SECTION_SIZE % 4 == 0);

    *env_addr = FLASH_ENV_START_ADDR;
    *env_user_size = FLASH_USER_SETTING_ENV_SIZE;
    *env_total_size = FLASH_ENV_SECTION_SIZE;
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

    FLASH_ASSERT(size >= 4);
    FLASH_ASSERT(size % 4 == 0);

    /*copy from flash to ram */
    for (; size > 0; size -= 4, addr += 4, buf++) {
        *buf = *(uint32_t *) addr;
    }

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
    FLASH_Status flash_status;
    size_t erase_pages, i;
    
    /* make sure the start address is a multiple of FLASH_ERASE_MIN_SIZE */
    FLASH_ASSERT(addr % FLASH_ERASE_MIN_SIZE == 0);
    
    /* calculate pages */
    erase_pages = size / PAGE_SIZE;
    if (size % PAGE_SIZE != 0) {
        erase_pages++;
    }

    /* start erase */
    FLASH_Unlock();
    FLASH_ClearFlag(FLASH_FLAG_BSY | FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPRTERR);
    for (i = 0; i < erase_pages; i++) {
        flash_status = FLASH_ErasePage(addr + (PAGE_SIZE * i));
        if (flash_status != FLASH_COMPLETE) {
            result = FLASH_ERASE_ERR;
            break;
        }
    }
    FLASH_Lock();

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
    size_t i;
    uint32_t read_data;

    FLASH_Unlock();
    FLASH_ClearFlag(FLASH_FLAG_BSY | FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPRTERR);
    for (i = 0; i < size; i += 4, buf++, addr += 4) {
        /* write data */
        FLASH_ProgramWord(addr, *buf);
        read_data = *(uint32_t *)addr;
        /* check data */
        if (read_data != *buf) {
            result = FLASH_WRITE_ERR;
            break;
        }
    }
    FLASH_Lock();

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
    rt_free(p);
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
    flash_print("[Flash](%s:%ld) ", file, line);
    /* must use vprintf to print */
    rt_vsprintf(log_buf, format, args);
    flash_print("%s", log_buf);
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
    flash_print("[Flash]");
    /* must use vprintf to print */
    rt_vsprintf(log_buf, format, args);
    flash_print("%s", log_buf);
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
    /* must use vprintf to print */
    rt_vsprintf(log_buf, format, args);
    rt_kprintf("%s", log_buf);
    va_end(args);
}
