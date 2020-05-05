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
 * Function: Time series log (like TSDB) feature implement source file.
 * Created on: 2020-04-30
 */

#include <string.h>
#include <easyflash.h>
#include <ef_low_lvl.h>

#define EF_LOG_TAG "[tsl]"
/* rewrite log prefix */
#undef  EF_LOG_PREFIX2
#define EF_LOG_PREFIX2()                         EF_PRINT("[%s] ", db_name(db))

#if defined(EF_USING_TSDB)

/* magic word(`T`, `S`, `L`, `0`) */
#define SECTOR_MAGIC_WORD                        0x304C5354

#define TSL_STATUS_TABLE_SIZE                    EF_STATUS_TABLE_SIZE(EF_TSL_STATUS_NUM)

#define SECTOR_HDR_DATA_SIZE                     (EF_WG_ALIGN(sizeof(struct sector_hdr_data)))
#define LOG_IDX_DATA_SIZE                        (EF_WG_ALIGN(sizeof(struct log_idx_data)))
#define LOG_IDX_TS_OFFSET                        ((unsigned long)(&((struct log_idx_data *)0)->time))
#define SECTOR_MAGIC_OFFSET                      ((unsigned long)(&((struct sector_hdr_data *)0)->magic))
#define SECTOR_START_TIME_OFFSET                 ((unsigned long)(&((struct sector_hdr_data *)0)->start_time))
#define SECTOR_END0_TIME_OFFSET                  ((unsigned long)(&((struct sector_hdr_data *)0)->end_info[0].time))
#define SECTOR_END0_IDX_OFFSET                   ((unsigned long)(&((struct sector_hdr_data *)0)->end_info[0].index))
#define SECTOR_END0_STATUS_OFFSET                ((unsigned long)(&((struct sector_hdr_data *)0)->end_info[0].status))
#define SECTOR_END1_TIME_OFFSET                  ((unsigned long)(&((struct sector_hdr_data *)0)->end_info[1].time))
#define SECTOR_END1_IDX_OFFSET                   ((unsigned long)(&((struct sector_hdr_data *)0)->end_info[1].index))
#define SECTOR_END1_STATUS_OFFSET                ((unsigned long)(&((struct sector_hdr_data *)0)->end_info[1].status))

/* the next address is get failed */
#define FAILED_ADDR                              0xFFFFFFFF

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

#define _EF_WRITE_STATUS(db, addr, status_table, status_num, status_index)     \
    do {                                                                       \
        result = _ef_write_status((ef_db_t)db, addr, status_table, status_num, status_index);\
        if (result != EF_NO_ERR) return result;                                \
    } while(0);

#define FLASH_WRITE(db, addr, buf, size)                                       \
    do {                                                                       \
        result = _ef_flash_write((ef_db_t)db, addr, buf, size);                \
        if (result != EF_NO_ERR) return result;                                \
    } while(0);

struct sector_hdr_data {
    uint8_t status[EF_STORE_STATUS_TABLE_SIZE];  /**< sector store status @see ef_sector_store_status_t */
    uint32_t magic;                              /**< magic word(`T`, `S`, `L`, `0`) */
    ef_time_t start_time;                        /**< the first start node's timestamp */
    struct {
		ef_time_t time;                          /**< the last end node's timestamp */
		uint32_t index;                          /**< the last end node's index */
		uint8_t status[TSL_STATUS_TABLE_SIZE];   /**< end node status, @see ef_tsl_status_t */
    } end_info[2];
    uint32_t reserved;
};
typedef struct sector_hdr_data *sector_hdr_data_t;

/* time series log node index data */
struct log_idx_data {
    uint8_t status_table[TSL_STATUS_TABLE_SIZE]; /**< node status, @see ef_tsl_status_t */
    ef_time_t time;                              /**< node timestamp */
    uint32_t log_len;                            /**< node total length (header + name + value), must align by EF_WRITE_GRAN */
    uint32_t log_addr;                           /**< node address */
};
typedef struct log_idx_data *log_idx_data_t;

struct query_count_args {
    ef_tsl_status_t status;
    size_t count;
};

