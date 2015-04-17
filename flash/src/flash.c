/*
 * This file is part of the EasyFlash Library.
 *
 * Copyright (c) 2014, Armink, <armink.ztl@gmail.com>
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
 * Function: Initialize interface for this library.
 * Created on: 2014-09-09
 */

/**
 * All Backup Area Flash storage index
 * |----------------------------|   Storage Size
 * | Environment variables area |   FLASH_ENV_SECTION_SIZE @see FLASH_ENV_SECTION_SIZE
 * |      1.system section      |   FLASH_ENV_SYSTEM_SIZE
 * |      2:data section        |   FLASH_ENV_SECTION_SIZE - FLASH_ENV_SYSTEM_SIZE
 * |----------------------------|
 * |(IAP)Downloaded application |   IAP already downloaded application size
 * |----------------------------|
 * |       Remain flash         |   All remaining
 * |----------------------------|
 *
 * Backup area storage index
 * 1.Environment variables area: @see FLASH_ENV_SECTION_SIZE
 * 2.Already downloaded application area for IAP function: unfixed size
 * 3.Remain
 *
 * Environment variables area has 2 section
 * 1.system section.(unit: 4 bytes, storage Environment variables's parameter)
 * 2.data section.(storage all environment variables)
 *
 */
#include "flash.h"

/**
 * Flash system initialize.
 *
 * @return result
 */
FlashErrCode flash_init(void) {
    extern FlashErrCode flash_port_init(uint32_t *env_addr, size_t *env_user_size,
            size_t *env_total_size, size_t *erase_min_size, flash_env const **default_env,
            size_t *default_env_size);
    extern FlashErrCode flash_env_init(uint32_t start_addr, size_t user_size, size_t total_size,
            size_t erase_min_size, flash_env const *default_env, size_t default_env_size);
    extern FlashErrCode flash_iap_init(uint32_t start_addr);

    uint32_t env_start_addr;
    size_t env_user_size, env_total_size, erase_min_size, default_env_set_size;
    const flash_env *default_env_set;
    FlashErrCode result = FLASH_NO_ERR;

    result = flash_port_init(&env_start_addr, &env_user_size, &env_total_size, &erase_min_size,
            &default_env_set, &default_env_set_size);

    if (result == FLASH_NO_ERR) {
        result = flash_env_init(env_start_addr, env_user_size, env_total_size, erase_min_size,
                default_env_set, default_env_set_size);
    }

    if (result == FLASH_NO_ERR) {
        result = flash_iap_init(env_start_addr + flash_get_env_total_size());
    }

    if (result == FLASH_NO_ERR) {
        FLASH_DEBUG("EasyFlash V%s is initialize success.\n", FLASH_SW_VERSION);
    } else {
        FLASH_DEBUG("EasyFlash V%s is initialize fail.\n", FLASH_SW_VERSION);
    }

    return result;
}
