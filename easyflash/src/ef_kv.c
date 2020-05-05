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
 * Function: Key-Value Database feature implement source file.
 * Created on: 2019-02-02
 */

#include <string.h>
#include <easyflash.h>
#include <ef_low_lvl.h>

#define EF_LOG_TAG "[kv]"
/* rewrite log prefix */
#undef  EF_LOG_PREFIX2
#define EF_LOG_PREFIX2()                         EF_PRINT("[%s] ", db_name(db))

#if defined(EF_USING_KVDB)

#ifndef EF_WRITE_GRAN
#error "Please configure flash write granularity (in ef_cfg.h)"
#endif

#if EF_WRITE_GRAN != 1 && EF_WRITE_GRAN != 8 && EF_WRITE_GRAN != 32
#error "the write gran can be only setting as 1, 8 and 32"
#endif

/* magic word(`E`, `F`, `4`, `0`) */
#define SECTOR_MAGIC_WORD                        0x30344645
/* magic word(`K`, `V`, `4`, `0`) */
#define KV_MAGIC_WORD                            0x3034564B

/* the sector remain threshold before full status */
#ifndef EF_SEC_REMAIN_THRESHOLD
#define EF_SEC_REMAIN_THRESHOLD                  (KV_HDR_DATA_SIZE + EF_KV_NAME_MAX)
#endif

/* the total remain empty sector threshold before GC */
#ifndef EF_GC_EMPTY_SEC_THRESHOLD
#define EF_GC_EMPTY_SEC_THRESHOLD                1
#endif

/* the string KV value buffer size for legacy ef_get_kv(db, ) function */
#ifndef EF_STR_KV_VALUE_MAX_SIZE
#define EF_STR_KV_VALUE_MAX_SIZE                128
#endif

#if EF_KV_CACHE_TABLE_SIZE > 0xFFFF
#error "The KV cache table size must less than 0xFFFF"
#endif

/* the sector is not combined value */
#define SECTOR_NOT_COMBINED                      0xFFFFFFFF
/* the next address is get failed */
#define FAILED_ADDR                              0xFFFFFFFF

#define KV_STATUS_TABLE_SIZE                     EF_STATUS_TABLE_SIZE(EF_KV_STATUS_NUM)

#define SECTOR_NUM                               (db_part_size(db) / db_sec_size(db))

#define SECTOR_HDR_DATA_SIZE                     (EF_WG_ALIGN(sizeof(struct sector_hdr_data)))
#define SECTOR_DIRTY_OFFSET                      ((unsigned long)(&((struct sector_hdr_data *)0)->status_table.dirty))
#define KV_HDR_DATA_SIZE                         (EF_WG_ALIGN(sizeof(struct kv_hdr_data)))
#define KV_MAGIC_OFFSET                          ((unsigned long)(&((struct kv_hdr_data *)0)->magic))
#define KV_LEN_OFFSET                            ((unsigned long)(&((struct kv_hdr_data *)0)->len))
#define KV_NAME_LEN_OFFSET                       ((unsigned long)(&((struct kv_hdr_data *)0)->name_len))

#define db_name(db)                              (((ef_db_t)db)->name)
#define db_init_ok(db)                           (((ef_db_t)db)->init_ok)
#define db_sec_size(db)                          (((ef_db_t)db)->sec_size)
#define db_part_size(db)                         (((ef_db_t)db)->part->len)
#define db_lock(db)                                                            \
    do {                                                                       \
        if (((ef_db_t)db)->lock) ((ef_db_t)db)->lock((ef_db_t)db);             \
    } while(0);

#define db_unlock(db)                                                          \
    do {                                                                       \
        if (((ef_db_t)db)->unlock) ((ef_db_t)db)->unlock((ef_db_t)db);         \
    } while(0);

#define VER_NUM_KV_NAME                         "__ver_num__"

struct sector_hdr_data {
    struct {
        uint8_t store[EF_STORE_STATUS_TABLE_SIZE];  /**< sector store status @see ef_sector_store_status_t */
        uint8_t dirty[EF_DIRTY_STATUS_TABLE_SIZE];  /**< sector dirty status @see sector_dirty_status_t */
    } status_table;
    uint32_t magic;                              /**< magic word(`E`, `F`, `4`, `0`) */
    uint32_t combined;                           /**< the combined next sector number, 0xFFFFFFFF: not combined */
    uint32_t reserved;
};
typedef struct sector_hdr_data *sector_hdr_data_t;

struct kv_hdr_data {
    uint8_t status_table[KV_STATUS_TABLE_SIZE];  /**< KV node status, @see ef_kv_status_t */
    uint32_t magic;                              /**< magic word(`K`, `V`, `4`, `0`) */
    uint32_t len;                                /**< KV node total length (header + name + value), must align by EF_WRITE_GRAN */
    uint32_t crc32;                              /**< KV node crc32(name_len + data_len + name + value) */
    uint8_t name_len;                            /**< name length */
    uint32_t value_len;                          /**< value length */
};
typedef struct kv_hdr_data *kv_hdr_data_t;

struct alloc_kv_cb_args {
    ef_kvdb_t db;
    size_t kv_size;
    uint32_t *empty_kv;
};

static void gc_collect(ef_kvdb_t db);

#ifdef EF_KV_USING_CACHE
/*
 * It's only caching the current using status sector's empty_addr
 */
static void update_sector_cache(ef_kvdb_t db, uint32_t sec_addr, uint32_t empty_addr)
{
    size_t i, empty_index = EF_SECTOR_CACHE_TABLE_SIZE;

    for (i = 0; i < EF_SECTOR_CACHE_TABLE_SIZE; i++) {
        if ((empty_addr > sec_addr) && (empty_addr < sec_addr + db_sec_size(db))) {
            /* update the sector empty_addr in cache */
            if (db->sector_cache_table[i].addr == sec_addr) {
                db->sector_cache_table[i].addr = sec_addr;
                db->sector_cache_table[i].empty_addr = empty_addr;
                return;
            } else if ((db->sector_cache_table[i].addr == EF_DATA_UNUSED) && (empty_index == EF_SECTOR_CACHE_TABLE_SIZE)) {
                empty_index = i;
            }
        } else if (db->sector_cache_table[i].addr == sec_addr) {
            /* delete the sector which status is not current using */
            db->sector_cache_table[i].addr = EF_DATA_UNUSED;
            return;
        }
    }
    /* add the sector empty_addr to cache */
    if (empty_index < EF_SECTOR_CACHE_TABLE_SIZE) {
        db->sector_cache_table[empty_index].addr = sec_addr;
        db->sector_cache_table[empty_index].empty_addr = empty_addr;
    }
}

/*
 * Get sector info from cache. It's return true when cache is hit.
 */
static bool get_sector_from_cache(ef_kvdb_t db, uint32_t sec_addr, uint32_t *empty_addr)
{
    size_t i;

    for (i = 0; i < EF_SECTOR_CACHE_TABLE_SIZE; i++) {
        if (db->sector_cache_table[i].addr == sec_addr) {
            if (empty_addr) {
                *empty_addr = db->sector_cache_table[i].empty_addr;
            }
            return true;
        }
    }

    return false;
}

