
#ifndef _INCLUDE_ADD_FUNC_H_
#define _INCLUDE_ADD_FUNC_H_

#include "user_config.h"
#include "ets_sys.h"
//(*(uint32*)0x3fffc704) cpu_frequency
//(*(uint32*)0x3fffc708) wdt_mode

#if SDK_VERSION > 999 // SDK > 0.9.6 b1
//uint32 system_adc_read(void); // user_interface.h
//void system_deep_sleep(uint32 time_in_us); // user_interface.h
//bool system_deep_sleep_set_option(uint8 option); // user_interface.h
//uint32 system_get_boot_mode(void); // user_interface.h
//uint32 system_get_boot_version(void); // user_interface.h
uint32 system_get_checksum(uint8 *ptr, uint32 len);
//uint32 system_get_chip_id(void); // user_interface.h
//uint32 system_get_cpu_freq(void); // user_interface.h // ets_get_cpu_frequency
//uint32 system_get_free_heap_size(void); // user_interface.h
bool system_get_os_print(void); // return os_print_enable
uint32 system_get_rtc_time(void); // return (uint32)(*((uint32*)0x6000071C))
//const uint8 *system_get_sdk_version(void); // user_interface.h
uint32 system_get_test_result(void);
//uint32 system_get_time(void); // x1 ms, return WdevTimOffSet + (uint32)(*((uint32*)0x3FF20C00)) // user_interface.h
uint32 phy_get_mactime(void); // x1 ms, return (uint32)(*((uint32*)0x3FF20C00))
uint32 system_get_userbin_addr(void);
//uint32 system_get_vdd33(void); // user_interface.h
//void system_init_done_cb(init_done_cb_t cb); typedef void (* init_done_cb_t)(void); // user_interface.h
//uint64 system_mktime(uint32 year, uint32 mon, uint32 day, uint32 hour, uint32 min, uint32 sec); // user_interface.h
//bool system_os_post(uint8 prio, os_signal_t sig, os_param_t par); // user_interface.h
//bool system_os_task(os_task_t task, uint8 prio, os_event_t *queue, uint8 qlen); // user_interface.h
bool system_overclock(void); // if(system_get_cpu_freq()==80) { cpu_overclock = 1, system_update_cpu_freq(160) }
//?system_param_error();
uint32 system_phy_temperature_alert(void);   // phy_get_check_flag(0);
//?system_pp_recycle_rx_pkt(); // ppRecycleRxPkt()-> lldesc_num2link() wDev_AppendRxBlocks() esf_buf_recycle()
//void system_print_meminfo(void); // user_interface.h
uint32 system_relative_time(uint32 x); // (*((uint32*)0x3FF20C00))- x
//void system_restart(void); // user_interface.h
//void system_restart_enhance(uint32 *bin); // system_get_boot_version() "failed: need boot >= 1.3\n" system_get_userbin_addr(), "restart to use user bin", wifi_param_save_protect(), system_restart()
//void system_restore(void); // user_interface.h
bool system_restoreclock(void); // if(cpu_overclock) system_update_cpu_freq(80) else return 0
//uint32 system_rtc_clock_cali_proc(void); // user_interface.h
//bool system_rtc_mem_read(uint8 src_addr, void *des_addr, uint16 load_size); // user_interface.h
//bool system_rtc_mem_write(uint8 des_addr, const void *src_addr, uint16 save_size); // user_interface.h
//void system_set_os_print(uint8 onoff); // user_interface.h
//?system_station_got_ip_set(a,b,c); // print"ip:%d.%d.%d.%d,mask:%d.%d.%d.%d,gw:%d.%", ets_timer_disarm(sta_con_timer)
//void system_timer_reinit(void); // user_interface.h // ������������� �������� ������� �� :16 ������ :256 � �.�.
//void system_uart_swap(void); // user_interface.h
//bool system_update_cpu_freq(uint32 cpu_freq); // ets_update_cpu_frequency + bit0 0x3FF00014 // user_interface.h
//uint8 system_upgrade_flag_check(); // user_interface.h
//void system_upgrade_flag_set(uint8 flag); // user_interface.h
//void system_upgrade_reboot(void); // user_interface.h
//uint8 system_upgrade_userbin_check(void); // user_interface.h
bool system_upgrade_userbin_set(uint32 flag); // system_get_boot_version(), store flags
#endif

int atoi(const char *str) ICACHE_FLASH_ATTR;

int os_printf_plus(const char *format, ...) ICACHE_FLASH_ATTR;
int ets_sprintf(char *str, const char *format, ...) ICACHE_FLASH_ATTR;

void wifi_softap_set_default_ssid(void) ICACHE_FLASH_ATTR;
void wDev_Set_Beacon_Int(uint32_t) ICACHE_FLASH_ATTR;
extern void wDev_ProcessFiq(void) ICACHE_FLASH_ATTR;
void ets_timer_arm_new(ETSTimer *ptimer, uint32_t milliseconds, int repeat_flag, int isMstimer) ICACHE_FLASH_ATTR;
void sleep_reset_analog_rtcreg_8266(void) ICACHE_FLASH_ATTR;
void wifi_softap_cacl_mac(uint8 *mac_out, uint8 *mac_in) ICACHE_FLASH_ATTR;
void user_init(void);
int wifi_mode_set(int mode) ICACHE_FLASH_ATTR;
int wifi_station_start(void) ICACHE_FLASH_ATTR;
int wifi_softap_start(void) ICACHE_FLASH_ATTR;
int register_chipv6_phy(uint8 * esp_init_data) ICACHE_FLASH_ATTR; // esp_init_data_default[128]
void ieee80211_phy_init(int phy_mode) ICACHE_FLASH_ATTR; // ieee80211_setup_ratetable()
void lmacInit(void) ICACHE_FLASH_ATTR;
void wDev_Initialize(uint8 * mac) ICACHE_FLASH_ATTR;
void pp_attach(void) ICACHE_FLASH_ATTR;
void ieee80211_ifattach(void *_g_ic) ICACHE_FLASH_ATTR; // g_ic in main\Include\libmain.h
void pm_attach(void) ICACHE_FLASH_ATTR;
void cnx_attach(void *_g_ic) ICACHE_FLASH_ATTR; // g_ic in main\Include\libmain.h
void wDevEnableRx(void) ICACHE_FLASH_ATTR; // io(0x3FF20004) |= 0x80000000;

void *pvPortMalloc(size_t xWantedSize) ICACHE_FLASH_ATTR;
void *pvPortRealloc(void * rmem, size_t newsize);
void vPortFree(void *pv) ICACHE_FLASH_ATTR;
size_t xPortGetFreeHeapSize(void) ICACHE_FLASH_ATTR;
void vPortInitialiseBlocks(void) ICACHE_FLASH_ATTR;
void *pvPortZalloc(size_t size) ICACHE_FLASH_ATTR;
uint32 readvdd33(void) ICACHE_FLASH_ATTR;
int get_noisefloor_sat(void) ICACHE_FLASH_ATTR;
int read_hw_noisefloor(void) ICACHE_FLASH_ATTR;
int ram_get_fm_sar_dout(int) ICACHE_FLASH_ATTR;
// noise_init(), rom_get_noisefloor(), ram_set_noise_floor(), noise_check_loop(), ram_start_noisefloor()
// void sys_check_timeouts(void *timer_arg) ICACHE_FLASH_ATTR; // lwip
uint32 system_get_checksum(uint8 *ptr, uint32 len) ICACHE_FLASH_ATTR;


#endif //_INCLUDE_ADD_FUNC_H_




