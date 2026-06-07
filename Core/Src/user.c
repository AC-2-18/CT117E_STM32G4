#include "user.h"

/* ===== 时间片轮询计数器 ===== */
u32 led_tick=0;
u8 led_num=0;
u32 lcd_tick=0;
char lcd_buf[30];
u8 lcd_page=0;
u32 key_tick=0;
u32 pwm_tick=0;
u32 rx_tick=0;
u32 adc_tick=0;
u32 rtc_tick=0;

/* ===== MCP DAC ===== */
u8 mcp_value=0;

/* ===== 按键 ===== */
struct keys key[4]={0};

/* ===== PWM 输出 (PA6/PA7) ===== */
u16 PA6_F=1000;
u8 PA6_D=50;
u16 PA7_F=1000;
u8 PA7_D=50;

/* ===== 串口 DMA ===== */
char rx_buf[100];
u8 rx_flg=0;
u16 rx_size=0;
char tx_buf[100];

/* ===== EEPROM ===== */
u8 EEP_value=0;

/* ===== 输入捕获 TIM2 CH1 (PA15) ===== */
uint32_t uwIC2Value1 = 0;
uint32_t uwIC2Value2 = 0;
uint32_t uwIC2Value3 = 0;
uint32_t high_tim = 0;
uint32_t low_tim = 0;
uint16_t uhCaptureIndex = 0;
u32 PA15_Frq = 0;
u8 PA15_Duty = 0;
u8 cap_valid = 0;           // 1=捕获数据有效

/* ===== ADC (PB14/PB15) ===== */
u32 PB15_value=0;
float PB15_Volt=0;
u32 PB14_value=0;
float PB14_Volt=0;

/* ===== RTC ===== */
RTC_DateTypeDef D;
RTC_TimeTypeDef T;

/* ================================================================
 *  LED 显示
 * ================================================================ */
void led_disp(u8 led_num){
	HAL_GPIO_WritePin(GPIOD,GPIO_PIN_2,GPIO_PIN_SET);
	HAL_GPIO_WritePin(GPIOC,0xff00,GPIO_PIN_SET);
	HAL_GPIO_WritePin(GPIOC,led_num<<8,GPIO_PIN_RESET);
	HAL_GPIO_WritePin(GPIOD,GPIO_PIN_2,GPIO_PIN_RESET);
}

/* ================================================================
 *  按键扫描
 * ================================================================ */
void key_scan(void){
	key[0].key_sta = HAL_GPIO_ReadPin(GPIOB,GPIO_PIN_0);
	key[1].key_sta = HAL_GPIO_ReadPin(GPIOB,GPIO_PIN_1);
	key[2].key_sta = HAL_GPIO_ReadPin(GPIOB,GPIO_PIN_2);
	key[3].key_sta = HAL_GPIO_ReadPin(GPIOA,GPIO_PIN_0);

	for(u8 i=0;i<4;i++){
		switch(key[i].judge_flg){
			case 0:
				if(key[i].key_sta==0){
					key[i].key_tim=0;
					key[i].judge_flg=1;
				}
				break;
			case 1:
				if(key[i].key_sta==0){
					key[i].key_tim++;
					key[i].judge_flg=2;
				}
				else
					key[i].judge_flg=0;
				break;
			case 2:
				key[i].key_tim++;
				if(key[i].key_sta==1){
					if(key[i].key_tim<100)
						key[i].key_flg=1;
					key[i].judge_flg=0;
					key[i].key_tim=0;
				}
				if(key[i].key_tim>=100){
					key[i].key_lflg=1;
					if(key[i].key_sta==1){
						key[i].judge_flg=0;
						key[i].key_tim=0;
					}
				}
				break;
		}
	}
}

/* ================================================================
 *  输入捕获中断回调 - TIM2 CH1 (PA15) 测频+占空比
 *  捕获流程: 上升沿->下降沿->上升沿
 *  TIM2 为 32 位定时器, 1MHz 计数时钟 (80MHz/(79+1))
 *  溢出保护使用 0xFFFFFFFF (32位最大值)
 * ================================================================ */
