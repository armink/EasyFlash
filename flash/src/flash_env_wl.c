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
 * Function: Environment variables operating interface. (wear leveling mode)
 * Created on: 2015-02-11
 */

#include "flash.h"
#include <string.h>
#include <stdlib.h>

#ifdef FLASH_ENV_USING_WEAR_LEVELING_MODE

/**
 * ENV area has 2 sections
 * 1. System section
 *    Storage ENV current using data section address.
 *    Units: Word. Total size: @see FLASH_ERASE_MIN_SIZE.
 * 2. Data section
 *    The data section storage ENV's parameters and detail.
 *    When an exception has occurred on flash erase or write. The current using data section
 *    address will move to next available position. This position depends on FLASH_MIN_ERASE_SIZE.
 *    2.1 ENV parameters part
 *        It storage ENV's parameters.
 *    2.2 ENV detail part
 *        It storage all ENV. Storage format is key=value\0.
 *        All ENV must be 4 bytes alignment. The remaining part must fill '\0'.
 *
 * @note Word = 4 Bytes in this file
 */

/* flash ENV parameters part index and size */
enum {
    /* data section ENV detail part end address index */
    ENV_PARAM_PART_INDEX_END_ADDR = 0,

#ifdef FLASH_ENV_USING_CRC_CHECK
    /* data section CRC32 code index */
    ENV_PARAM_PART_INDEX_DATA_CRC,
#endif

    /* ENV parameters part word size */
    ENV_PARAM_PART_WORD_SIZE,
    /* ENV parameters part byte size */
    ENV_PARAM_PART_BYTE_SIZE = ENV_PARAM_PART_WORD_SIZE * 4,
};

/* default ENV set, must be initialized by user */
static flash_env const *default_env_set = NULL;
/* default ENV set size, must be initialized by user */
static size_t default_env_set_size = NULL;
/* flash ENV all section total size */
static size_t env_total_size = NULL;
/* the minimum size of flash erasure */
static size_t flash_erase_min_size = NULL;
/* ENV RAM cache */
static uint32_t env_cache[FLASH_USER_SETTING_ENV_SIZE / 4] = { 0 };
/* ENV start address in flash */
static uint32_t env_start_addr = NULL;
/* current using data section address */
static uint32_t cur_using_data_addr = NULL;

static uint32_t get_env_start_addr(void);
static uint32_t get_cur_using_data_addr(void);
static uint32_t get_env_detail_addr(void);
static uint32_t get_env_detail_end_addr(void);
static void set_cur_using_data_addr(uint32_t using_data_addr);
static void set_env_detail_end_addr(uint32_t end_addr);
static FlashErrCode write_env(const char *key, const char *value);
static uint32_t *find_env(const char *key);
static size_t get_env_detail_size(void);
static size_t get_env_user_used_size(void);
static FlashErrCode create_env(const char *key, const char *value);
static FlashErrCode save_cur_using_data_addr(uint32_t cur_data_addr);

#ifdef FLASH_ENV_USING_CRC_CHECK
static uint32_t calc_env_crc(void);
static bool_t env_crc_is_ok(void);
#endif

/**
 * Flash ENV initialize.
 *
 * @param start_addr ENV start address in flash
 * @param total_size ENV section total size (@note must be word alignment)
 * @param erase_min_size the minimum size of flash erasure
 * @param default_env default ENV set for user
 * @param default_env_size default ENV set size
 *
 * @return result
 */
FlashErrCode flash_env_init(uint32_t start_addr, size_t total_size, size_t erase_min_size,
        flash_env const *default_env, size_t default_env_size) {
    FlashErrCode result = FLASH_NO_ERR;

    FLASH_ASSERT(start_addr);
    FLASH_ASSERT(total_size);
    FLASH_ASSERT(erase_min_size);
    FLASH_ASSERT(default_env);
    FLASH_ASSERT(default_env_size < FLASH_USER_SETTING_ENV_SIZE);
    /* must be word alignment for ENV */
    FLASH_ASSERT(FLASH_USER_SETTING_ENV_SIZE % 4 == 0);
    FLASH_ASSERT(total_size % 4 == 0);

    env_start_addr = start_addr;
    env_total_size = total_size;
    flash_erase_min_size = erase_min_size;
    default_env_set = default_env;
    default_env_set_size = default_env_size;

    FLASH_DEBUG("Env start address is 0x%08X, size is %d bytes.\n", start_addr, total_size);

    flash_load_env();

    return result;
}

/**
 * ENV set default.
 *
 * @return result
 */
