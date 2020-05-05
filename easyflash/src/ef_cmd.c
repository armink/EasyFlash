/*
 * This file is part of the EasyFlash Library.
 *
 * Copyright (c) 2015, Armink, <armink.ztl@gmail.com>
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
 * Function: RT-Thread Finsh/MSH command for EasyFlash.
 * Created on: 2018-05-19
 */

#include <easyflash.h>
#include <rtthread.h>

extern struct ef_kvdb _global_kvdb;
extern struct ef_tsdb _global_tsdb;

#if defined(RT_USING_FINSH) && defined(FINSH_USING_MSH) && defined(EF_USING_ENV)
#include <finsh.h>
#if defined(EF_USING_ENV)
static void __setenv(uint8_t argc, char **argv) {
    uint8_t i;

    if (argc > 3) {
        /* environment variable value string together */
        for (i = 0; i < argc - 2; i++) {
            argv[2 + i][rt_strlen(argv[2 + i])] = ' ';
        }
    }
    if (argc == 1) {
        rt_kprintf("Please input: setenv <key> [value]\n");
    } else if (argc == 2) {
        ef_kv_set(&_global_kvdb, argv[1], NULL);
    } else {
        ef_kv_set(&_global_kvdb, argv[1], argv[2]);
    }
}
MSH_CMD_EXPORT_ALIAS(__setenv, setenv, Set an envrionment variable.);

static void printenv(uint8_t argc, char **argv) {
    ef_kv_print(&_global_kvdb);
}
MSH_CMD_EXPORT(printenv, Print all envrionment variables.);

static void getvalue(uint8_t argc, char **argv) {
    char *value = NULL;
    value = ef_kv_get(&_global_kvdb, argv[1]);
    if (value) {
        rt_kprintf("The %s value is %s.\n", argv[1], value);
    } else {
        rt_kprintf("Can't find %s.\n", argv[1]);
    }
}
MSH_CMD_EXPORT(getvalue, Get an envrionment variable by name.);

static void resetenv(uint8_t argc, char **argv) {
    ef_kv_set_default(&_global_kvdb);
}
MSH_CMD_EXPORT(resetenv, Reset all envrionment variable to default.);

#endif /* defined(EF_USING_ENV) */

static bool tsl_cb(ef_tsl_t tsl, void *arg)
{
    struct ef_blob blob;
    char *log = rt_malloc(tsl->log_len);
    size_t read_len;

    if (log) {
        ef_blob_make(&blob, log, tsl->log_len);
        read_len = ef_blob_read((ef_db_t)&_global_tsdb, ef_tsl_to_blob(tsl, &blob));

        rt_kprintf("TSL time: %d\n", tsl->time);
        rt_kprintf("TSL blob content: %.*s\n", read_len, blob.buf);
        rt_free(log);
    }

    return false;
}

static bool tsl_bench_cb(ef_tsl_t tsl, void *arg)
{
    rt_tick_t *end_tick = arg;

    *end_tick = rt_tick_get();

    return false;
}

static void tsl(uint8_t argc, char **argv) {
    struct ef_blob blob;
    struct tm tm_from = { .tm_year = 1970 - 1900, .tm_mon = 0, .tm_mday = 1, .tm_hour = 0, .tm_min = 0, .tm_sec = 0 };
    struct tm tm_to = { .tm_year = 2030 - 1900, .tm_mon = 0, .tm_mday = 1, .tm_hour = 0, .tm_min = 0, .tm_sec = 0 };
    time_t from_time = mktime(&tm_from), to_time = mktime(&tm_to);
    rt_tick_t start_tick = rt_tick_get(), end_tick;

    if (!strcmp(argv[1], "add") && (argc > 2)) {
        ef_tsl_append(&_global_tsdb, ef_blob_make(&blob, argv[2], strlen(argv[2])));
    } else if (!strcmp(argv[1], "get") && (argc > 1)) {
        ef_tsl_iter_by_time(&_global_tsdb, from_time, to_time, tsl_cb, NULL);
//        ef_tsl_iter_by_time(&_global_tsdb, atoi(argv[2]), atoi(argv[3]), tsl_cb, NULL);
    } else if (!strcmp(argv[1], "clean") && (argc > 1)) {
        ef_tsl_clean(&_global_tsdb);
    } else if (!strcmp(argv[1], "query") && (argc > 2)) {
        int status = atoi(argv[2]);
        size_t count;
        count = ef_tsl_query_count(&_global_tsdb, from_time, to_time, status);
        rt_kprintf("query count: %d\n", count);
    } else if (!strcmp(argv[1], "bench") && (argc > 1)) {
#define BENCH_TIMEOUT        (5*1000)
        struct ef_blob blob;
        static char data[11], log[128];
        size_t append_num = 0;
        ef_time_t start, end, cur;
        rt_tick_t bench_start_tick, spent_tick, min_tick = 9999, max_tick = 0, total_tick = 0;
        float temp;

        ef_tsl_clean(&_global_tsdb);
        bench_start_tick = rt_tick_get();
        start = _global_tsdb.get_time();
        while (rt_tick_get() - bench_start_tick <= (rt_tick_t)rt_tick_from_millisecond(BENCH_TIMEOUT)) {
            rt_snprintf(data, sizeof(data), "%d", append_num++);
            ef_tsl_append(&_global_tsdb, ef_blob_make(&blob, data, rt_strnlen(data, sizeof(data))));
        }
        end = _global_tsdb.get_time();
        temp = (float) append_num / (float)(BENCH_TIMEOUT / 1000);
        snprintf(log, sizeof(log), "Append %d TSL in %d seconds, average: %.2f tsl/S, %.2f ms/per\n", append_num,
                BENCH_TIMEOUT / 1000, temp, 1000.0f / temp);
        rt_kprintf("%s", log);
        cur = start;
        while(cur < end) {
            end_tick = bench_start_tick = rt_tick_get();
            ef_tsl_iter_by_time(&_global_tsdb, cur, cur, tsl_bench_cb, &end_tick);
//            spent_tick = end_tick - bench_start_tick;
            spent_tick = rt_tick_get() - bench_start_tick;
            if (spent_tick < min_tick) {
                min_tick = spent_tick;
            }
            if (spent_tick > max_tick) {
                max_tick = spent_tick;
            }
            total_tick += spent_tick;
            cur ++;
        }
        snprintf(log, sizeof(log), "Query total spent %ld (tick) for %ld TSL, min %ld, max %ld, average: %.2f tick/per\n", total_tick, end - start, min_tick, max_tick,
                (float) total_tick / (float) (end - start));
        rt_kprintf("%s", log);
        ef_tsl_clean(&_global_tsdb);
    } else {
        rt_kprintf("Please input: tsl [add log content|get [from_s to_s]]\n");
    }

    rt_kprintf("exec time: %d ticks\n", rt_tick_get() - start_tick);
}
MSH_CMD_EXPORT_ALIAS(tsl, tsl, Time series log. tsl [add log content|get [from_s to_s]|clean].);

#endif /* defined(RT_USING_FINSH) && defined(FINSH_USING_MSH) */
