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
 * Function: Save logs to flash.
 * Created on: 2015-06-04
 */

#include "flash.h"

#ifdef FLASH_USING_LOG

/* the stored logs start address and end address. It's like a ring buffer which implement by flash. */
static uint32_t log_start_addr = 0, log_end_addr = 0;
/* saved log area address for flash */
static uint32_t log_area_start_addr = 0;
/* saved log area total size */
static size_t flash_log_size = 0;
/* the minimum size of flash erasure */
static size_t flash_erase_min_size = 0;
/* initialize OK flag */
static bool init_ok = false;

static void find_start_and_end_addr(void);
static uint32_t get_next_flash_sec_addr(uint32_t cur_addr);

/**
 * The flash save log function initialize.
 *
 * @param start_addr log area start address
 * @param log_size log area total size
 * @param erase_min_size the minimum size of flash erasure
 *
 * @return result
 */
FlashErrCode flash_log_init(uint32_t start_addr, size_t log_size, size_t erase_min_size) {
    FlashErrCode result = FLASH_NO_ERR;

    FLASH_ASSERT(start_addr);
    FLASH_ASSERT(log_size);
    FLASH_ASSERT(erase_min_size);
    /* the log area size must be an integral multiple of erase minimum size. */
    FLASH_ASSERT(log_size % erase_min_size == 0);
    /* the log area size must be more than 2 multiple of erase minimum size */
    FLASH_ASSERT(log_size / erase_min_size >= 2);

    log_area_start_addr = start_addr;
    flash_log_size = log_size;
    flash_erase_min_size = erase_min_size;

    /* find the log store start address and end address */
    find_start_and_end_addr();
    /* initialize OK */
    init_ok = true;

    return result;
}

/**
 * Find the log store start address and end address.
 * It's like a ring buffer which implement by flash.
 * The flash log area has two state when find start address and end address.
 *                        state 1                                state 2
 *                   |============|                         |============|
 * log area start--> |############| <-- start address       |############| <-- end address
 *                   |############|                         |   empty    |
 *                   |------------|                         |------------|
 *                   |############|                         |############| <-- start address
 *                   |############|                         |############|
 *                   |------------|                         |------------|
 *                   |     .      |                         |     .      |
 *                   |     .      |                         |     .      |
 *                   |     .      |                         |     .      |
 *                   |------------|                         |------------|
 *                   |############| <-- end address         |############|
 *                   |   empty    |                         |############|
 *  log area end --> |============|                         |============|
 *
 *  flash_log_size = log area end - log area star
 *
 */
