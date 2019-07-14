# EasyFlash API 说明

---

所有支持的API接口都在[`\easyflash\inc\easyflash.h`](https://github.com/armink/EasyFlash/blob/master/easyflash/inc/easyflash.h)中声明。以下内容较多，建议使用 **CTRL+F** 搜索。

名词介绍：

- **备份区** ：是EasyFlash定义的一个存放环境变量、已下载程序及日志的Flash区域，详细存储架构可以参考[`\easyflash\src\easyflash.c`](https://github.com/armink/EasyFlash/blob/master/easyflash/src/easyflash.c#L29-L58)文件头位置的注释说明或[移植文档中关于备份区参数配置](https://github.com/armink/EasyFlash/blob/master/docs/zh/port.md#55-备份区)。

## 1、用户使用接口

### 1.1 初始化

初始化的EasyFlash的各个组件，初始化后才可以使用下面的API。

针对环境变量组件，如果 flash 第一次初始化，会自动调用 `ef_env_set_default` ，恢复移植文件中定义的默认环境变量（详见移植文档），之后每次启动会自动检查 flash 中的环境变量状态，flash 出现严重损坏时也会恢复环境变量至默认。如果开启增量升级功能，还会做增量升级的检查，版本号不一致时执行增量升级。

```C
EfErrCode easyflash_init(void)
```

### 1.2 环境变量

在 V4.0 以后，环境变量在 EasyFlash 底层都是按照二进制数据格式进行存储，即 **blob 格式** ，这样上层支持传入任意类型。而在 V4.0 之前底层使用的是字符串格式进行存储，字符串格式的环境变量也可以转换为 blob 格式，所以 V4.0 能做到完全兼容以前的 API ，例如：`ef_get_env/ef_set_env` ，这些 API **只能** 给字符串 ENV 使用。

另外字符串 ENV 也有长度的限制，默认 `EF_STR_ENV_VALUE_MAX_SIZE` 配置为 128 字节，超过长度的可能无法使用 `ef_get_env` 获取，只能使用 `ef_get_env_blob` 获取。

#### 1.2.1 获取环境变量

通过环境变量的名字来获取其对应的值。支持两种接口

##### 1.2.1.1 获取 blob 类型环境变量

```C
size_t ef_get_env_blob(const char *key, void *value_buf, size_t buf_len, size_t *save_value_len)
```

|参数                                    |描述|
|:-----                                  |:----|
|key                                     |环境变量名称|
|value_buf |存放环境变量的缓冲区|
|buf_len |该缓冲区的大小|
|save_value_len |返回该环境变量实际存储在 flash 中的大小|
|返回 |成功存放至缓冲区中的数据长度|

示例：

```C
char value[32];
size_t len;
/* 如果环境变量长度未知，可以先获取 Flash 上存储的实际长度，将通过 len 返回 */
ef_get_env_blob("key", NULL, 0, &len);
/* 如果长度已知，使用 value 缓冲区，存放读取回来的环境变量值数据，并将实际长度返回 */
len = ef_get_env_blob("key", value, sizeof(value) , NULL);
```

##### 1.2.1.2 获取字符串类型环境变量

**注意** ：

- 该函数**已废弃**，不推荐继续使用，可以使用上面的函数替代；
- 该函数不允许连续使用，使用时需使用 strdup 包裹，确保每次返回回来的字符串内存空间独立；
- 该函数不支持可重入，返回的值位于函数内部缓冲区，出于安全考虑，请加锁保护。

```C
char *ef_get_env(const char *key)
```

|参数                                    |描述|
|:-----                                  |:----|
|key                                     |环境变量名称|
|返回 |环境变量值|


#### 1.2.2 设置环境变量

使用此方法可以实现对环境变量的增加、修改及删除功能。

- **增加** ：当环境变量表中不存在该名称的环境变量时，则会执行新增操作；
- **修改** ：入参中的环境变量名称在当前环境变量表中存在，则把该环境变量值修改为入参中的值；
- **删除**：当入参中的value为NULL时，则会删除入参名对应的环境变量。 

##### 1.2.1.1 设置 blob 类型环境变量

```C
EfErrCode ef_set_env_blob(const char *key, const void *value_buf, size_t buf_len)
```

|参数                                    |描述|
|:-----                                  |:----|
|key                                     |环境变量名称|
|value_buf                                   |环境变量值缓冲区|
|buf_len |缓冲区长度，即值的长度|

##### 1.2.1.2 设置字符串类型环境变量


```C
EfErrCode ef_set_env(const char *key, const char *value)
```

|参数                                    |描述|
|:-----                                  |:----|
|key                                     |环境变量名称|
|value                                   |环境变量值|

#### 1.2.3 删除环境变量

使用此方法可以实现对环境变量的删除功能。

```c
EfErrCode ef_del_env(const char *key)
```

| 参数 | 描述         |
| :--- | :----------- |
| key  | 环境变量名称 |


#### 1.2.4 重置环境变量
将内存中的环境变量表重置为默认值。关于默认环境变量的设定方法可以参考移植文档。

```C
EfErrCode ef_env_set_default(void)
```

#### 1.2.5 打印环境变量

通过在移植接口([`\easyflash\port\ef_port.c`](https://github.com/armink/EasyFlash/blob/master/easyflash/port/ef_port.c))中定义的`ef_print`打印方法，来将Flash中的所有环境变量输出出来。

```C
void ef_print_env(void)
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

#### 1.3.3 通过用户指定的擦除方法来擦除应用程序

当用户的应用程序与备份区 **不在同一个** Flash 时，则需要用户额外指定擦除应用程序的方法。而 `ef_erase_user_app` 会使用移植文件中的 `ef_port_erase` 方法进行擦除，除此之外的其余功能，两个方法均一致。

注意：请不要在应用程序中调用该方法

```C
EfErrCode ef_erase_spec_user_app(uint32_t user_app_addr, size_t app_size,
        EfErrCode (*app_erase)(uint32_t addr, size_t size));
```

|参数                                    |描述|
|:-----                                  |:----|
|user_app_addr                           |用户应用程序入口地址|
|user_app_size                           |用户应用程序大小|
|app_erase                               |用户指定应用程序擦写方法|

#### 1.3.4 擦除Bootloader

注意：请不要在Bootloader中调用该方法

```C
EfErrCode ef_erase_bl(uint32_t bl_addr, size_t bl_size)
```

|参数                                    |描述|
|:-----                                  |:----|
|bl_addr                                 |Bootloader入口地址|
|bl_size                                 |Bootloader大小|

#### 1.3.5 写数据到备份区

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

#### 1.3.6 从备份拷贝应用程序

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

#### 1.3.7 通过用户指定的写操作方法来拷贝应用程序

当用户的应用程序与备份区 **不在同一个** Flash 时，则需要用户额外指定写应用程序的方法。而 `ef_copy_app_from_bak` 会使用移植文件中的 `ef_port_write` 方法进行写操作，除此之外的其余功能，两个方法均一致。

```C
EfErrCode ef_copy_spec_app_from_bak(uint32_t user_app_addr, size_t app_size,
        EfErrCode (*app_write)(uint32_t addr, const uint32_t *buf, size_t size))
```

|参数                                    |描述|
|:-----                                  |:----|
|user_app_addr                           |用户应用程序入口地址|
|user_app_size                           |用户应用程序大小|
|app_write                               |用户指定应用程序写操作方法|

#### 1.3.8 从备份拷贝Bootloader

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

## 2、配置

参照EasyFlash 移植说明（[`\docs\zh\port.md`](https://github.com/armink/EasyFlash/blob/master/docs/zh/port.md#5设置参数)）中的 `设置参数` 章节

## 3、注意

- 写数据前务必记得先擦除
- 不要在应用程序及Bootloader中执行擦除及拷贝自身的动作
- Log功能对Flash擦除和写入要求4个字节对齐，擦除的最小单位则需根据用户的平台来确定