FlashErrCode flash_env_set_default(void){
    FlashErrCode result = FLASH_NO_ERR;
    size_t i;

    FLASH_ASSERT(default_env_set);
    FLASH_ASSERT(default_env_set_size);

    /* set ENV detail part end address is at ENV detail part start address */
    set_env_detail_end_addr(get_env_detail_addr());

    /* create default ENV */
    for (i = 0; i < default_env_set_size; i++) {
        create_env(default_env_set[i].key, default_env_set[i].value);
    }

    flash_save_env();

    return result;
}

/**
 * Get ENV start address in flash.
 *
 * @return ENV start address in flash
 */
static uint32_t get_env_start_addr(void) {
    FLASH_ASSERT(env_start_addr);
    return env_start_addr;
}
/**
 * Get current using data section address.
 *
 * @return current using data section address
 */
static uint32_t get_cur_using_data_addr(void) {
    FLASH_ASSERT(cur_using_data_addr);
    return cur_using_data_addr;
}

/**
 * Set current using data section address.
 *
 * @param using_data_addr current using data section address
 */
static void set_cur_using_data_addr(uint32_t using_data_addr) {
    cur_using_data_addr = using_data_addr;
}

/**
 * Get ENV detail part start address.
 *
 * @return detail part start address
 */
static uint32_t get_env_detail_addr(void) {
    FLASH_ASSERT(cur_using_data_addr);
    return get_cur_using_data_addr() + ENV_PARAM_PART_BYTE_SIZE;
}

/**
 * Get ENV detail part end address.
 * It's the first word in ENV.
 *
 * @return ENV end address
 */
static uint32_t get_env_detail_end_addr(void) {
    /* it is the first word */
    return env_cache[ENV_PARAM_PART_INDEX_END_ADDR];
}

/**
 * Set ENV detail part end address.
 * It's the first word in ENV.
 *
 * @param end_addr ENV end address
 */
static void set_env_detail_end_addr(uint32_t end_addr) {
    env_cache[ENV_PARAM_PART_INDEX_END_ADDR] = end_addr;
}

/**
 * Get current ENV detail part size.
 *
 * @return size
 */
static size_t get_env_detail_size(void) {
    return get_env_detail_end_addr() - get_env_detail_addr();
}

/**
 * Get current user used ENV size.
 *
 * @see FLASH_USER_SETTING_ENV_SIZE
 *
 * @return size
 */
    /* must be initialized */
static size_t get_env_user_used_size(void) {

    return get_env_detail_end_addr() - get_cur_using_data_addr();
}

/**
 * Get current ENV already write bytes.
 *
 * @return write bytes
 */
uint32_t flash_get_env_write_bytes(void) {
    return get_env_detail_end_addr() - get_env_start_addr();
}

/**
 * Get current ENV section total size.
 *
 * @see flash_get_env_total_size
 *
 * @return size
 */
size_t flash_get_env_total_size(void) {
    /* must be initialized */
    FLASH_ASSERT(env_total_size);

    return env_total_size;
}

/**
 * Write an ENV at the end of cache.
 *
 * @param key ENV name
 * @param value ENV value
 *
 * @return result
 */
static FlashErrCode write_env(const char *key, const char *value) {
    FlashErrCode result = FLASH_NO_ERR;
    size_t ker_len = strlen(key), value_len = strlen(value), env_str_len;
    char *env_cache_bak = (char *)env_cache;

    /* calculate ENV storage length, contain '=' and '\0'. */
    env_str_len = ker_len + value_len + 2;
    if (env_str_len % 4 != 0) {
        env_str_len = (env_str_len / 4 + 1) * 4;
    }
    /* check capacity of ENV  */
    if (env_str_len + get_env_detail_size() >= FLASH_USER_SETTING_ENV_SIZE) {
        return FLASH_ENV_FULL;
    }
    /* calculate current ENV ram cache end address */
    env_cache_bak += ENV_PARAM_PART_BYTE_SIZE + get_env_detail_size();
    /* copy key name */
    memcpy(env_cache_bak, key, ker_len);
    env_cache_bak += ker_len;
    /* copy equal sign */
    *env_cache_bak = '=';
    env_cache_bak++;
    /* copy value */
    memcpy(env_cache_bak, value, value_len);
    env_cache_bak += value_len;
    /* fill '\0' for string end sign */
    *env_cache_bak = '\0';
    env_cache_bak ++;
    /* fill '\0' for word alignment */
    memset(env_cache_bak, 0, env_str_len - (ker_len + value_len + 2));
    set_env_detail_end_addr(get_env_detail_end_addr() + env_str_len);

    return result;
}

