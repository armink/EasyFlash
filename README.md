# EasyFlash：超轻量级嵌入式数据库

[![GitHub release](https://img.shields.io/github/release/armink/EasyFlash.svg)](https://github.com/armink/EasyFlash/releases/latest) [![GitHub commits](https://img.shields.io/github/commits-since/armink/EasyFlash/4.1.0.svg)](https://github.com/armink/EasyFlash/compare/4.1.0...master) [![MIT licensed](https://img.shields.io/badge/license-MIT-blue.svg)](https://raw.githubusercontent.com/armink/EasyFlash/master/LICENSE)

## 简介

EasyFlash 是一款超轻量级的嵌入式数据库开源项目，专注于提供嵌入式产品的数据存储方案。其拥有较强的可靠性及性能，并在保证极低的资源占用前提下，尽可能延长 Flash 使用寿命。

EasyFlash 提供两种数据库模式：

- **键值数据库** ：是一种非关系数据库，它将数据存储为键值（Key-Value）对集合，其中键作为唯一标识符。KVDB 操作简洁，可扩展性强。
- **时序数据库** ：时间序列数据库 （Time Series Database , 简称 TSDB），它将数据按照 **时间顺序存储** 。TSDB 数据具有时间戳，数据存储量大，插入及查询性能高。

## 使用场景

如今，物联网产品种类越来越多，运行时产生的数据种类及总量及也在不断变大。EasyFlash 提供了多样化的数据存储方案，不仅资源占用小，并且存储容量大，非常适合用于物联网产品。下面是主要应用场景：

- **键值数据库** ：
  - 产品参数存储
  - 用户配置信息存储
  - 小文件管理
- **时序数据库** ：
  - 存储动态产生的结构化数据：如 温湿度传感器采集的环境监测信息，智能手环实时记录的人体健康信息等
  - 记录运行日志：存储产品历史的运行日志，异常告警的记录等

## 主要特性

- 资源占用极低，内存占用几乎为 **0** ;
- 支持 多分区，**多实例** 。数据量大时，可细化分区，降低检索时间；
- 支持 **磨损平衡** ，延长 Flash 寿命；
- 支持 **掉电保护** 功能，可靠性高；
- 支持 字符串及 blob 两种 KV 类型，方便用户操作；
- 支持 KV **增量升级** ，产品固件升级后， KVDB 内容也支持自动升级；
- 支持 修改每条 TSDB 记录的状态，方便用户进行管理；

## 性能及资源占用

### TSDB 性能测试1 （nor flash W25Q64）

```shell
msh />tsl bench
Append 1250 TSL in 5 seconds, average: 250.00 tsl/S, 4.00 ms/per
Query total spent 2248 (ms) for 1251 TSL, min 1, max 2, average: 1.80 ms/per
```

插入平均：4 ms，查询平均：1.8 ms

### TSDB 性能测试2 （stm32f2 onchip flash）

```shell
msh />tsl bench
Append 13527 TSL in 5 seconds, average: 2705.40 tsl/S, 0.37 ms/per
Query total spent 1621 (ms) for 13528 TSL, min 0, max 1, average: 0.12 ms/per
```

插入平均：0.37 ms，查询平均：0.12 ms

### 资源占用 (stm32f4 IAR8.20)

```shell
    Module                  ro code  ro data  rw data
    ------                  -------  -------  -------
    easyflash.o                 276      232        1
    ef_kv.o                   4 584      356        1
    ef_ts_log.o               1 160      236
    ef_utils.o                  418    1 024
```

上面是 IAR 的 map 文件信息，可见 EasyFlash 的资源占用非常低

## 如何使用

从 EasyFlash V5.0 开始，由于软件架构完全重构，导致代码改动巨大，为此牺牲了之前 API 的兼容性。后续 EasyFlash 的 V4.X 版本将会继续维护（见 stable-v4.x 分支），希望大家谅解。

### 移植

EasyFlash 底层依赖于 RT-Thread 的 FAL (Flash Abstraction Layer) Flash 抽象层开源软件包 ，该开源库也支持运行在裸机平台 [(点击查看介绍)](http://packages.rt-thread.org/detail.html?package=fal)。所以只需要将所用到的 Flash 对接到 FAL ，即可完成整个移植工作。

 FAL 移植主要流程：

- 定义 flash 设备，详见 ([GitHub](https://github.com/RT-Thread-packages/fal#21%E5%AE%9A%E4%B9%89-flash-%E8%AE%BE%E5%A4%87)|[Gitee](https://gitee.com/RT-Thread-Mirror/fal#21%E5%AE%9A%E4%B9%89-flash-%E8%AE%BE%E5%A4%87))
- 定义 flash 设备表，详见 ([GitHub](https://github.com/RT-Thread-packages/fal#22%E5%AE%9A%E4%B9%89-flash-%E8%AE%BE%E5%A4%87%E8%A1%A8)|[Gitee](https://gitee.com/RT-Thread-Mirror/fal#22%E5%AE%9A%E4%B9%89-flash-%E8%AE%BE%E5%A4%87%E8%A1%A8))
- 定义 flash 分区表，详见 ([GitHub](https://github.com/RT-Thread-packages/fal#23%E5%AE%9A%E4%B9%89-flash-%E5%88%86%E5%8C%BA%E8%A1%A8)|[Gitee](https://gitee.com/RT-Thread-Mirror/fal#23%E5%AE%9A%E4%B9%89-flash-%E5%88%86%E5%8C%BA%E8%A1%A8))

### 示例

EasyFlash 提供了主要功能的示例，直接加入工程即可运行，并具有一定的参考性

| 文件路径                                                     | 介绍                                    | 备注 |
| ------------------------------------------------------------ | --------------------------------------- | ---- |
| [`samples/kvdb_type_string_sample.c`](samples/kvdb_type_string_sample.c) | KVDB 使用字符型键值的示例               |      |
| [`samples/kvdb_type_blob_sample.c`](samples/kvdb_type_blob_sample.c) | KVDB 使用 blob 型（任意类型）键值的示例 |      |
| [`samples/tsdb_sample.c`](samples/tsdb_sample.c)             | TSDB 示例                               |      |

## 支持

 ![support](docs/zh/images/wechat_support.png)

如果 EasyFlash 解决了你的问题，不妨扫描上面二维码请我 **喝杯咖啡**~ 

## 许可

采用 MIT 开源协议，细节请阅读项目中的 LICENSE 文件内容。