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
 * Function: Time series log (like TSDB) feature samples source file.
 * Created on: 2019-05-05
 */

#include <easyflash.h>
#include <string.h>

struct env_status {
    int temp;
    int humi;
};

/* TSDB object */
static struct ef_tsdb tsdb = { 0 };

static bool query_cb(ef_tsl_t tsl, void *arg);
static bool set_status_cb(ef_tsl_t tsl, void *arg);

static void lock(ef_db_t db)
{
    /* YOUR CODE HERE */
}

static void unlock(ef_db_t db)
{
    /* YOUR CODE HERE */
}
static ef_time_t get_time(void)
{
    return time(NULL);
}

void tsdb_sample(void)
{
    EfErrCode result;
    struct ef_blob blob;

    { /* database initialization */
        /* set the lock and unlock function if you want */
        ef_db_lock_set((ef_db_t)&tsdb, lock, unlock);
        /* Time series database initialization
         *
         *       &tsdb: database object
         *       "log": database name
         *  "ef_tsdb1": The flash partition name base on FAL. Please make sure it's in FAL partition table.
         *              Please change to YOUR partition name.
         *    get_time: The get current timestamp function.
         *         128: maximum length of each log
         *        NULL: The user data if you need, now is empty.
         */
        result = ef_tsdb_init(&tsdb, "log", "ef_tsdb1", get_time, 128, NULL);

        if (result != EF_NO_ERR) {
            return;
        }
    }

    { /* APPEND new TSL */
        struct env_status status;

        /* append new log to TSDB */
        status.temp = 36;
        status.humi = 85;
        ef_tsl_append(&tsdb, ef_blob_make(&blob, &status, sizeof(status)));

        status.temp = 38;
        status.humi = 90;
        ef_tsl_append(&tsdb, ef_blob_make(&blob, &status, sizeof(status)));
    }

    { /* QUERY the TSDB */
        /* query all TSL in TSDB by iterator */
        ef_tsl_iter(&tsdb, query_cb, &tsdb);
    }

    { /* QUERY the TSDB by time */
        /* prepare query time (from 1970-01-01 00:00:00 to 2020-05-05 00:00:00) */
        struct tm tm_from = { .tm_year = 1970 - 1900, .tm_mon = 0, .tm_mday = 1, .tm_hour = 0, .tm_min = 0, .tm_sec = 0 };
        struct tm tm_to = { .tm_year = 2020 - 1900, .tm_mon = 4, .tm_mday = 5, .tm_hour = 0, .tm_min = 0, .tm_sec = 0 };
        time_t from_time = mktime(&tm_from), to_time = mktime(&tm_to);
        size_t count;
        /* query all TSL in TSDB by time */
        ef_tsl_iter_by_time(&tsdb, from_time, to_time, query_cb, &tsdb);
        /* query all EF_TSL_WRITE status TSL's count in TSDB by time */
        count = ef_tsl_query_count(&tsdb, from_time, to_time, EF_TSL_WRITE);
        EF_PRINT("query count: %lu\n", count);
    }

    { /* SET the TSL status */
        /* Change the TSL status by iterator or time iterator
         * set_status_cb: the change operation will in this callback
         *
         * NOTE: The actions to modify the state must be in order.
         *       EF_TSL_WRITE -> EF_TSL_USER_STATUS1 -> EF_TSL_DELETED -> EF_TSL_USER_STATUS2
         */
        ef_tsl_iter(&tsdb, set_status_cb, &tsdb);
    }
}

static bool query_cb(ef_tsl_t tsl, void *arg)
{
    struct ef_blob blob;
    struct env_status status;
    ef_tsdb_t db = arg;

    ef_blob_read((ef_db_t) db, ef_tsl_to_blob(tsl, ef_blob_make(&blob, &status, sizeof(status))));
    EF_PRINT("time: %d, temp: %d, humi: %d\n", tsl->time, status.temp, status.humi);

    return false;
}

static bool set_status_cb(ef_tsl_t tsl, void *arg)
{
    ef_tsdb_t db = arg;

    ef_tsl_set_status(db, tsl, EF_TSL_USER_STATUS1);

    return false;
}