void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim)
{
	if(htim==&htim2){
		if (htim->Channel == HAL_TIM_ACTIVE_CHANNEL_1)
		{
			if(uhCaptureIndex == 0)			// 第1次上升沿
			{
				uwIC2Value1 = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_1);
				__HAL_TIM_SET_CAPTUREPOLARITY(htim, TIM_CHANNEL_1,TIM_INPUTCHANNELPOLARITY_FALLING);
				uhCaptureIndex = 1;
			}
			else if(uhCaptureIndex == 1)	// 下降沿 -> 高电平时间
			{
				uwIC2Value2 = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_1);
				if (uwIC2Value2 > uwIC2Value1)
					high_tim = uwIC2Value2 - uwIC2Value1;
				else	// 32位溢出处理
					high_tim = (0xFFFFFFFF - uwIC2Value1) + uwIC2Value2 + 1;

				__HAL_TIM_SET_CAPTUREPOLARITY(htim, TIM_CHANNEL_1,TIM_INPUTCHANNELPOLARITY_RISING);
				uhCaptureIndex = 2;
			}
			else if(uhCaptureIndex == 2)	// 第2次上升沿 -> 周期
			{
				uwIC2Value3 = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_1);
				if (uwIC2Value3 > uwIC2Value2)
					low_tim = uwIC2Value3 - uwIC2Value2;
				else	// 32位溢出处理
					low_tim = (0xFFFFFFFF - uwIC2Value2) + uwIC2Value3 + 1;

				u32 period = high_tim + low_tim;
				if(period > 0){
					PA15_Frq  = 1000000 / period;      // 1MHz -> 单位Hz
					PA15_Duty = 100 * high_tim / period;
				}
				cap_valid = 1;
				uhCaptureIndex = 0;
			}
		}
	}
}

/* ================================================================
 *  EEPROM (AT24C02) - I2C 读写
 * ================================================================ */
void EEP_Write(u8 add,u8 data){
	I2CStart();
	I2CSendByte(0xA0);
	I2CWaitAck();
	I2CSendByte(add);
	I2CWaitAck();
	I2CSendByte(data);
	I2CWaitAck();
	I2CStop();
	HAL_Delay(10);
}

u8 EEP_Read(u8 add){
	u8 data=0;
	I2CStart();
	I2CSendByte(0xA0);
	I2CWaitAck();
	I2CSendByte(add);
	I2CWaitAck();

	I2CStart();
	I2CSendByte(0xA1);
	I2CWaitAck();
	data = I2CReceiveByte();
	I2CSendNotAck();
	I2CStop();
	HAL_Delay(10);
	return data;
}

/* ================================================================
 *  串口 DMA - 空闲中断接收回调
 *  接收不定长数据, 通过 Size 参数获取实际长度
 * ================================================================ */
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size){
	if(huart==&huart1){
		rx_flg=1;
		rx_size=Size;
		HAL_UARTEx_ReceiveToIdle_DMA(&huart1,(u8 *)rx_buf,100);
	}
}

/* ================================================================
 *  进程函数 - RTC
 * ================================================================ */
void rtc_proc(void){
	if(uwTick - rtc_tick<100)
		return;
	rtc_tick = uwTick;

	HAL_RTC_GetTime(&hrtc, &T, RTC_FORMAT_BIN);
	HAL_RTC_GetDate(&hrtc, &D, RTC_FORMAT_BIN);
}

/* ================================================================
 *  进程函数 - PWM 输出 (PA6: TIM16_CH1, PA7: TIM17_CH1)
 *  动态调整频率和占空比
 * ================================================================ */
void pwm_proc(void){
	if(uwTick - pwm_tick<100)
		return;
	pwm_tick = uwTick;

	__HAL_TIM_SetAutoreload(&htim16,1000000/PA6_F);
	__HAL_TIM_SetCompare(&htim16,TIM_CHANNEL_1,PA6_D*1000000/PA6_F/100);

	__HAL_TIM_SetAutoreload(&htim17,1000000/PA7_F);
	__HAL_TIM_SetCompare(&htim17,TIM_CHANNEL_1,PA7_D*1000000/PA7_F/100);
}

/* ================================================================
 *  进程函数 - ADC 采集 (PB14: ADC1, PB15: ADC2)
 * ================================================================ */