/**
 * Find ENV.
 *
 * @param key ENV name
 *
 * @return index of ENV in ram cache
 */
static uint32_t *find_env(const char *key) {
    uint32_t *env_cache_addr = NULL;
    char *env_start, *env_end, *env, *env_bak;

    FLASH_ASSERT(cur_using_data_addr);

    if (*key == NULL) {
        FLASH_INFO("Flash ENV name must be not empty!\n");
        return NULL;
    }

    /* from data section start to data section end */
    env_start = (char *) ((char *) env_cache + ENV_PARAM_PART_BYTE_SIZE);
    env_end = (char *) ((char *) env_cache + ENV_PARAM_PART_BYTE_SIZE + get_env_detail_size());

    /* ENV is null */
    if (env_start == env_end) {
        return NULL;
    }

    env = env_start;
    while (env < env_end) {
        /* storage model is key=value\0 */
        env_bak = strstr(env, key);
        /* the key name length must be equal */
        if (env_bak && (env_bak[strlen(key)] == '=')) {
            env_cache_addr = (uint32_t *) env_bak;
            break;
        } else {
            /* next ENV and word alignment */
            if ((strlen(env) + 1) % 4 == 0) {
                env += strlen(env) + 1;
            } else {
                env += ((strlen(env) + 1) / 4 + 1) * 4;
            }
        }
    }
    return env_cache_addr;
}

/**
 * If the ENV is not exist, create it.
 * @see flash_write_env
 *
 * @param key ENV name
 * @param value ENV value
 *
 * @return result
 */
static FlashErrCode create_env(const char *key, const char *value) {
    FlashErrCode result = FLASH_NO_ERR;

    FLASH_ASSERT(key);
    FLASH_ASSERT(value);

    if (*key == NULL) {
        FLASH_INFO("Flash ENV name must be not empty!\n");
        return FLASH_ENV_NAME_ERR;
    }

    if (strstr(key, "=")) {
        FLASH_INFO("Flash ENV name can't contain '='.\n");
        return FLASH_ENV_NAME_ERR;
    }

    /* find ENV */
    if (find_env(key)) {
        FLASH_INFO("The name of \"%s\" is already exist.\n", key);
        return FLASH_ENV_NAME_EXIST;
    }
    /* write ENV at the end of cache */
    result = write_env(key, value);

    return result;
}

/**
 * Delete an ENV in cache.
 *
 * @param key ENV name
 *
 * @return result
 */
FlashErrCode flash_del_env(const char *key){
    FlashErrCode result = FLASH_NO_ERR;
    char *del_env_str = NULL;
    size_t del_env_length, remain_env_length;

    FLASH_ASSERT(key);

    if (*key == NULL) {
        FLASH_INFO("Flash ENV name must be not NULL!\n");
        return FLASH_ENV_NAME_ERR;
    }

    if (strstr(key, "=")) {
        FLASH_INFO("Flash ENV name or value can't contain '='.\n");
        return FLASH_ENV_NAME_ERR;
    }

    /* find ENV */
    del_env_str = (char *) find_env(key);
    if (!del_env_str) {
        FLASH_INFO("Not find \"%s\" in ENV.\n", key);
        return FLASH_ENV_NAME_ERR;
    }
    del_env_length = strlen(del_env_str);
    /* '\0' also must be as ENV length */
    del_env_length ++;
    /* the address must multiple of 4 */
    if (del_env_length % 4 != 0) {
        del_env_length = (del_env_length / 4 + 1) * 4;
    }
    /* calculate remain ENV length */
    remain_env_length = get_env_detail_size()
            - (((uint32_t) del_env_str + del_env_length) - ((uint32_t) env_cache + ENV_PARAM_PART_BYTE_SIZE));
    /* remain ENV move forward */
    memcpy(del_env_str, del_env_str + del_env_length, remain_env_length);
    /* reset ENV end address */
    set_env_detail_end_addr(get_env_detail_end_addr() - del_env_length);

    return result;
}

/**
 * Set an ENV. If it value is empty, delete it.
 * If not find it in ENV table, then create it.
 *
 * @param key ENV name
 * @param value ENV value
 *
 * @return result
 */
FlashErrCode flash_set_env(const char *key, const char *value) {
    FlashErrCode result = FLASH_NO_ERR;

    /* if ENV value is empty, delete it */
    if (*value == NULL) {
        result = flash_del_env(key);
    } else {
        /* if find this ENV, then delete it and recreate it  */
        if (find_env(key)) {
            result = flash_del_env(key);
        }
        if (result == FLASH_NO_ERR) {
            result = create_env(key, value);
        }
    }
    return result;
}

