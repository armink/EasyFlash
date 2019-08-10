# EasyFlash 

[![GitHub release](https://img.shields.io/github/release/armink/EasyFlash.svg)](https://github.com/armink/EasyFlash/releases/latest) [![GitHub commits](https://img.shields.io/github/commits-since/armink/EasyFlash/4.0.0.svg)](https://github.com/armink/EasyFlash/compare/4.0.0...master) [![MIT licensed](https://img.shields.io/badge/license-MIT-blue.svg)](https://raw.githubusercontent.com/armink/EasyFlash/master/LICENSE)

## 1、介绍（[English](#1-introduction)）

[EasyFlash](https://github.com/armink/EasyFlash)是一款开源的轻量级嵌入式Flash存储器库，方便开发者更加轻松的实现基于Flash存储器的常见应用开发。非常适合智能家居、可穿戴、工控、医疗、物联网等需要断电存储功能的产品，资源占用极低，支持各种 MCU 片上存储器。该库主要包括 **三大实用功能** ：

- **ENV** 快速保存产品参数，支持 **写平衡（磨损平衡）** 及 **掉电保护** 功能

EasyFlash不仅能够实现对产品的 **设定参数** 或 **运行日志** 等信息的掉电保存功能，还封装了简洁的 **增加、删除、修改及查询** 方法， 降低了开发者对产品参数的处理难度，也保证了产品在后期升级时拥有更好的扩展性。让Flash变为NoSQL（非关系型数据库）模型的小型键值（Key-Value）存储数据库。

- **IAP** 在线升级再也不是难事儿

该库封装了IAP(In-Application Programming)功能常用的接口，支持CRC32校验，同时支持Bootloader及Application的升级。

- **Log** 无需文件系统，日志可直接存储在Flash上

非常适合应用在小型的不带文件系统的产品中，方便开发人员快速定位、查找系统发生崩溃或死机的原因。同时配合[EasyLogger](https://github.com/armink/EasyLogger)(我开源的超轻量级、高性能C日志库，它提供与EasyFlash的无缝接口)一起使用，轻松实现C日志的Flash存储功能。

### 1.1、V4.0 NG 模式

自 2019 年春节后，EasyFlash 经过 4 年多的迭代，结合众多开发者的需求及建议，终于发布了 V4.0 版本，该版本中的 ENV 功能被命名为 **NG** (Next Generation) 模式，这是一个完全重构的新版本，具有以下新特性：

- 更小的资源占用，内存占用 **几乎为 0** ；（V4.0 以前版本会使用额外的 RAM 空间进行缓存）
- ENV 的值类型支持 **任意类型** 、任意长度，相当于直接 memcpy 变量至 flash ；（V4.0 之前只支持存储字符串）
- ENV 操作效率比以前的模式高，充分利用剩余空闲区域，擦除次数及操作时间显著降低；
- **原生支持** 磨损平衡、掉电保护功能 （V4.0 之前需要占用额外的 Flash 扇区）；
- ENV 支持 **增量升级** ，固件升级后 ENV 也支持升级；
- 支持大数据存储模式，**长度无限制**，数据可在多个 Flash 扇区上顺序存储。像脚本程序、音频等占用 Flash 超过 1 个扇区的资源也都可以存入 ENV（即将在 V4.1 支持）；
- 支持 **数据加密** ，提升存储的安全性，物联网时代的必备功能（即将在 V4.2 支持）；
- 支持 **数据压缩** ，减低 Flash 占用（即将在 V4.3 支持）；

V4.0 设计及内部原理，V4.0 迁移指南等更多内容请继续阅读下面的 [文档章节](#3文档) 

### 1.2、资源占用

```
最低要求： ROM: 6K bytes     RAM: 0.1K bytes
```

### 1.3、支持平台

目前已移植硬件平台有 `stm32f10x`与 `stm32f4xx` 系列的片内Flash，片外的 SPI Flash（基于 [SFUD](https://github.com/armink/SFUD)），这些也是笔者产品使用的平台。其余平台的移植难度不大，在项目的设计之初就有考虑针对所有平台的适配性问题（64位除外），所以对所有移植接口都有做预留。移植只需修改 [`\easyflash\port\ef_port.c`](https://github.com/armink/EasyFlash/blob/master/easyflash/port/ef_port.c) 一个文件，实现里面的擦、写、读及打印功能即可。

欢迎大家 **fork and pull request**([Github](https://github.com/armink/EasyFlash)|[OSChina](http://git.oschina.net/armink/EasyFlash)|[Coding](https://coding.net/u/armink/p/EasyFlash/git)) 。如果觉得这个开源项目很赞，可以点击[项目主页](https://github.com/armink/EasyFlash) 右上角的 **Star**，同时把它推荐给更多有需要的朋友。

## 2、流程

### 2.1、ENV：环境变量（KV数据库）

下图为通过控制台（终端）来调用环境变量的常用接口，演示了以下过程，这些接口都支持被应用层直接调用。

- 1、创建“温度”的环境变量，名为 `temp`，并且赋值为 `123`；
- 2、保存“温度”到Flash中并重启（V4.0 版本的每个操作完自动保存，无需额外保存）；
- 3、检查“温度”是否被成功保存；
- 4、修改“温度”值为 `456` 并保存、重启；
- 5、检查“温度”是否被成功修改；
- 6、删除“温度”的环境变量。

![easy_flash_env](/docs/zh/images/EnvDemo.gif)

### 2.2、IAP：在线升级

下图演示了通过控制台来进行IAP升级软件的过程，使用的是库中自带的IAP功能接口，演示采用的是串口+Ymodem协议的方式。你还也可以实现通过CAN、485、以太网等总线，来实现远程网络更新。

![easy_flash_iap](/docs/zh/images/IapDemo.gif)

### 2.3、Log：日志存储

下图过程为通过控制台输出日志，并将输出的日志存储到Flash中。重启再读取上次保存的日志，最后清空Flash日志。

![easy_flash_log](/docs/zh/images/LogDemo.gif)

## 3、文档

- API 文档：[`\docs\zh\api.md`](docs/zh/api.md)
- 移植文档：[`\docs\zh\port.md`](docs/zh/port.md)
- V4.0 迁移指南：[`\docs\zh\v4_migrate.md`](docs/zh/v4_migrate.md)
- V4.0 ENV 功能设计与实现：[`\docs\zh\design.md`](docs/zh/design.md)

务必保证在 **阅读文档** 后再移植使用。

## 4、支持

 ![support](/docs/zh/images/wechat_support.png)

如果 EasyFlash 解决了你的问题，不妨扫描上面二维码请我 **喝杯咖啡**~ 

## 5、许可

采用 MIT 开源协议，细节请阅读项目中的 LICENSE 文件内容。

---

## 1 Introduction

[EasyFlash](https://github.com/armink/EasyFlash) is an open source lightweight embedded flash memory library. It provides convenient application interface for MCU (Micro Control Unit). The developers can achieve more efficient and common application development based on Flash memory. The library currently provides **three useful features** ：

- **Env(environment variables)** Fast Saves product parameters. Support **write balance mode(wear leveling)** and **power fail safeguard**. 

EasyFlash can store **setting parameters** or **running logs** and other information which you want to keep after power down. It contains add, delete, modify and query methods. It helps developer to process the product parameters, and makes sure the product has better scalability after upgrade. Turns the Flash into a small NoSQL (non-relational databases) model and Key-Value stores database.

- **IAP** : online upgrade is no longer a difficult thing.

The library encapsulates the IAP (In-Application Programming) feature common interface. Support CRC32 checksum. While supporting the bootloader and application upgrade.

- **Log** : The logs can store to product's flash which has no file-system.

It's very suitable for small without a file system products. The developer can easy to locate and query problem when system crashes or freezes. You can use [EasyLogger](https://github.com/armink/EasyLogger)( A super-lightweight, high-performance C log library which open source by me. It provides a seamless interface with EasyFlash) at the same time. So, it's so easy to store the logs to flash.

### 1.1 Resource consumption

```
Minimum : ROM: 6K bytes     RAM: 0.2K bytes
```

### 1.2 Supported platforms

Hardware platform has been ported SPI Flash, `stm32f10x` and `stm32f4xx` series of on-chip Flash. These are my product platforms. Remaining platform porting difficulty is little. To port it just modify [`\easyflash\port\ef_port.c`](https://github.com/armink/EasyFlash/blob/master/easyflash/port/ef_port.c) file. Implement erase, write, read, print feature.

Welcome everyone to **fork and pull request**([Github](https://github.com/armink/EasyFlash)|[OSChina](http://git.oschina.net/armink/EasyFlash)|[Coding](https://coding.net/u/armink/p/EasyFlash/git)). If you think this open source project is awesome. You can press the **Star** on the top right corner of [project home page](https://github.com/armink/EasyFlash), and recommend it to more friends.

## 2 Flow

### 2.1 Env(KV database)

The figure below shows an ENV's common interface be called by the console (terminal). These interfaces are supported by the application layer called.

- 1.Create temperature environment variable. It's name is `temp` and value is `123`;
- 2.Save temperature to flash and reboot;
- 3.Check the temperature has saved successfully;
- 4.Change the temperature value to `456` and save, reboot;
- 5.Check the temperature has changed successfully;
- 6.Delete temperature environment variable.

![easy_flash_env](/docs/en/images/EnvDemo.gif)

### 2.2 IAP

The figure below shows the process of upgrading software through the console by IAP. It use this library comes with IAP function interface. It uses a serial port + Ymodem protocol mode. You can also be achieved through CAN, 485, Ethernet bus to online upgrade.

![easy_flash_iap](/docs/en/images/IapDemo.gif)

### 2.3 Log

The following figure is the output of the log process through the console. The logs are saved to flash at real time. Then the board is rebooted and the logs back are read back from flash. At last logs will be erased.

![easy_flash_log](/docs/en/images/LogDemo.gif)

## 3 Documents

All documents are in the [`\docs\en\`](https://github.com/armink/EasyFlash/tree/master/docs/en) folder. Please **read the documents** before porting it and using it.

## 4 License

Using MIT open source license, please read the project LICENSE file.