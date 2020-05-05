/*
 * Copyright (c) 2020, Armink, <armink.ztl@gmail.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2020-01-25     armink       the first version
 */

#include "utest.h"
#include <easyflash.h>
#include <stdio.h>
#include <stdlib.h>

#define TEST_TSL_PART_NAME             "ef_kvdb1"
#define TEST_KV_BLOB_NAME              "kv_blob_test"
#define TEST_KV_NAME                   "kv_test"

#if defined(RT_USING_UTEST) && defined(EF_USING_KVDB)

static struct ef_default_kv_node default_kv_set[] = {
        {"iap_need_copy_app", "0"},
        {"iap_need_crc32_check", "0"},
        {"iap_copy_app_size", "0"},
        {"stop_in_bootloader", "0"},
};

static struct ef_kvdb test_kvdb;

static void test_easyflash_init(void)
{
    struct ef_default_kv default_kv;

    default_kv.kvs = default_kv_set;
    default_kv.num = sizeof(default_kv_set) / sizeof(default_kv_set[0]);
    uassert_true(ef_kvdb_init(&test_kvdb, "test_kv", "ef_kvdb1", &default_kv, NULL) == EF_NO_ERR);
}

static void test_ef_create_kv_blob(void)
{
    EfErrCode result = EF_NO_ERR;
    rt_tick_t tick = rt_tick_get(), read_tick;
    size_t read_len;
    struct kv_node_obj kv_obj;
    struct ef_blob blob;
    uint8_t value_buf[sizeof(tick)];

    result = ef_kv_set_blob(&test_kvdb, TEST_KV_BLOB_NAME, ef_blob_make(&blob, &tick, sizeof(tick)));
    uassert_true(result == EF_NO_ERR);

    read_len = ef_kv_get_blob(&test_kvdb, TEST_KV_BLOB_NAME, ef_blob_make(&blob, &read_tick, sizeof(read_tick)));
    uassert_int_equal(blob.saved.len, sizeof(read_tick));
    uassert_int_equal(blob.saved.len, read_len);
    uassert_int_equal(tick, read_tick);

    uassert_true(ef_kv_get_obj(&test_kvdb, TEST_KV_BLOB_NAME, &kv_obj) != NULL);

    ef_blob_make(&blob, value_buf, sizeof(value_buf));
    read_len = ef_blob_read((ef_db_t)&test_kvdb, ef_kv_to_blob(&kv_obj, &blob));
    uassert_int_equal(read_len, sizeof(value_buf));
    uassert_buf_equal(&tick, value_buf, sizeof(value_buf));
}

static void test_ef_change_kv_blob(void)
{
    EfErrCode result = EF_NO_ERR;
    rt_tick_t tick = rt_tick_get(), read_tick;
    size_t read_len;
    struct ef_blob blob_obj, *blob = &blob_obj;

    read_len = ef_kv_get_blob(&test_kvdb, TEST_KV_BLOB_NAME, ef_blob_make(&blob_obj, &read_tick, sizeof(read_tick)));
    uassert_int_equal(blob->saved.len, sizeof(read_tick));
    uassert_int_equal(blob->saved.len, read_len);
    uassert_int_not_equal(tick, read_tick);

    result = ef_kv_set_blob(&test_kvdb, TEST_KV_BLOB_NAME, ef_blob_make(&blob_obj, &tick, sizeof(tick)));
    uassert_true(result == EF_NO_ERR);

    read_len = ef_kv_get_blob(&test_kvdb, TEST_KV_BLOB_NAME, ef_blob_make(&blob_obj, &read_tick, sizeof(read_tick)));
    uassert_int_equal(blob->saved.len, sizeof(read_tick));
    uassert_int_equal(blob->saved.len, read_len);
    uassert_int_equal(tick, read_tick);
}

