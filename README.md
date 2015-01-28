# EasyFlash 使用说明

---

## 1、介绍

EasyFlash是一款开源的轻量级嵌入式Flash存储器库，主要为MCU(Micro Control Unit)提供便捷、通用的上层应用接口，使得开发者更加高效实现基于的Flash存储器常见应用开发。该库目前提供 **两大实用功能** ：

 - **Env** 让变量轻轻松松实现掉电保存，无需担心变量长度、对其等问题
 
 使用 **键值对(key-value)** 方式将变量存储到Flash中，类似U-Boot的 `环境变量` ，使用方式与U-Boot一致。
 - **IAP** 在线升级再也不是难事儿
 
 该库封装了IAP(In-Application Programming)功能常用的接口，支持CRC32校验，同时支持Bootloader及Application的升级。

### 1.1、文件结构

|源文件                                 |描述   |
|:------------------------------        |:----- |
|\flash\src\flash_env.c                 |Env（环境变量）相关操作接口及实现源码|
|\flash\src\flash_iap.c                 |IAP（在线升级）相关操作接口及实现源码|
|\flash\src\flash_utils.c               |EasyFlash常用小工具，例如：CRC32|
|\flash\src\flash.c                     |目前只包含EasyFlash初始化方法|
|\flash\port\flash_port.c               |不同平台下的EasyFlash移植接口及配置参数|
|\demo\stm32f10x                        |stm32f10x平台下的demo|

### 1.2、资源占用

```
最低要求： ROM: 6K bytes     RAM: 0.5K bytes + (Env大小)

Demo平台：STM32F103RET6 + RT-Thread 1.2.2 + Env(2K bytes)
实际占用： ROM: 6K bytes     RAM: 2.6K bytes
```

### 1.3、支持平台

目前已移植平台只有 `STM32F10X` 系列的片内Flash，这个也是笔者产品使用的平台。其余平台的移植难度不大，在项目的设计之初就有考虑针对所有平台的适配性问题（64位除外），所以对所有移植接口都有做预留。移植只需修改 `\flash\port\flash_port.c` 一个文件，实现里面的擦、写、读及打印功能即可。

欢迎大家 **fork and pull request** 。开源软件的成功离不开所有人的努力，也希望该项目能够帮助大家降低开发周期，让产品更早的获得成功。

## 2、流程

### 2.1、环境变量

下图为人工通过控制台来调用环境变量的常用接口，演示了环境变量 `"temp"` 从创建到保存，再修改，最后删除的过程。这些接口都支持被应用层直接调用。

