#ifndef __USER_H
#define __USER_H

#include "main.h"
#include "adc.h"
#include "i2c_hal.h"
#include "lcd.h"
#include "rtc.h"
#include "stdio.h"
#include "string.h"
#include "tim.h"
#include "usart.h"

/* ===== 项目信息 ===== */
#define GIT_URL  "github.com/AC-2-18/CT117E_STM32G4"

/* ===== 按键结构 ===== */
struct keys{
	u8 key_sta;      // 当前电平
	u8 judge_flg;    // 0:空闲 1:消抖 2:按下确认
	u8 key_flg;      // 短按标志
	u8 key_lflg;     // 长按标志
	u32 key_tim;     // 按下时间计数
};

/* ===== 驱动函数 ===== */
void led_disp(u8 led_num);
void key_scan(void);
void EEP_Write(u8 add,u8 data);
u8 EEP_Read(u8 add);

/* ===== 进程函数 ===== */
void pwm_proc(void);
void adc_proc(void);
void rx_proc(void);
void rtc_proc(void);
void led_proc(void);
void lcd_proc(void);
void key_proc(void);

/* ===== 全局变量 ===== */
extern char tx_buf[100];
extern char rx_buf[100];
extern u8 EEP_value;
extern u8 cap_valid;    // 输入捕获有效标志

#endif
