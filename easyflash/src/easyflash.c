/*
 * This file is part of the EasyFlash Library.
 *
 * Copyright (c) 2014-2020, Armink, <armink.ztl@gmail.com>
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
 * Function: Some initialize interface for this library.
 * Created on: 2014-09-09
 */

#include <easyflash.h>
#include <ef_low_lvl.h>

#define EF_LOG_TAG ""

/**
 * Set database lock and unlock funtion.
 *
 * @param db database object
 * @param lock lock function
 * @param unlock lock function
 */
void ef_db_lock_set(ef_db_t db, void (*lock)(ef_db_t db), void (*unlock)(ef_db_t db))
{
    EF_ASSERT(db);

    db->lock = lock;
    db->unlock = unlock;
}

/**
 * Set the sector size for database.
 *
 * @note The sector size MUST align by partition block size.
 * @note The sector size change MUST before database initialization.
 *
 * @param db database object
 * @param sec_size
 */
void ef_db_sec_size_set(ef_db_t db, uint32_t sec_size)
{
    EF_ASSERT(db);
    /* the sector size change MUST before database initialization */
    EF_ASSERT(db->init_ok == false);

    db->sec_size = sec_size;
}

EfErrCode _ef_init_ex(ef_db_t db, const char *name, const char *part_name, ef_db_type type, void *user_data)
{
    size_t block_size;

    EF_ASSERT(db);
    EF_ASSERT(name);
    EF_ASSERT(part_name);

    if (db->init_ok) {
        return EF_NO_ERR;
    }

    db->name = name;
    db->type = type;
    db->user_data = user_data;
    /* FAL (Flash Abstraction Layer) initialization */
    fal_init();
    /* check the flash partition */
    if ((db->part = fal_partition_find(part_name)) == NULL) {
        EF_INFO("Error: Partition (%s) not found.\n", part_name);
        return EF_PART_NOT_FOUND;
    }

    block_size = fal_flash_device_find(db->part->flash_name)->blk_size;
    if (db->sec_size == 0) {
        db->sec_size = block_size;
    } else {
        /* must be aligned with block size */
        EF_ASSERT(db->sec_size % block_size == 0);
    }

    return EF_NO_ERR;
}

void _ef_init_finish(ef_db_t db, EfErrCode result)
{
    static bool log_is_show = false;
    if (result == EF_NO_ERR) {
        db->init_ok = true;
        if (!log_is_show) {
            EF_INFO("EasyFlash V%s is initialize success.\n", EF_SW_VERSION);
            EF_INFO("You can get the latest version on https://github.com/armink/EasyFlash .\n");
            log_is_show = true;
        }
    } else {
        EF_INFO("Error: %s(%s) at partition %s is initialize fail(%d).\n", db->type == EF_DB_TYPE_KV ? "KV" : "TSL",
                db->name, db->part->name, result);
    }
}
