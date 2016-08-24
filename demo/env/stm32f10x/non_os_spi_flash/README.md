# stm32f10x 裸机平台外部 SPI Flash Demo

---

## 1、简介

通过 `app\src\app.c` 的 `test_env()` 方法来演示环境变量的读取及修改功能，每次系统启动并且初始化EasyFlash成功后会调用该方法。

在 `test_env()` 方法中，会先读取系统的启动次数，读取后对启动次数加一，再存入到环境变量中，实现记录系统启动（开机）次数的功能。

对于 SPI Flash 的驱动这里采用的一款开源的万能 SPI Flash 驱动库 SFUD ，使得此 Demo 能够支持市面上尽可能多的 SPI Flash， [点击查看](https://github.com/armink/SFUD) SFUD 详细介绍说明及使用方法。

### 1.1、使用方法

- 1、打开电脑的终端与Demo的串口1进行连接，串口配置 115200 8 1 N，此时可以在终端上看到Demo的打印日志
- 2、断电重启Demo
- 3、等待重启完成后，即可查看到打印信息中的启动次数有所增加，日志信息大致如下

```
[SFUD](..\components\sfud\src\sfud.c:79) Start initialize Serial Flash Universal Driver(SFUD) V0.07.13.
[SFUD](..\components\sfud\src\sfud.c:709) The flash device manufacturer ID is 0xBF, memory type ID is 0x25, capacity ID is 0x41.
[SFUD]Error: Check SFDP signature error. It's must be 50444653h('S' 'F' 'D' 'P').
[SFUD]Warning: Read SFDP parameter header information failed. The SST25VF016B is not support JEDEC SFDP.
[SFUD]Find a SST SST25VF016B flash chip. Size is 2097152 bytes.
[SFUD](..\components\sfud\src\sfud.c:688) Flash device reset success.
[SFUD]SST25VF016B flash device is initialize success.
[Flash](..\..\..\..\..\easyflash\src\ef_env.c:141) ENV start address is 0x00000000, size is 4096 bytes.
[Flash](..\..\..\..\..\easyflash\src\ef_env.c:714) Calculate ENV CRC32 number is 0x14B7A4B1.
[Flash]Warning: ENV CRC check failed. Set it to default.
[Flash](..\..\..\..\..\easyflash\src\ef_env.c:714) Calculate ENV CRC32 number is 0xF967C182.
[Flash]Erased ENV OK.
[Flash]Saved ENV OK.
[Flash](..\..\..\..\..\easyflash\src\easyflash.c:97) EasyFlash V1.12.16 is initialize success.
The system now boot 1 times
[Flash](..\..\..\..\..\easyflash\src\ef_env.c:714) Calculate ENV CRC32 number is 0xC407E832.
[Flash]Erased ENV OK.
[Flash]Saved ENV OK.
```


> 注意：对于无法连接终端的用户，也可以使用仿真器与Demo平台进行连接，来观察启动次数的变化

## 2、文件（夹）说明

`components\easyflash\port\ef_port.c` 移植参考文件

`RVMDK` 下为Keil工程文件

`EWARM` 下为IAR工程文件