static void update_kv_cache(ef_kvdb_t db, const char *name, size_t name_len, uint32_t addr)
{
    size_t i, empty_index = EF_KV_CACHE_TABLE_SIZE, min_activity_index = EF_KV_CACHE_TABLE_SIZE;
    uint16_t name_crc = (uint16_t) (ef_calc_crc32(0, name, name_len) >> 16), min_activity = 0xFFFF;

    for (i = 0; i < EF_KV_CACHE_TABLE_SIZE; i++) {
        if (addr != EF_DATA_UNUSED) {
            /* update the KV address in cache */
            if (db->kv_cache_table[i].name_crc == name_crc) {
                db->kv_cache_table[i].addr = addr;
                return;
            } else if ((db->kv_cache_table[i].addr == EF_DATA_UNUSED) && (empty_index == EF_KV_CACHE_TABLE_SIZE)) {
                empty_index = i;
            } else if (db->kv_cache_table[i].addr != EF_DATA_UNUSED) {
                if (db->kv_cache_table[i].active > 0) {
                    db->kv_cache_table[i].active--;
                }
                if (db->kv_cache_table[i].active < min_activity) {
                    min_activity_index = i;
                    min_activity = db->kv_cache_table[i].active;
                }
            }
        } else if (db->kv_cache_table[i].name_crc == name_crc) {
            /* delete the KV */
            db->kv_cache_table[i].addr = EF_DATA_UNUSED;
            db->kv_cache_table[i].active = 0;
            return;
        }
    }
    /* add the KV to cache, using LRU (Least Recently Used) like algorithm */
    if (empty_index < EF_KV_CACHE_TABLE_SIZE) {
        db->kv_cache_table[empty_index].addr = addr;
        db->kv_cache_table[empty_index].name_crc = name_crc;
        db->kv_cache_table[empty_index].active = 0;
    } else if (min_activity_index < EF_KV_CACHE_TABLE_SIZE) {
        db->kv_cache_table[min_activity_index].addr = addr;
        db->kv_cache_table[min_activity_index].name_crc = name_crc;
        db->kv_cache_table[min_activity_index].active = 0;
    }
}

/*
 * Get KV info from cache. It's return true when cache is hit.
 */
static bool get_kv_from_cache(ef_kvdb_t db, const char *name, size_t name_len, uint32_t *addr)
{
    size_t i;
    uint16_t name_crc = (uint16_t) (ef_calc_crc32(0, name, name_len) >> 16);

    for (i = 0; i < EF_KV_CACHE_TABLE_SIZE; i++) {
        if ((db->kv_cache_table[i].addr != EF_DATA_UNUSED) && (db->kv_cache_table[i].name_crc == name_crc)) {
            char saved_name[EF_KV_NAME_MAX];
            /* read the KV name in flash */
            _ef_flash_read((ef_db_t)db, db->kv_cache_table[i].addr + KV_HDR_DATA_SIZE, (uint32_t *) saved_name, EF_KV_NAME_MAX);
            if (!strncmp(name, saved_name, name_len)) {
                *addr = db->kv_cache_table[i].addr;
                if (db->kv_cache_table[i].active >= 0xFFFF - EF_KV_CACHE_TABLE_SIZE) {
                    db->kv_cache_table[i].active = 0xFFFF;
                } else {
                    db->kv_cache_table[i].active += EF_KV_CACHE_TABLE_SIZE;
                }
                return true;
            }
        }
    }

    return false;
}
#endif /* EF_KV_USING_CACHE */

/*
 * find the next KV address by magic word on the flash
 */
static uint32_t find_next_kv_addr(ef_kvdb_t db, uint32_t start, uint32_t end)
{
    uint8_t buf[32];
    uint32_t start_bak = start, i;
    uint32_t magic;

#ifdef EF_KV_USING_CACHE
    uint32_t empty_kv;

    if (get_sector_from_cache(db, EF_ALIGN_DOWN(start, db_sec_size(db)), &empty_kv) && start == empty_kv) {
        return FAILED_ADDR;
    }
#endif /* EF_KV_USING_CACHE */

    for (; start < end; start += (sizeof(buf) - sizeof(uint32_t))) {
        _ef_flash_read((ef_db_t)db, start, (uint32_t *) buf, sizeof(buf));
        for (i = 0; i < sizeof(buf) - sizeof(uint32_t) && start + i < end; i++) {
#ifndef EF_BIG_ENDIAN            /* Little Endian Order */
            magic = buf[i] + (buf[i + 1] << 8) + (buf[i + 2] << 16) + (buf[i + 3] << 24);
#else                       /* Big Endian Order */
            magic = buf[i + 3] + (buf[i + 2] << 8) + (buf[i + 1] << 16) + (buf[i] << 24);
#endif
            if (magic == KV_MAGIC_WORD && (start + i - KV_MAGIC_OFFSET) >= start_bak) {
                return start + i - KV_MAGIC_OFFSET;
            }
        }
    }

    return FAILED_ADDR;
}

static uint32_t get_next_kv_addr(ef_kvdb_t db, kv_sec_info_t sector, kv_node_obj_t pre_kv)
{
    uint32_t addr = FAILED_ADDR;

    if (sector->status.store == EF_SECTOR_STORE_EMPTY) {
        return FAILED_ADDR;
    }

    if (pre_kv->addr.start == FAILED_ADDR) {
        /* the first KV address */
        addr = sector->addr + SECTOR_HDR_DATA_SIZE;
    } else {
        if (pre_kv->addr.start <= sector->addr + db_sec_size(db)) {
            if (pre_kv->crc_is_ok) {
                addr = pre_kv->addr.start + pre_kv->len;
            } else {
                /* when pre_kv CRC check failed, maybe the flash has error data
                 * find_next_kv_addr after pre_kv address */
                addr = pre_kv->addr.start + EF_WG_ALIGN(1);
            }
            /* check and find next KV address */
            addr = find_next_kv_addr(db, addr, sector->addr + db_sec_size(db) - SECTOR_HDR_DATA_SIZE);

            if (addr > sector->addr + db_sec_size(db) || pre_kv->len == 0) {
                //TODO 扇区连续模式
                return FAILED_ADDR;
            }
        } else {
            /* no KV */
            return FAILED_ADDR;
        }
    }

    return addr;
}

static EfErrCode read_kv(ef_kvdb_t db, kv_node_obj_t kv)
{
    struct kv_hdr_data kv_hdr;
    uint8_t buf[32];
    uint32_t calc_crc32 = 0, crc_data_len, kv_name_addr;
    EfErrCode result = EF_NO_ERR;
    size_t len, size;
    /* read KV header raw data */
    _ef_flash_read((ef_db_t)db, kv->addr.start, (uint32_t *)&kv_hdr, sizeof(struct kv_hdr_data));
    kv->status = (ef_kv_status_t) _ef_get_status(kv_hdr.status_table, EF_KV_STATUS_NUM);
    kv->len = kv_hdr.len;

    if (kv->len == ~0UL || kv->len > db_part_size(db) || kv->len < KV_NAME_LEN_OFFSET) {
        /* the KV length was not write, so reserved the info for current KV */
        kv->len = KV_HDR_DATA_SIZE;
        if (kv->status != EF_KV_ERR_HDR) {
            kv->status = EF_KV_ERR_HDR;
            EF_DEBUG("Error: The KV @0x%08lX length has an error.\n", kv->addr.start);
            _ef_write_status((ef_db_t)db, kv->addr.start, kv_hdr.status_table, EF_KV_STATUS_NUM, EF_KV_ERR_HDR);
        }
        kv->crc_is_ok = false;
        return EF_READ_ERR;
    } else if (kv->len > db_sec_size(db) - SECTOR_HDR_DATA_SIZE && kv->len < db_part_size(db)) {
        //TODO 扇区连续模式，或者写入长度没有写入完整
        EF_ASSERT(0);
    }

    /* CRC32 data len(header.name_len + header.value_len + name + value) */
    crc_data_len = kv->len - KV_NAME_LEN_OFFSET;
    /* calculate the CRC32 value */
    for (len = 0, size = 0; len < crc_data_len; len += size) {
        if (len + sizeof(buf) < crc_data_len) {
            size = sizeof(buf);
        } else {
            size = crc_data_len - len;
        }

        _ef_flash_read((ef_db_t)db, kv->addr.start + KV_NAME_LEN_OFFSET + len, (uint32_t *) buf, EF_WG_ALIGN(size));
        calc_crc32 = ef_calc_crc32(calc_crc32, buf, size);
    }
    /* check CRC32 */
    if (calc_crc32 != kv_hdr.crc32) {
        kv->crc_is_ok = false;
        result = EF_READ_ERR;
    } else {
        kv->crc_is_ok = true;
        /* the name is behind aligned KV header */
        kv_name_addr = kv->addr.start + KV_HDR_DATA_SIZE;
        _ef_flash_read((ef_db_t)db, kv_name_addr, (uint32_t *) kv->name, EF_WG_ALIGN(kv_hdr.name_len));
        /* the value is behind aligned name */
        kv->addr.value = kv_name_addr + EF_WG_ALIGN(kv_hdr.name_len);
        kv->value_len = kv_hdr.value_len;
        kv->name_len = kv_hdr.name_len;
    }

    return result;
}

