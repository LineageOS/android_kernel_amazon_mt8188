#ifndef _BATTERY_METRICS_H
#define _BATTERY_METRICS_H
enum {
	METRICS_FAULT_NONE = 0,
	METRICS_FAULT_VBUS_OVP = 1,
	METRICS_FAULT_VBAT_OVP = 2,
	METRICS_FAULT_SAFETY_TIMEOUT = 3,
};

enum {
	SCREEN_OFF,
	SCREEN_ON,
};

struct screen_state {
	struct timespec64 screen_on_time;
	struct timespec64 screen_off_time;
	int screen_on_soc;
	int screen_off_soc;
	int screen_state;
};

struct pm_state {
	struct timespec64 suspend_ts;
	struct timespec64 resume_ts;
	int suspend_soc;
	int resume_soc;
	int suspend_bat_car;
	int resume_bat_car;
};

struct iavg_data {
	struct timespec64 last_ts;
	int pre_bat_car;
	int pre_screen_state;
};

struct bat_metrics_data {
	bool is_top_off_mode;
	bool metrics_psy_ready;
	u8 fault_type_old;
	u32 chg_sts_old;
	unsigned int bcm_flag_old;
	int aging_factor_old;
	int bat_cycle_old;
	int qmax_old;
	struct mtk_gauge *gauge;
	struct mtk_battery *gm;
	struct iavg_data iavg;
	struct screen_state screen;
	struct pm_state pm;
	struct delayed_work dwork;
#if IS_ENABLED(CONFIG_DRM_MEDIATEK)
	struct notifier_block pm_notifier;
#endif
};

#if IS_ENABLED(CONFIG_AMAZON_MINERVA_METRICS_LOG) || IS_ENABLED(CONFIG_AMAZON_METRICS_LOG)
int bat_metrics_chrdet(u32 chr_type);
int bat_metrics_chg_fault(u8 fault_type);
int bat_metrics_chg_state(u32 chg_sts);
int bat_metrics_critical_shutdown(void);
int bat_metrics_top_off_mode(bool is_on);
int bat_metrics_aging(int aging_factor, int bat_cycle, int qmax);
int bat_metrics_invalid_charger(void);
int bat_metrics_dpm_eoc_mode(int vcharge);
int bat_metrics_init(void);
int bat_metrics_adapter_power(u32 type, u32 aicl_ma);
int bat_metrics_suspend(void);
int bat_metrics_resume(void);
int bat_metrics_bcm_flag(unsigned int bcm_flag);
void bat_metrics_uninit(void);
#else
static inline void bat_metrics_chrdet(u32 chr_type) {}
static inline void bat_metrics_chg_fault(u8 fault_type) {}
static inline void bat_metrics_chg_state(u32 chg_sts) {}
static inline void bat_metrics_critical_shutdown(void) {}
static inline void bat_metrics_top_off_mode(bool is_on) {}
static inline void bat_metrics_aging(int aging_factor, int bat_cycle, int qmax) {}
static inline void bat_metrics_invalid_charger(void) {}
static inline void bat_metrics_dpm_eoc_mode(int vcharge) {}
static inline void bat_metrics_init(void) {}
static inline void bat_metrics_adapter_power(u32 type, u32 aicl_ma) {}
static inline void bat_metrics_suspend(void) {}
static inline void bat_metrics_resume(void) {}
static inline void bat_metrics_bcm_flag(unsigned int bcm_flag) {}
static inline void bat_metrics_uninit(void) {}
#endif
#endif /* _BATTERY_METRICS_H */