static void test_ef_del_kv_blob(void)
{
    EfErrCode result = EF_NO_ERR;
    rt_tick_t tick = rt_tick_get(), read_tick;
    size_t read_len;
    struct ef_blob blob;

    read_len = ef_kv_get_blob(&test_kvdb, TEST_KV_BLOB_NAME, ef_blob_make(&blob, &read_tick, sizeof(read_tick)));
    uassert_int_equal(blob.saved.len, sizeof(read_tick));
    uassert_int_equal(blob.saved.len, read_len);
    uassert_int_not_equal(tick, read_tick);

    result = ef_kv_set_blob(&test_kvdb, TEST_KV_BLOB_NAME, ef_blob_make(&blob, NULL, 0));
    uassert_true(result == EF_NO_ERR);

    read_len = ef_kv_get_blob(&test_kvdb, TEST_KV_BLOB_NAME, ef_blob_make(&blob, &read_tick, sizeof(read_tick)));
    uassert_int_equal(blob.saved.len, 0);
    uassert_int_equal(read_len, 0);
}

static void test_ef_create_kv(void)
{
    EfErrCode result = EF_NO_ERR;
    rt_tick_t tick = rt_tick_get(), read_tick;
    char value_buf[14], *read_value;

    snprintf(value_buf, sizeof(value_buf), "%d", tick);
    result = ef_kv_set(&test_kvdb, TEST_KV_NAME, value_buf);
    uassert_true(result == EF_NO_ERR);

    read_value = ef_kv_get(&test_kvdb, TEST_KV_NAME);
    uassert_not_null(read_value);
    read_tick = atoi(read_value);
    uassert_int_equal(tick, read_tick);
}

static void test_ef_change_kv(void)
{
    EfErrCode result = EF_NO_ERR;
    rt_tick_t tick = rt_tick_get(), read_tick;
    char value_buf[14], *read_value;

    read_value = ef_kv_get(&test_kvdb, TEST_KV_NAME);
    uassert_not_null(read_value);
    read_tick = atoi(read_value);
    uassert_int_not_equal(tick, read_tick);

    snprintf(value_buf, sizeof(value_buf), "%d", tick);
    result = ef_kv_set(&test_kvdb, TEST_KV_NAME, value_buf);
    uassert_true(result == EF_NO_ERR);

    read_value = ef_kv_get(&test_kvdb, TEST_KV_NAME);
    uassert_not_null(read_value);
    read_tick = atoi(read_value);
    uassert_int_equal(tick, read_tick);
}

static void test_ef_del_kv(void)
{
    EfErrCode result = EF_NO_ERR;
    rt_tick_t tick = rt_tick_get(), read_tick;
    char *read_value;

    read_value = ef_kv_get(&test_kvdb, TEST_KV_NAME);
    uassert_not_null(read_value);
    read_tick = atoi(read_value);
    uassert_int_not_equal(tick, read_tick);

    result = ef_kv_del(&test_kvdb, TEST_KV_NAME);
    uassert_true(result == EF_NO_ERR);

    read_value = ef_kv_get(&test_kvdb, TEST_KV_NAME);
    uassert_null(read_value);
}

static rt_err_t utest_tc_init(void)
{
    return RT_EOK;
}

static rt_err_t utest_tc_cleanup(void)
{
    return RT_EOK;
}

static void testcase(void)
{
    UTEST_UNIT_RUN(test_easyflash_init);
    UTEST_UNIT_RUN(test_ef_create_kv_blob);
    UTEST_UNIT_RUN(test_ef_change_kv_blob);
    UTEST_UNIT_RUN(test_ef_del_kv_blob);
    UTEST_UNIT_RUN(test_ef_create_kv);
    UTEST_UNIT_RUN(test_ef_change_kv);
    UTEST_UNIT_RUN(test_ef_del_kv);
}
UTEST_TC_EXPORT(testcase, "packages.tools.easyflash.kv", utest_tc_init, utest_tc_cleanup, 20);

#endif /* defined(RT_USING_UTEST) && defined(EF_USING_TSDB) */
