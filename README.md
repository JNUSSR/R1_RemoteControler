# R1遥控器

R1遥控器的工程仓库，包括PCB工程文件，代码工程文件，外壳文件，使用说明。

- 陶晶驰资料中心：http://wiki.tjc1688.com/
- 固件协议详见：[software/Romote_Controler/README.md](software/Romote_Controler/README.md)

## 更新

**2026-06**: 固件通信协议由自定义二进制帧迁移至 **MAVLink v2**，新增 Web 串口监视器。

```
software/Romote_Controler/
├── mavlink_xml/
│   ├── remote_control.xml   # MAVLink 协议定义 (MsgID=150)
│   └── index.html           # Web 串口监视器
├── Hardware/
│   ├── INTERRUPT/           # MAVLink 打包发送 (替代原 E28 协议)
│   └── mavlink/             # MAVLink C 库
└── README.md                # 详细说明
```