void adc_proc(void){
	if(uwTick - adc_tick<100)
		return;
	adc_tick = uwTick;

	HAL_ADC_Start(&hadc1);
	PB14_value = HAL_ADC_GetValue(&hadc1);

	HAL_ADC_Start(&hadc2);
	PB15_value = HAL_ADC_GetValue(&hadc2);

	PB15_Volt = 3.3 * PB15_value / 4095;
	PB14_Volt = 3.3 * PB14_value / 4095;
}

/* ================================================================
 *  进程函数 - 串口数据处理 (DMA 版环回)
 *  收到数据后原样发回
 * ================================================================ */
void rx_proc(void){
	if(uwTick - rx_tick<100)
		return;
	rx_tick = uwTick;

	if(rx_flg){
		rx_flg=0;
		sprintf(tx_buf,"%s",rx_buf);
		HAL_UART_Transmit_DMA(&huart1,(u8 *)tx_buf,strlen(tx_buf));
		memset(rx_buf,0,100);
	}
}

/* ================================================================
 *  进程函数 - LED 数码管
 * ================================================================ */
void led_proc(void){
	if(uwTick - led_tick<100)
		return;
	led_tick = uwTick;

	led_disp(led_num);
}

/* ================================================================
 *  进程函数 - LCD 显示
 *  显示项目信息 + GitHub 地址, 供下一届同学获取资料
 * ================================================================ */
void lcd_proc(void){
	if(uwTick - lcd_tick<100)
		return;
	lcd_tick = uwTick;

	/* Line0: 标题 */
	sprintf(lcd_buf,"  CT117E STM32G4 Board");
	LCD_DisplayStringLine(Line0, (u8*)lcd_buf);

	/* Line1-Line2: GitHub 地址 */
	sprintf(lcd_buf,"GitHub: %s",GIT_URL);
	LCD_DisplayStringLine(Line1, (u8*)lcd_buf);

	/* Line3: 分隔线 */
	sprintf(lcd_buf,"====================");
	LCD_DisplayStringLine(Line3, (u8*)lcd_buf);

	/* Line4: ADC */
	sprintf(lcd_buf,"ADC PB14:%.2fV PB15:%.2fV",PB14_Volt,PB15_Volt);
	LCD_DisplayStringLine(Line4, (u8*)lcd_buf);

	/* Line5: 输入捕获 */
	sprintf(lcd_buf,"CAP:%dHz %d%%",PA15_Frq,PA15_Duty);
	LCD_DisplayStringLine(Line5, (u8*)lcd_buf);

	/* Line6: PWM */
	sprintf(lcd_buf,"PWM PA6:%dHz PA7:%dHz",PA6_F,PA7_F);
	LCD_DisplayStringLine(Line6, (u8*)lcd_buf);

	/* Line7: RTC */
	sprintf(lcd_buf,"RTC:%02d:%02d:%02d",T.Hours,T.Minutes,T.Seconds);
	LCD_DisplayStringLine(Line7, (u8*)lcd_buf);

	/* Line8: 分隔线 */
	sprintf(lcd_buf,"====================");
	LCD_DisplayStringLine(Line8, (u8*)lcd_buf);
}

/* ================================================================
 *  进程函数 - 按键处理
 *  短按 B0/B1/B2 -> EEPROM 写入不同值
 *  长按 B0/B1   -> MCP DAC 加减
 * ================================================================ */
void key_proc(void){
	if(uwTick - key_tick<10)
		return;
	key_tick = uwTick;

	key_scan();
	for(u8 i=0;i<4;i++){
		if(key[i].key_flg==1){
			key[i].key_flg=0;
			switch(i){
				case 0:{ EEP_Write(14,120); }break;
				case 1:{ EEP_Write(14,130); }break;
				case 2:{ EEP_Write(14,10);  }break;
				case 3:{ /* 保留 */ }break;
			}
		}
		if(key[i].key_lflg==1){
			key[i].key_lflg=0;
			switch(i){
				case 0:{
					if(mcp_value<127){
						mcp_value+=1;
						mcp_write(mcp_value);
					}
				}break;
				case 1:{
					if(mcp_value>0){
						mcp_value-=1;
						mcp_write(mcp_value);
					}
				}break;
				case 2:{ /* 保留 */ }break;
				case 3:{ /* 保留 */ }break;
			}
		}
	}
}