static EfErrCode read_sector_info(ef_kvdb_t db, uint32_t addr, kv_sec_info_t sector, bool traversal)
{
    EfErrCode result = EF_NO_ERR;
    struct sector_hdr_data sec_hdr;

    EF_ASSERT(addr % db_sec_size(db) == 0);
    EF_ASSERT(sector);

    /* read sector header raw data */
    _ef_flash_read((ef_db_t)db, addr, (uint32_t *)&sec_hdr, sizeof(struct sector_hdr_data));

    sector->addr = addr;
    sector->magic = sec_hdr.magic;
    /* check magic word */
    if (sector->magic != SECTOR_MAGIC_WORD) {
        sector->check_ok = false;
        sector->combined = SECTOR_NOT_COMBINED;
        return EF_INIT_FAILED;
    }
    sector->check_ok = true;
    /* get other sector info */
    sector->combined = sec_hdr.combined;
    sector->status.store = (ef_sector_store_status_t) _ef_get_status(sec_hdr.status_table.store, EF_SECTOR_STORE_STATUS_NUM);
    sector->status.dirty = (ef_sector_dirty_status_t) _ef_get_status(sec_hdr.status_table.dirty, EF_SECTOR_DIRTY_STATUS_NUM);
    /* traversal all KV and calculate the remain space size */
    if (traversal) {
        sector->remain = 0;
        sector->empty_kv = sector->addr + SECTOR_HDR_DATA_SIZE;
        if (sector->status.store == EF_SECTOR_STORE_EMPTY) {
            sector->remain = db_sec_size(db) - SECTOR_HDR_DATA_SIZE;
        } else if (sector->status.store == EF_SECTOR_STORE_USING) {
            struct kv_node_obj kv_obj;

#ifdef EF_KV_USING_CACHE
            if (get_sector_from_cache(db, addr, &sector->empty_kv)) {
                sector->remain = db_sec_size(db) - (sector->empty_kv - sector->addr);
                return result;
            }
#endif /* EF_KV_USING_CACHE */

            sector->remain = db_sec_size(db) - SECTOR_HDR_DATA_SIZE;
            kv_obj.addr.start = sector->addr + SECTOR_HDR_DATA_SIZE;
            do {

                read_kv(db, &kv_obj);
                if (!kv_obj.crc_is_ok) {
                    if (kv_obj.status != EF_KV_PRE_WRITE && kv_obj.status != EF_KV_ERR_HDR) {
                        EF_INFO("Error: The KV (@0x%08lX) CRC32 check failed!\n", kv_obj.addr.start);
                        sector->remain = 0;
                        result = EF_READ_ERR;
                        break;
                    }
                }
                sector->empty_kv += kv_obj.len;
                sector->remain -= kv_obj.len;
            } while ((kv_obj.addr.start = get_next_kv_addr(db, sector, &kv_obj)) != FAILED_ADDR);
            /* check the empty KV address by read continue 0xFF on flash  */
            {
                uint32_t ff_addr;

                ff_addr = _ef_continue_ff_addr((ef_db_t)db, sector->empty_kv, sector->addr + db_sec_size(db));
                /* check the flash data is clean */
                if (sector->empty_kv != ff_addr) {
                    /* update the sector information */
                    sector->empty_kv = ff_addr;
                    sector->remain = db_sec_size(db) - (ff_addr - sector->addr);
                }
            }

#ifdef EF_KV_USING_CACHE
            update_sector_cache(db, sector->addr, sector->empty_kv);
#endif
        }
    }

    return result;
}

static uint32_t get_next_sector_addr(ef_kvdb_t db, kv_sec_info_t pre_sec)
{
    uint32_t next_addr;

    if (pre_sec->addr == FAILED_ADDR) {
        /* the next sector is on the top of the partition */
        return 0;
    } else {
        /* check KV sector combined */
        if (pre_sec->combined == SECTOR_NOT_COMBINED) {
            next_addr = pre_sec->addr + db_sec_size(db);
        } else {
            next_addr = pre_sec->addr + pre_sec->combined * db_sec_size(db);
        }
        /* check range */
        if (next_addr < db_part_size(db)) {
            return next_addr;
        } else {
            /* no sector */
            return FAILED_ADDR;
        }
    }
}

static void kv_iterator(ef_kvdb_t db, kv_node_obj_t kv, void *arg1, void *arg2,
        bool (*callback)(kv_node_obj_t kv, void *arg1, void *arg2))
{
    struct kv_sec_info sector;
    uint32_t sec_addr;

    sec_addr = 0;
    /* search all sectors */
    do {
        if (read_sector_info(db, sec_addr, &sector, false) != EF_NO_ERR) {
            continue;
        }
        if (callback == NULL) {
            continue;
        }
        /* sector has KV */
        if (sector.status.store == EF_SECTOR_STORE_USING || sector.status.store == EF_SECTOR_STORE_FULL) {
            kv->addr.start = sector.addr + SECTOR_HDR_DATA_SIZE;
            /* search all KV */
            do {
                read_kv(db, kv);
                /* iterator is interrupted when callback return true */
                if (callback(kv, arg1, arg2)) {
                    return;
                }
            } while ((kv->addr.start = get_next_kv_addr(db, &sector, kv)) != FAILED_ADDR);
        }
    } while ((sec_addr = get_next_sector_addr(db, &sector)) != FAILED_ADDR);
}

static bool find_kv_cb(kv_node_obj_t kv, void *arg1, void *arg2)
{
    const char *key = arg1;
    bool *find_ok = arg2;
    size_t key_len = strlen(key);

    if (key_len != kv->name_len) {
        return false;
    }
    /* check KV */
    if (kv->crc_is_ok && kv->status == EF_KV_WRITE && !strncmp(kv->name, key, key_len)) {
        *find_ok = true;
        return true;
    }
    return false;
}

static bool find_kv_no_cache(ef_kvdb_t db, const char *key, kv_node_obj_t kv)
{
    bool find_ok = false;

    kv_iterator(db, kv, (void *)key, &find_ok, find_kv_cb);

    return find_ok;
}

static bool find_kv(ef_kvdb_t db, const char *key, kv_node_obj_t kv)
{
    bool find_ok = false;

#ifdef EF_KV_USING_CACHE
    size_t key_len = strlen(key);

    if (get_kv_from_cache(db, key, key_len, &kv->addr.start)) {
        read_kv(db, kv);
        return true;
    }
#endif /* EF_KV_USING_CACHE */

    find_ok = find_kv_no_cache(db, key, kv);

#ifdef EF_KV_USING_CACHE
    if (find_ok) {
        update_kv_cache(db, key, key_len, kv->addr.start);
    }
#endif /* EF_KV_USING_CACHE */

    return find_ok;
}

