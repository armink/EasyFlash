/*
 * This file is part of the EasyFlash Library.
 *
 * Copyright (c) 2019, Armink, <armink.ztl@gmail.com>
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
 * Function: Environment variables operating interface. This is the Next Generation version.
 * Created on: 2019-02-02
 */

#include <easyflash.h>

#if defined(EF_USING_ENV)

/* the flash write granularity, unit: bit */
//#define EF_WRITE_GRAN                          1
#define EF_WRITE_GRAN                            8



#if EF_WRITE_GRAN != 1 && EF_WRITE_GRAN != 8 && EF_WRITE_GRAN != 32 && EF_WRITE_GRAN != 64
#error "the write gran can be only setting as 1, 8, 32 and 64"
#endif

/* the ENV max name length must less then it */
#define EF_ENV_NAME_MAX                          32
/* magic word(`E`, `F`, `4`, `0`) */
#define SECTOR_MAGIC_WORD                        0x30344645

/* the using status sector table length */
#ifndef USING_SECTOR_TABLE_LEN
#define USING_SECTOR_TABLE_LEN                   4
#endif

/* the string ENV value buffer size for legacy ef_get_env() function */
#ifndef EF_STR_ENV_VALUE_MAX_SIZE
#define EF_STR_ENV_VALUE_MAX_SIZE                128
#endif

/* the sector remain threshold before full status */
#ifndef EF_SEC_REMAIN_THRESHOLD
#define EF_SEC_REMAIN_THRESHOLD                  (ENV_HDR_DATA_SIZE + EF_ENV_NAME_MAX)
#endif

/* the total remain empty sector threshold before GC */
#ifndef EF_GC_EMPTY_SEC_THRESHOLD
#define EF_GC_EMPTY_SEC_THRESHOLD                1
#endif

/* the sector is not combined value */
#define SECTOR_NOT_COMBINED                      0xFFFFFFFF
/* the next address is get failed */
#define GET_ADDR_FAILED                          0xFFFFFFFF

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

#if (EF_WRITE_GRAN == 1)
#define STATUS_TABLE_SIZE(status_number)         ((status_number * EF_WRITE_GRAN + 7)/8)
#else
#define STATUS_TABLE_SIZE(status_number)         (((status_number - 1) * EF_WRITE_GRAN + 7)/8)
#endif

#define STORE_STATUS_TABLE_SIZE                  STATUS_TABLE_SIZE(SECTOR_STORE_STATUS_NUM)
#define DIRTY_STATUS_TABLE_SIZE                  STATUS_TABLE_SIZE(SECTOR_DIRTY_STATUS_NUM)
#define ENV_STATUS_TABLE_SIZE                    STATUS_TABLE_SIZE(ENV_STATUS_NUM)

#define SECTOR_SIZE                              EF_ERASE_MIN_SIZE
#define SECTOR_NUM                               (ENV_AREA_SIZE / EF_ERASE_MIN_SIZE)

#define SECTOR_HDR_DATA_SIZE                     (EF_WG_ALIGN(sizeof(struct sector_hdr_data)))
#define SECTOR_DIRTY_OFFSET                      ((unsigned long)(&((struct sector_hdr_data *)0)->status_table.dirty))
#define ENV_HDR_DATA_SIZE                        (EF_WG_ALIGN(sizeof(struct env_hdr_data)))
#define ENV_LEN_OFFSET                           ((unsigned long)(&((struct env_hdr_data *)0)->len))
#define ENV_NAME_LEN_OFFSET                      ((unsigned long)(&((struct env_hdr_data *)0)->name_len))

enum sector_store_status {
    SECTOR_STORE_UNUSED,
    SECTOR_STORE_EMPTY,
    SECTOR_STORE_USING,
    SECTOR_STORE_FULL,
    SECTOR_STORE_STATUS_NUM,
};
typedef enum sector_store_status sector_store_status_t;

enum sector_dirty_status {
    SECTOR_DIRTY_UNUSED,
    SECTOR_DIRTY_FALSE,
    SECTOR_DIRTY_TRUE,
    SECTOR_DIRTY_GC,
    SECTOR_DIRTY_STATUS_NUM,
};
typedef enum sector_dirty_status sector_dirty_status_t;

enum env_status {
    ENV_UNUSED,
    ENV_PRE_WRITE,
    ENV_WRITE,
    ENV_PRE_DELETE,
    ENV_DELETED,
    ENV_ERR_HDR,
    ENV_STATUS_NUM,
};
typedef enum env_status env_status_t;

struct sector_hdr_data {
    struct {
        uint8_t store[STORE_STATUS_TABLE_SIZE];  /**< sector store status @see sector_store_status_t */
        uint8_t dirty[DIRTY_STATUS_TABLE_SIZE];  /**< sector dirty status @see sector_dirty_status_t */
    } status_table;
    uint32_t magic;                              /**< magic word(`E`, `F`, `4`, `0`) */
    uint32_t combined;                           /**< the combined next sector number, 0xFFFFFFFF: not combined */
    uint32_t reserved;
};
typedef struct sector_hdr_data *sector_hdr_data_t;

struct sector_meta_data {
    bool check_ok;                               /**< sector header check is OK */
    struct {
        sector_store_status_t store;             /**< sector store status @see sector_store_status_t */
        sector_dirty_status_t dirty;             /**< sector dirty status @see sector_dirty_status_t */
    } status;
    uint32_t addr;                               /**< sector start address */
    uint32_t magic;                              /**< magic word(`E`, `F`, `4`, `0`) */
    uint32_t combined;                           /**< the combined next sector number, 0xFFFFFFFF: not combined */
    size_t remain;                               /**< remain size */
    uint32_t empty_env;                          /**< the next empty ENV node start address */
};
typedef struct sector_meta_data *sector_meta_data_t;

