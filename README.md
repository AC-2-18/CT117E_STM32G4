# CT117E STM32G4 蓝桥杯嵌入式竞赛训练项目

## 项目说明

本项目基于 **STM32G431RB** 微控制器，运行于 **CT117E 嵌入式竞赛板**（蓝桥杯官方板），
涵盖比赛常见外设模块的驱动与演示代码。

## 硬件平台

| 组件 | 说明 |
|------|------|
| MCU | STM32G431RB (Cortex-M4, 170MHz) |
| 开发板 | CT117E 嵌入式竞赛板 |
| IDE | Keil MDK-ARM (uvprojx) |
| CubeMX | STM32CubeMX (1.ioc) |

## 外设模块

- **GPIO** — LED 数码管 + 4 个独立按键
- **USART1** — 串口 DMA (空闲中断) 收发
- **ADC1/ADC2** — 双通道模拟采集 PB14/PB15
- **TIM2** — 输入捕获 PA15 (测频+占空比)
- **TIM16/TIM17** — PWM 输出 PA6/PA7
- **RTC** — 实时时钟 (LSI)
- **I2C (GPIO模拟)** — EEPROM (AT24C02) + MCP47xx DAC
- **LCD** — 320x240 TFT 液晶屏

## 引脚映射

| 引脚 | 功能 |
|------|------|
| PB0/PB1/PB2 | 按键 K1/K2/K3 |
| PA0 | 按键 K4 (WKUP) |
| PB14/PB15 | ADC 输入 |
| PA6/PA7 | PWM 输出 |
| PA15 | 输入捕获 |
| PB6/PB7 | I2C (SCL/SDA) |
| PA9/PA10 | USART1 (TX/RX) |

## 快速开始

1. 用 Keil MDK-ARM 打开 `MDK-ARM\1.uvprojx`
2. 编译并下载到 CT117E 开发板
3. LCD 将显示项目信息和当前各模块状态

## GitHub

本仓库地址由 LCD 显示，修改 `Core/Inc/user.h` 中的 `GIT_URL` 宏即可更新。