static bool ef_is_str(uint8_t *value, size_t len)
{
#define __is_print(ch)       ((unsigned int)((ch) - ' ') < 127u - ' ')
    size_t i;

    for (i = 0; i < len; i++) {
        if (!__is_print(value[i])) {
            return false;
        }
    }
    return true;
}

static size_t get_kv(ef_kvdb_t db, const char *key, void *value_buf, size_t buf_len, size_t *value_len)
{
    struct kv_node_obj kv;
    size_t read_len = 0;

    if (find_kv(db, key, &kv)) {
        if (value_len) {
            *value_len = kv.value_len;
        }
        if (buf_len > kv.value_len) {
            read_len = kv.value_len;
        } else {
            read_len = buf_len;
        }
        if (value_buf){
            _ef_flash_read((ef_db_t)db, kv.addr.value, (uint32_t *) value_buf, read_len);
        }
    } else if (value_len) {
        *value_len = 0;
    }

    return read_len;
}

/**
 * Get a KV object by key name
 *
 * @param db database object
 * @param key KV name
 * @param kv KV object
 *
 * @return KV object when is not NULL
 */
ef_kv_t ef_kv_get_obj(ef_kvdb_t db, const char *key, ef_kv_t kv)
{
    bool find_ok = false;

    if (!db_init_ok(db)) {
        EF_INFO("Error: KV (%s) isn't initialize OK.\n", db_name(db));
        return 0;
    }

    /* lock the KV cache */
    db_lock(db);

    find_ok = find_kv(db, key, kv);

    /* unlock the KV cache */
    db_unlock(db);

    return find_ok ? kv : NULL;
}

/**
 * Convert the KV object to blob object
 *
 * @param kv KV object
 * @param blob blob object
 *
 * @return new blob object
 */
ef_blob_t ef_kv_to_blob(ef_kv_t kv, ef_blob_t blob)
{
	blob->saved.addr = kv->addr.start;
	blob->saved.addr = kv->addr.value;
	blob->saved.len = kv->value_len;

	return blob;
}

/**
 * Get a blob KV value by key name.
 *
 * @param db database object
 * @param key KV name
 * @param blob blob object
 *
 * @return the actually get size on successful
 */
size_t ef_kv_get_blob(ef_kvdb_t db, const char *key, ef_blob_t blob)
{
    size_t read_len = 0;

    if (!db_init_ok(db)) {
        EF_INFO("Error: KV (%s) isn't initialize OK.\n", db_name(db));
        return 0;
    }

    /* lock the KV cache */
    db_lock(db);

    read_len = get_kv(db, key, blob->buf, blob->size, &blob->saved.len);

    /* unlock the KV cache */
    db_unlock(db);

    return read_len;
}

/**
 * Get an KV value by key name.
 *
 * @note this function is NOT supported reentrant
 * @note this function is DEPRECATED
 *
 * @param db database object
 * @param key KV name
 *
 * @return value
 */
char *ef_kv_get(ef_kvdb_t db, const char *key)
{
    static char value[EF_STR_KV_VALUE_MAX_SIZE + 1];
    size_t get_size;
    struct ef_blob blob;

    if ((get_size = ef_kv_get_blob(db, key, ef_blob_make(&blob, value, EF_STR_KV_VALUE_MAX_SIZE))) > 0) {
        /* the return value must be string */
        if (ef_is_str((uint8_t *)value, get_size)) {
            value[get_size] = '\0';
            return value;
        } else if (blob.saved.len > EF_STR_KV_VALUE_MAX_SIZE) {
            EF_INFO("Warning: The default string KV value buffer length (%d) is too less (%d).\n", EF_STR_KV_VALUE_MAX_SIZE,
                    blob.saved.len);
        } else {
            EF_INFO("Warning: The KV value isn't string. Could not be returned\n");
            return NULL;
        }
    }

    return NULL;
}

static EfErrCode write_kv_hdr(ef_kvdb_t db, uint32_t addr, kv_hdr_data_t kv_hdr)
{
    EfErrCode result = EF_NO_ERR;
    /* write the status will by write granularity */
    result = _ef_write_status((ef_db_t)db, addr, kv_hdr->status_table, EF_KV_STATUS_NUM, EF_KV_PRE_WRITE);
    if (result != EF_NO_ERR) {
        return result;
    }
    /* write other header data */
    result = _ef_flash_write((ef_db_t)db, addr + KV_MAGIC_OFFSET, &kv_hdr->magic, sizeof(struct kv_hdr_data) - KV_MAGIC_OFFSET);

    return result;
}

static EfErrCode format_sector(ef_kvdb_t db, uint32_t addr, uint32_t combined_value)
{
    EfErrCode result = EF_NO_ERR;
    struct sector_hdr_data sec_hdr;

    EF_ASSERT(addr % db_sec_size(db) == 0);

    result = _ef_flash_erase((ef_db_t)db, addr, db_sec_size(db));
    if (result == EF_NO_ERR) {
        /* initialize the header data */
        memset(&sec_hdr, 0xFF, sizeof(struct sector_hdr_data));
        _ef_set_status(sec_hdr.status_table.store, EF_SECTOR_STORE_STATUS_NUM, EF_SECTOR_STORE_EMPTY);
        _ef_set_status(sec_hdr.status_table.dirty, EF_SECTOR_DIRTY_STATUS_NUM, EF_SECTOR_DIRTY_FALSE);
        sec_hdr.magic = SECTOR_MAGIC_WORD;
        sec_hdr.combined = combined_value;
        sec_hdr.reserved = 0xFFFFFFFF;
        /* save the header */
        result = _ef_flash_write((ef_db_t)db, addr, (uint32_t *)&sec_hdr, sizeof(struct sector_hdr_data));

#ifdef EF_KV_USING_CACHE
        /* delete the sector cache */
        update_sector_cache(db, addr, addr + db_sec_size(db));
#endif /* EF_KV_USING_CACHE */
    }

    return result;
}

static EfErrCode update_sec_status(ef_kvdb_t db, kv_sec_info_t sector, size_t new_kv_len, bool *is_full)
{
    uint8_t status_table[EF_STORE_STATUS_TABLE_SIZE];
    EfErrCode result = EF_NO_ERR;
    /* change the current sector status */
    if (sector->status.store == EF_SECTOR_STORE_EMPTY) {
        /* change the sector status to using */
        result = _ef_write_status((ef_db_t)db, sector->addr, status_table, EF_SECTOR_STORE_STATUS_NUM, EF_SECTOR_STORE_USING);
    } else if (sector->status.store == EF_SECTOR_STORE_USING) {
        /* check remain size */
        if (sector->remain < EF_SEC_REMAIN_THRESHOLD || sector->remain - new_kv_len < EF_SEC_REMAIN_THRESHOLD) {
            /* change the sector status to full */
            result = _ef_write_status((ef_db_t)db, sector->addr, status_table, EF_SECTOR_STORE_STATUS_NUM, EF_SECTOR_STORE_FULL);

#ifdef EF_KV_USING_CACHE
            /* delete the sector cache */
            update_sector_cache(db, sector->addr, sector->addr + db_sec_size(db));
#endif /* EF_KV_USING_CACHE */

            if (is_full) {
                *is_full = true;
            }
        } else if (is_full) {
            *is_full = false;
        }
    }

    return result;
}