![easy_flash_env](https://cloud.githubusercontent.com/assets/1734686/5886463/46ad7efa-a3db-11e4-8401-75c00a4c35ba.gif)

### 2.2、在线升级

下图演示了通过控制台来进行IAP升级软件的过程，使用的是库中自带的IAP功能接口，演示采用的是串口+Ymodem协议的方式。你还也可以实现通过CAN、485、以太网等总线，来实现远程网络更新。

![easy_flash_iap](https://cloud.githubusercontent.com/assets/1734686/5886462/40f7d62c-a3db-11e4-866a-ba827c809370.gif)

## 3、API

所有支持的API接口都在 `\flash\inc\flash.h` 中声明。以下内容较多，建议使用 **CTRL+F** 搜索。

名词介绍：
**备份区** ：是EasyFlash定义的一个存放环境变量及已下载程序的Flash区域，详细存储架构可以参考 `\flash\src\flash.c` 文件头位置的注释说明。
**环境变量表** ：负责存放所有的环境变量，该表在Flash及RAM中均存在，上电后需从Flash加载到RAM中，修改后，则需要保存其至Flash中。

### 3.1 初始化

初始化EasyFlash。在初始化的过程中会使用 `\flash\port\flash_port.c` 中的用户自定义参数。

```C
FlashErrCode flash_init(void)
```


### 3.2 环境变量

#### 3.2.1 加载环境变量

加载Flash中的所有环境变量到系统内存中。

```C
void flash_load_env(void)
```

#### 3.2.2 打印环境变量

通过在移植接口( `\flash\port\flash_port.c` )中定义的 `flash_print` 打印方法，来将Flash中的所有环境变量输出出来。

```C
void flash_print_env(void)
```

#### 3.2.3 获取环境变量

通过环境变量的名字来获取其对应的值。（注意：此处的环境变量指代的已加载到内存中的环境变量）

```C
char *flash_get_env(const char *key)
```

|参数                                    |描述|
|:-----                                  |:----|
|key                                     |环境变量名称|

#### 3.2.4 设置环境变量

使用此方法可以实现对环境变量的增加、修改及删除功能。（注意：此处的环境变量指代的已加载到内存中的环境变量）
**增加** ：当环境变量表中不存在该名称的环境变量时，则会执行新增操作；
**修改** ：入参中的环境变量名称在当前环境变量表中存在，则把该环境变量值修改为入参中的值；
**删除** ：当入参中的value为空时，则会删除入参名对应的环境变量。

```C
FlashErrCode flash_set_env(const char *key, const char *value)
```

|参数                                    |描述|
|:-----                                  |:----|
|key                                     |环境变量名称|
|value                                   |环境变量值|

#### 3.2.5 保存环境变量

保存内存中的环境变量表到Flash中。

```C
FlashErrCode flash_save_env(void)
```

#### 3.2.6 恢复环境变量
将内存中的环境变量表恢复为默认值。

```C
FlashErrCode flash_env_set_default(void)
```

#### 3.2.7 获取环境变量分区的总容量

```C
uint32_t flash_get_env_total_size(void)
```

#### 3.2.8 获取当前已使用环境变量的大小

```C
uint32_t flash_get_env_used_size(void)
```

### 3.3 在线升级

#### 3.3.1 擦除备份区中的应用程序

```C
FlashErrCode flash_erase_bak_app(size_t app_size)
```

#### 3.3.2 擦除用户的应用程序

注意：请不要在应用程序中调用该方法

```C
FlashErrCode flash_erase_user_app(uint32_t user_app_addr, size_t user_app_size)
```

|参数                                    |描述|
|:-----                                  |:----|
|user_app_addr                           |用户应用程序入口地址|
|user_app_size                           |用户应用程序大小|

#### 3.3.3 擦除Bootloader

注意：请不要在Bootloader中调用该方法

```C
FlashErrCode flash_erase_bl(uint32_t bl_addr, size_t bl_size)
```

|参数                                    |描述|
|:-----                                  |:----|
|bl_addr                                 |Bootloader入口地址|
|bl_size                                 |Bootloader大小|

#### 3.3.4 写数据到备份区

为下载程序到备份区定制的Flash连续写方法。
注意：写之前请先确认Flash已进行擦除。

```C
FlashErrCode flash_write_data_to_bak(uint8_t *data,
                                     size_t size,
                                     size_t *cur_size,
                                     size_t total_size)
```

|参数                                    |描述|
|:-----                                  |:----|
|data                                    |需要写入到备份区中的数据存储地址|
|size                                    |此次写入数据的大小（字节）|
|cur_size                                |之前已写入到备份区中的数据大小（字节）|
|total_size                              |需要写入到备份区的数据总大小（字节）|

#### 3.3.5 从备份拷贝应用程序

将备份区已下载好的应用程序拷贝至用户应用程序起始地址。
注意：
1、拷贝前必须对原有的应用程序进行擦除
2、不要在应用程序中调用该方法

```C
FlashErrCode flash_copy_app_from_bak(uint32_t user_app_addr, size_t app_size)
```

|参数                                    |描述|
|:-----                                  |:----|
|user_app_addr                           |用户应用程序入口地址|
|user_app_size                           |用户应用程序大小|

#### 3.3.5 从备份拷贝Bootloader

将备份区已下载好的Bootloader拷贝至Bootloader起始地址。
注意：
1、拷贝前必须对原有的Bootloader进行擦除
2、不要在Bootloader中调用该方法

```C
FlashErrCode flash_copy_bl_from_bak(uint32_t bl_addr, size_t bl_size)
```

|参数                                    |描述|
|:-----                                  |:----|
|bl_addr                                 |Bootloader入口地址|
|bl_size                                 |Bootloader大小|

### 3.4 移植


#### 3.4.1 读取Flash

最小单位为4个字节

```C
FlashErrCode flash_read(uint32_t addr, uint32_t *buf, size_t size)
```

|参数                                    |描述|
|:-----                                  |:----|
|addr                                    |读取起始地址|
|buf                                     |存放读取数据的缓冲区|
|size                                    |读取数据的大小（字节）|

#### 3.4.2 擦除Flash

```C
FlashErrCode flash_erase(uint32_t addr, size_t size)
```

|参数                                    |描述|
|:-----                                  |:----|
|addr                                    |擦除起始地址|
|size                                    |擦除数据的大小（字节）|

#### 3.4.3 写入Flash

最小单位为4个字节

```C
FlashErrCode flash_write(uint32_t addr, const uint32_t *buf, size_t size)
```

|参数                                    |描述|
|:-----                                  |:----|
|addr                                    |写入的起始地址|
|buf                                     |源数据的缓冲区|
|size                                    |写入数据的大小（字节）|

#### 3.4.4 分配动态内存

```C
void *flash_malloc(size_t size)
```

|参数                                    |描述|
|:-----                                  |:----|
|size                                    |需分配的内存大小|

#### 3.4.5 释放动态内存

```C
void flash_free(void *p)
```

|参数                                    |描述|
|:-----                                  |:----|
|p                                       |需释放的动态内存地址|

#### 3.4.6 打印调试日志信息

在定义 `FLASH_PRINT_DEBUG` 宏后，打印调试日志信息

```C
void flash_log_debug(const char *file, const long line, const char *format, ...)
```

|参数                                    |描述|
|:-----                                  |:----|
|file                                    |调用该方法的文件|
|line                                    |调用该方法的行号|
|format                                  |打印格式|
|...                                     |不定参|

#### 3.4.7 打印普通日志信息

```C
void flash_log_info(const char *format, ...)
```

|参数                                    |描述|
|:-----                                  |:----|
|format                                  |打印格式|
|...                                     |不定参|

#### 3.4.8 无格式打印信息

该方法输出无固定格式的打印信息，为 `flash_print_env` 方法所用。而 `flash_log_debug` 及 `flash_log_info` 可以输出带指定前缀及格式的打印日志信息。

```C
void flash_print(const char *format, ...)
```

|参数                                    |描述|
|:-----                                  |:----|
|format                                  |打印格式|
|...                                     |不定参|

## 4、注意

1、写数据前务必记得先擦除；
2、环境变量设置完后，只有调用 `flash_save_env` 才会保存在Flash中，否则开机会丢失修改的内容；
3、不要在应用程序及Bootloader中执行擦除及拷贝自身的动作；
4、Flash读取和写入方法的最小单位为4个字节，擦除的最小单位则需根据用户的平台来确定；


