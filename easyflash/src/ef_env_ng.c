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
#define EF_ENV_NAME_MAX                          256
/* magic word(`E`, `F`, `4`, `0`) */
#define SECTOR_MAGIC_WORD                        0x45463430

/* the using status sector table length */
#ifndef USING_SECTOR_TABLE_LEN
#define USING_SECTOR_TABLE_LEN                   4
#endif

/* Return the most contiguous size aligned at specified width. RT_ALIGN(13, 4)
 * would return 16.
 */
#define EF_ALIGN(size, align)                    (((size) + (align) - 1) & ~((align) - 1))

#define STATUS_MASK                              ((1UL << EF_WRITE_GRAN) - 1)

#define STATUS_TABLE_SIZE(status_number)         (EF_ALIGN(status_number * EF_WRITE_GRAN, 8) / 8)

#define STORE_STATUS_TABLE_SIZE                  STATUS_TABLE_SIZE(SECTOR_STORE_STATUS_NUM)
#define DIRTY_STATUS_TABLE_SIZE                  STATUS_TABLE_SIZE(SECTOR_DIRTY_STATUS_NUM)
#define ENV_STATUS_TABLE_SIZE                    STATUS_TABLE_SIZE(ENV_STATUS_NUM)

enum sector_store_status {
    SECTOR_STORE_EMPTY,
    SECTOR_STORE_USING,
    SECTOR_STORE_FULL,
    SECTOR_STORE_STATUS_NUM,
};
typedef enum sector_store_status sector_store_status_t;

enum sector_dirty_status {
    SECTOR_DIRTY_FALSE,
    SECTOR_DIRTY_TRUE,
    SECTOR_DIRTY_STATUS_NUM,
};
typedef enum sector_dirty_status sector_dirty_status_t;

enum env_status {
    ENV_PRE_WRITE,
    ENV_WRITE,
    ENV_DELETED,
    ENV_GC,
    ENV_STATUS_NUM,
};
typedef enum env_status env_status_t;

struct sector_hdr_data {
    uint32_t magic;                              /**< magic word(`E`, `F`, `4`, `0`) */
    uint32_t combined;                           /**< the combined next sector number, 0xFFFFFFFF: not combined */
    uint32_t reserved;
    struct {
        uint8_t store[STORE_STATUS_TABLE_SIZE];  /**< sector store status @see sector_store_status_t */
        uint8_t dirty[DIRTY_STATUS_TABLE_SIZE];  /**< sector dirty status @see sector_dirty_status_t */
    } status_table;
};

struct sector_meta_data {
    struct {
        sector_store_status_t store;             /**< sector store status @see sector_store_status_t */
        sector_dirty_status_t dirty;             /**< sector dirty status @see sector_dirty_status_t */
    } status;
    uint32_t addr;                               /**< sector start address */
    uint32_t magic;                              /**< magic word(`E`, `F`, `4`, `0`) */
    uint32_t combined;                           /**< the combined next sector number, 0xFFFFFFFF: not combined */
    size_t remain;                               /**< remain size */
    uint32_t first_env;                          /**< the first ENV node address on this sector */
    uint32_t empty_env;                          /**< the next empty ENV node start address */
};
typedef struct sector_meta_data *sector_meta_data_t;

struct env_hdr_data {
    uint32_t len;                                /**< ENV node total length (header + name + value), must align by EF_WRITE_GRAN */
    uint8_t status_table[ENV_STATUS_TABLE_SIZE]; /**< ENV node status, @see node_status_t */
    uint32_t crc32;                              /**< ENV node crc32(name_len + data_len + name + value) */
    uint8_t name_len;                            /**< name length */
    uint32_t value_len;                          /**< value length */
};

struct env_meta_data {
    bool crc_is_ok;                              /**< ENV node CRC32 check is OK */
    uint8_t name_len;                            /**< name length */
    env_status_t status;                         /**< ENV node status, @see node_status_t */
    uint32_t value_len;                          /**< value length */
    struct {
        uint32_t start;                          /**< ENV node start adress */
        uint32_t name;                           /**< name start address */
        uint32_t value;                          /**< value start address */
    } addr;
};
typedef struct env_meta_data *env_meta_data_t;

static const uint64_t status_mask = ((1UL << EF_WRITE_GRAN) - 1);
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

static env_meta_data_t get_next_env(sector_meta_data_t sector, env_meta_data_t pre_env)
{
    //TODO 计算下一个节点的地址，注意考虑越界
    //TODO 读取节点的各个元数据
    //TODO 检查 CRC32 是否正确

    return pre_env;
}

static EfErrCode read_sector_meta_data(uint32_t addr, sector_meta_data_t sector)
{
    EfErrCode result = EF_NO_ERR;

    //TODO 检查入参
    //TODO 通过 flash 读取数据
    //TODO 检查各个数据有效性，出错则退出
    //TODO 计算剩余空间

    return result;
}

static EfErrCode find_env(const char *key, env_meta_data_t env)
{
    EfErrCode result = EF_NO_ERR;

    //TODO 遍历所有正在使用及已满扇区

    return result;
}

/**
 * Get an ENV value by key name.
 *
 * @param key ENV name
 *
 * @return value
 */
char *ef_get_env(const char *key)
{
    //TODO 查找 env
    //TODO 找到后检查其是否为字符串，否则弹出警告


    //TODO  如何返回值！！！！！！！！

    return NULL;
}

static EfErrCode create_env_blob(const char *key, void *value, size_t len)
{
    //TODO 计算 ENV 节点占用 Flash 大小，结合 flash 写粒度
    //TODO 检查节点长度是否过大，过大则断言，后期支持大数据存储模式
    //TODO 检查所有扇区的剩余空间，是否有合适的扇区，优先 using_sec_table
    //TODO 计算 CRC，拷贝数据，写入等
    //TODO 更新 using_sec_table
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

    //TODO 从第一个扇区开始查找所有正在使用及已满扇区，确认该环境变量是否存在
    //TODO 如果存在则备份原始节点元数据，标记该节点为正在删除
    //TODO 新增节点
    //TODO 如果之前存在节点，则标记以前的节点为已删除
    //TODO GC


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

    return result;
}

/**
 * Print ENV.
 */
void ef_print_env(void)
{

}

#ifdef EF_ENV_AUTO_UPDATE
/**
 * Auto update ENV to latest default when current EF_ENV_VER is changed.
 *
 * @return result
 */
static EfErrCode env_auto_update(void)
{
    size_t i;

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

/**
 * Check and load the flash ENV meta data.
 *
 * @return result
 */
EfErrCode ef_load_env(void)
{
    //TODO 检查各个扇区状态，magic 不对则退出 返回 EF_ENV_INIT_FAILED
    //TODO 如果异常则执行全盘格式化，还原默认环境变量
    //TODO 检查是否存在未完成的环境变量操作，存在则执行掉电恢复
    //TODO 检查是否存在未完成的垃圾回收工作，存在则继续整理
    //TODO 装载环境变量元数据，using_sec_table

    return EF_NO_ERR;
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

    EF_ASSERT(ENV_AREA_SIZE);
    EF_ASSERT(EF_ERASE_MIN_SIZE);
    /* must be aligned with erase_min_size */
    EF_ASSERT(ENV_AREA_SIZE % EF_ERASE_MIN_SIZE == 0);
    EF_ASSERT(default_env);

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