static void find_start_and_end_addr(void) {
    size_t cur_size = 0;
    FlashSecrorStatus cur_sec_status, last_sec_status;
    uint32_t cur_using_sec_addr = 0;
    /* all status sector counts */
    size_t empty_sec_counts = 0, using_sec_counts = 0, full_sector_counts = 0;
    /* total sector number */
    size_t total_sec_num = flash_log_size / flash_erase_min_size;
    /* see comment of find_start_and_end_addr function */
    uint8_t cur_log_sec_state = 0;

    /* get the first sector status */
    cur_sec_status = flash_get_sector_status(log_area_start_addr, flash_erase_min_size);
    last_sec_status = cur_sec_status;

    for (cur_size = flash_erase_min_size; cur_size < flash_log_size; cur_size += flash_erase_min_size) {
        /* get current sector status */
        cur_sec_status = flash_get_sector_status(log_area_start_addr + cur_size, flash_erase_min_size);
        /* compare last and current status */
        switch (last_sec_status) {
        case FLASH_SECTOR_EMPTY: {
            switch (cur_sec_status) {
            case FLASH_SECTOR_EMPTY:
                break;
            case FLASH_SECTOR_USING:
                FLASH_DEBUG("Error: Log area error! Now will clean all log area.\n");
                flash_log_clean();
                return;
            case FLASH_SECTOR_FULL:
                FLASH_DEBUG("Error: Log area error! Now will clean all log area.\n");
                flash_log_clean();
                return;
            }
            empty_sec_counts++;
            break;
        }
        case FLASH_SECTOR_USING: {
            switch (cur_sec_status) {
            case FLASH_SECTOR_EMPTY:
                /* like state 1 */
                cur_log_sec_state = 1;
                log_start_addr = log_area_start_addr;
                cur_using_sec_addr = log_area_start_addr + cur_size - flash_erase_min_size;
                break;
            case FLASH_SECTOR_USING:
                FLASH_DEBUG("Error: Log area error! Now will clean all log area.\n");
                flash_log_clean();
                return;
            case FLASH_SECTOR_FULL:
                /* like state 2 */
                cur_log_sec_state = 2;
                log_start_addr = log_area_start_addr + cur_size;
                cur_using_sec_addr = log_area_start_addr + cur_size - flash_erase_min_size;
                break;
            }
            using_sec_counts++;
            break;
        }
        case FLASH_SECTOR_FULL: {
            switch (cur_sec_status) {
            case FLASH_SECTOR_EMPTY:
                /* like state 1 */
                if (cur_log_sec_state == 2) {
                    FLASH_DEBUG("Error: Log area error! Now will clean all log area.\n");
                    flash_log_clean();
                    return;
                } else {
                    cur_log_sec_state = 1;
                    log_start_addr = log_area_start_addr;
                    /* word alignment */
                    log_end_addr = log_area_start_addr + cur_size - 4;
                    cur_using_sec_addr = log_area_start_addr + cur_size - flash_erase_min_size;
                }
                break;
            case FLASH_SECTOR_USING:
                if(total_sec_num <= 2) {
                    /* like state 1 */
                    cur_log_sec_state = 1;
                    log_start_addr = log_area_start_addr;
                    cur_using_sec_addr = log_area_start_addr + cur_size;
                } else {
                    /* state 1 or 2*/
                }
                break;
            case FLASH_SECTOR_FULL:
                break;
            }
            full_sector_counts++;
            break;
        }
        }
        last_sec_status = cur_sec_status;
    }

    /* the last sector status counts */
    if (cur_sec_status == FLASH_SECTOR_EMPTY) {
        empty_sec_counts++;
    } else if (cur_sec_status == FLASH_SECTOR_USING) {
        using_sec_counts++;
    } else if (cur_sec_status == FLASH_SECTOR_FULL) {
        full_sector_counts++;
    }

    if (using_sec_counts > 1) {
        FLASH_DEBUG("Error: Log area error! Now will clean all log area.\n");
        flash_log_clean();
        return;
    } else if (empty_sec_counts == total_sec_num) {
        log_start_addr = log_end_addr = log_area_start_addr;
    } else if (full_sector_counts == total_sec_num) {
        /* this state is almost impossible */
        FLASH_DEBUG("Error: Log area error! Now will clean all log area.\n");
        flash_log_clean();
        return;
    } else if (((cur_log_sec_state == 1) && (cur_using_sec_addr != 0))
            || (cur_log_sec_state == 2)) {
        /* find the end address */
        log_end_addr = flash_find_sec_using_end_addr(cur_using_sec_addr, flash_erase_min_size);
    }
}

/**
 * Get log used flash total size.
 *
 * @return log used flash total size
 */
size_t flash_log_get_used_size(void) {
    FLASH_ASSERT(log_start_addr);
    FLASH_ASSERT(log_end_addr);

    /* must be call this function after initialize OK */
    FLASH_ASSERT(init_ok);

    if (log_start_addr < log_end_addr) {
        return log_end_addr - log_start_addr + 4;
    } else if (log_start_addr > log_end_addr) {
        return flash_log_size - (log_start_addr - log_end_addr) + 4;
    } else {
        return 0;
    }
}

/**
 * Read log from flash.
 *
 * @param index index for saved log.
 *        Minimum index is 0.
 *        Maximum index is log used flash total size - 1.
 * @param log the log which will read from flash
 * @param size read bytes size
 *
 * @return result
 */
FlashErrCode flash_log_read(size_t index, uint32_t *log, size_t size) {
    FlashErrCode result = FLASH_NO_ERR;
    size_t cur_using_size = flash_log_get_used_size();
    size_t read_size_temp = 0;

    FLASH_ASSERT(size % 4 == 0);
    FLASH_ASSERT(index + size <= cur_using_size);
    /* must be call this function after initialize OK */
    FLASH_ASSERT(init_ok);

    if (!size) {
        return result;
    }

    if (log_start_addr < log_end_addr) {
        result = flash_read(log_area_start_addr + index, log, size);
    } else if (log_start_addr > log_end_addr) {
        if (log_start_addr + index + size <= log_area_start_addr + flash_log_size) {
            /*                          Flash log area
             *                         |--------------|
             * log_area_start_addr --> |##############|
             *                         |##############|
             *                         |##############|
             *                         |--------------|
             *                         |##############|
             *                         |##############|
             *                         |##############| <-- log_end_addr
             *                         |--------------|
             *      log_start_addr --> |##############|
             *          read start --> |**************| <-- read end
             *                         |##############|
             *                         |--------------|
             *
             * read from (log_start_addr + index) to (log_start_addr + index + size)
             */
            result = flash_read(log_start_addr + index, log, size);
        } else if (log_start_addr + index < log_area_start_addr + flash_log_size) {
            /*                          Flash log area
             *                         |--------------|
             * log_area_start_addr --> |**************| <-- read end
             *                         |##############|
             *                         |##############|
             *                         |--------------|
             *                         |##############|
             *                         |##############|
             *                         |##############| <-- log_end_addr
             *                         |--------------|
             *      log_start_addr --> |##############|
             *          read start --> |**************|
             *                         |**************|
             *                         |--------------|
             * read will by 2 steps
             * step1: read from (log_start_addr + index) to flash log area end address
             * step2: read from flash log area start address to read size's end address
             */
            read_size_temp = (log_area_start_addr + flash_log_size) - (log_start_addr + index);
            result = flash_read(log_start_addr + index, log, read_size_temp);
            if (result == FLASH_NO_ERR) {
                result = flash_read(log_area_start_addr, log + read_size_temp,
                        size - read_size_temp);
            }
        } else {
            /*                          Flash log area
             *                         |--------------|
             * log_area_start_addr --> |##############|
             *          read start --> |**************|
             *                         |**************| <-- read end
             *                         |--------------|
             *                         |##############|
             *                         |##############|
             *                         |##############| <-- log_end_addr
             *                         |--------------|
             *      log_start_addr --> |##############|
             *                         |##############|
             *                         |##############|
             *                         |--------------|
             * read from (log_start_addr + index - flash_log_size) to read size's end address
             */
            result = flash_read(log_start_addr + index - flash_log_size, log, size);
        }
    }

    return result;
}