struct env_hdr_data {
    uint8_t status_table[ENV_STATUS_TABLE_SIZE]; /**< ENV node status, @see node_status_t */
    uint32_t len;                                /**< ENV node total length (header + name + value), must align by EF_WRITE_GRAN */
    uint32_t crc32;                              /**< ENV node crc32(name_len + data_len + name + value) */
    uint8_t name_len;                            /**< name length */
    uint32_t value_len;                          /**< value length */
};
typedef struct env_hdr_data *env_hdr_data_t;

struct env_meta_data {
    bool crc_is_ok;                              /**< ENV node CRC32 check is OK */
    env_status_t status;                         /**< ENV node status, @see node_status_t */
    uint8_t name_len;                            /**< name length */
    uint32_t len;                                /**< ENV node total length (header + name + value), must align by EF_WRITE_GRAN */
    uint32_t value_len;                          /**< value length */
    char name[EF_ENV_NAME_MAX];                  /**< name */
    struct {
        uint32_t start;                          /**< ENV node start address */
        uint32_t value;                          /**< value start address */
    } addr;
};
typedef struct env_meta_data *env_meta_data_t;

/* ENV start address in flash */
static uint32_t env_start_addr = 0;
/* default ENV set, must be initialized by user */
static ef_env const *default_env_set;
/* default ENV set size, must be initialized by user */
static size_t default_env_set_size = 0;
/* initialize OK flag */
static bool init_ok = false;
/* the using status sector table */
static struct sector_meta_data using_sec_table[USING_SECTOR_TABLE_LEN];
/* the GC is triggered */
static bool gc_is_triggered = false;

static size_t set_status(uint8_t status_table[], size_t status_num, size_t status_index)
{
    size_t byte_index = ~0UL;
    /*
     * | write garn |       status0       |       status1       |      status2         |
     * ---------------------------------------------------------------------------------
     * |    1bit    | 0xFF                | 0x7F                |  0x3F                |
     * |    8bit    | 0xFFFF              | 0x00FF              |  0x0000              |
     * |   32bit    | 0xFFFFFFFF FFFFFFFF | 0x00FFFFFF FFFFFFFF |  0x00FFFFFF 00FFFFFF |
     */
    memset(status_table, 0xFF, STATUS_TABLE_SIZE(status_num));
    if (status_index > 0) {
#if (EF_WRITE_GRAN == 1)
        byte_index = (status_index - 1) / 8;
        status_table[byte_index] &= ~(0x80 >> ((status_index - 1) % 8));
#else
        byte_index = (status_index - 1) * (EF_WRITE_GRAN / 8);
        status_table[byte_index] = 0x00;
#endif /* EF_WRITE_GRAN == 1 */
    }

    return byte_index;
}

static size_t get_status(uint8_t status_table[], size_t status_num)
{
    size_t i = 0, status_num_bak = --status_num;

    while (status_num --) {
        /* get the first 0 position from end address to start address */
#if (EF_WRITE_GRAN == 1)
        if ((status_table[status_num / 8] & (0x80 >> (status_num % 8))) == 0x00) {
            break;
        }
#else /*  (EF_WRITE_GRAN == 8) ||  (EF_WRITE_GRAN == 32) ||  (EF_WRITE_GRAN == 64) */
        if (status_table[status_num * EF_WRITE_GRAN / 8] == 0x00) {
            break;
        }
#endif /* EF_WRITE_GRAN == 1 */
        i++;
    }

    return status_num_bak - i;
}

static EfErrCode write_status(uint32_t addr, uint8_t status_table[], size_t status_num, size_t status_index)
{
    EfErrCode result = EF_NO_ERR;
    size_t byte_index;

    EF_ASSERT(status_index < status_num);
    EF_ASSERT(status_table);

    /* set the status first */
    byte_index = set_status(status_table, status_num, status_index);

    /* the first status table value is all 1, so no need to write flash */
    if (byte_index == ~0UL) {
        return EF_NO_ERR;
    }
#if (EF_WRITE_GRAN == 1)
    result = ef_port_write(addr + byte_index, (uint32_t *)&status_table[byte_index], 1);
#else /*  (EF_WRITE_GRAN == 8) ||  (EF_WRITE_GRAN == 32) ||  (EF_WRITE_GRAN == 64) */
    /* write the status by write granularity
     * some flash (like stm32 onchip) NOT supported repeated write before erase */
    result = ef_port_write(addr + byte_index, (uint32_t *) &status_table[byte_index], EF_WRITE_GRAN / 8);
#endif /* EF_WRITE_GRAN == 1 */

    return result;
}

static size_t read_status(uint32_t addr, uint8_t status_table[], size_t total_num)
{
    EF_ASSERT(status_table);

    ef_port_read(addr, (uint32_t *) status_table, STATUS_TABLE_SIZE(total_num));

    return get_status(status_table, total_num);
}

