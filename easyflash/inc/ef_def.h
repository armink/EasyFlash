/*
 * This file is part of the EasyFlash Library.
 *
 * Copyright (c) 2019-2020, Armink, <armink.ztl@gmail.com>
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
 * Function: It is the definitions head file for this library.
 * Created on: 2019-11-20
 */

#ifndef EF_DEF_H_
#define EF_DEF_H_

#ifdef __cplusplus
extern "C" {
#endif

/* EasyFlash software version number */
#define EF_SW_VERSION                  "5.0.0 beta"
#define EF_SW_VERSION_NUM              0x50000

/* the KV max name length must less then it */
#ifndef EF_KV_NAME_MAX
#define EF_KV_NAME_MAX                 32
#endif

/* the KV cache table size, it will improve KV search speed when using cache */
#ifndef EF_KV_CACHE_TABLE_SIZE
#define EF_KV_CACHE_TABLE_SIZE         16
#endif

/* the sector cache table size, it will improve KV save speed when using cache */
#ifndef EF_SECTOR_CACHE_TABLE_SIZE
#define EF_SECTOR_CACHE_TABLE_SIZE     4
#endif

#if (EF_KV_CACHE_TABLE_SIZE > 0) && (EF_SECTOR_CACHE_TABLE_SIZE > 0)
#define EF_KV_USING_CACHE
#endif

/* EasyFlash log function. default EF_PRINT macro is printf() */
#ifndef EF_PRINT
#define EF_PRINT(...)                  printf(__VA_ARGS__)
#endif
#define EF_LOG_PREFIX1()               EF_PRINT("[EasyFlash]"EF_LOG_TAG)
#define EF_LOG_PREFIX2()               EF_PRINT(" ")
#define EF_LOG_PREFIX()                EF_LOG_PREFIX1();EF_LOG_PREFIX2()
#ifdef EF_DEBUG_ENABLE
#define EF_DEBUG(...)                  EF_LOG_PREFIX();EF_PRINT("(%s:%d) ", __FILE__, __LINE__);EF_PRINT(__VA_ARGS__)
#else
#define EF_DEBUG(...)
#endif
/* EasyFlash routine print function. Must be implement by user. */
#define EF_INFO(...)                   EF_LOG_PREFIX();EF_PRINT(__VA_ARGS__)
/* EasyFlash assert for developer. */
#define EF_ASSERT(EXPR)                                                       \
if (!(EXPR))                                                                  \
{                                                                             \
    EF_DEBUG("(%s) has assert failed at %s.\n", #EXPR, __FUNCTION__);         \
    while (1);                                                                \
}

typedef time_t ef_time_t;
#ifdef EF_USING_TIMESTAMP_64BIT
typedef int64_t ef_time_t;
#endif
typedef ef_time_t (*ef_get_time)(void);

struct ef_default_kv_node {
    char *key;
    void *value;
    size_t value_len;
};

struct ef_default_kv {
    struct ef_default_kv_node *kvs;
    size_t num;
};

/* EasyFlash error code */
typedef enum {
    EF_NO_ERR,
    EF_ERASE_ERR,
    EF_READ_ERR,
    EF_WRITE_ERR,
    EF_PART_NOT_FOUND,
    EF_KV_NAME_ERR,
    EF_KV_NAME_EXIST,
    EF_KV_FULL,
    EF_INIT_FAILED,
} EfErrCode;

enum ef_kv_status {
    EF_KV_UNUSED,
    EF_KV_PRE_WRITE,
    EF_KV_WRITE,
    EF_KV_PRE_DELETE,
    EF_KV_DELETED,
    EF_KV_ERR_HDR,
    EF_KV_STATUS_NUM,
};
typedef enum ef_kv_status ef_kv_status_t;

enum ef_tsl_status {
    EF_TSL_UNUSED,
    EF_TSL_PRE_WRITE,
    EF_TSL_WRITE,
    EF_TSL_USER_STATUS1,
    EF_TSL_DELETED,
    EF_TSL_USER_STATUS2,
    EF_TSL_STATUS_NUM,
};
typedef enum ef_tsl_status ef_tsl_status_t;

struct kv_node_obj {
    ef_kv_status_t status;                       /**< node status, @see ef_kv_status_t */
    bool crc_is_ok;                              /**< node CRC32 check is OK */
    uint8_t name_len;                            /**< name length */
    uint32_t magic;                              /**< magic word(`K`, `V`, `4`, `0`) */
    uint32_t len;                                /**< node total length (header + name + value), must align by EF_WRITE_GRAN */
    uint32_t value_len;                          /**< value length */
    char name[EF_KV_NAME_MAX];                   /**< name */
    struct {
        uint32_t start;                          /**< node start address */
        uint32_t value;                          /**< value start address */
    } addr;
};
typedef struct kv_node_obj *kv_node_obj_t;
typedef kv_node_obj_t ef_kv_t;

/* time series log node object */
struct tsl_node_obj {
    ef_tsl_status_t status;                      /**< node status, @see ef_log_status_t */
    ef_time_t time;                              /**< node timestamp */
    uint32_t log_len;                            /**< log length, must align by EF_WRITE_GRAN */
    struct {
        uint32_t index;                          /**< node index address */
        uint32_t log;                            /**< log data address */
    } addr;
};
typedef struct tsl_node_obj *tsl_node_obj_t;
typedef tsl_node_obj_t ef_tsl_t;
typedef bool (*ef_tsl_cb)(ef_tsl_t tsl, void *arg);

typedef enum {
    EF_DB_TYPE_KV,
    EF_DB_TYPE_TSL,
} ef_db_type;

/* the flash sector store status */
enum ef_sector_store_status {
    EF_SECTOR_STORE_UNUSED,
    EF_SECTOR_STORE_EMPTY,
    EF_SECTOR_STORE_USING,
    EF_SECTOR_STORE_FULL,
    EF_SECTOR_STORE_STATUS_NUM,
};
typedef enum ef_sector_store_status ef_sector_store_status_t;

/* the flash sector dirty status */
enum ef_sector_dirty_status {
    EF_SECTOR_DIRTY_UNUSED,
    EF_SECTOR_DIRTY_FALSE,
    EF_SECTOR_DIRTY_TRUE,
    EF_SECTOR_DIRTY_GC,
    EF_SECTOR_DIRTY_STATUS_NUM,
};
typedef enum ef_sector_dirty_status ef_sector_dirty_status_t;

/* KV section information */
struct kv_sec_info {
    bool check_ok;                               /**< sector header check is OK */
    struct {
        ef_sector_store_status_t store;          /**< sector store status @see ef_sector_store_status_t */
        ef_sector_dirty_status_t dirty;          /**< sector dirty status @see sector_dirty_status_t */
    } status;
    uint32_t addr;                               /**< sector start address */
    uint32_t magic;                              /**< magic word(`E`, `F`, `4`, `0`) */
    uint32_t combined;                           /**< the combined next sector number, 0xFFFFFFFF: not combined */
    size_t remain;                               /**< remain size */
    uint32_t empty_kv;                           /**< the next empty KV node start address */
};
typedef struct kv_sec_info *kv_sec_info_t;

/* TSL section information */
struct tsl_sec_info {
    bool check_ok;                               /**< sector header check is OK */
    ef_sector_store_status_t status;             /**< sector store status @see ef_sector_store_status_t */
    uint32_t addr;                               /**< sector start address */
    uint32_t magic;                              /**< magic word(`T`, `S`, `L`, `0`) */
    ef_time_t start_time;                        /**< the first start node's timestamp, 0xFFFFFFFF: unused */
    ef_time_t end_time;                          /**< the last end node's timestamp, 0xFFFFFFFF: unused */
    uint32_t end_idx;                            /**< the last end node's index, 0xFFFFFFFF: unused */
    ef_tsl_status_t end_info_stat[2];            /**< the last end node's info status */
    size_t remain;                               /**< remain size */
    uint32_t empty_idx;                          /**< the next empty node index address */
    uint32_t empty_data;                         /**< the next empty node's data end address */
};
typedef struct tsl_sec_info *tsl_sec_info_t;

struct kv_cache_node {
    uint16_t name_crc;                           /**< KV name's CRC32 low 16bit value */
    uint16_t active;                             /**< KV node access active degree */
    uint32_t addr;                               /**< KV node address */
};
typedef struct kv_cache_node *kv_cache_node_t;

struct sector_cache_node {
    uint32_t addr;                               /**< sector start address */
    uint32_t empty_addr;                         /**< sector empty address */
};
typedef struct sector_cache_node *sector_cache_node_t;

/* EasyFlash database structure */
typedef struct ef_db *ef_db_t;
struct ef_db {
    const char *name;                            /**< database name */
    ef_db_type type;                             /**< database type */
    const struct fal_partition *part;            /**< flash partition */
    uint32_t sec_size;                           /**< flash section size. It's a multiple of block size */
    bool init_ok;                                /**< initialized successfully */
    void (*lock)(ef_db_t db);                    /**< lock the database operate */
    void (*unlock)(ef_db_t db);                  /**< unlock the database operate */

    void *user_data;
};

/* KVDB structure */
struct ef_kvdb {
    struct ef_db parent;                         /**< inherit from ef_db */
    struct ef_default_kv default_kvs;            /**< default KV */
    bool gc_request;                             /**< request a GC check */
    bool in_recovery_check;                      /**< is in recovery check status when first reboot */

#ifdef EF_KV_USING_CACHE
    /* KV cache table */
    struct kv_cache_node kv_cache_table[EF_KV_CACHE_TABLE_SIZE];
    /* sector cache table, it caching the sector info which status is current using */
    struct sector_cache_node sector_cache_table[EF_SECTOR_CACHE_TABLE_SIZE];
#endif /* EF_KV_USING_CACHE */

#ifdef EF_KV_AUTO_UPDATE
    uint32_t ver_num;                            /**< setting version number for update */
#endif

    void *user_data;
};
typedef struct ef_kvdb *ef_kvdb_t;

/* TSDB structure */
struct ef_tsdb {
    struct ef_db parent;                         /**< inherit from ef_db */
    struct tsl_sec_info cur_sec;                 /**< current using sector */
    ef_time_t last_time;                         /**< last TSL timestamp */
    ef_get_time get_time;                        /**< the current timestamp get function */
    size_t max_len;                              /**< the max log length */
    uint32_t oldest_addr;                        /**< the oldest sector start address */

    void *user_data;
};
typedef struct ef_tsdb *ef_tsdb_t;

/* blob structure */
struct ef_blob {
    void *buf;                                   /**< blob data buffer */
    size_t size;                                 /**< blob data buffer size */
    struct {
        uint32_t meta_addr;                      /**< saved KV or TSL index address */
        uint32_t addr;                           /**< blob data saved address */
        size_t len;                              /**< blob data saved length */
    } saved;
};
typedef struct ef_blob *ef_blob_t;

#ifdef __cplusplus
}
#endif

#endif /* EF_DEF_H_ */
