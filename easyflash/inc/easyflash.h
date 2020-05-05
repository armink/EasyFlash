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
 * Function: It is an head file for this library. You can see all be called functions.
 * Created on: 2014-09-10
 */


#ifndef EASYFLASH_H_
#define EASYFLASH_H_

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <time.h>
#include <fal.h>
#include <ef_cfg.h>
#include <ef_def.h>

#ifdef __cplusplus
extern "C" {
#endif

/* easyflash.c */
void      ef_db_lock_set(ef_db_t db, void (*lock)(ef_db_t db), void (*unlock)(ef_db_t db));
void      ef_db_sec_size_set(ef_db_t db, uint32_t sec_size);
EfErrCode ef_kvdb_init(ef_kvdb_t db, const char *name, const char *part_name, struct ef_default_kv *default_kv,
        void *user_data);
EfErrCode ef_tsdb_init(ef_tsdb_t db, const char *name, const char *part_name, ef_get_time get_time, size_t max_len,
        void *user_data);

/* blob API */
ef_blob_t ef_blob_make     (ef_blob_t blob, const void *value_buf, size_t buf_len);
size_t    ef_blob_read     (ef_db_t db, ef_blob_t blob);

/* Key-Value API like a KV DB */
EfErrCode ef_kv_set        (ef_kvdb_t db, const char *key, const char *value);
char     *ef_kv_get        (ef_kvdb_t db, const char *key);
EfErrCode ef_kv_set_blob   (ef_kvdb_t db, const char *key, ef_blob_t blob);
size_t    ef_kv_get_blob   (ef_kvdb_t db, const char *key, ef_blob_t blob);
EfErrCode ef_kv_del        (ef_kvdb_t db, const char *key);
ef_kv_t   ef_kv_get_obj    (ef_kvdb_t db, const char *key, ef_kv_t kv);
ef_blob_t ef_kv_to_blob    (ef_kv_t  kv, ef_blob_t blob);
EfErrCode ef_kv_set_default(ef_kvdb_t db);
void      ef_kv_print      (ef_kvdb_t db);

/* Time series log API like a TSDB */
EfErrCode ef_tsl_append      (ef_tsdb_t db, ef_blob_t blob);
void      ef_tsl_iter        (ef_tsdb_t db, ef_tsl_cb cb, void *cb_arg);
void      ef_tsl_iter_by_time(ef_tsdb_t db, ef_time_t from, ef_time_t to, ef_tsl_cb cb, void *cb_arg);
size_t    ef_tsl_query_count (ef_tsdb_t db, ef_time_t from, ef_time_t to, ef_tsl_status_t status);
EfErrCode ef_tsl_set_status  (ef_tsdb_t db, ef_tsl_t tsl, ef_tsl_status_t status);
void      ef_tsl_clean       (ef_tsdb_t db);
ef_blob_t ef_tsl_to_blob     (ef_tsl_t tsl, ef_blob_t blob);

/* ef_utils.c */
uint32_t ef_calc_crc32(uint32_t crc, const void *buf, size_t size);

#ifdef __cplusplus
}
#endif

#endif /* EASYFLASH_H_ */