static uint32_t get_next_env_addr(sector_meta_data_t sector, env_meta_data_t pre_env)
{
    uint8_t status_table[ENV_STATUS_TABLE_SIZE];
    uint32_t addr = GET_ADDR_FAILED;

    //TODO 可否直接共用 empty env addr
    if (sector->status.store == SECTOR_STORE_EMPTY) {
        return GET_ADDR_FAILED;
    }

    if (pre_env->addr.start == GET_ADDR_FAILED) {
        /* the first ENV address */
        addr = sector->addr + SECTOR_HDR_DATA_SIZE;
    } else {
        if (pre_env->addr.start <= sector->addr + SECTOR_SIZE) {
            /* next ENV address */
            addr = pre_env->addr.start + pre_env->len;
            if (addr > sector->addr + SECTOR_SIZE) {
                //TODO 扇区连续模式
                EF_ASSERT(0);
            }
        } else {
            /* no ENV */
            return GET_ADDR_FAILED;
        }
    }
    /* check ENV status, it's ENV_UNUSED when not using */
    if (read_status(addr, status_table, ENV_STATUS_NUM) != ENV_UNUSED) {
        return addr;
    } else {
        /* no ENV */
        return GET_ADDR_FAILED;
    }
}

static EfErrCode read_env(env_meta_data_t env)
{
    struct env_hdr_data env_hdr;
    uint8_t buf[32];
    uint32_t calc_crc32 = 0, crc_data_len, env_name_addr;
    EfErrCode result = EF_NO_ERR;
    /* read ENV header raw data */
    ef_port_read(env->addr.start, (uint32_t *)&env_hdr, sizeof(struct env_hdr_data));
    env->status = get_status(env_hdr.status_table, ENV_STATUS_NUM);
    if (env_hdr.len > ENV_AREA_SIZE) {
        if (env->status != ENV_ERR_HDR) {
            EF_DEBUG("Error: The ENV length is too big. May be the flash data has some errors.\n");
        }
        env->crc_is_ok = false;
        return EF_READ_ERR;
    }
    env->len = env_hdr.len;
    /* CRC32 data len(header.name_len + header.value_len + name + value) */
    crc_data_len = env->len - ENV_NAME_LEN_OFFSET;
    /* calculate the CRC32 value */
    for (size_t len = 0, size = 0; len < crc_data_len; len += size) {
        if (len + sizeof(buf) < crc_data_len) {
            size = sizeof(buf);
        } else {
            size = crc_data_len - len;
        }

        ef_port_read(env->addr.start + ENV_NAME_LEN_OFFSET + len, (uint32_t *) buf, EF_WG_ALIGN(size));
        calc_crc32 = ef_calc_crc32(calc_crc32, buf, size);
    }
    /* check CRC32 */
    if (calc_crc32 != env_hdr.crc32) {
        env->crc_is_ok = false;
        result = EF_READ_ERR;
    } else {
        env->crc_is_ok = true;
        /* the name is behind aligned ENV header */
        env_name_addr = env->addr.start + ENV_HDR_DATA_SIZE;
        ef_port_read(env_name_addr, (uint32_t *) env->name, EF_WG_ALIGN(env_hdr.name_len));
        /* the value is behind aligned name */
        env->addr.value = env_name_addr + EF_WG_ALIGN(env_hdr.name_len);
        env->value_len = env_hdr.value_len;
        env->name_len = env_hdr.name_len;
    }

    return result;
}

static EfErrCode read_sector_meta_data(uint32_t addr, sector_meta_data_t sector, bool traversal)
{
    EfErrCode result = EF_NO_ERR;
    struct sector_hdr_data sec_hdr;

    EF_ASSERT(addr % SECTOR_SIZE == 0);
    EF_ASSERT(sector);

    //TODO 支持通过 using_sec_table 获取
    /* read sector header raw data */
    ef_port_read(addr, (uint32_t *)&sec_hdr, sizeof(struct sector_hdr_data));

    sector->addr = addr;
    sector->magic = sec_hdr.magic;
    /* check magic word */
    if (sector->magic != SECTOR_MAGIC_WORD) {
        sector->check_ok = false;
        return EF_ENV_INIT_FAILED;
    }
    sector->check_ok = true;
    /* get other sector meta data */
    sector->combined = sec_hdr.combined;
    sector->status.store = get_status(sec_hdr.status_table.store, SECTOR_STORE_STATUS_NUM);
    sector->status.dirty = get_status(sec_hdr.status_table.dirty, SECTOR_DIRTY_STATUS_NUM);
    /* traversal all ENV and calculate the remain space size */
    if (traversal) {
        sector->remain = 0;
        sector->empty_env = sector->addr + SECTOR_HDR_DATA_SIZE;
        if (sector->status.store == SECTOR_STORE_EMPTY) {
            sector->remain = SECTOR_SIZE - SECTOR_HDR_DATA_SIZE;
        } else if (sector->status.store == SECTOR_STORE_USING) {
            struct env_meta_data env_meta;
            sector->remain = SECTOR_SIZE - SECTOR_HDR_DATA_SIZE;
            env_meta.addr.start = GET_ADDR_FAILED;
            while ((env_meta.addr.start = get_next_env_addr(sector, &env_meta)) != GET_ADDR_FAILED) {
                read_env(&env_meta);
                if (!env_meta.crc_is_ok) {
                    //TODO 完善 CRC 校验出错后的处理，比如标记扇区已经损坏
                    if (env_meta.status == ENV_PRE_WRITE || env_meta.status == ENV_ERR_HDR) {
                        /* ENV length was not write */
                        if (env_meta.len == ~0UL) {
                            /* the ENV length was not write */
                            env_meta.len = ENV_LEN_OFFSET;
                        } else if (env_meta.len > SECTOR_SIZE - SECTOR_HDR_DATA_SIZE && env_meta.len < ENV_AREA_SIZE) {
                            //TODO 扇区连续模式，或者写入长度没有写入完整
                            EF_ASSERT(0);
                        }
                    } else {
                        EF_INFO("Error: The ENV (@0x%08x) CRC32 check failed!\n", env_meta.addr.start);
                        sector->remain = 0;
                        result = EF_READ_ERR;
                        break;
                    }
                }
                sector->empty_env += env_meta.len;
                sector->remain -= env_meta.len;
            }
        }
    }

    return result;
}