static void sector_iterator(ef_kvdb_t db, kv_sec_info_t sector, ef_sector_store_status_t status, void *arg1, void *arg2,
        bool (*callback)(kv_sec_info_t sector, void *arg1, void *arg2), bool traversal_kv)
{
    uint32_t sec_addr;

    /* search all sectors */
    sec_addr = 0;
    do {
        read_sector_info(db, sec_addr, sector, false);
        if (status == EF_SECTOR_STORE_UNUSED || status == sector->status.store) {
            if (traversal_kv) {
                read_sector_info(db, sec_addr, sector, true);
            }
            /* iterator is interrupted when callback return true */
            if (callback && callback(sector, arg1, arg2)) {
                return;
            }
        }
    } while ((sec_addr = get_next_sector_addr(db, sector)) != FAILED_ADDR);
}

static bool sector_statistics_cb(kv_sec_info_t sector, void *arg1, void *arg2)
{
    size_t *empty_sector = arg1, *using_sector = arg2;

    if (sector->check_ok && sector->status.store == EF_SECTOR_STORE_EMPTY) {
        (*empty_sector)++;
    } else if (sector->check_ok && sector->status.store == EF_SECTOR_STORE_USING) {
        (*using_sector)++;
    }

    return false;
}

static bool alloc_kv_cb(kv_sec_info_t sector, void *arg1, void *arg2)
{
    struct alloc_kv_cb_args *arg = arg1;

    /* 1. sector has space
     * 2. the NO dirty sector
     * 3. the dirty sector only when the gc_request is false */
    if (sector->check_ok && sector->remain > arg->kv_size
            && ((sector->status.dirty == EF_SECTOR_DIRTY_FALSE)
                    || (sector->status.dirty == EF_SECTOR_DIRTY_TRUE && !arg->db->gc_request))) {
        *(arg->empty_kv) = sector->empty_kv;
        return true;
    }

    return false;
}

static uint32_t alloc_kv(ef_kvdb_t db, kv_sec_info_t sector, size_t kv_size)
{
    uint32_t empty_kv = FAILED_ADDR;
    size_t empty_sector = 0, using_sector = 0;
    struct alloc_kv_cb_args arg = {db, kv_size, &empty_kv};

    /* sector status statistics */
    sector_iterator(db, sector, EF_SECTOR_STORE_UNUSED, &empty_sector, &using_sector, sector_statistics_cb, false);
    if (using_sector > 0) {
        /* alloc the KV from the using status sector first */
        sector_iterator(db, sector, EF_SECTOR_STORE_USING, &arg, NULL, alloc_kv_cb, true);
    }
    if (empty_sector > 0 && empty_kv == FAILED_ADDR) {
        if (empty_sector > EF_GC_EMPTY_SEC_THRESHOLD || db->gc_request) {
            sector_iterator(db, sector, EF_SECTOR_STORE_EMPTY, &arg, NULL, alloc_kv_cb, true);
        } else {
            /* no space for new KV now will GC and retry */
            EF_DEBUG("Trigger a GC check after alloc KV failed.\n");
            db->gc_request = true;
        }
    }

    return empty_kv;
}

static EfErrCode del_kv(ef_kvdb_t db, const char *key, kv_node_obj_t old_kv, bool complete_del)
{
    EfErrCode result = EF_NO_ERR;
    uint32_t dirty_status_addr;
    static bool last_is_complete_del = false;

#if (KV_STATUS_TABLE_SIZE >= EF_DIRTY_STATUS_TABLE_SIZE)
    uint8_t status_table[KV_STATUS_TABLE_SIZE];
#else
    uint8_t status_table[DIRTY_STATUS_TABLE_SIZE];
#endif

    /* need find KV */
    if (!old_kv) {
        struct kv_node_obj kv;
        /* find KV */
        if (find_kv(db, key, &kv)) {
            old_kv = &kv;
        } else {
            EF_DEBUG("Not found '%s' in KV.\n", key);
            return EF_KV_NAME_ERR;
        }
    }
    /* change and save the new status */
    if (!complete_del) {
        result = _ef_write_status((ef_db_t)db, old_kv->addr.start, status_table, EF_KV_STATUS_NUM, EF_KV_PRE_DELETE);
        last_is_complete_del = true;
    } else {
        result = _ef_write_status((ef_db_t)db, old_kv->addr.start, status_table, EF_KV_STATUS_NUM, EF_KV_DELETED);

        if (!last_is_complete_del && result == EF_NO_ERR) {
#ifdef EF_KV_USING_CACHE
            /* delete the KV in flash and cache */
            if (key != NULL) {
                /* when using del_kv(db, key, NULL, true) or del_kv(db, key, kv, true) in ef_del_kv(db, ) and set_kv(db, ) */
                update_kv_cache(db, key, strlen(key), EF_DATA_UNUSED);
            } else if (old_kv != NULL) {
                /* when using del_kv(db, NULL, kv, true) in move_kv(db, ) */
                update_kv_cache(db, old_kv->name, old_kv->name_len, EF_DATA_UNUSED);
            }
#endif /* EF_KV_USING_CACHE */
        }

        last_is_complete_del = false;
    }

    dirty_status_addr = EF_ALIGN_DOWN(old_kv->addr.start, db_sec_size(db)) + SECTOR_DIRTY_OFFSET;
    /* read and change the sector dirty status */
    if (result == EF_NO_ERR
            && _ef_read_status((ef_db_t)db, dirty_status_addr, status_table, EF_SECTOR_DIRTY_STATUS_NUM) == EF_SECTOR_DIRTY_FALSE) {
        result = _ef_write_status((ef_db_t)db, dirty_status_addr, status_table, EF_SECTOR_DIRTY_STATUS_NUM, EF_SECTOR_DIRTY_TRUE);
    }

    return result;
}

/*
 * move the KV to new space
 */
static EfErrCode move_kv(ef_kvdb_t db, kv_node_obj_t kv)
{
    EfErrCode result = EF_NO_ERR;
    uint8_t status_table[KV_STATUS_TABLE_SIZE];
    uint32_t kv_addr;
    struct kv_sec_info sector;

    /* prepare to delete the current KV */
    if (kv->status == EF_KV_WRITE) {
        del_kv(db, NULL, kv, false);
    }

    if ((kv_addr = alloc_kv(db, &sector, kv->len)) != FAILED_ADDR) {
        if (db->in_recovery_check) {
            struct kv_node_obj kv_bak;
            char name[EF_KV_NAME_MAX + 1] = { 0 };
            strncpy(name, kv->name, kv->name_len);
            /* check the KV in flash is already create success */
            if (find_kv_no_cache(db, name, &kv_bak)) {
                /* already create success, don't need to duplicate */
                result = EF_NO_ERR;
                goto __exit;
            }
        }
    } else {
        return EF_KV_FULL;
    }
    /* start move the KV */
    {
        uint8_t buf[32];
        size_t len, size, kv_len = kv->len;

        /* update the new KV sector status first */
        update_sec_status(db, &sector, kv->len, NULL);

        _ef_write_status((ef_db_t)db, kv_addr, status_table, EF_KV_STATUS_NUM, EF_KV_PRE_WRITE);
        kv_len -= KV_MAGIC_OFFSET;
        for (len = 0, size = 0; len < kv_len; len += size) {
            if (len + sizeof(buf) < kv_len) {
                size = sizeof(buf);
            } else {
                size = kv_len - len;
            }
            _ef_flash_read((ef_db_t)db, kv->addr.start + KV_MAGIC_OFFSET + len, (uint32_t *) buf, EF_WG_ALIGN(size));
            result = _ef_flash_write((ef_db_t)db, kv_addr + KV_MAGIC_OFFSET + len, (uint32_t *) buf, size);
        }
        _ef_write_status((ef_db_t)db, kv_addr, status_table, EF_KV_STATUS_NUM, EF_KV_WRITE);

#ifdef EF_KV_USING_CACHE
        update_sector_cache(db, EF_ALIGN_DOWN(kv_addr, db_sec_size(db)),
                kv_addr + KV_HDR_DATA_SIZE + EF_WG_ALIGN(kv->name_len) + EF_WG_ALIGN(kv->value_len));
        update_kv_cache(db, kv->name, kv->name_len, kv_addr);
#endif /* EF_KV_USING_CACHE */
    }

    EF_DEBUG("Moved the KV (%.*s) from 0x%08lX to 0x%08lX.\n", kv->name_len, kv->name, kv->addr.start, kv_addr);

__exit:
    del_kv(db, NULL, kv, true);

    return result;
}

