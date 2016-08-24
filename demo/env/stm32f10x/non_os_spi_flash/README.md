# stm32f10x ���ƽ̨�ⲿ SPI Flash Demo

---

## 1�����

ͨ�� `app\src\app.c` �� `test_env()` ��������ʾ���������Ķ�ȡ���޸Ĺ��ܣ�ÿ��ϵͳ�������ҳ�ʼ��EasyFlash�ɹ������ø÷�����

�� `test_env()` �����У����ȶ�ȡϵͳ��������������ȡ�������������һ���ٴ��뵽���������У�ʵ�ּ�¼ϵͳ�����������������Ĺ��ܡ�

���� SPI Flash ������������õ�һ�Դ������ SPI Flash ������ SFUD ��ʹ�ô� Demo �ܹ�֧�������Ͼ����ܶ�� SPI Flash�� [����鿴](https://github.com/armink/SFUD) SFUD ��ϸ����˵����ʹ�÷�����

### 1.1��ʹ�÷���

- 1���򿪵��Ե��ն���Demo�Ĵ���1�������ӣ��������� 115200 8 1 N����ʱ�������ն��Ͽ���Demo�Ĵ�ӡ��־
- 2���ϵ�����Demo
- 3���ȴ�������ɺ󣬼��ɲ鿴����ӡ��Ϣ�е����������������ӣ���־��Ϣ��������

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


> ע�⣺�����޷������ն˵��û���Ҳ����ʹ�÷�������Demoƽ̨�������ӣ����۲����������ı仯

## 2���ļ����У�˵��

`components\easyflash\port\ef_port.c` ��ֲ�ο��ļ�

`RVMDK` ��ΪKeil�����ļ�

`EWARM` ��ΪIAR�����ļ