static uint32_t get_next_sector_addr(sector_meta_data_t pre_sec)
{
    uint32_t next_addr;

    if (pre_sec->addr == GET_ADDR_FAILED) {
        return env_start_addr;
    } else {
        /* check ENV sector combined */
        if (pre_sec->combined == SECTOR_NOT_COMBINED) {
            next_addr = pre_sec->addr + SECTOR_SIZE;
        } else {
            next_addr = pre_sec->addr + pre_sec->combined * SECTOR_SIZE;
        }
        /* check range */
        if (next_addr < env_start_addr + ENV_AREA_SIZE) {
            return next_addr;
        } else {
            /* no sector */
            return GET_ADDR_FAILED;
        }
    }
}

static void env_iterator(env_meta_data_t env, void *arg1, void *arg2,
        bool (*callback)(env_meta_data_t env, void *arg1, void *arg2))
{
    struct sector_meta_data sector;
    uint32_t sec_addr;

    //TODO 支持通过 using_sec_table 遍历

    sector.addr = GET_ADDR_FAILED;
    /* search all sectors */
    while ((sec_addr = get_next_sector_addr(&sector)) != GET_ADDR_FAILED) {
        //TODO 检查所有扇区的剩余空间，是否有合适的扇区，优先 using_sec_table
        if (read_sector_meta_data(sec_addr, &sector, false) != EF_NO_ERR) {
            continue;
        }
        if (callback == NULL) {
            continue;
        }
        /* sector has ENV */
        if (sector.status.store == SECTOR_STORE_USING || sector.status.store == SECTOR_STORE_FULL) {
            env->addr.start = GET_ADDR_FAILED;
            /* search all ENV */
            while ((env->addr.start = get_next_env_addr(&sector, env)) != GET_ADDR_FAILED) {
                read_env(env);
                /* iterator is interrupted when callback return true */
                if (callback(env, arg1, arg2)) {
                    return;
                }
            }
        }
    }
}

static bool find_env_cb(env_meta_data_t env, void *arg1, void *arg2)
{
    const char *key = arg1;
    bool *find_ok = arg2;
    uint8_t max_len = strlen(key);

    if (max_len < env->name_len) {
        max_len = env->name_len;
    }
    /* check ENV */
    if (env->crc_is_ok && env->status == ENV_WRITE && !strncmp(env->name, key, max_len)) {
        *find_ok = true;
        return true;
    }
    return false;
}

static bool find_env(const char *key, env_meta_data_t env)
{
    bool find_ok = false;

    env_iterator(env, (void *)key, &find_ok, find_env_cb);

    return find_ok;
}

static bool ef_is_str(uint8_t *value, size_t len)
{
#define __is_print(ch)       ((unsigned int)((ch) - ' ') < 127u - ' ')

    for (size_t i; i < len; i++) {
        if (!__is_print(value[i])) {
            return false;
        }
    }
    return true;
}

/**
 * Get an ENV value by key name.
 *
 * @note this function is NOT supported reentrant
 * @note this function is DEPRECATED
 *
 * @param key ENV name
 *
 * @return value
 */
char *ef_get_env(const char *key)
{
    static char value[EF_STR_ENV_VALUE_MAX_SIZE];
    struct env_meta_data env;

    if (!init_ok) {
        EF_INFO("ENV isn't initialize OK.\n");
        return NULL;
    }

    if (find_env(key, &env)) {
        ef_port_read(env.addr.value, (uint32_t *)value, EF_WG_ALIGN(env.value_len));
        /* the return value must be string */
        if (ef_is_str((uint8_t *)value, env.value_len)) {
            return value;
        } else {
            EF_INFO("Warning: The ENV value isn't string. Could not be returned\n");
            return NULL;
        }
    }

    return NULL;
}

static EfErrCode write_env_hdr(uint32_t addr, env_hdr_data_t env_hdr) {
    EfErrCode result = EF_NO_ERR;
    /* write the status will by write granularity */
    result = write_status(addr, env_hdr->status_table, ENV_STATUS_NUM, ENV_PRE_WRITE);
    if (result != EF_NO_ERR) {
        return result;
    }
    /* write other header data */
    result = ef_port_write(addr + ENV_LEN_OFFSET, &env_hdr->len, sizeof(struct env_hdr_data) - ENV_LEN_OFFSET);

    return result;
}

static EfErrCode format_sector(uint32_t addr, uint32_t combined_value)
{
    EfErrCode result = EF_NO_ERR;
    struct sector_hdr_data sec_hdr;

    EF_ASSERT(addr % SECTOR_SIZE == 0);

    result = ef_port_erase(addr, SECTOR_SIZE);
    if (result == EF_NO_ERR) {
        /* initialize the header data */
        memset(&sec_hdr, 0xFF, sizeof(struct sector_hdr_data));
        set_status(sec_hdr.status_table.store, SECTOR_STORE_STATUS_NUM, SECTOR_STORE_EMPTY);
        set_status(sec_hdr.status_table.dirty, SECTOR_DIRTY_STATUS_NUM, SECTOR_DIRTY_FALSE);
        sec_hdr.magic = SECTOR_MAGIC_WORD;
        sec_hdr.combined = combined_value;
        sec_hdr.reserved = 0xFFFFFFFF;
        /* save the header */
        ef_port_write(addr, (uint32_t *)&sec_hdr, sizeof(struct sector_hdr_data));
    }

    return result;
}

