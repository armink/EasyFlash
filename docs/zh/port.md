# EasyFlash 移植说明

---

## 1、下载源码

[点击此链接](https://github.com/armink/EasyFlash/archive/master.zip)即可直接下载位于Github上的源码。

> 建议：点击项目主页 https://github.com/armink/EasyFlash 右上角 **Watch & Star**，这样项目有更新时，会及时以邮件形式通知你。

如果Github下载太慢，也可以点击项目位于的国内仓库下载的链接([OSChina](https://git.oschina.net/Armink/EasyFlash/repository/archive?ref=master)|[Coding](https://coding.net/u/armink/p/EasyFlash/git/archive/master))。

## 2、导入项目

在导入到项目前，先打开[`\demo\`](https://github.com/armink/EasyFlash/tree/master/demo)文件夹，检查下有没有与项目Flash规格一致的Demo。如果有则先直接跳过2、3、4章节，按照第5章的要求设置参数，并运行、验证Demo。验证通过再按照下面的导入项目要求，将Demo中的移植文件直接导入到项目中即可。

- 1、先解压下载好的源码包，文件的目录结构大致如下：

|源文件                                 |描述   |
|:------------------------------        |:----- |
|\easyflash\src\ef_env.c                |Env（常规模式）相关操作接口及实现源码|
|\easyflash\src\ef_env_wl.c             |Env（磨损平衡模式）相关操作接口及实现源码|
|\easyflash\src\ef_iap.c                |IAP 相关操作接口及实现源码|
|\easyflash\src\ef_log.c                |Log 相关操作接口及实现源码|
|\easyflash\src\ef_utils.c              |EasyFlash常用小工具，例如：CRC32|
|\easyflash\src\easyflash.c             |目前只包含EasyFlash初始化方法|
|\easyflash\port\ef_port.c              |不同平台下的EasyFlash移植接口|
|\demo\env\stm32f10x\non_os             |stm32f10x裸机片内Flash的Env demo|
|\demo\env\stm32f10x\non_os_spi_flash   |stm32f10x裸机SPI Flash的Env demo|
|\demo\env\stm32f10x\rtt                |stm32f10x基于[RT-Thread](http://www.rt-thread.org/)的片内Flash Env demo|
|\demo\env\stm32f4xx                    |stm32f4xx基于[RT-Thread](http://www.rt-thread.org/)的片内Flash Env demo|
|\demo\iap\ymodem+rtt.c                 |使用[RT-Thread](http://www.rt-thread.org/)+[Ymodem](https://github.com/RT-Thread/rt-thread/tree/master/components/utilities/ymodem)的IAP Demo|
|\demo\log\easylogger.c                 |基于[EasyLogger](https://github.com/armink/EasyLogger)的Log Demo|


- 2、将`\easyflash\`（里面包含`inc`、`src`及`port`的那个）文件夹拷贝到项目中；
- 3、添加`\easyflash\src\easyflash.c`、`\easyflash\src\ef_utils.c`及`\easyflash\port\ef_port.c`这些文件到项目的编译路径中；
- 4、根据项目需求，选择性添加`\easyflash\src\`中的其他源码文件到项目的编译路径中；
- 5、添加`\easyflash\inc\`文件夹到编译的头文件目录列表中；

## 3、Flash规格

在移植时务必先要了解项目的Flash规格，这里需要了解是规格是 **最小擦除单位** 及 **内部存储结构** ，各个厂家的Flash规格都有差异，同一厂家不同系列的规格也有差异。例如：stm32f10x系列中的大容量MCU自带Flash的页大小均为2K，而中小容量的页大小均为1K。在stm32f4xx系列中，每个页大小不是像stm32f10x那样平均分配，最大的有128K，最小的有16K。

> 注意：务必保证熟悉Flash规格后，再继续下章节。

## 4、移植接口

### 4.1 移植初始化

EasyFlash移植初始化。可以传递默认环境变量，初始化EasyFlash移植所需的资源等等。

```C
EfErrCode ef_port_init(ef_env const **default_env, size_t *default_env_size)
```

|参数                                    |描述|
|:-----                                  |:----|
|default_env                             |默认的环境变量|
|default_env_size                        |默认环境变量的数量|

### 4.2 读取Flash

最小单位为4个字节。

```C
EfErrCode ef_port_read(uint32_t addr, uint32_t *buf, size_t size)
```

|参数                                    |描述|
|:-----                                  |:----|
|addr                                    |读取起始地址|
|buf                                     |存放读取数据的缓冲区|
|size                                    |读取数据的大小（字节）|

### 4.3 擦除Flash

```C
EfErrCode ef_port_erase(uint32_t addr, size_t size)
```

|参数                                    |描述|
|:-----                                  |:----|
|addr                                    |擦除起始地址|
|size                                    |擦除数据的大小（字节）|

### 4.4 写入Flash

最小单位为4个字节。

```C
EfErrCode ef_port_write(uint32_t addr, const uint32_t *buf, size_t size)
```

|参数                                    |描述|
|:-----                                  |:----|
|addr                                    |写入的起始地址|
|buf                                     |源数据的缓冲区|
|size                                    |写入数据的大小（字节）|

### 4.5 对环境变量缓冲区加锁

为了保证RAM缓冲区在并发执行的安全性，所以需要对其进行加锁（如果项目的使用场景不存在并发情况，则可以忽略）。有操作系统时可以使用获取信号量来加锁，裸机时可以通过关闭全局中断来加锁。

```C
void ef_port_env_lock(void)
```

### 4.6 对环境变量缓冲区解锁

有操作系统是可以使用释放信号量来解锁，裸机时可以通过开启全局中断来解锁。

```C
void ef_port_env_unlock(void)
```

### 4.7 打印调试日志信息

在定义`PRINT_DEBUG`宏后，打印调试日志信息。

```C
void ef_log_debug(const char *file, const long line, const char *format, ...)
```

|参数                                    |描述|
|:-----                                  |:----|
|file                                    |调用该方法的文件|
|line                                    |调用该方法的行号|
|format                                  |打印格式|
|...                                     |不定参|

### 4.8 打印普通日志信息

```C
void ef_log_info(const char *format, ...)
```

|参数                                    |描述|
|:-----                                  |:----|
|format                                  |打印格式|
|...                                     |不定参|

### 4.9 无格式打印信息

该方法输出无固定格式的打印信息，为`ef_print_env`方法所用（如果不使用`ef_print_env`则可以忽略）。而`ef_log_debug`及`ef_log_info`可以输出带指定前缀及格式的打印日志信息。

```C
void ef_print(const char *format, ...)
```

|参数                                    |描述|
|:-----                                  |:----|
|format                                  |打印格式|
|...                                     |不定参|

## 5、设置参数

配置时需要修改项目中的`ef_cfg.h`文件，开启、关闭、修改对应的宏即可。

### 5.1 环境变量功能

- 默认状态：开启
- 操作方法：开启、关闭`EF_USING_ENV`宏即可

#### 5.1.1 磨损平衡/常规 模式

磨损平衡：由于flash在写操作之前需要擦除且使用寿命有限，所以需要设计合理的磨损平衡（写平衡）机制，来保证数据被安全的保存在未到擦写寿命的Flash区中。

- 默认状态：常规模式
- 常规模式：关闭`EF_ENV_USING_WL_MODE`
- 磨损平衡模式：打开`EF_ENV_USING_WL_MODE`

#### 5.1.2 掉电保护

掉电保护：Power Fail Safeguard，当此项设置为可用时，如果在环境变量保存过程中发生掉电，已保存在Flash中的环境变量将不会有丢失的危险。下次上电后，环境变量将会被自动还原至之前的状态。（注意：本保护是基于软件实现的保护功能，更加可靠的掉电保护功能需要通过硬件来实现）

- 默认状态：关闭
- 操作方法：开启、关闭`EF_ENV_USING_PFS_MODE`宏即可

#### 5.1.3 自动更新（增量更新）

可以对 ENV 设置版本号（参照 5.1.4）。当 ENV 初始化时，如果检测到产品存储的版本号与设定版本号不一致，会自动追加默认环境变量集合中新增的环境变量。

该功能非常适用于经常升级的产品中，当产品功能变更时，有可能会新增环境变量，此时只需要增大当前设定的 ENV 版本号，下次固件升级后，新增的环境变量将会自动追加上去。

- 默认状态：关闭
- 操作方法：开启、关闭`EF_ENV_AUTO_UPDATE`宏即可

#### 5.1.4 环境变量版本号

该配置依赖于 5.1.3 配置。设置的环境变量版本号为整形数值，可以从 0 开始。如果在默认环境变量表中增加了环境变量，此时需要对该配置进行修改（通常加 1 ）。

- 操作方法：修改`EF_ENV_VER_NUM`宏对应值即可

### 5.2 在线升级功能

- 默认状态：开启
- 操作方法：开启、关闭`EF_USING_IAP`宏即可

### 5.3 日志功能

- 默认状态：开启
- 操作方法：开启、关闭`EF_USING_LOG`宏即可

### 5.4 Flash最小擦除单位

- 操作方法：修改`EF_ERASE_MIN_SIZE`宏对应值即可

### 5.5 备份区

备份区共计包含3个区域，依次为：环境变量区、日志区及在线升级区。分区方式如下图所示

![backup_area_partiton](http://git.oschina.net/Armink/EasyFlash/raw/master/docs/zh/images/BackupAreaPartition.jpg)

在配置时需要注意以下几点：

- 1、所有的区域必须按照`EF_ERASE_MIN_SIZE`对齐；
- 2、由于EasyFlash对所有的环境变量都会使用RAM缓存，但是在更多时候用户使用的环境变量大小会比`EF_ERASE_MIN_SIZE`小，所以需要再定义`ENV_USER_SETTING_SIZE`来指定用户设定的环境变量大小。
- 3、环境变量区总容量在不同的模式下会有差异
 - 1、常规模式：没有差异；
 - 2、擦写平衡模式：系统区将会占用1个`EF_ERASE_MIN_SIZE`大小，数据区至少等使用2个以上Flash扇区；
 - 3、掉电保护模式：环境变量区将会被备份，所以总容量是常规模式的2倍；
 - 4、擦写平衡+掉电保护模式：所需容量将会是擦写平衡模式下总容量的2倍。
 - 例如：`EF_ERASE_MIN_SIZE`是128K，`ENV_USER_SETTING_SIZE`是2K，那么你可以这样定义不同模式下的环境变量总容量：
 - 1、常规模式：`1*EF_ERASE_MIN_SIZE`；
 - 2、擦写平衡模式：`3*EF_ERASE_MIN_SIZE`（它将会有3个Flash扇区去存储环境变量，1个系统区，2个数据区，按照每个Flash扇区可被擦写10W次计算，那么当前配置至少可擦写20W次）;
 - 3、掉电保护模式：`2*EF_ERASE_MIN_SIZE`;
 - 4、擦写平衡+掉电保护模式：`6*EF_ERASE_MIN_SIZE`;

#### 5.5.1 备份区起始地址

- 操作方法：修改`EF_START_ADDR`宏对应值即可

#### 5.5.2 用户设定环境变量大小

- 操作方法：修改`ENV_USER_SETTING_SIZE`宏对应值即可

> 注意：不使用环境变量功能时，可以不定义此宏。

#### 5.5.3 环境变量区总容量

- 操作方法：修改`ENV_AREA_SIZE`宏对应值即可

> 注意：不使用环境变量功能时，可以不定义此宏。

#### 5.5.4 日志区总容量

- 操作方法：修改`LOG_AREA_SIZE`宏对应值即可

> 注意：不使用日志功能时，可以不定义此宏。

### 5.6 调试日志

开启后，将会库运行时自动输出调试日志

- 默认状态：开启
- 操作方法：开启、关闭`PRINT_DEBUG`宏即可

## 6、测试验证

如果`\demo\`文件夹下有与项目Flash规格一致的Demo，则直接编译运行，观察测试结果即可。无需关注下面的步骤。

每次使用前，务必先执行`easyflash_init()`方法对EasyFlash库及所使用的Flash进行初始化，保证初始化没问题后，再使用各功能的API方法。如果出现错误或断言，需根据提示信息检查移植配置及接口。

### 6.1 环境变量

查看[`\demo\env\`](https://github.com/armink/EasyFlash/tree/master/demo/env)子文件夹中例子的`README.md`说明文档。测试时可以将`\demo\env\stm32f10x\non_os\app\src\app.c`中的`static void test_env(void)`方法体复制到项目中，然后运行测试。

### 6.2 在线升级

查看[`\demo\iap\README.md`](https://github.com/armink/EasyFlash/tree/master/demo/iap)说明文档。

### 6.3 日志

查看[`\demo\log\README.md`](https://github.com/armink/EasyFlash/tree/master/demo/log)说明文档。

> 注意：`easylogger.c`是使用[EasyLogger](https://github.com/armink/EasyLogger)与EasyFlash的无缝接口的例子，EasyLogger提供针对日志的很多常用功能封装，详细功能可以查看其介绍。使用这个例子时，务必记得将EasyLogger一并导入到项目中。