/**
 * Get an ENV value by key name.
 *
 * @param key ENV name
 *
 * @return value
 */
char *flash_get_env(const char *key) {
    uint32_t *env_cache_addr = NULL;
    char *value = NULL;

    /* find ENV */
    env_cache_addr = find_env(key);
    if (env_cache_addr == NULL) {
        return NULL;
    }
    /* get value address */
    value = strstr((char *) env_cache_addr, "=");
    if (value != NULL) {
        /* the equal sign next character is value */
        value++;
    }
    return value;
}
/**
 * Print ENV.
 */
void flash_print_env(void) {
    uint32_t *env_cache_detail_addr = env_cache + ENV_PARAM_PART_WORD_SIZE,
            *env_cache_end_addr =
            (uint32_t *) (env_cache + ENV_PARAM_PART_WORD_SIZE + get_env_detail_size() / 4);
    uint8_t j;
    char c;

    for (; env_cache_detail_addr < env_cache_end_addr; env_cache_detail_addr += 1) {
        for (j = 0; j < 4; j++) {
            c = (*env_cache_detail_addr) >> (8 * j);
            flash_print("%c", c);
            if (c == NULL) {
                flash_print("\n");
                break;
            }
        }
    }
    flash_print("\nENV size: %ld/%ld bytes, write bytes %ld/%ld, mode: wear leveling.\n",
            get_env_user_used_size(), FLASH_USER_SETTING_ENV_SIZE, flash_get_env_write_bytes(),
            flash_get_env_total_size());
}

/**
 * Load flash ENV to ram.
 */
void flash_load_env(void) {
    uint32_t *env_cache_bak, env_end_addr, using_data_addr;

    /* read current using data section address */
    flash_read(get_env_start_addr(), &using_data_addr, 4);
    /* if ENV is not initialize or flash has dirty data, set default for it */
    if ((using_data_addr == 0xFFFFFFFF)
            || (using_data_addr > get_env_start_addr() + flash_get_env_total_size())
            || (using_data_addr < get_env_start_addr() + flash_erase_min_size)) {
        /* initialize current using data section address */
        set_cur_using_data_addr(get_env_start_addr() + flash_erase_min_size);
        /* save current using data section address to flash*/
        save_cur_using_data_addr(get_cur_using_data_addr());
        /* set default ENV */
        flash_env_set_default();
    } else {
        /* set current using data section address */
        set_cur_using_data_addr(using_data_addr);
        /* read ENV detail part end address from flash */
        flash_read(get_cur_using_data_addr() + ENV_PARAM_PART_INDEX_END_ADDR * 4, &env_end_addr, 4);
        /* if ENV end address has error, set default for ENV */
        if (env_end_addr > get_env_start_addr() + flash_get_env_total_size()) {
            flash_env_set_default();
        } else {
            /* set ENV detail part end address */
            set_env_detail_end_addr(env_end_addr);

            env_cache_bak = env_cache + ENV_PARAM_PART_WORD_SIZE;
            /* read all ENV from flash */
            flash_read(get_env_detail_addr(), env_cache_bak, get_env_detail_size());

#ifdef FLASH_ENV_USING_CRC_CHECK
            /* read ENV CRC code from flash */
            flash_read(get_cur_using_data_addr() + ENV_PARAM_PART_INDEX_DATA_CRC * 4,
                    &env_cache[ENV_PARAM_PART_INDEX_DATA_CRC], 4);

            /* if ENV CRC32 check is fault, set default for it */
            if (!env_crc_is_ok()) {
                FLASH_INFO("Warning: ENV CRC check failed. Set it to default.\n");
                flash_env_set_default();
            }
        }
#endif

    }
}

/**
 * Save ENV to flash.
 */