static EfErrCode copy_env(uint32_t to, uint32_t from, size_t env_len)
{
    EfErrCode result = EF_NO_ERR;
    uint8_t status_table[ENV_STATUS_TABLE_SIZE];

    result = write_status(to, status_table, ENV_STATUS_NUM, ENV_PRE_WRITE);
    if (result == EF_NO_ERR) {
        uint8_t buf[32];
        env_len -= ENV_LEN_OFFSET;
        for (size_t len = 0, size = 0; len < env_len; len += size) {
            if (len + sizeof(buf) < env_len) {
                size = sizeof(buf);
            } else {
                size = env_len - len;
            }
            ef_port_read(from + ENV_LEN_OFFSET + len, (uint32_t *) buf, EF_WG_ALIGN(size));
            result = ef_port_write(to + ENV_LEN_OFFSET + len, (uint32_t *) buf, size);
            if (result != EF_NO_ERR) {
                return result;
            }
        }
        result = write_status(to, status_table, ENV_STATUS_NUM, ENV_WRITE);
    }

    return result;
}

static void sector_iterator(sector_meta_data_t sector, sector_store_status_t status, void *arg1, void *arg2,
        bool (*callback)(sector_meta_data_t sector, void *arg1, void *arg2), bool traversal_env) {
    uint32_t sec_addr;

    /* search all sectors */
    sector->addr = GET_ADDR_FAILED;
    while ((sec_addr = get_next_sector_addr(sector)) != GET_ADDR_FAILED) {
        //TODO 检查所有扇区的剩余空间，是否有合适的扇区，优先 using_sec_table
        read_sector_meta_data(sec_addr, sector, false);
        if (status == SECTOR_STORE_UNUSED || status == sector->status.store) {
            if (traversal_env) {
                read_sector_meta_data(sec_addr, sector, traversal_env);
            }
            /* iterator is interrupted when callback return true */
            if (callback && callback(sector, arg1, arg2)) {
                return;
            }
        }
    }
}

static bool alloc_env_cb(sector_meta_data_t sector, void *arg1, void *arg2)
{
    size_t *env_size = arg1;
    uint32_t *empty_env = arg2;

    /* sector has space */
    if (sector->check_ok && sector->remain > *env_size) {
        *empty_env = sector->empty_env;
        return true;
    }

    return false;
}

static uint32_t alloc_env(sector_meta_data_t sector, size_t env_size)
{
    uint32_t empty_env = GET_ADDR_FAILED;

    /* alloc the ENV from the using status sector first */
    sector_iterator(sector, SECTOR_STORE_USING, &env_size, &empty_env, alloc_env_cb, true);
    if (empty_env == GET_ADDR_FAILED) {
        sector_iterator(sector, SECTOR_STORE_EMPTY, &env_size, &empty_env, alloc_env_cb, true);
    }

    return empty_env;
}

static EfErrCode update_sec_status(sector_meta_data_t sector, size_t new_env_len, bool *is_full)
{
    uint8_t status_table[STORE_STATUS_TABLE_SIZE];
    EfErrCode result = EF_NO_ERR;
    /* change the current sector status */
    if (sector->status.store == SECTOR_STORE_EMPTY) {
        /* change the sector status to using */
        result = write_status(sector->addr, status_table, SECTOR_STORE_STATUS_NUM, SECTOR_STORE_USING);
    } else if (sector->status.store == SECTOR_STORE_USING) {
        /* check remain size */
        if (sector->remain - new_env_len < EF_SEC_REMAIN_THRESHOLD) {
            /* change the sector status to full */
            result = write_status(sector->addr, status_table, SECTOR_STORE_STATUS_NUM, SECTOR_STORE_FULL);
            if (is_full) {
                *is_full = true;
            }
        } else if (is_full) {
            *is_full = false;
        }
    }

    return result;
}

static bool gc_check_cb(sector_meta_data_t sector, void *arg1, void *arg2)
{
    size_t *empty_sec = arg1;

    if (sector->check_ok) {
        *empty_sec = *empty_sec + 1;
    }

    return false;

}

static bool do_gc(sector_meta_data_t sector, void *arg1, void *arg2)
{
    struct env_meta_data env;

    if (sector->check_ok && (sector->status.dirty == SECTOR_DIRTY_TRUE || sector->status.dirty == SECTOR_DIRTY_GC)) {
#if (ENV_STATUS_TABLE_SIZE >= DIRTY_STATUS_TABLE_SIZE)
        uint8_t status_table[ENV_STATUS_TABLE_SIZE];
#else
        uint8_t status_table[DIRTY_STATUS_TABLE_SIZE];
#endif
        //TODO 重复写入安全性验证
        /* change the sector status to GC */
        write_status(sector->addr, status_table, SECTOR_DIRTY_STATUS_NUM, SECTOR_DIRTY_GC);
        /* search all ENV */
        env.addr.start = GET_ADDR_FAILED;
        while ((env.addr.start = get_next_env_addr(sector, &env)) != GET_ADDR_FAILED) {
            read_env(&env);
            if (env.crc_is_ok && (env.status == ENV_WRITE || env.status == ENV_PRE_DELETE)) {
                uint32_t env_addr;
                struct sector_meta_data new_sector;
                //TODO 异常掉电测试
                /* change the current ENV status to prepare delete */
                write_status(env.addr.start, status_table, ENV_STATUS_NUM, ENV_PRE_DELETE);
                /* alloc new space for move the old ENV */
                if ((env_addr = alloc_env(&new_sector, env.len)) != GET_ADDR_FAILED) {
                    /* update the new ENV sector status */
                    update_sec_status(&new_sector, env.len, NULL);
                    /* copy the old ENV to new space */
                    copy_env(env_addr, env.addr.start, env.len);
                }
                write_status(env.addr.start, status_table, ENV_STATUS_NUM, ENV_DELETED);
            }
        }
        format_sector(sector->addr, SECTOR_NOT_COMBINED);
        EF_DEBUG("Collect a sector @0x%08X\n", sector->addr);
    }

    return false;
}