struct check_sec_hdr_cb_args {
	ef_tsdb_t db;
    bool check_failed;
    size_t empty_num;
    uint32_t empty_addr;
};

static EfErrCode read_tsl(ef_tsdb_t db, tsl_node_obj_t tsl)
{
    struct log_idx_data idx;
    /* read TSL index raw data */
    _ef_flash_read((ef_db_t)db, tsl->addr.index, (uint32_t *) &idx, sizeof(struct log_idx_data));
    tsl->status = (ef_tsl_status_t) _ef_get_status(idx.status_table, EF_TSL_STATUS_NUM);
    if (tsl->status == EF_TSL_PRE_WRITE) {
        tsl->log_len = db->max_len;
        tsl->addr.log = EF_DATA_UNUSED;
        tsl->time = 0;
    } else {
        tsl->log_len = idx.log_len;
        tsl->addr.log = idx.log_addr;
        tsl->time = idx.time;
    }

    return EF_NO_ERR;
}

static uint32_t get_next_sector_addr(ef_tsdb_t db, tsl_sec_info_t pre_sec, uint32_t traversed_len)
{
    if (traversed_len + db_sec_size(db) <= db_part_size(db)) {
        if (pre_sec->addr + db_sec_size(db) < db_part_size(db)) {
            return pre_sec->addr + db_sec_size(db);
        } else {
            /* the next sector is on the top of the partition */
            return 0;
        }
    } else {
        /* finished */
        return FAILED_ADDR;
    }
}

static uint32_t get_next_tsl_addr(tsl_sec_info_t sector, tsl_node_obj_t pre_tsl)
{
    uint32_t addr = FAILED_ADDR;

    if (sector->status == EF_SECTOR_STORE_EMPTY) {
        return FAILED_ADDR;
    }

    if (pre_tsl->addr.index + LOG_IDX_DATA_SIZE <= sector->end_idx) {
        addr = pre_tsl->addr.index + LOG_IDX_DATA_SIZE;
    } else {
        /* no TSL */
        return FAILED_ADDR;
    }

    return addr;
}

static EfErrCode read_sector_info(ef_tsdb_t db, uint32_t addr, tsl_sec_info_t sector, bool traversal)
{
    EfErrCode result = EF_NO_ERR;
    struct sector_hdr_data sec_hdr;

    EF_ASSERT(sector);

    /* read sector header raw data */
    _ef_flash_read((ef_db_t)db, addr, (uint32_t *)&sec_hdr, sizeof(struct sector_hdr_data));

    sector->addr = addr;
    sector->magic = sec_hdr.magic;

    /* check magic word */
    if (sector->magic != SECTOR_MAGIC_WORD) {
        sector->check_ok = false;
        return EF_INIT_FAILED;
    }
    sector->check_ok = true;
    sector->status = (ef_sector_store_status_t) _ef_get_status(sec_hdr.status, EF_SECTOR_STORE_STATUS_NUM);
    sector->start_time = sec_hdr.start_time;
    sector->end_info_stat[0] = (ef_tsl_status_t) _ef_get_status(sec_hdr.end_info[0].status, EF_TSL_STATUS_NUM);
    sector->end_info_stat[1] = (ef_tsl_status_t) _ef_get_status(sec_hdr.end_info[1].status, EF_TSL_STATUS_NUM);
    if (sector->end_info_stat[0] == EF_TSL_WRITE) {
        sector->end_time = sec_hdr.end_info[0].time;
        sector->end_idx = sec_hdr.end_info[0].index;
    } else if (sector->end_info_stat[1] == EF_TSL_WRITE) {
        sector->end_time = sec_hdr.end_info[1].time;
        sector->end_idx = sec_hdr.end_info[1].index;
    } else if (sector->end_info_stat[0] == EF_TSL_PRE_WRITE && sector->end_info_stat[1] == EF_TSL_PRE_WRITE) {
        //TODO There is no valid end node info on this sector, need impl fast query this sector by ef_tsl_iter_by_time
        EF_ASSERT(0);
    }
    /* traversal all TSL and calculate the remain space size */
    sector->empty_idx = sector->addr + SECTOR_HDR_DATA_SIZE;
    sector->empty_data = sector->addr + db_sec_size(db);
    /* the TSL's data is saved from sector bottom, and the TSL's index saved from the sector top */
    sector->remain = sector->empty_data - sector->empty_idx;
    if (sector->status == EF_SECTOR_STORE_USING && traversal) {
        struct tsl_node_obj tsl;

        tsl.addr.index = sector->empty_idx;
        while (read_tsl(db, &tsl) == EF_NO_ERR) {
            if (tsl.status == EF_TSL_UNUSED) {
                break;
            }
            sector->end_time = tsl.time;
            sector->end_idx = tsl.addr.index;
            sector->empty_idx += LOG_IDX_DATA_SIZE;
            sector->empty_data -= EF_WG_ALIGN(tsl.log_len);
            tsl.addr.index += LOG_IDX_DATA_SIZE;
            if (sector->remain > LOG_IDX_DATA_SIZE + EF_WG_ALIGN(tsl.log_len)) {
                sector->remain -= (LOG_IDX_DATA_SIZE + EF_WG_ALIGN(tsl.log_len));
            } else {
                EF_INFO("Error: this TSL (0x%08lX) size (%lu) is out of bound.\n", tsl.addr.index, tsl.log_len);
                sector->remain = 0;
                result = EF_READ_ERR;
                break;
            }
        }
    }

    return result;
}

