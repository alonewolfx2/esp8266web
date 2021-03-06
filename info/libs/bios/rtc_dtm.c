/******************************************************************************
 * FileName: SpiFlash.c
 * Description: mem funcs in ROM-BIOS
 * Alternate SDK ver 0.0.0 (b0)
 * Author: PV`
 * (c) PV` 2015
*******************************************************************************/
#include "c_types.h"
#include "hw/esp8266.h"
#include "bios/rtc_dtm.h"

void software_reset(void)
{
	rtc_[0] |= 1 << 31; // IOREG(0x60000700) |= 0x80000000;
}

void rtc_set_sleep_mode(uint32 a, uint32 t, uint32 m)
{

	IO_RTC_SLP_VAL = IO_RTC_SLP_CNT_VAL + t * 100000; // 0x6000071C - 0x60000704
	rtc_[6] = m; // 0x60000718
	rtc_[2] |= a; // 0x60000708
}

uint32 rtc_get_reset_reason(void)
{
	uint32 x = rtc_[5] & 7; // IOREG(0x60000714) & 7;
	if(x == 5) {
		return x;
	}
	else {
		x = (rtc_[6]>>8) & 0x3F; // (IOREG(0x60000718)>>8) & ((1<<6)-1);
		if(x == 1) x = 6;
		else if(x == 8) x = 0;
	}
	rtc_[2] &= ~(1<<21); // IOREG(0x60000708) &= ~(1<<21);
	return x;
}

// RAM_BIOS:3FFFDD64
extern dtm_params dtm_params;

//ROM:400027A4
void save_rxbcn_mactime(uint32 t)
{
	dtm_params + 0x18 = t;
}

// ROM:400027AC
void save_tsf_us(uint32 us)
{
	dtm_params + 0x1C = us;
}

// ROM:400026C8
void dtm_set_intr_mask(uint32 mask)
{
	dtm_params + 0x38 = mask;
}

// ROM:400026D0
uint32 dtm_get_intr_mask(void)
{
	return dtm_params + 0x38;
}

// ROM:4000269C
void dtm_params_init(void * sleep_func, void * int_func)
{
	ets_memset(&dtm_params, 0, 68);
	dtm_params + 0x3C = sleep_func;
	dtm_params + 0x40 = int_func;
}
// ROM:400026DC
void dtm_set_params(int a2, int time_ms_a3, int a4, int a5, int a6)
{
	dtm_params + 0x30 = a2;
	dtm_params + 0x2c = a4;
	dtm_params + 0x28 = time_ms_a3*1000;
	dtm_params + 0x14 = a6;
	if(a5+1) a5 = 0;
	dtm_params + 0x34 = a5;
	if(a2&1) {
		__floatunsidf(__floatsidf(rand()))
		..
		dtm_params + 0x20 = ?;
	}
	else dtm_params + 0x20 = time_ms_a3*1000;

	if(a2&2) {
		__floatunsidf(__floatsidf(rand()))
		..
		dtm_params + 0x24 = ?;
	}
	else {
		dtm_params + 0x24 = a4;
	}
}

// ROM:400027D4
void loc_400027D4()
{
	if(dtm_params + 0x40 != NULL) dtm_params + 0x40();
	if(dtm_params + 0x38) ets_isr_unmask(dtm_params + 0x28);
	ets_timer_disarm(&dtm_params);
	ets_timer_setfn(&dtm_params, ets_enter_sleep, NULL);
	x = dtm_params + 0x2C;
	if((dtm_params + 0x30)&2) {__floatunsidf(__floatsidf(rand())) ... }
	dtm_params + 0x24 = x;
	ets_timer_arm(&dtm_params, x, 0);
	if((dtm_params + 0x34) >= 2) {
		(dtm_params + 0x34)--;
	}
}

// ROM:400029EC
void rtc_intr_handler(void)
{
	uint32 x = IO_RTC_INT_ST & 7; // IOREG(0x60000728) & 7;
	ets_set_idle_cb(NULL, NULL);
	IO_RTC_INT_CLR |= x; // IOREG(0x60000724) |= x;
	IO_RTC_INT_ENA &= 0x78; // IOREG(0x60000720) &= 0x78;
	loc_400027D4();
}

// ROM:40002A40
void ets_rtc_int_register(void)
{
	IO_RTC_INT_ENA &= 0xFF8; // IOREG(0x60000720)
	ets_isr_attach(3, rtc_intr_handler, 0);
	IO_RTC_INT_CLR |= 7; // OREG(0x60000724)
	ets_isr_unmask(1<<3);
}


void rtc_enter_sleep(void)
{
	rtc_[4] = 0; // IOREG(0x60000710) = 0;
	RTC_CALIB_SYNC = 9;
	RTC_CALIB_SYNC |= 1<<31;
	uint32 x = dtm_params + 0x38;
	if(x) ets_isr_mask(x);
	if(gpio_input_get() & 2) gpio_pin_wakeup_enable(2, 4);
	else gpio_pin_wakeup_enable(2, 5);
	rtc_[6]  = 0x18; // IOREG(0x60000718)
	RTC_GPIO5_CFG  = 1; //	IOREG(0x600007A8) = 0x1;
	while((RTC_CALIB_VALUE & 0x1F)==0) // IOREG(0x60000370)
	rtc_claib = RTC_CALIB_VALUE & 0xFFFFF;
	if(dtm_params + 0x3C) dtm_params + 0x3C ();
	x = dtm_params + 0x28;
	if((dtm_params + 0x30)&1) {
		rand() ....
		x = ?
	}
	dtm_params + 0x20 = x;
	x -= dtm_params + 0x1C;
	x -= ((dtm_params + 0x14)<<3)<< 1;
	x += x << 2;
	x <<= 6;
	x -= 76800;
	dtm_params + 0x44 += 1;
	IO_RTC_SLP_VAL = IO_RTC_SLP_CNT_VAL + __udivsi3(x, rtc_claib) + 3791; // 0x6000071C
	IO_RTC_INT_CLR |= 3; // 0x60000724
	IO_RTC_INT_ST |= 3; // 0x60000720
	rtc_[2] |= 1 << 20; // 0x60000708
}

void ets_enter_sleep(void)
{
	ets_set_idle_cb(rtc_enter_sleep, NULL);
}