static void gc_collect(void)
{
    struct sector_meta_data sector;
    size_t empty_sec = 0;

    /* GC check the empty sector number */
    sector_iterator(&sector, SECTOR_STORE_EMPTY, &empty_sec, NULL, gc_check_cb, false);

    /* do GC collect */
    if (empty_sec <= EF_GC_EMPTY_SEC_THRESHOLD) {
        EF_DEBUG("The remain empty sector is %d, GC threshold is %d.\n", empty_sec, EF_GC_EMPTY_SEC_THRESHOLD);
        sector_iterator(&sector, SECTOR_STORE_FULL, NULL, NULL, do_gc, false);
    }
}

static EfErrCode align_write(uint32_t addr, const uint32_t *buf, size_t size)
{
    EfErrCode result = EF_NO_ERR;
    uint8_t align_data[EF_WRITE_GRAN / 8];
    size_t align_remain;

    memset(align_data, 0xFF, sizeof(align_data));
    result = ef_port_write(addr, buf, EF_WG_ALIGN_DOWN(size));

    align_remain = size - EF_WG_ALIGN_DOWN(size);
    if (result == EF_NO_ERR && align_remain) {
        memcpy(align_data, (uint8_t *)buf + EF_WG_ALIGN_DOWN(size), align_remain);
        result = ef_port_write(addr + EF_WG_ALIGN_DOWN(size), (uint32_t *) align_data, sizeof(align_data));
    }

    return result;
}

static EfErrCode create_env_blob(const char *key, const void *value, size_t len)
{
    EfErrCode result = EF_NO_ERR;
    struct env_hdr_data env_hdr;
    static struct sector_meta_data sector;
    uint32_t env_addr;
    bool is_full = false;

    if (strlen(key) > EF_ENV_NAME_MAX) {
        EF_INFO("Error: The ENV name length is more than %d\n", EF_ENV_NAME_MAX);
        return EF_ENV_NAME_ERR;
    }

    memset(&env_hdr, 0xFF, sizeof(struct env_hdr_data));
    env_hdr.name_len = strlen(key);
    env_hdr.value_len = len;
    env_hdr.len = ENV_HDR_DATA_SIZE + EF_WG_ALIGN(env_hdr.name_len) + EF_WG_ALIGN(env_hdr.value_len);

    if (env_hdr.len > SECTOR_SIZE - SECTOR_HDR_DATA_SIZE) {
        EF_INFO("Error: The ENV size is too big\n");
        return EF_ENV_FULL;
    }

    if ((env_addr = alloc_env(&sector, env_hdr.len)) != GET_ADDR_FAILED) {
        size_t align_remain;
        uint32_t sec_addr = EF_ALIGN_DOWN(env_addr, SECTOR_SIZE);
        /* update the sector status */
        if (result == EF_NO_ERR) {
            result = update_sec_status(&sector, env_hdr.len, &is_full);
        }
        if (result == EF_NO_ERR) {
            uint8_t ff = 0xFF;
            /* start calculate CRC32 */
            env_hdr.crc32 = ef_calc_crc32(0, &env_hdr.name_len, ENV_HDR_DATA_SIZE - ENV_NAME_LEN_OFFSET);
            env_hdr.crc32 = ef_calc_crc32(env_hdr.crc32, key, env_hdr.name_len);
            align_remain = EF_WG_ALIGN(env_hdr.name_len) - env_hdr.name_len;
            while (align_remain--) {
                env_hdr.crc32 = ef_calc_crc32(env_hdr.crc32, &ff, 1);
            }
            env_hdr.crc32 = ef_calc_crc32(env_hdr.crc32, value, env_hdr.value_len);
            align_remain = EF_WG_ALIGN(env_hdr.value_len) - env_hdr.value_len;
            while (align_remain--) {
                env_hdr.crc32 = ef_calc_crc32(env_hdr.crc32, &ff, 1);
            }
            /* write ENV header data */
            result = write_env_hdr(env_addr, &env_hdr);
        }
        /* write key name */
        if (result == EF_NO_ERR) {
            result = align_write(env_addr + ENV_HDR_DATA_SIZE, (uint32_t *) key, env_hdr.name_len);
        }
        /* write value */
        if (result == EF_NO_ERR) {
            result = align_write(env_addr + ENV_HDR_DATA_SIZE + EF_WG_ALIGN(env_hdr.name_len), value,
                    env_hdr.value_len);
        }
        /* change the ENV status to ENV_WRITE */
        if (result == EF_NO_ERR) {
            result = write_status(env_addr, env_hdr.status_table, ENV_STATUS_NUM, ENV_WRITE);
        }
        /* trigger GC collect when current sector is full */
        if (result == EF_NO_ERR && is_full) {
            uint8_t status_table[DIRTY_STATUS_TABLE_SIZE];
            /* change the sector status to GC */
            write_status(sector.addr, status_table, SECTOR_DIRTY_STATUS_NUM, SECTOR_DIRTY_GC);
            EF_DEBUG("Trigger a GC collect.\n");
            gc_is_triggered = true;
        }
        //TODO 更新 using_sec_table
    }

    return result;
}

