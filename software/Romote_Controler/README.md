# R1 遥控器 · 固件

本目录为 R1 遥控器的 STM32F405 固件工程。原通信协议为自定义二进制帧（E28 模块），现已迁移为标准 **MAVLink v2** 协议。

---

## MAVLink v2 通信协议

### 协议格式

使用 MAVLink v2 帧格式，通过 UART6 (E28 无线模块) 发送，115200/9600 baud 可配置。

### 自定义消息：REMOTE_CONTROL_STATE (MsgID = 150)

| 字段 | 类型 | 偏移 | 说明 |
|---|---|---|---|
| `kfs` | `uint32_t` | 0 | 梅林 KFS 按键矩阵 4×3，每元素 2 bit（值 0~3），共 24 bit |
| `encoder_0` | `int16_t` | 4 | 编码器 0 旋转位置值 |
| `encoder_1` | `int16_t` | 6 | 编码器 1 旋转位置值 |
| `encoder_2` | `int16_t` | 8 | 编码器 2 旋转位置值 |
| `encoder_3` | `int16_t` | 10 | 编码器 3 旋转位置值 |
| `move_0` | `int16_t` | 12 | 摇杆通道 0 移动量 |
| `move_1` | `int16_t` | 14 | 摇杆通道 1 移动量 |
| `move_2` | `int16_t` | 16 | 摇杆通道 2 移动量 |
| `move_3` | `int16_t` | 18 | 摇杆通道 3 移动量 |
| `mode` | `uint8_t` | 20 | 4 路模式拨码开关，bit0~bit3 对应 mode[0]~mode[3] |
| `act` | `uint8_t` | 21 | 屏幕触发的动作指令字节 |
| `kfs_put` | `uint8_t` | 22 | R2 九宫格按键矩阵 2×3，每元素 1 bit（值 0~1），共 6 bit |

**Payload 大小**: 23 字节

### KFS 位打包规则

```
kfs (uint32_t, bit[23:0]):
  kfs[0][0] → bit[ 1: 0]    kfs[2][0] → bit[13:12]
  kfs[0][1] → bit[ 3: 2]    kfs[2][1] → bit[15:14]
  kfs[0][2] → bit[ 5: 4]    kfs[2][2] → bit[17:16]
  kfs[1][0] → bit[ 7: 6]    kfs[3][0] → bit[19:18]
  kfs[1][1] → bit[ 9: 8]    kfs[3][1] → bit[21:20]
  kfs[1][2] → bit[11:10]    kfs[3][2] → bit[23:22]
  bit[31:24] → 保留

kfs_put (uint8_t, bit[5:0]):
  kfs_put[0][0] → bit[0]    kfs_put[1][0] → bit[3]
  kfs_put[0][1] → bit[1]    kfs_put[1][1] → bit[4]
  kfs_put[0][2] → bit[2]    kfs_put[1][2] → bit[5]
  bit[7:6] → 保留
```

### 发送条件

- **硬守卫**: E28 模块空闲 (PB15 引脚 = HIGH)
- **软守卫**: STM32 UART6 DMA 发送就绪 (`huart6.gState == HAL_UART_STATE_READY`)
- **模式条件**: 仅当 `mode[0]` 拨下（低电平）时发送

### 发送后消费

发送成功后清零 `act` 和 `kfs`/`kfs_put` 矩阵，避免数据重复发送。

---

## 目录结构

```
software/Romote_Controler/
├── Core/                    # STM32CubeMX 生成的核心代码
│   ├── Inc/
│   │   └── main.h
│   └── Src/
│       ├── main.c           # 主程序，printf → USART2 串口屏
│       ├── syscalls.c       # newlib/picolibc 系统调用 (printf 重定向)
│       └── ...
├── Hardware/
│   ├── E28/                 # E28 模块旧驱动 (已弃用，保留参考)
│   │   └── e28.c
│   ├── INTERRUPT/           # 中断处理
│   │   ├── interrupt.c      # TIM5 中断回调 + MAVLink 打包发送
│   │   └── interrupt.h
│   ├── SCREEN/              # 触摸屏驱动 (USART2)
│   │   ├── screen.c         # 屏幕数据解析 (HAL_UARTEx_RxEventCallback)
│   │   └── screen.h
│   ├── ENCODER/             # 编码器读取
│   ├── ADC_GET/             # ADC 摇杆数据采集
│   ├── INIT/                # 系统初始化
│   └── mavlink/             # MAVLink C 库 (从 XML 生成)
│       ├── mavlink_types.h
│       ├── mavlink_helpers.h
│       └── remote_control/
│           ├── mavlink.h
│           └── mavlink_msg_remote_control_state.h
├── mavlink_xml/
│   ├── remote_control.xml   # MAVLink 协议定义 (dialect XML)
│   └── index.html           # Web 串口监视器 (Chrome/Edge)
├── CMakeLists.txt
└── README.md
```

---

## 更新日志

### 2026-06 — MAVLink v2 协议迁移

| 变更 | 文件 | 说明 |
|---|---|---|
| 新增 | `mavlink_xml/remote_control.xml` | MAVLink v2 协议定义，消息 ID=150 |
| 新增 | `mavlink_xml/index.html` | 网页版 MAVLink 串口监视器 (Web Serial API) |
| 新增 | `Hardware/mavlink/` | 从 XML 生成的 MAVLink C 库 |
| 修改 | `Hardware/INTERRUPT/interrupt.c` | 新增 `MAVLink_Pack_And_Send()` 替代原 `Pack_And_Send_E28()` |
| 修改 | `Hardware/INTERRUPT/interrupt.h` | 更新依赖：`e28.h` → `mavlink.h` |
| 修改 | `Core/Src/main.c` | 新增 `__io_putchar()` 修复 GCC 下 printf 重定向 |

**数据精度优化**:
- `kfs[4][3]`: 12 字节 → 4 字节（uint32_t 位压缩，每元素 2 bit）
- `kfs_put[2][3]`: 6 字节 → 1 字节（uint8_t 位压缩，每元素 1 bit）
- `mode[4]`: 新增，1 字节（4 bit）
- 总 payload: 37 字节 → **23 字节**

**关键修复**:
- 修复 DMA 发送缓冲区栈内存生命周期问题 (改为 static/全局)
- 修复 `mavlink_message_t`(~295B) 在 ISR 栈分配导致栈溢出
- 修复 GCC/newlib 工具链 `printf` 缺少 `__io_putchar()` 实现

---

## Web 监视器使用

1. 用 Chrome/Edge 浏览器打开 `mavlink_xml/index.html`
2. 选择对应 COM 口和波特率，点击「连接串口」
3. 遥控器 `mode[0]` 拨下后，页面实时更新所有数据

---

## 构建

```bash
cd software/Romote_Controler
cmake -B build -G "Unix Makefiles"  # 或 Ninja
cmake --build build
```

工具链: `arm-none-eabi-gcc` (STM32CubeIDE / GNU Arm Embedded Toolchain)