/**
 * Write log to flash.
 *
 * @param log the log which will be write to flash
 * @param size write bytes size
 *
 * @return result
 */
FlashErrCode flash_log_write(const uint32_t *log, size_t size) {
    FlashErrCode result = FLASH_NO_ERR;
    size_t cur_using_size = flash_log_get_used_size(), write_size = 0, writable_size = 0;
    uint32_t write_addr, erase_addr;

    FLASH_ASSERT(size % 4 == 0);
    /* must be call this function after initialize OK */
    FLASH_ASSERT(init_ok);

    /* write address is after log end address  */
    write_addr = log_end_addr + 4;
    /* write the already erased but not used area */
    writable_size = flash_erase_min_size - ((write_addr - log_area_start_addr) % flash_erase_min_size);
    if (writable_size != flash_erase_min_size) {
        if (size > writable_size) {
            result = flash_write(write_addr, log, writable_size);
            if (result != FLASH_NO_ERR) {
                goto exit;
            }
            write_size += writable_size;
        } else {
            result = flash_write(write_addr, log, size);
            log_end_addr = write_addr + size - 4;
            goto exit;
        }
    }
    /* erase and write remain log */
    while (true) {
        /* calculate next available sector address */
        erase_addr = write_addr = get_next_flash_sec_addr(write_addr - 4);
        /* move the flash log start address to next available sector address */
        if (log_start_addr == erase_addr) {
            log_start_addr = get_next_flash_sec_addr(log_start_addr);
        }
        /* erase sector */
        result = flash_erase(erase_addr, flash_erase_min_size);
        if (result == FLASH_NO_ERR) {
            if (size - write_size > flash_erase_min_size) {
                result = flash_write(write_addr, log + write_size / 4, flash_erase_min_size);
                if (result != FLASH_NO_ERR) {
                    goto exit;
                }
                log_end_addr = write_addr + flash_erase_min_size - 4;
                write_size += flash_erase_min_size;
                write_addr += flash_erase_min_size;
            } else {
                result = flash_write(write_addr, log + write_size / 4, size - write_size);
                if (result != FLASH_NO_ERR) {
                    goto exit;
                }
                log_end_addr = write_addr + (size - write_size) - 4;
                break;
            }
        } else {
            goto exit;
        }
    }

exit:
    return result;
}

/**
 * Get next flash sector address.The log total sector like ring buffer which implement by flash.
 *
 * @param cur_addr cur flash address
 *
 * @return next flash sector address
 */
static uint32_t get_next_flash_sec_addr(uint32_t cur_addr) {
    size_t cur_sec_id = (cur_addr - log_area_start_addr) / flash_erase_min_size;
    size_t sec_total_num = flash_log_size / flash_erase_min_size;
    if (cur_sec_id + 1 >= sec_total_num) {
        /* return to ring head */
        return log_area_start_addr;
    } else {
        return log_area_start_addr + (cur_sec_id + 1) * flash_erase_min_size;
    }
}

/**
 * Clean all log which in flash.
 *
 * @return result
 */
FlashErrCode flash_log_clean(void) {
    FlashErrCode result = FLASH_NO_ERR;

    FLASH_ASSERT(log_area_start_addr);
    FLASH_ASSERT(flash_log_size);

    /* clean address */
    log_start_addr = log_end_addr = log_area_start_addr;
    /* erase log flash area */
    result = flash_erase(log_area_start_addr, flash_log_size);

    return result;
}

#endif