static uint32_t new_kv(ef_kvdb_t db, kv_sec_info_t sector, size_t kv_size)
{
    bool already_gc = false;
    uint32_t empty_kv = FAILED_ADDR;

__retry:

    if ((empty_kv = alloc_kv(db, sector, kv_size)) == FAILED_ADDR && db->gc_request && !already_gc) {
        EF_DEBUG("Warning: Alloc an KV (size %d) failed when new KV. Now will GC then retry.\n", kv_size);
        gc_collect(db);
        already_gc = true;
        goto __retry;
    }

    return empty_kv;
}

static uint32_t new_kv_ex(ef_kvdb_t db, kv_sec_info_t sector, size_t key_len, size_t buf_len)
{
    size_t kv_len = KV_HDR_DATA_SIZE + EF_WG_ALIGN(key_len) + EF_WG_ALIGN(buf_len);

    return new_kv(db, sector, kv_len);
}

static bool gc_check_cb(kv_sec_info_t sector, void *arg1, void *arg2)
{
    size_t *empty_sec = arg1;

    if (sector->check_ok) {
        *empty_sec = *empty_sec + 1;
    }

    return false;

}

static bool do_gc(kv_sec_info_t sector, void *arg1, void *arg2)
{
    struct kv_node_obj kv;
    ef_kvdb_t db = arg1;

    if (sector->check_ok && (sector->status.dirty == EF_SECTOR_DIRTY_TRUE || sector->status.dirty == EF_SECTOR_DIRTY_GC)) {
        uint8_t status_table[EF_DIRTY_STATUS_TABLE_SIZE];
        /* change the sector status to GC */
        _ef_write_status((ef_db_t)db, sector->addr + SECTOR_DIRTY_OFFSET, status_table, EF_SECTOR_DIRTY_STATUS_NUM, EF_SECTOR_DIRTY_GC);
        /* search all KV */
        kv.addr.start = sector->addr + SECTOR_HDR_DATA_SIZE;
        do {
            read_kv(db, &kv);
            if (kv.crc_is_ok && (kv.status == EF_KV_WRITE || kv.status == EF_KV_PRE_DELETE)) {
                /* move the KV to new space */
                if (move_kv(db, &kv) != EF_NO_ERR) {
                    EF_DEBUG("Error: Moved the KV (%.*s) for GC failed.\n", kv.name_len, kv.name);
                }
            }
        } while ((kv.addr.start = get_next_kv_addr(db, sector, &kv)) != FAILED_ADDR);
        format_sector(db, sector->addr, SECTOR_NOT_COMBINED);
        EF_DEBUG("Collect a sector @0x%08lX\n", sector->addr);
    }

    return false;
}

/*
 * The GC will be triggered on the following scene:
 * 1. alloc an KV when the flash not has enough space
 * 2. write an KV then the flash not has enough space
 */
static void gc_collect(ef_kvdb_t db)
{
    struct kv_sec_info sector;
    size_t empty_sec = 0;

    /* GC check the empty sector number */
    sector_iterator(db, &sector, EF_SECTOR_STORE_EMPTY, &empty_sec, NULL, gc_check_cb, false);

    /* do GC collect */
    EF_DEBUG("The remain empty sector is %d, GC threshold is %d.\n", empty_sec, EF_GC_EMPTY_SEC_THRESHOLD);
    if (empty_sec <= EF_GC_EMPTY_SEC_THRESHOLD) {
        sector_iterator(db, &sector, EF_SECTOR_STORE_UNUSED, db, NULL, do_gc, false);
    }

    db->gc_request = false;
}

static EfErrCode align_write(ef_kvdb_t db, uint32_t addr, const uint32_t *buf, size_t size)
{
    EfErrCode result = EF_NO_ERR;
    size_t align_remain;

#if (EF_WRITE_GRAN / 8 > 0)
    uint8_t align_data[EF_WRITE_GRAN / 8];
    size_t align_data_size = sizeof(align_data);
#else
    /* For compatibility with C89 */
    uint8_t align_data_u8, *align_data = &align_data_u8;
    size_t align_data_size = 1;
#endif

    memset(align_data, 0xFF, align_data_size);
    result = _ef_flash_write((ef_db_t)db, addr, buf, EF_WG_ALIGN_DOWN(size));

    align_remain = size - EF_WG_ALIGN_DOWN(size);
    if (result == EF_NO_ERR && align_remain) {
        memcpy(align_data, (uint8_t *)buf + EF_WG_ALIGN_DOWN(size), align_remain);
        result = _ef_flash_write((ef_db_t)db, addr + EF_WG_ALIGN_DOWN(size), (uint32_t *) align_data, align_data_size);
    }

    return result;
}

static EfErrCode create_kv_blob(ef_kvdb_t db, kv_sec_info_t sector, const char *key, const void *value, size_t len)
{
    EfErrCode result = EF_NO_ERR;
    struct kv_hdr_data kv_hdr;
    bool is_full = false;
    uint32_t kv_addr = sector->empty_kv;

    if (strlen(key) > EF_KV_NAME_MAX) {
        EF_INFO("Error: The KV name length is more than %d\n", EF_KV_NAME_MAX);
        return EF_KV_NAME_ERR;
    }

    memset(&kv_hdr, 0xFF, sizeof(struct kv_hdr_data));
    kv_hdr.magic = KV_MAGIC_WORD;
    kv_hdr.name_len = strlen(key);
    kv_hdr.value_len = len;
    kv_hdr.len = KV_HDR_DATA_SIZE + EF_WG_ALIGN(kv_hdr.name_len) + EF_WG_ALIGN(kv_hdr.value_len);

    if (kv_hdr.len > db_sec_size(db) - SECTOR_HDR_DATA_SIZE) {
        EF_INFO("Error: The KV size is too big\n");
        return EF_KV_FULL;
    }

    if (kv_addr != FAILED_ADDR || (kv_addr = new_kv(db, sector, kv_hdr.len)) != FAILED_ADDR) {
        size_t align_remain;
        /* update the sector status */
        if (result == EF_NO_ERR) {
            result = update_sec_status(db, sector, kv_hdr.len, &is_full);
        }
        if (result == EF_NO_ERR) {
            uint8_t ff = 0xFF;
            /* start calculate CRC32 */
            kv_hdr.crc32 = ef_calc_crc32(0, &kv_hdr.name_len, KV_HDR_DATA_SIZE - KV_NAME_LEN_OFFSET);
            kv_hdr.crc32 = ef_calc_crc32(kv_hdr.crc32, key, kv_hdr.name_len);
            align_remain = EF_WG_ALIGN(kv_hdr.name_len) - kv_hdr.name_len;
            while (align_remain--) {
                kv_hdr.crc32 = ef_calc_crc32(kv_hdr.crc32, &ff, 1);
            }
            kv_hdr.crc32 = ef_calc_crc32(kv_hdr.crc32, value, kv_hdr.value_len);
            align_remain = EF_WG_ALIGN(kv_hdr.value_len) - kv_hdr.value_len;
            while (align_remain--) {
                kv_hdr.crc32 = ef_calc_crc32(kv_hdr.crc32, &ff, 1);
            }
            /* write KV header data */
            result = write_kv_hdr(db, kv_addr, &kv_hdr);

        }
        /* write key name */
        if (result == EF_NO_ERR) {
            result = align_write(db, kv_addr + KV_HDR_DATA_SIZE, (uint32_t *) key, kv_hdr.name_len);

#ifdef EF_KV_USING_CACHE
            if (!is_full) {
                update_sector_cache(db, sector->addr,
                        kv_addr + KV_HDR_DATA_SIZE + EF_WG_ALIGN(kv_hdr.name_len) + EF_WG_ALIGN(kv_hdr.value_len));
            }
            update_kv_cache(db, key, kv_hdr.name_len, kv_addr);
#endif /* EF_KV_USING_CACHE */
        }
        /* write value */
        if (result == EF_NO_ERR) {
            result = align_write(db, kv_addr + KV_HDR_DATA_SIZE + EF_WG_ALIGN(kv_hdr.name_len), value,
                    kv_hdr.value_len);
        }
        /* change the KV status to KV_WRITE */
        if (result == EF_NO_ERR) {
            result = _ef_write_status((ef_db_t)db, kv_addr, kv_hdr.status_table, EF_KV_STATUS_NUM, EF_KV_WRITE);
        }
        /* trigger GC collect when current sector is full */
        if (result == EF_NO_ERR && is_full) {
            EF_DEBUG("Trigger a GC check after created KV.\n");
            db->gc_request = true;
        }
    } else {
        result = EF_KV_FULL;
    }

    return result;
}

