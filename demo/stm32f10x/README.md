# stm32f10x 平台测试例子

---

## 1、简介

通过 `\demo\stm32f10x\app\src\app_task.c` 的 `test_env()` 方法来演示环境变量的读取及修改功能，每次系统启动并且初始化EasyFlash成功后会调用该方法。

在 `test_env()` 方法中，会先读取系统的启动次数，读取后对启动次数加一，再存入到环境变量中，实现记录系统启动（开机）次数的功能。

## 2、文件说明

`\demo\stm32f10x\components\flash\port\flash_port.c` 移植参考文件

`\demo\stm32f10x\RVMDK` 下为Keil工程文件

`\demo\stm32f10x\EWARM` 下为IAR工程文件