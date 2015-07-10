# EasyFlash API 说明

---

所有支持的API接口都在 `\easyflash\inc\easyflash.h` 中声明。以下内容较多，建议使用 **CTRL+F** 搜索。

名词介绍：

**备份区** ：是EasyFlash定义的一个存放环境变量及已下载程序的Flash区域，详细存储架构可以参考 `\easyflash\src\easyflash.c` 文件头位置的注释说明。

**环境变量表** ：负责存放所有的环境变量，该表在Flash及RAM中均存在，上电后需从Flash加载到RAM中，修改后，则需要保存其至Flash中。。

## 1、用户使用接口

### 1.1 初始化

初始化EasyFlash。在初始化的过程中会使用 `\easyflash\port\ef_port.c` 中的用户自定义参数。

```C
EfErrCode easyflash_init(void)
```

### 1.2 环境变量

#### 1.2.1 加载环境变量

加载Flash中的所有环境变量到系统内存中。

```C
void ef_load_env(void)
```

#### 1.2.2 打印环境变量

通过在移植接口( `\easyflash\port\ef_port.c` )中定义的 `ef_print` 打印方法，来将Flash中的所有环境变量输出出来。

```C
void ef_print_env(void)
```

#### 1.2.3 获取环境变量

通过环境变量的名字来获取其对应的值。（注意：此处的环境变量指代的已加载到内存中的环境变量）

```C
char *ef_get_env(const char *key)
```

|参数                                    |描述|
|:-----                                  |:----|
|key                                     |环境变量名称|

#### 1.2.4 设置环境变量

使用此方法可以实现对环境变量的增加、修改及删除功能。（注意：此处的环境变量指代的已加载到内存中的环境变量）

- **增加** ：当环境变量表中不存在该名称的环境变量时，则会执行新增操作；

- **修改** ：入参中的环境变量名称在当前环境变量表中存在，则把该环境变量值修改为入参中的值；

- **删除** ：当入参中的value为空时，则会删除入参名对应的环境变量。

```C
EfErrCode ef_set_env(const char *key, const char *value)
```

|参数                                    |描述|
|:-----                                  |:----|
|key                                     |环境变量名称|
|value                                   |环境变量值|

#### 1.2.5 保存环境变量

保存内存中的环境变量表到Flash中。

```C
EfErrCode ef_save_env(void)
```

#### 1.2.6 重置环境变量
将内存中的环境变量表重置为默认值。

```C
EfErrCode ef_env_set_default(void)
```

#### 1.2.7 获取环境变量分区的总容量

```C
size_t ef_get_env_total_size(void)
```

#### 1.2.8 获取当前环境变量写入到Flash的字节大小

```C
size_t ef_get_env_write_bytes(void)
```

### 1.3 在线升级

#### 1.3.1 擦除备份区中的应用程序

```C
EfErrCode ef_erase_bak_app(size_t app_size)
```

#### 1.3.2 擦除用户的应用程序

注意：请不要在应用程序中调用该方法

```C
EfErrCode ef_erase_user_app(uint32_t user_app_addr, size_t user_app_size)
```

|参数                                    |描述|
|:-----                                  |:----|
|user_app_addr                           |用户应用程序入口地址|
|user_app_size                           |用户应用程序大小|

#### 1.3.3 擦除Bootloader

注意：请不要在Bootloader中调用该方法

```C
EfErrCode ef_erase_bl(uint32_t bl_addr, size_t bl_size)
```

|参数                                    |描述|
|:-----                                  |:----|
|bl_addr                                 |Bootloader入口地址|
|bl_size                                 |Bootloader大小|

#### 1.3.4 写数据到备份区

为下载程序到备份区定制的Flash连续写方法。
注意：写之前请先确认Flash已进行擦除。