static EfErrCode format_sector(ef_tsdb_t db, uint32_t addr)
{
    EfErrCode result = EF_NO_ERR;
    struct sector_hdr_data sec_hdr;

    EF_ASSERT(addr % db_sec_size(db) == 0);

    result = _ef_flash_erase((ef_db_t)db, addr, db_sec_size(db));
    if (result == EF_NO_ERR) {
        _EF_WRITE_STATUS(db, addr, sec_hdr.status, EF_SECTOR_STORE_STATUS_NUM, EF_SECTOR_STORE_EMPTY);
        /* set the magic */
        sec_hdr.magic = SECTOR_MAGIC_WORD;
        FLASH_WRITE(db, addr + SECTOR_MAGIC_OFFSET, &sec_hdr.magic, sizeof(sec_hdr.magic));
    }

    return result;
}

static void sector_iterator(ef_tsdb_t db, tsl_sec_info_t sector, ef_sector_store_status_t status, void *arg1,
        void *arg2, bool (*callback)(tsl_sec_info_t sector, void *arg1, void *arg2), bool traversal)
{
    uint32_t sec_addr = sector->addr, traversed_len = 0;

    /* search all sectors */
    do {
        read_sector_info(db, sec_addr, sector, false);
        if (status == EF_SECTOR_STORE_UNUSED || status == sector->status) {
            if (traversal) {
                read_sector_info(db, sec_addr, sector, true);
            }
            /* iterator is interrupted when callback return true */
            if (callback && callback(sector, arg1, arg2)) {
                return;
            }
        }
        traversed_len += db_sec_size(db);
    } while ((sec_addr = get_next_sector_addr(db, sector, traversed_len)) != FAILED_ADDR);
}

static EfErrCode write_tsl(ef_tsdb_t db, ef_blob_t blob, ef_time_t time)
{
    EfErrCode result = EF_NO_ERR;
    struct log_idx_data idx;
    uint32_t idx_addr = db->cur_sec.empty_idx;

    idx.log_len = blob->size;
    idx.time = time;
    idx.log_addr = db->cur_sec.empty_data - EF_WG_ALIGN(idx.log_len);
    /* write the status will by write granularity */
    _EF_WRITE_STATUS(db, idx_addr, idx.status_table, EF_TSL_STATUS_NUM, EF_TSL_PRE_WRITE);
    /* write other index info */
    FLASH_WRITE(db, idx_addr + LOG_IDX_TS_OFFSET, &idx.time,  sizeof(struct log_idx_data) - LOG_IDX_TS_OFFSET);
    /* write blob data */
    FLASH_WRITE(db, idx.log_addr, blob->buf, blob->size);
    /* write the status will by write granularity */
    _EF_WRITE_STATUS(db, idx_addr, idx.status_table, EF_TSL_STATUS_NUM, EF_TSL_WRITE);

    return result;
}