static EfErrCode del_env(const char *key, env_meta_data_t old_env, bool complete_del) {
    EfErrCode result = EF_NO_ERR;
    uint32_t dirty_status_addr;

#if (ENV_STATUS_TABLE_SIZE >= DIRTY_STATUS_TABLE_SIZE)
    uint8_t status_table[ENV_STATUS_TABLE_SIZE];
#else
    uint8_t status_table[ENV_STATUS_TABLE_SIZE];
#endif

    /* need find ENV */
    if (!old_env) {
        struct env_meta_data env;
        /* find ENV */
        if (find_env(key, &env)) {
            old_env = &env;
        } else {
            EF_DEBUG("Not found '%s' in ENV.\n", key);
            return EF_ENV_NAME_ERR;
        }
    }
    /* change and save the new status */
    if (!complete_del) {
        result = write_status(old_env->addr.start, status_table, ENV_STATUS_NUM, ENV_PRE_DELETE);
    } else {
        result = write_status(old_env->addr.start, status_table, ENV_STATUS_NUM, ENV_DELETED);
    }

    dirty_status_addr = EF_ALIGN_DOWN(old_env->addr.start, SECTOR_SIZE) + SECTOR_DIRTY_OFFSET;
    /* read and change the sector dirty status */
    if (result == EF_NO_ERR
            && read_status(dirty_status_addr, status_table, SECTOR_DIRTY_STATUS_NUM) == SECTOR_DIRTY_FALSE) {
        result = write_status(dirty_status_addr, status_table, SECTOR_DIRTY_STATUS_NUM, SECTOR_DIRTY_TRUE);
    }

    return result;
}

/**
 * Delete an ENV.
 *
 * @param key ENV name
 *
 * @return result
 */
EfErrCode ef_del_env(const char *key)
{
    EfErrCode result = EF_NO_ERR;

    if (!init_ok) {
        EF_INFO("Error: ENV isn't initialize OK.\n");
        return EF_ENV_INIT_FAILED;
    }

    /* lock the ENV cache */
    ef_port_env_lock();

    result = del_env(key, NULL, true);

    /* unlock the ENV cache */
    ef_port_env_unlock();

    return result;
}

/**
 * Set an ENV.If it value is NULL, delete it.
 * If not find it in ENV table, then create it.
 *
 * @param key ENV name
 * @param value ENV value
 *
 * @return result
 */
EfErrCode ef_set_env(const char *key, const char *value)
{
    EfErrCode result = EF_NO_ERR;
    struct env_meta_data env;
    bool env_is_found = false;

    if (!init_ok) {
        EF_INFO("ENV isn't initialize OK.\n");
        return EF_ENV_INIT_FAILED;
    }

    /* lock the ENV cache */
    ef_port_env_lock();

    if (value == NULL) {
        result = del_env(key, NULL, true);
    } else {
        env_is_found = find_env(key, &env);
        /* prepare to delete the old ENV */
        if (env_is_found) {
            result = del_env(key, &env, false);
        }
        /* create the new ENV */
        if (result == EF_NO_ERR) {
            result = create_env_blob(key, value, strlen(value));
        }
        /* delete the old ENV */
        if (env_is_found && result == EF_NO_ERR) {
            result = del_env(key, &env, true);
        }
        /* process the GC after set ENV */
        if (result == EF_NO_ERR && gc_is_triggered) {
            gc_collect();
            gc_is_triggered = false;
        }
    }

    /* unlock the ENV cache */
    ef_port_env_unlock();

    return result;
}

/**
 * Save ENV to flash.
 */
EfErrCode ef_save_env(void)
{
    /* do nothing not cur mode */
    return EF_NO_ERR;
}

/**
 * ENV set default.
 *
 * @return result
 */
EfErrCode ef_env_set_default(void)
{
    EfErrCode result = EF_NO_ERR;
    uint32_t addr, i, value_len;

    EF_ASSERT(default_env_set);
    EF_ASSERT(default_env_set_size);

    /* lock the ENV cache */
    ef_port_env_lock();
    /* format all sectors */
    for (addr = env_start_addr; addr < env_start_addr + ENV_AREA_SIZE; addr += SECTOR_SIZE) {
        result = format_sector(addr, SECTOR_NOT_COMBINED);
        if (result != EF_NO_ERR) {
            goto __exit;
        }
    }
    /* create default ENV */
    for (i = 0; i < default_env_set_size; i++) {
        /* It seems to be a string when value length is 0.
         * This mechanism is for compatibility with older versions (less then V4.0). */
        if (default_env_set[i].value_len == 0) {
            value_len = strlen(default_env_set[i].value);
        } else {
            value_len = default_env_set[i].value_len;
        }
        create_env_blob(default_env_set[i].key, default_env_set[i].value, value_len);
        if (result != EF_NO_ERR) {
            goto __exit;
        }
    }

__exit:
    /* unlock the ENV cache */
    ef_port_env_unlock();

    return result;
}