/**
 * Delete an KV.
 *
 * @param db database object
 * @param key KV name
 *
 * @return result
 */
EfErrCode ef_kv_del(ef_kvdb_t db, const char *key)
{
    EfErrCode result = EF_NO_ERR;

    if (!db_init_ok(db)) {
        EF_INFO("Error: KV (%s) isn't initialize OK.\n", db_name(db));
        return EF_INIT_FAILED;
    }

    /* lock the KV cache */
    db_lock(db);

    result = del_kv(db, key, NULL, true);

    /* unlock the KV cache */
    db_unlock(db);

    return result;
}

static EfErrCode set_kv(ef_kvdb_t db, const char *key, const void *value_buf, size_t buf_len)
{
    EfErrCode result = EF_NO_ERR;
    static struct kv_node_obj kv;
    static struct kv_sec_info sector;
    bool kv_is_found = false;

    if (value_buf == NULL) {
        result = del_kv(db, key, NULL, true);
    } else {
        /* make sure the flash has enough space */
        if (new_kv_ex(db, &sector, strlen(key), buf_len) == FAILED_ADDR) {
            return EF_KV_FULL;
        }
        kv_is_found = find_kv(db, key, &kv);
        /* prepare to delete the old KV */
        if (kv_is_found) {
            result = del_kv(db, key, &kv, false);
        }
        /* create the new KV */
        if (result == EF_NO_ERR) {
            result = create_kv_blob(db, &sector, key, value_buf, buf_len);
        }
        /* delete the old KV */
        if (kv_is_found && result == EF_NO_ERR) {
            result = del_kv(db, key, &kv, true);
        }
        /* process the GC after set KV */
        if (db->gc_request) {
            gc_collect(db);
        }
    }

    return result;
}

/**
 * Set a blob KV. If it blob value is NULL, delete it.
 * If not find it in flash, then create it.
 *
 * @param db database object
 * @param key KV name
 * @param blob blob object
 *
 * @return result
 */
EfErrCode ef_kv_set_blob(ef_kvdb_t db, const char *key, ef_blob_t blob)
{
    EfErrCode result = EF_NO_ERR;

    if (!db_init_ok(db)) {
        EF_INFO("Error: KV (%s) isn't initialize OK.\n", db_name(db));
        return EF_INIT_FAILED;
    }

    /* lock the KV cache */
    db_lock(db);

    result = set_kv(db, key, blob->buf, blob->size);

    /* unlock the KV cache */
    db_unlock(db);

    return result;
}

/**
 * Set a string KV. If it value is NULL, delete it.
 * If not find it in flash, then create it.
 *
 * @param db database object
 * @param key KV name
 * @param value KV value
 *
 * @return result
 */
EfErrCode ef_kv_set(ef_kvdb_t db, const char *key, const char *value)
{
    struct ef_blob blob;

    return ef_kv_set_blob(db, key, ef_blob_make(&blob, value, strlen(value)));
}

/**
 * recovery all KV to default.
 *
 * @param db database object
 * @return result
 */
EfErrCode ef_kv_set_default(ef_kvdb_t db)
{
    EfErrCode result = EF_NO_ERR;
    uint32_t addr, i, value_len;
    struct kv_sec_info sector;

    /* lock the KV cache */
    db_lock(db);
    /* format all sectors */
    for (addr = 0; addr < db_part_size(db); addr += db_sec_size(db)) {
        result = format_sector(db, addr, SECTOR_NOT_COMBINED);
        if (result != EF_NO_ERR) {
            goto __exit;
        }
    }
    /* create default KV */
    for (i = 0; i < db->default_kvs.num; i++) {
        /* It seems to be a string when value length is 0.
         * This mechanism is for compatibility with older versions (less then V4.0). */
        if (db->default_kvs.kvs[i].value_len == 0) {
            value_len = strlen(db->default_kvs.kvs[i].value);
        } else {
            value_len = db->default_kvs.kvs[i].value_len;
        }
        sector.empty_kv = FAILED_ADDR;
        create_kv_blob(db, &sector, db->default_kvs.kvs[i].key, db->default_kvs.kvs[i].value, value_len);
        if (result != EF_NO_ERR) {
            goto __exit;
        }
    }

__exit:
    /* unlock the KV cache */
    db_unlock(db);

    return result;
}

static bool print_kv_cb(kv_node_obj_t kv, void *arg1, void *arg2)
{
    bool value_is_str = true, print_value = false;
    size_t *using_size = arg1;
    ef_kvdb_t db = arg2;

    if (kv->crc_is_ok) {
        /* calculate the total using flash size */
        *using_size += kv->len;
        /* check KV */
        if (kv->status == EF_KV_WRITE) {
            EF_PRINT("%.*s=", kv->name_len, kv->name);

            if (kv->value_len < EF_STR_KV_VALUE_MAX_SIZE ) {
                uint8_t buf[32];
                size_t len, size;
__reload:
                /* check the value is string */
                for (len = 0, size = 0; len < kv->value_len; len += size) {
                    if (len + sizeof(buf) < kv->value_len) {
                        size = sizeof(buf);
                    } else {
                        size = kv->value_len - len;
                    }
                    _ef_flash_read((ef_db_t)db, kv->addr.value + len, (uint32_t *) buf, EF_WG_ALIGN(size));
                    if (print_value) {
                        EF_PRINT("%.*s", size, buf);
                    } else if (!ef_is_str(buf, size)) {
                        value_is_str = false;
                        break;
                    }
                }
            } else {
                value_is_str = false;
            }
            if (value_is_str && !print_value) {
                print_value = true;
                goto __reload;
            } else if (!value_is_str) {
                EF_PRINT("blob @0x%08lX %lubytes", kv->addr.value, kv->value_len);
            }
            EF_PRINT("\n");
        }
    }

    return false;
}


/**
 * Print all KV.
 *
 * @param db database object
 */