static EfErrCode update_sec_status(ef_tsdb_t db, tsl_sec_info_t sector, ef_blob_t blob, ef_time_t cur_time)
{
	EfErrCode result = EF_NO_ERR;
	uint8_t status[EF_STORE_STATUS_TABLE_SIZE];

	if (sector->status == EF_SECTOR_STORE_USING && sector->remain < LOG_IDX_DATA_SIZE + EF_WG_ALIGN(blob->size)) {
        uint8_t end_status[TSL_STATUS_TABLE_SIZE];
        uint32_t end_index = sector->empty_idx - LOG_IDX_DATA_SIZE, new_sec_addr, cur_sec_addr = sector->addr;
        /* save the end node index and timestamp */
        if (sector->end_info_stat[0] == EF_TSL_UNUSED) {
            _EF_WRITE_STATUS(db, cur_sec_addr + SECTOR_END0_STATUS_OFFSET, end_status, EF_TSL_STATUS_NUM, EF_TSL_PRE_WRITE);
            FLASH_WRITE(db, cur_sec_addr + SECTOR_END0_TIME_OFFSET, (uint32_t * )&db->last_time, sizeof(ef_time_t));
            FLASH_WRITE(db, cur_sec_addr + SECTOR_END0_IDX_OFFSET, &end_index, sizeof(end_index));
            _EF_WRITE_STATUS(db, cur_sec_addr + SECTOR_END0_STATUS_OFFSET, end_status, EF_TSL_STATUS_NUM, EF_TSL_WRITE);
        } else if (sector->end_info_stat[1] == EF_TSL_UNUSED) {
            _EF_WRITE_STATUS(db, cur_sec_addr + SECTOR_END1_STATUS_OFFSET, end_status, EF_TSL_STATUS_NUM, EF_TSL_PRE_WRITE);
            FLASH_WRITE(db, cur_sec_addr + SECTOR_END1_TIME_OFFSET, (uint32_t * )&db->last_time, sizeof(ef_time_t));
            FLASH_WRITE(db, cur_sec_addr + SECTOR_END1_IDX_OFFSET, &end_index, sizeof(end_index));
            _EF_WRITE_STATUS(db, cur_sec_addr + SECTOR_END1_STATUS_OFFSET, end_status, EF_TSL_STATUS_NUM, EF_TSL_WRITE);
        }
        /* change current sector to full */
        _EF_WRITE_STATUS(db, cur_sec_addr, status, EF_SECTOR_STORE_STATUS_NUM, EF_SECTOR_STORE_FULL);
        /* calculate next sector address */
        if (sector->addr + db_sec_size(db) < db_part_size(db)) {
            new_sec_addr = sector->addr + db_sec_size(db);
        } else {
            new_sec_addr = 0;
        }
        read_sector_info(db, new_sec_addr, &db->cur_sec, false);
        if (sector->status != EF_SECTOR_STORE_EMPTY) {
            /* calculate the oldest sector address */
            if (new_sec_addr + db_sec_size(db) < db_part_size(db)) {
                db->oldest_addr = new_sec_addr + db_sec_size(db);
            } else {
                db->oldest_addr = 0;
            }
            format_sector(db, new_sec_addr);
            read_sector_info(db, new_sec_addr, &db->cur_sec, false);
        }
    }

    if (sector->status == EF_SECTOR_STORE_EMPTY) {
        /* change the sector to using */
        sector->status = EF_SECTOR_STORE_USING;
        sector->start_time = cur_time;
        _EF_WRITE_STATUS(db, sector->addr, status, EF_SECTOR_STORE_STATUS_NUM, EF_SECTOR_STORE_USING);
        /* save the start timestamp */
        FLASH_WRITE(db, sector->addr + SECTOR_START_TIME_OFFSET, (uint32_t *)&cur_time, sizeof(ef_time_t));
    }

    return result;
}