```C
EfErrCode ef_write_data_to_bak(uint8_t *data,
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

#### 1.3.5 从备份拷贝应用程序

将备份区已下载好的应用程序拷贝至用户应用程序起始地址。
注意：
1、拷贝前必须对原有的应用程序进行擦除
2、不要在应用程序中调用该方法

```C
EfErrCode ef_copy_app_from_bak(uint32_t user_app_addr, size_t app_size)
```

|参数                                    |描述|
|:-----                                  |:----|
|user_app_addr                           |用户应用程序入口地址|
|user_app_size                           |用户应用程序大小|

#### 1.3.6 从备份拷贝Bootloader

将备份区已下载好的Bootloader拷贝至Bootloader起始地址。
注意：
1、拷贝前必须对原有的Bootloader进行擦除
2、不要在Bootloader中调用该方法

```C
EfErrCode ef_copy_bl_from_bak(uint32_t bl_addr, size_t bl_size)
```

|参数                                    |描述|
|:-----                                  |:----|
|bl_addr                                 |Bootloader入口地址|
|bl_size                                 |Bootloader大小|

### 1.4 日志存储

#### 1.4.1 从Flash中读取已存在的日志

```C
EfErrCode ef_log_read(size_t index, uint32_t *log, size_t size);
```

|参数                                    |描述|
|:-----                                  |:----|
|index                                   |日志读取的索引顺序|
|log                                     |存储待读取日志的缓冲区|
|size                                    |读取日志的大小|

#### 1.4.2 往Flash中保存日志

```C
EfErrCode ef_log_write(const uint32_t *log, size_t size);
```

|参数                                    |描述|
|:-----                                  |:----|
|log                                     |存储待保存的日志|
|size                                    |待保存日志的大小|

#### 1.4.3 清空存储在Flash中全部日志

```C
EfErrCode ef_log_clean(void);
```

#### 1.4.4 获取已存储在Flash中的日志大小

```C
size_t ef_log_get_used_size(void);
```

## 2 移植接口

### 2.1 读取Flash

最小单位为4个字节

```C
EfErrCode ef_port_read(uint32_t addr, uint32_t *buf, size_t size)
```

|参数                                    |描述|
|:-----                                  |:----|
|addr                                    |读取起始地址|
|buf                                     |存放读取数据的缓冲区|
|size                                    |读取数据的大小（字节）|

### 2.2 擦除Flash

```C
EfErrCode ef_port_erase(uint32_t addr, size_t size)
```

|参数                                    |描述|
|:-----                                  |:----|
|addr                                    |擦除起始地址|
|size                                    |擦除数据的大小（字节）|

### 2.3 写入Flash

最小单位为4个字节

```C
EfErrCode ef_port_write(uint32_t addr, const uint32_t *buf, size_t size)
```

|参数                                    |描述|
|:-----                                  |:----|
|addr                                    |写入的起始地址|
|buf                                     |源数据的缓冲区|
|size                                    |写入数据的大小（字节）|

### 2.4 对环境变量缓冲区加锁

```C
void ef_port_env_lock(void)
```

### 2.5 对环境变量缓冲区解锁

```C
void ef_port_env_unlock(void)
```

### 2.6 打印调试日志信息

在定义 `PRINT_DEBUG` 宏后，打印调试日志信息

```C
void ef_log_debug(const char *file, const long line, const char *format, ...)
```

|参数                                    |描述|
|:-----                                  |:----|
|file                                    |调用该方法的文件|
|line                                    |调用该方法的行号|
|format                                  |打印格式|
|...                                     |不定参|

### 2.7 打印普通日志信息

```C
void ef_log_info(const char *format, ...)
```

|参数                                    |描述|
|:-----                                  |:----|
|format                                  |打印格式|
|...                                     |不定参|

### 2.8 无格式打印信息

该方法输出无固定格式的打印信息，为 `ef_print_env` 方法所用。而 `ef_log_debug` 及 `ef_log_info` 可以输出带指定前缀及格式的打印日志信息。

```C
void ef_print(const char *format, ...)
```

|参数                                    |描述|
|:-----                                  |:----|
|format                                  |打印格式|
|...                                     |不定参|

## 3、配置

配置该库需要打开`\easyflash\easyflash.h`文件，开启、关闭、修改对应的宏即可。

### 3.1 ENV功能

- 默认状态：开启
- 操作方法：开启、关闭`EF_USING_ENV`宏即可

#### 3.1.1 环境变量的容量

- 默认容量：2K Bytes
- 操作方法：修改`EF_USER_SETTING_ENV_SIZE`宏定义即可

#### 3.1.2 磨损平衡/常规 模式

> 磨损平衡：由于flash在写操作之前需要擦除且使用寿命有限，所以需要设计合理的磨损平衡（写平衡）机制，来保证数据被安全的保存在未到擦写寿命的Flash区中。

- 默认状态：常规模式
- 磨损平衡模式：打开`EF_ENV_USING_WL_MODE`，关闭`EF_ENV_USING_NORMAL_MODE`
- 常规模式：打开`EF_ENV_USING_NORMAL_MODE`，关闭`FLASH_ENV_USING_WL_MODE`

> 注意：只能选择其中一种模式，两种模式不能同时使用

#### 3.1.3 掉电保护

> 掉电保护：Power Fail Safeguard，当此项设置为可用时，如果在环境变量保存过程中发生掉电，已保存在Flash中的环境变量将不会有丢失的危险。下次上电后，环境变量将会被自动还原至之前的状态。（注意：本保护是基于软件实现的保护功能，更加可靠的掉电保护功能需要通过硬件来实现）

- 默认状态：关闭
- 操作方法：开启、关闭`EF_ENV_USING_PFS_MODE`宏即可

### 3.2 IAP功能

- 默认状态：开启
- 操作方法：开启、关闭`EF_USING_IAP`宏即可

### 3.3 Log功能

- 默认状态：开启
- 操作方法：开启、关闭`EF_USING_LOG`宏即可

## 4、注意

- 写数据前务必记得先擦除
- 环境变量设置完后，只有调用 `ef_save_env`才会保存在Flash中，否则开机会丢失修改的内容
- 不要在应用程序及Bootloader中执行擦除及拷贝自身的动作
- ENV及Log功能对Flash擦除和写入要求4个字节对齐，擦除的最小单位则需根据用户的平台来确定
