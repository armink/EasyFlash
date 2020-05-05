/*
 * This file is part of the EasyFlash Library.
 *
 * Copyright (c) 2020, Armink, <armink.ztl@gmail.com>
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
 * Function: It is the low level API and definition for this library.
 * Created on: 2020-04-30
 */

#ifndef _EF_LOW_LVL_H_
#define _EF_LOW_LVL_H_

#include <ef_cfg.h>
#include <ef_def.h>

#if (EF_WRITE_GRAN == 1)
#define EF_STATUS_TABLE_SIZE(status_number)       ((status_number * EF_WRITE_GRAN + 7)/8)
#else
#define EF_STATUS_TABLE_SIZE(status_number)       (((status_number - 1) * EF_WRITE_GRAN + 7)/8)
#endif

/* Return the most contiguous size aligned at specified width. RT_ALIGN(13, 4)
 * would return 16.
 */
#define EF_ALIGN(size, align)                    (((size) + (align) - 1) & ~((align) - 1))
/* align by write granularity */
#define EF_WG_ALIGN(size)                        (EF_ALIGN(size, (EF_WRITE_GRAN + 7)/8))
/**
 * Return the down number of aligned at specified width. RT_ALIGN_DOWN(13, 4)
 * would return 12.
 */
#define EF_ALIGN_DOWN(size, align)               ((size) & ~((align) - 1))
/* align down by write granularity */
#define EF_WG_ALIGN_DOWN(size)                   (EF_ALIGN_DOWN(size, (EF_WRITE_GRAN + 7)/8))

#define EF_STORE_STATUS_TABLE_SIZE               EF_STATUS_TABLE_SIZE(EF_SECTOR_STORE_STATUS_NUM)
#define EF_DIRTY_STATUS_TABLE_SIZE               EF_STATUS_TABLE_SIZE(EF_SECTOR_DIRTY_STATUS_NUM)

/* the data is unused */
#define EF_DATA_UNUSED                           0xFFFFFFFF

EfErrCode _ef_kv_load(ef_kvdb_t db);
size_t _ef_set_status(uint8_t status_table[], size_t status_num, size_t status_index);
size_t _ef_get_status(uint8_t status_table[], size_t status_num);
uint32_t _ef_continue_ff_addr(ef_db_t db, uint32_t start, uint32_t end);
EfErrCode _ef_init_ex(ef_db_t db, const char *name, const char *part_name, ef_db_type type, void *user_data);
void _ef_init_finish(ef_db_t db, EfErrCode result);
EfErrCode _ef_write_status(ef_db_t db, uint32_t addr, uint8_t status_table[], size_t status_num, size_t status_index);
size_t _ef_read_status(ef_db_t db, uint32_t addr, uint8_t status_table[], size_t total_num);
EfErrCode _ef_flash_read(ef_db_t db, uint32_t addr, void *buf, size_t size);
EfErrCode _ef_flash_erase(ef_db_t db, uint32_t addr, size_t size);
EfErrCode _ef_flash_write(ef_db_t db, uint32_t addr, const void *buf, size_t size);

#endif /* _EF_LOW_LVL_H_ */