static EfErrCode tsl_append(ef_tsdb_t db, ef_blob_t blob)
{
    EfErrCode result = EF_NO_ERR;
    ef_time_t cur_time = db->get_time();

    EF_ASSERT(blob->size <= db->max_len);

    update_sec_status(db, &db->cur_sec, blob, cur_time);

    /* write the TSL node */
    result = write_tsl(db, blob, cur_time);

    /* recalculate the current using sector info */
    db->cur_sec.end_idx = db->cur_sec.empty_idx;
    db->cur_sec.end_time = cur_time;
    db->cur_sec.empty_idx += LOG_IDX_DATA_SIZE;
    db->cur_sec.empty_data -= EF_WG_ALIGN(blob->size);
    db->cur_sec.remain -= LOG_IDX_DATA_SIZE + EF_WG_ALIGN(blob->size);

    if (cur_time >= db->last_time) {
        db->last_time = cur_time;
    } else {
        EF_INFO("Warning: current timestamp (%ld) is less than the last save timestamp (%ld)\n", cur_time, db->last_time);
    }

    return result;
}

/**
 * Append a new log to TSDB.
 *
 * @param db database object
 * @param blob log blob data
 *
 * @return result
 */
EfErrCode ef_tsl_append(ef_tsdb_t db, ef_blob_t blob)
{
    EfErrCode result = EF_NO_ERR;

    if (!db_init_ok(db)) {
        EF_INFO("Error: TSL (%s) isn't initialize OK.\n", db_name(db));
        return EF_INIT_FAILED;
    }

    db_lock(db);
    result = tsl_append(db, blob);
    db_unlock(db);

    return result;
}

/**
 * The TSDB iterator for each TSL.
 *
 * @param db database object
 * @param cb callback
 * @param arg callback argument
 */
void ef_tsl_iter(ef_tsdb_t db, ef_tsl_cb cb, void *arg)
{
    struct tsl_sec_info sector;
    uint32_t sec_addr, traversed_len = 0;
    struct tsl_node_obj tsl;

    if (!db_init_ok(db)) {
        EF_INFO("Error: TSL (%s) isn't initialize OK.\n", db_name(db));
    }

    if (cb == NULL) {
        return;
    }

    sec_addr = db->oldest_addr;
    /* search all sectors */
    do {
        if (read_sector_info(db, sec_addr, &sector, false) != EF_NO_ERR) {
            continue;
        }
        /* sector has TSL */
        if (sector.status == EF_SECTOR_STORE_USING || sector.status == EF_SECTOR_STORE_FULL) {
            if (sector.status == EF_SECTOR_STORE_USING) {
                /* copy the current using sector status  */
                sector = db->cur_sec;
            }
            tsl.addr.index = sector.addr + SECTOR_HDR_DATA_SIZE;
            /* search all TSL */
            do {
                read_tsl(db, &tsl);
                /* iterator is interrupted when callback return true */
                if (cb(&tsl, arg)) {
                    return;
                }
            } while ((tsl.addr.index = get_next_tsl_addr(&sector, &tsl)) != FAILED_ADDR);
        }
        traversed_len += db_sec_size(db);
    } while ((sec_addr = get_next_sector_addr(db, &sector, traversed_len)) != FAILED_ADDR);
}

/**
 * The TSDB iterator for each TSL by timestamp.
 *
 * @param db database object
 * @param from starting timestap
 * @param to ending timestap
 * @param cb callback
 * @param arg callback argument
 */
