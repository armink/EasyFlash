# V4.0 迁移指南

## 1、V3.0 与 V4.0 差异

### 1.1 API 接口方面

#### 1.1.1 完全兼容旧版

V4.0 在设计时，已经做到接口完全兼容旧版本，所以如果你的应用使用的是旧版本，那么无需修改任何源代码即可做到无缝迁移。

#### 1.1.2 新增接口

V4.0 底层对于 ENV 的存储使用的 blob 格式，所以增加如下 blob 操作接口，替代 V3.0 的基于字符串的接口

- `size_t ef_get_env_blob(const char *key, void *value_buf, size_t buf_len, size_t *value_len)`
- `EfErrCode ef_set_env_blob(const char *key, const void *value_buf, size_t buf_len)`

#### 1.1.3 废弃接口

以下接口在 V4.0 中仍然可用，但已经由于种种原因被废弃，可能将会在 V5.0 版本中被正式删除

- `char *ef_get_env(const char *key)`

  - 注意：由于 V4.0 版本开始，在该函数内部具有环境变量的缓冲区，不允许连续多次同时使用该函数，例如如下代码：

    ```C
    // 错误的使用方法
    ssid = ef_get_env("ssid");
    password = ef_get_env("password"); // 由于 buf 共用，password 与 ssid 会返回相同的 buf 地址
    
    // 建议改为下面的方式
    ssid = strdup(ef_get_env("ssid")); // 克隆获取回来的环境变量
    password = strdup(ef_get_env("password"));
    
    // 使用完成后，释放资源
    free(ssid); // 与 strdup 成对
    free(password);
    ```

    

- `EfErrCode ef_save_env(void)`

- `EfErrCode ef_set_and_save_env(const char *key, const char *value)`

- `EfErrCode ef_del_and_save_env(const char *key)`

- `size_t ef_get_env_write_bytes(void)`

## 2、主要修改项

### 2.1 配置方面

- 删除 EF_ENV_USING_WL_MODE：V4.0 原生支持磨损平衡，无需额外开启
- 删除 EF_ENV_USING_PFS_MODE：V4.0 自带掉电保护功能，无需额外开启
- 删除 ENV_USER_SETTING_SIZE：V4.0 无需 RAM 缓存
- 增加 EF_WRITE_GRAN ：详见移植文档

### 2.2 接口改进

- 建议使用 ef_get_env_blob 接口替代 ef_get_env，使用方法详见 API 文档
- V4.0 无需额外执行保存动作，所以使用这些接口的代码在 V4..0 无意义，可以移除
  - ef_save_env
  - ef_set_and_save_env
  - ef_del_and_save_env  