void ef_kv_print(ef_kvdb_t db)
{
    struct kv_node_obj kv;
    size_t using_size = 0;

    if (!db_init_ok(db)) {
        EF_INFO("Error: KV (%s) isn't initialize OK.\n", db_name(db));
        return;
    }

    /* lock the KV cache */
    db_lock(db);

    kv_iterator(db, &kv, &using_size, db, print_kv_cb);

    EF_PRINT("\nmode: next generation\n");
    EF_PRINT("size: %lu/%lu bytes.\n", using_size + (SECTOR_NUM - EF_GC_EMPTY_SEC_THRESHOLD) * SECTOR_HDR_DATA_SIZE,
            db_part_size(db) - db_sec_size(db) * EF_GC_EMPTY_SEC_THRESHOLD);

    /* unlock the KV cache */
    db_unlock(db);
}

#ifdef EF_KV_AUTO_UPDATE
/*
 * Auto update KV to latest default when current setting version number is changed.
 */
static void kv_auto_update(ef_kvdb_t db)
{
    size_t saved_ver_num, setting_ver_num = db->ver_num;

    if (get_kv(db, VER_NUM_KV_NAME, &saved_ver_num, sizeof(size_t), NULL) > 0) {
        /* check version number */
        if (saved_ver_num != setting_ver_num) {
            struct kv_node_obj kv;
            size_t i, value_len;
            struct kv_sec_info sector;
            EF_DEBUG("Update the KV from version %d to %d.\n", saved_ver_num, setting_ver_num);
            for (i = 0; i < db->default_kvs.num; i++) {
                /* add a new KV when it's not found */
                if (!find_kv(db, db->default_kvs.kvs[i].key, &kv)) {
                    /* It seems to be a string when value length is 0.
                     * This mechanism is for compatibility with older versions (less then V4.0). */
                    if (db->default_kvs.kvs[i].value_len == 0) {
                        value_len = strlen(db->default_kvs.kvs[i].value);
                    } else {
                        value_len = db->default_kvs.kvs[i].value_len;
                    }
                    sector.empty_kv = FAILED_ADDR;
                    create_kv_blob(db, &sector, db->default_kvs.kvs[i].key, db->default_kvs.kvs[i].value, value_len);
                }
            }
        } else {
            /* version number not changed now return */
            return;
        }
    }

    set_kv(db, VER_NUM_KV_NAME, &setting_ver_num, sizeof(size_t));
}
#endif /* EF_KV_AUTO_UPDATE */

static bool check_sec_hdr_cb(kv_sec_info_t sector, void *arg1, void *arg2)
{
    if (!sector->check_ok) {
        size_t *failed_count = arg1;
        ef_kvdb_t db = arg2;

        EF_INFO("Warning: Sector header check failed. Format this sector (0x%08lX).\n", sector->addr);
        (*failed_count) ++;
        format_sector(db, sector->addr, SECTOR_NOT_COMBINED);
    }

    return false;
}

static bool check_and_recovery_gc_cb(kv_sec_info_t sector, void *arg1, void *arg2)
{
    ef_kvdb_t db = arg1;

    if (sector->check_ok && sector->status.dirty == EF_SECTOR_DIRTY_GC) {
        /* make sure the GC request flag to true */
        db->gc_request = true;
        /* resume the GC operate */
        gc_collect(db);
    }

    return false;
}

static bool check_and_recovery_kv_cb(kv_node_obj_t kv, void *arg1, void *arg2)
{
    ef_kvdb_t db = arg1;

    /* recovery the prepare deleted KV */
    if (kv->crc_is_ok && kv->status == EF_KV_PRE_DELETE) {
        EF_INFO("Found an KV (%.*s) which has changed value failed. Now will recovery it.\n", kv->name_len, kv->name);
        /* recovery the old KV */
        if (move_kv(db, kv) == EF_NO_ERR) {
            EF_DEBUG("Recovery the KV successful.\n");
        } else {
            EF_DEBUG("Warning: Moved an KV (size %lu) failed when recovery. Now will GC then retry.\n", kv->len);
            return true;
        }
    } else if (kv->status == EF_KV_PRE_WRITE) {
        uint8_t status_table[KV_STATUS_TABLE_SIZE];
        /* the KV has not write finish, change the status to error */
        //TODO 绘制异常处理的状态装换图
        _ef_write_status((ef_db_t)db, kv->addr.start, status_table, EF_KV_STATUS_NUM, EF_KV_ERR_HDR);
        return true;
    }

    return false;
}

/**
 * Check and load the flash KV.
 *
 * @return result
 */
EfErrCode _ef_kv_load(ef_kvdb_t db)
{
    EfErrCode result = EF_NO_ERR;
    struct kv_node_obj kv;
    struct kv_sec_info sector;
    size_t check_failed_count = 0;

    db->in_recovery_check = true;
    /* check all sector header */
    sector_iterator(db, &sector, EF_SECTOR_STORE_UNUSED, &check_failed_count, db, check_sec_hdr_cb, false);
    /* all sector header check failed */
    if (check_failed_count == SECTOR_NUM) {
        EF_INFO("Warning: All sector header check failed. Set it to default.\n");
        ef_kv_set_default(db);
    }

    /* lock the KV cache */
    db_lock(db);
    /* check all sector header for recovery GC */
    sector_iterator(db, &sector, EF_SECTOR_STORE_UNUSED, db, NULL, check_and_recovery_gc_cb, false);

__retry:
    /* check all KV for recovery */
    kv_iterator(db, &kv, db, NULL, check_and_recovery_kv_cb);
    if (db->gc_request) {
        gc_collect(db);
        goto __retry;
    }

    db->in_recovery_check = false;

    /* unlock the KV cache */
    db_unlock(db);

    return result;
}

/**
 * The KV database initialization.
 *
 * @param db database object
 * @param name database name
 * @param part_name partition name
 * @param default_kv the default KV set @see ef_default_kv
 * @param user_data user data
 *
 * @return result
 */
EfErrCode ef_kvdb_init(ef_kvdb_t db, const char *name, const char *part_name, struct ef_default_kv *default_kv,
        void *user_data)
{
    EfErrCode result = EF_NO_ERR;

#ifdef EF_KV_USING_CACHE
    size_t i;
#endif

    EF_ASSERT(default_kv);
    /* must be aligned with write granularity */
    EF_ASSERT((EF_STR_KV_VALUE_MAX_SIZE * 8) % EF_WRITE_GRAN == 0);

    result = _ef_init_ex((ef_db_t)db, name, part_name, EF_DB_TYPE_KV, user_data);

    db->gc_request = false;
    db->in_recovery_check = false;
    db->default_kvs = *default_kv;
    /* there is at least one empty sector for GC. */
    EF_ASSERT((EF_GC_EMPTY_SEC_THRESHOLD > 0 && EF_GC_EMPTY_SEC_THRESHOLD < SECTOR_NUM))
    /* must be aligned with sector size */
    EF_ASSERT(db_part_size(db) % db_sec_size(db) == 0);
    /* must be has more than 2 sector */
    EF_ASSERT(db_part_size(db) / db_sec_size(db) >= 2);

#ifdef EF_KV_USING_CACHE
    for (i = 0; i < EF_SECTOR_CACHE_TABLE_SIZE; i++) {
        db->sector_cache_table[i].addr = EF_DATA_UNUSED;
    }
    for (i = 0; i < EF_KV_CACHE_TABLE_SIZE; i++) {
        db->kv_cache_table[i].addr = EF_DATA_UNUSED;
    }
#endif /* EF_KV_USING_CACHE */


    EF_DEBUG("KV in partition %s, size is %u bytes.\n", ((ef_db_t)db)->part->name, db_part_size(db));

    result = _ef_kv_load(db);

#ifdef EF_KV_AUTO_UPDATE
    if (result == EF_NO_ERR) {
        kv_auto_update(db);
    }
#endif

    _ef_init_finish((ef_db_t)db, result);

    return result;
}

#endif /* defined(EF_USING_KVDB) */
