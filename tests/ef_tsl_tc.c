/*
 * Copyright (c) 2020, Armink, <armink.ztl@gmail.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2020-05-02     armink       the first version
 */

#include "utest.h"
#include <easyflash.h>
#include <stdio.h>
#include <stdlib.h>

#if defined(RT_USING_UTEST) && defined(EF_USING_TSDB)

#define TEST_TSL_PART_NAME             "ef_tsdb1"
#define TEST_TSL_COUNT                 256
#define TEST_TSL_USER_STATUS1_COUNT    (TEST_TSL_COUNT/2)
#define TEST_TSL_DELETED_COUNT         (TEST_TSL_COUNT - TEST_TSL_USER_STATUS1_COUNT)

static char log[10];

static struct ef_tsdb test_tsdb;
static int cur_times = 0;

static ef_time_t get_time(void)
{
    return cur_times ++;
}

static void test_ef_tsdb_init_ex(void)
{
    uassert_true(ef_tsdb_init(&test_tsdb, "test_tsl", TEST_TSL_PART_NAME, get_time, 128, NULL) == EF_NO_ERR);
}

static void test_ef_tsl_append(void)
{
    struct ef_blob blob;
    int i;

    for (i = 0; i < TEST_TSL_COUNT; ++i) {
        rt_snprintf(log, sizeof(log), "%d", i);
        uassert_true(ef_tsl_append(&test_tsdb, ef_blob_make(&blob, log, rt_strnlen(log, sizeof(log)))) == EF_NO_ERR);
    }
}

static bool test_ef_tsl_iter_cb(ef_tsl_t tsl, void *arg)
{
    struct ef_blob blob;
    char data[sizeof(log)];
    size_t read_len;

    ef_blob_make(&blob, data, tsl->log_len);
    read_len = ef_blob_read((ef_db_t) &test_tsdb, ef_tsl_to_blob(tsl, &blob));

    data[read_len] = '\0';
    uassert_true(tsl->time == atoi(data));

    return false;
}

static void test_ef_tsl_iter(void)
{
    ef_tsl_iter(&test_tsdb, test_ef_tsl_iter_cb, NULL);
}

static void test_ef_tsl_iter_by_time(void)
{
    ef_time_t from = 0, to = TEST_TSL_COUNT -1;

    ef_tsl_iter_by_time(&test_tsdb, from, to, test_ef_tsl_iter_cb, NULL);
}

static void test_ef_tsl_query_count(void)
{
    ef_time_t from = 0, to = TEST_TSL_COUNT -1;

    uassert_true(ef_tsl_query_count(&test_tsdb, from, to, EF_TSL_WRITE) == TEST_TSL_COUNT);
}

static bool est_ef_tsl_set_status_cb(ef_tsl_t tsl, void *arg)
{
	ef_tsdb_t db = arg;

    if (tsl->time >= 0 && tsl->time < TEST_TSL_USER_STATUS1_COUNT) {
        uassert_true(ef_tsl_set_status(db, tsl, EF_TSL_USER_STATUS1) == EF_NO_ERR);
    } else {
        uassert_true(ef_tsl_set_status(db, tsl, EF_TSL_DELETED) == EF_NO_ERR);
    }

    return false;
}

static void test_ef_tsl_set_status(void)
{
    ef_time_t from = 0, to = TEST_TSL_COUNT -1;

    ef_tsl_iter_by_time(&test_tsdb, from, to, est_ef_tsl_set_status_cb, &test_tsdb);

    uassert_true(ef_tsl_query_count(&test_tsdb, from, to, EF_TSL_USER_STATUS1) == TEST_TSL_USER_STATUS1_COUNT);
    uassert_true(ef_tsl_query_count(&test_tsdb, from, to, EF_TSL_DELETED) == TEST_TSL_DELETED_COUNT);
}

static bool test_ef_tsl_clean_cb(ef_tsl_t tsl, void *arg)
{
    size_t *count = arg;

    (*count) ++;

    return false;
}

static void test_ef_tsl_clean(void)
{
    size_t count = 0;

    ef_tsl_clean(&test_tsdb);

    ef_tsl_iter(&test_tsdb, test_ef_tsl_clean_cb, &count);

    uassert_true(count == 0);
}

static rt_err_t utest_tc_init(void)
{
    cur_times = 0;
    rt_memset(&test_tsdb, 0, sizeof(struct ef_tsdb));

    return RT_EOK;
}

static rt_err_t utest_tc_cleanup(void)
{
    return RT_EOK;
}

static void testcase(void)
{
    UTEST_UNIT_RUN(test_ef_tsdb_init_ex);
    UTEST_UNIT_RUN(test_ef_tsl_clean);
    UTEST_UNIT_RUN(test_ef_tsl_append);
    UTEST_UNIT_RUN(test_ef_tsl_iter);
    UTEST_UNIT_RUN(test_ef_tsl_iter_by_time);
    UTEST_UNIT_RUN(test_ef_tsl_query_count);
    UTEST_UNIT_RUN(test_ef_tsl_set_status);
    UTEST_UNIT_RUN(test_ef_tsl_clean);
}
UTEST_TC_EXPORT(testcase, "packages.tools.easyflash.tsl", utest_tc_init, utest_tc_cleanup, 20);

#endif /* defined(RT_USING_UTEST) && defined(EF_USING_TSDB) */