void ef_tsl_iter_by_time(ef_tsdb_t db, ef_time_t from, ef_time_t to, ef_tsl_cb cb, void *cb_arg)
{
    struct tsl_sec_info sector;
    uint32_t sec_addr, traversed_len = 0;
    struct tsl_node_obj tsl;
    bool found_start_tsl = false;

    if (!db_init_ok(db)) {
        EF_INFO("Error: TSL (%s) isn't initialize OK.\n", db_name(db));
    }

//    EF_INFO("from %s", ctime((const time_t * )&from));
//    EF_INFO("to %s", ctime((const time_t * )&to));

    if (cb == NULL) {
        return;
    }

    sec_addr = db->oldest_addr;
    /* search all sectors */
    do {
        if (read_sector_info(db, sec_addr, &sector, false) != EF_NO_ERR) {
            continue;
        }
        /* sector has TSL */
        if ((sector.status == EF_SECTOR_STORE_USING || sector.status == EF_SECTOR_STORE_FULL)) {
            if (sector.status == EF_SECTOR_STORE_USING) {
                /* copy the current using sector status  */
                sector = db->cur_sec;
            }
            if ((!found_start_tsl && ((sector.start_time <= from && sector.end_time >= from) || (sector.start_time <= to)))
                    || (found_start_tsl)) {
                uint32_t start = sector.addr + SECTOR_HDR_DATA_SIZE, end = sector.end_idx;

                found_start_tsl = true;
                /* search start TSL address, using binary search algorithm */
                while (start <= end) {
                    tsl.addr.index = start + ((end - start) / 2 + 1) / LOG_IDX_DATA_SIZE * LOG_IDX_DATA_SIZE;
                    read_tsl(db, &tsl);
                    if (tsl.time < from) {
                        start = tsl.addr.index + LOG_IDX_DATA_SIZE;
                    } else {
                        end = tsl.addr.index - LOG_IDX_DATA_SIZE;
                    }
                }
                tsl.addr.index = start;
                /* search all TSL */
                do {
                    read_tsl(db, &tsl);
                    if (tsl.time >= from && tsl.time <= to) {
                        /* iterator is interrupted when callback return true */
                        if (cb(&tsl, cb_arg)) {
                            return;
                        }
                    } else {
                        return;
                    }
                } while ((tsl.addr.index = get_next_tsl_addr(&sector, &tsl)) != FAILED_ADDR);
            }
		} else if (sector.status == EF_SECTOR_STORE_EMPTY) {
			return;
		}
        traversed_len += db_sec_size(db);
    } while ((sec_addr = get_next_sector_addr(db, &sector, traversed_len)) != FAILED_ADDR);
}

static bool query_count_cb(tsl_node_obj_t tsl, void *arg)
{
    struct query_count_args *args = arg;

    if (tsl->status == args->status) {
        args->count++;
    }

    return false;
}

/**
 * Query some TSL's count by timestamp and status.
 *
 * @param db database object
 * @param from starting timestap
 * @param to ending timestap
 * @param status status
 */
size_t ef_tsl_query_count(ef_tsdb_t db, ef_time_t from, ef_time_t to, ef_tsl_status_t status)
{
    struct query_count_args arg = { 0 };

    arg.status = status;

    if (!db_init_ok(db)) {
        EF_INFO("Error: TSL (%s) isn't initialize OK.\n", db_name(db));
        return EF_INIT_FAILED;
    }

    ef_tsl_iter_by_time(db, from, to, query_count_cb, &arg);

    return arg.count;

}

/**
 * Set the TSL status.
 *
 * @param db database object
 * @param tsl TSL object
 * @param status status
 *
 * @return result
 */
EfErrCode ef_tsl_set_status(ef_tsdb_t db, ef_tsl_t tsl, ef_tsl_status_t status)
{
    EfErrCode result = EF_NO_ERR;
    uint8_t status_table[TSL_STATUS_TABLE_SIZE];

    /* write the status will by write granularity */
    _EF_WRITE_STATUS(db, tsl->addr.index, status_table, EF_TSL_STATUS_NUM, status);

    return result;
}

/**
 * Convert the TSL object to blob object
 *
 * @param tsl TSL object
 * @param blob blob object
 *
 * @return new blob object
 */
ef_blob_t ef_tsl_to_blob(ef_tsl_t tsl, ef_blob_t blob)
{
    blob->saved.addr = tsl->addr.log;
    blob->saved.meta_addr = tsl->addr.index;
    blob->saved.len = tsl->log_len;

    return blob;
}