static bool print_env_cb(env_meta_data_t env, void *arg1, void *arg2)
{
    bool value_is_str = true, print_value = false;
    size_t *using_size = arg1;

    if (env->crc_is_ok) {
        /* calculate the total using flash size */
        *using_size += env->len;
        /* check ENV */
        if (env->status == ENV_WRITE) {
            ef_print("%.*s=", env->name_len, env->name);

            if (env->value_len < EF_STR_ENV_VALUE_MAX_SIZE ) {
                uint8_t buf[32];
__reload:
                /* check the value is string */
                for (size_t len = 0, size = 0; len < env->value_len; len += size) {
                    if (len + sizeof(buf) < env->value_len) {
                        size = sizeof(buf);
                    } else {
                        size = env->value_len - len;
                    }
                    ef_port_read(env->addr.value + len, (uint32_t *) buf, EF_WG_ALIGN(size));
                    if (print_value) {
                        ef_print("%.*s", size, buf);
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
                ef_print("blob @0x%08x %dbytes", env->addr.value, env->value_len);
            }
            ef_print("\n");
        }
    }

    return false;
}


/**
 * Print ENV.
 */
void ef_print_env(void)
{
    struct env_meta_data env;
    size_t using_size = 0;

    if (!init_ok) {
        EF_INFO("ENV isn't initialize OK.\n");
        return;
    }

    /* lock the ENV cache */
    ef_port_env_lock();

    env_iterator(&env, &using_size, NULL, print_env_cb);

    ef_print("\nmode: next generation\n");
    ef_print("size: %lu/%lu bytes.\n", using_size + SECTOR_NUM * SECTOR_HDR_DATA_SIZE, ENV_AREA_SIZE);

    /* unlock the ENV cache */
    ef_port_env_unlock();
}

#ifdef EF_ENV_AUTO_UPDATE
/**
 * Auto update ENV to latest default when current EF_ENV_VER is changed.
 *
 * @return result
 */
static EfErrCode env_auto_update(void)
{
    /* lock the ENV cache */
    ef_port_env_lock();

    //TODO 检查环境变量版本号
    //TODO 自动升级环境变量
    //TODO 更新环境变量版本号

    /* unlock the ENV cache */
    ef_port_env_unlock();

    return EF_NO_ERR;
}
#endif /* EF_ENV_AUTO_UPDATE */

static bool check_sec_hdr_cb(sector_meta_data_t sector, void *arg1, void *arg2)
{
    if (!sector->check_ok) {
        EF_INFO("Warning: Sector header check failed. Set it to default.\n");
        ef_port_env_unlock();
        ef_env_set_default();
        ef_port_env_lock();
        return true;
    } else if(sector->status.dirty == SECTOR_DIRTY_GC) {
        /* resume the GC operate */
        gc_collect();
    }

    return false;
}

static bool check_and_recovery_env_cb(env_meta_data_t env, void *arg1, void *arg2)
{
    uint32_t env_addr;

    /* recovery the prepare deleted ENV */
    if (env->crc_is_ok && env->status == ENV_PRE_DELETE) {
        struct sector_meta_data sector;
        EF_INFO("Found a ENV (%.*s) which has changed value failed. Now will recovery it.\n", env->name_len, env->name);
        if ((env_addr = alloc_env(&sector, env->len)) != GET_ADDR_FAILED) {
            struct env_meta_data env_bak;
            char name[EF_ENV_NAME_MAX + 1] = { 0 };
            strncpy(name, env->name, env->name_len);
            /* check the ENV is already create success */
            if (!find_env(name, &env_bak)) {
                /* recovery the old ENV by flash copy */
                copy_env(env_addr, env->addr.start, env->len);
            }
            /* delete the old ENV */
            del_env(name, env, true);
            EF_INFO("Recovery the ENV to 0x%08X successful.\n", env_addr);
        }
    } else if (env->status == ENV_PRE_WRITE) {
        uint8_t status_table[ENV_STATUS_TABLE_SIZE];
        /* the ENV has not write finish, change the status to error */
        //TODO 绘制异常处理的状态装换图
        write_status(env->addr.start, status_table, ENV_STATUS_NUM, ENV_ERR_HDR);
        return true;
    }

    return false;
}

/**
 * Check and load the flash ENV meta data.
 *
 * @return result
 */
EfErrCode ef_load_env(void)
{
    EfErrCode result = EF_NO_ERR;
    struct env_meta_data env;
    struct sector_meta_data sector;

    /* lock the ENV cache */
    ef_port_env_lock();

    //TODO 装载环境变量元数据，using_sec_table
    //TODO 检查是否存在未完成的垃圾回收工作，存在则垃圾回收
    /* check all sector header */
    sector_iterator(&sector, SECTOR_DIRTY_UNUSED, NULL, NULL, check_sec_hdr_cb, false);
    /* check all ENV for recovery */
    env_iterator(&env, NULL, NULL, check_and_recovery_env_cb);

    /* unlock the ENV cache */
    ef_port_env_unlock();

    return result;
}

/**
 * Flash ENV initialize.
 *
 * @param default_env default ENV set for user
 * @param default_env_size default ENV set size
 *
 * @return result
 */
EfErrCode ef_env_init(ef_env const *default_env, size_t default_env_size) {
    EfErrCode result = EF_NO_ERR;

    EF_ASSERT(default_env);
    EF_ASSERT(ENV_AREA_SIZE);
    EF_ASSERT(EF_ERASE_MIN_SIZE);
    /* must be aligned with erase_min_size */
    EF_ASSERT(ENV_AREA_SIZE % EF_ERASE_MIN_SIZE == 0);
    /* must be aligned with write granularity */
    EF_ASSERT((EF_STR_ENV_VALUE_MAX_SIZE * 8) % EF_WRITE_GRAN == 0);

    //TODO GC 扇区数量的检查

    env_start_addr = EF_START_ADDR;
    default_env_set = default_env;
    default_env_set_size = default_env_size;

    EF_DEBUG("ENV start address is 0x%08X, size is %d bytes.\n", EF_START_ADDR, ENV_AREA_SIZE);

    result = ef_load_env();

#ifdef EF_ENV_AUTO_UPDATE
    if (result == EF_NO_ERR) {
        env_auto_update();
    }
#endif

    if (result == EF_NO_ERR) {
        init_ok = true;
    }

    return result;
}

#endif /* defined(EF_USING_ENV) */