FlashErrCode flash_save_env(void) {
    FlashErrCode result = FLASH_NO_ERR;
    uint32_t cur_data_addr_bak = get_cur_using_data_addr(), move_offset_addr;
    size_t env_detail_size = get_env_detail_size();

    /* wear leveling process, automatic move ENV to next available position */
    while (get_cur_using_data_addr() + env_detail_size
            < get_env_start_addr() + flash_get_env_total_size()) {

#ifdef FLASH_ENV_USING_CRC_CHECK
    /* calculate and cache CRC32 code */
    env_cache[ENV_PARAM_PART_INDEX_DATA_CRC] = calc_env_crc();
#endif
        /* erase ENV */
        result = flash_erase(get_cur_using_data_addr(), ENV_PARAM_PART_BYTE_SIZE + env_detail_size);
        switch (result) {
        case FLASH_NO_ERR: {
            FLASH_INFO("Erased ENV OK.\n");
            break;
        }
        case FLASH_ERASE_ERR: {
            FLASH_INFO("Warning: Erased ENV fault!\n");
            FLASH_INFO("Moving ENV to next available position.\n");
            /* Calculate move offset address.
             * Current strategy is optimistic. It will offset the flash erasure minimum size.
             */
            move_offset_addr = flash_erase_min_size;
            /* calculate and set next available data section address */
            set_cur_using_data_addr(get_cur_using_data_addr() + move_offset_addr);
            /* calculate and set next available ENV detail part end address */
            set_env_detail_end_addr(get_env_detail_end_addr() + move_offset_addr);
            continue;
        }
        }
        /* write ENV to flash */
        result = flash_write(get_cur_using_data_addr(), env_cache,
                ENV_PARAM_PART_BYTE_SIZE + env_detail_size);
        switch (result) {
        case FLASH_NO_ERR: {
            FLASH_INFO("Saved ENV OK.\n");
            break;
        }
        case FLASH_WRITE_ERR: {
            FLASH_INFO("Warning: Saved ENV fault!\n");
            FLASH_INFO("Moving ENV to next available position.\n");
            /* Calculate move offset address.
             * Current strategy is optimistic. It will offset the flash erasure minimum size.
             */
            move_offset_addr = flash_erase_min_size;
            /* calculate and set next available data section address */
            set_cur_using_data_addr(get_cur_using_data_addr() + move_offset_addr);
            /* calculate and set next available ENV detail part end address */
            set_env_detail_end_addr(get_env_detail_end_addr() + move_offset_addr);
            continue;
        }
        }
        /* save ENV success */
        if (result == FLASH_NO_ERR) {
            break;
        }
    }

    if (get_cur_using_data_addr() + env_detail_size
            < get_env_start_addr() + flash_get_env_total_size()) {
        /* current using data section address has changed, save it */
        if (get_cur_using_data_addr() != cur_data_addr_bak) {
            save_cur_using_data_addr(get_cur_using_data_addr());
        }
    } else {
        result = FLASH_ENV_FULL;
        FLASH_INFO("Error: The flash has no available space to save ENV.\n");
        /* clear current using data section address on flash */
        save_cur_using_data_addr(0xFFFFFFFF);
    }

    return result;
}

#ifdef FLASH_ENV_USING_CRC_CHECK
/**
 * Calculate the cached ENV CRC32 value.
 *
 * @return CRC32 value
 */
static uint32_t calc_env_crc(void) {
    uint32_t crc32 = 0;

    extern uint32_t calc_crc32(uint32_t crc, const void *buf, size_t size);
    /* Calculate the ENV end address and all ENV data CRC32.
     * The 4 is ENV end address bytes size. */
    crc32 = calc_crc32(crc32, &env_cache[ENV_PARAM_PART_INDEX_END_ADDR], 4);
    crc32 = calc_crc32(crc32, &env_cache[ENV_PARAM_PART_WORD_SIZE], get_env_detail_size());
    FLASH_DEBUG("Calculate Env CRC32 number is 0x%08X.\n", crc32);

    return crc32;
}
#endif

#ifdef FLASH_ENV_USING_CRC_CHECK
/**
 * Check the ENV CRC32
 *
 * @return true is ok
 */
static bool_t env_crc_is_ok(void) {
    if (calc_env_crc() == env_cache[ENV_PARAM_PART_INDEX_DATA_CRC]) {
        FLASH_DEBUG("Verify Env CRC32 result is OK.\n");
        return TRUE;
    } else {
        return FALSE;
    }
}
#endif

/**
 * Save current using data section address to flash.
 *
 * @param cur_data_addr current using data section address
 *
 * @return result
 */
static FlashErrCode save_cur_using_data_addr(uint32_t cur_data_addr) {
    FlashErrCode result = FLASH_NO_ERR;
    /* erase ENV system section */
    result = flash_erase(get_env_start_addr(), 4);
    if (result == FLASH_NO_ERR) {
        /* write current using data section address to flash */
        result = flash_write(get_env_start_addr(), &cur_data_addr, 4);
        if (result == FLASH_WRITE_ERR) {
            FLASH_INFO("Error: Write system section fault!\n");
            FLASH_INFO("Note: The ENV can not be used.\n");
        }
    } else {
        FLASH_INFO("Error: Erased system section fault!\n");
        FLASH_INFO("Note: The ENV can not be used\n");
    }
    return result;
}
#endif