static bool check_sec_hdr_cb(tsl_sec_info_t sector, void *arg1, void *arg2)
{
    struct check_sec_hdr_cb_args *arg = arg1;
    ef_tsdb_t db = arg->db;

    if (!sector->check_ok) {
        EF_INFO("Warning: Sector (0x%08lX) header check failed.\n", sector->addr);
        (arg->check_failed) = true;
        return true;
    } else if (sector->status == EF_SECTOR_STORE_USING) {
        if (db->cur_sec.addr == EF_DATA_UNUSED) {
            memcpy(&db->cur_sec, sector, sizeof(struct tsl_sec_info));
        } else {
            EF_INFO("Warning: Sector status is wrong, there are multiple sectors in use.\n");
            (arg->check_failed) = true;
            return true;
        }
    } else if (sector->status == EF_SECTOR_STORE_EMPTY) {
        (arg->empty_num) += 1;
        arg->empty_addr = sector->addr;
        if ((arg->empty_num) == 1 && db->cur_sec.addr == EF_DATA_UNUSED) {
            memcpy(&db->cur_sec, sector, sizeof(struct tsl_sec_info));
        }
    }

    return false;
}
static bool format_all_cb(tsl_sec_info_t sector, void *arg1, void *arg2)
{
	ef_tsdb_t db = arg1;

    format_sector(db, sector->addr);

    return false;
}

static void tsl_format_all(ef_tsdb_t db)
{
    struct tsl_sec_info sector;

    sector.addr = 0;
    sector_iterator(db, &sector, EF_SECTOR_STORE_UNUSED, db, NULL, format_all_cb, false);
    db->oldest_addr = 0;
    db->cur_sec.addr = 0;
    /* read the current using sector info */
    read_sector_info(db, db->cur_sec.addr, &db->cur_sec, false);

    EF_INFO("All sector format finished.\n");
}

/**
 * Clean all the data in the TSDB.
 *
 * @note It's DANGEROUS. This operation is not reversible.
 *
 * @param db database object
 */
void ef_tsl_clean(ef_tsdb_t db)
{
    db_lock(db);
    tsl_format_all(db);
    db_unlock(db);
}

/**
 * The time series database initialization.
 *
 * @param db database object
 * @param name database name
 * @param part_name partition name
 * @param get_time get current time function
 * @param max_len maximum length of each log
 * @param user_data user data
 *
 * @return result
 */
EfErrCode ef_tsdb_init(ef_tsdb_t db, const char *name, const char *part_name, ef_get_time get_time, size_t max_len, void *user_data)
{
    EfErrCode result = EF_NO_ERR;
    struct tsl_sec_info sector;
    struct check_sec_hdr_cb_args check_sec_arg = { db, false, 0, 0};

    EF_ASSERT(get_time);

    result = _ef_init_ex((ef_db_t)db, name, part_name, EF_DB_TYPE_TSL, user_data);

    db->get_time = get_time;
    db->max_len = max_len;
    db->oldest_addr = EF_DATA_UNUSED;
    db->cur_sec.addr = EF_DATA_UNUSED;
    /* must align with sector size */
    EF_ASSERT(db_part_size(db) % db_sec_size(db) == 0);
    /* must have more than or equal 2 sector */
    EF_ASSERT(db_part_size(db) / db_sec_size(db) >= 2);
    /* must less than sector size */
    EF_ASSERT(max_len < db_sec_size(db));

    /* check all sector header */
    sector.addr = 0;
    sector_iterator(db, &sector, EF_SECTOR_STORE_UNUSED, &check_sec_arg, NULL, check_sec_hdr_cb, true);
    /* format all sector when check failed */
    if (check_sec_arg.check_failed) {
        tsl_format_all(db);
    } else {
        uint32_t latest_addr;
        if (check_sec_arg.empty_num > 0) {
            latest_addr = check_sec_arg.empty_addr;
        } else {
            latest_addr = db->cur_sec.addr;
        }
        /* db->cur_sec is the latest sector, and the next is the oldest sector */
        if (latest_addr + db_sec_size(db) >= db_part_size(db)) {
            /* db->cur_sec is the the bottom of the partition */
            db->oldest_addr = 0;
        } else {
            db->oldest_addr = latest_addr + db_sec_size(db);
        }
    }
    EF_DEBUG("tsdb (%s) oldest sectors is 0x%08lX, current using sector is 0x%08lX.\n", db_name(db), db->oldest_addr,
            db->cur_sec.addr);
    /* read the current using sector info */
    read_sector_info(db, db->cur_sec.addr, &db->cur_sec, true);

    _ef_init_finish((ef_db_t)db, result);

    return result;
}

#endif /* defined(EF_USING_TSDB) */
