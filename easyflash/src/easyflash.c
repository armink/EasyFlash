/*
 * This file is part of the EasyFlash Library.
 *
 * Copyright (c) 2014, Armink, <armink.ztl@gmail.com>
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
 * |      Saved log area        |   Storage size: @see FLASH_LOG_AREA_SIZE
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
#include "easyflash.h"

/**
 * EasyFlash system initialize.
 *
 * @return result
 */
EfErrCode easyflash_init(void) {
    extern EfErrCode ef_port_init(uint32_t *env_addr, size_t *env_total_size,
            size_t *erase_min_size, ef_env const **default_env, size_t *default_env_size,
            size_t *log_size);
    extern EfErrCode ef_env_init(uint32_t start_addr, size_t total_size, size_t erase_min_size,
            ef_env const *default_env, size_t default_env_size);
    extern EfErrCode ef_iap_init(uint32_t start_addr);
    extern EfErrCode ef_log_init(uint32_t start_addr, size_t log_size, size_t erase_min_size);

    uint32_t env_start_addr;
    size_t env_total_size = 0, erase_min_size = 0, default_env_set_size = 0, log_size = 0;
    const ef_env *default_env_set;
    EfErrCode result = EF_NO_ERR;

    result = ef_port_init(&env_start_addr, &env_total_size, &erase_min_size, &default_env_set,
            &default_env_set_size, &log_size);

#ifdef EF_USING_ENV
    if (result == EF_NO_ERR) {
        result = ef_env_init(env_start_addr, env_total_size, erase_min_size, default_env_set,
                default_env_set_size);
    }
#endif

#ifdef EF_USING_IAP
    if (result == EF_NO_ERR) {
        if (ef_get_env_total_size() < erase_min_size) {
            result = ef_iap_init(env_start_addr + erase_min_size + log_size);
        } else if (ef_get_env_total_size() % erase_min_size == 0) {
            result = ef_iap_init(env_start_addr + ef_get_env_total_size() + log_size);
        } else {
            result = ef_iap_init((ef_get_env_total_size() / erase_min_size + 1) * erase_min_size + log_size);
        }
    }
#endif

#ifdef EF_USING_LOG
    if (result == EF_NO_ERR) {
        if (ef_get_env_total_size() < erase_min_size) {
            result = ef_log_init(env_start_addr + erase_min_size, log_size, erase_min_size);
        } else if (ef_get_env_total_size() % erase_min_size == 0) {
            result = ef_log_init(env_start_addr + ef_get_env_total_size(), log_size, erase_min_size);
        } else {
            result = ef_log_init((ef_get_env_total_size() / erase_min_size + 1) * erase_min_size,
                    log_size, erase_min_size);
        }
    }
#endif

    if (result == EF_NO_ERR) {
        EF_DEBUG("EasyFlash V%s is initialize success.\n", EF_SW_VERSION);
    } else {
        EF_DEBUG("EasyFlash V%s is initialize fail.\n", EF_SW_VERSION);
    }

    return result;
}
