// Handcrafted — GaggiMate Manual Pressure Mode screen
// LVGL version: 8.3.11

#ifndef UI_MANUALBREWSCREEN_H
#define UI_MANUALBREWSCREEN_H

#ifdef __cplusplus
extern "C" {
#endif

// SCREEN: ui_ManualBrewScreen
extern void ui_ManualBrewScreen_screen_init(void);
extern void ui_ManualBrewScreen_screen_destroy(void);
extern void ui_event_ManualBrewScreen(lv_event_t *e);
extern lv_obj_t *ui_ManualBrewScreen;
extern lv_obj_t *ui_ManualBrewScreen_dials;
extern void ui_event_ManualBrewScreen_backBtn(lv_event_t *e);
extern lv_obj_t *ui_ManualBrewScreen_backBtn;
extern lv_obj_t *ui_ManualBrewScreen_contentPanel;
extern lv_obj_t *ui_ManualBrewScreen_titleLabel;
extern lv_obj_t *ui_ManualBrewScreen_chart;
extern lv_chart_series_t *ui_ManualBrewScreen_chartPressureSeries;
extern lv_chart_series_t *ui_ManualBrewScreen_chartFlowSeries;
extern lv_chart_series_t *ui_ManualBrewScreen_chartTempSeries;
extern lv_obj_t *ui_ManualBrewScreen_liveLabel;
extern lv_obj_t *ui_ManualBrewScreen_tempReadoutLabel;
extern lv_obj_t *ui_ManualBrewScreen_flowReadoutLabel;
extern void ui_event_ManualBrewScreen_downTempBtn(lv_event_t *e);
extern lv_obj_t *ui_ManualBrewScreen_downTempBtn;
extern lv_obj_t *ui_ManualBrewScreen_tempLabel;
extern void ui_event_ManualBrewScreen_upTempBtn(lv_event_t *e);
extern lv_obj_t *ui_ManualBrewScreen_upTempBtn;
extern lv_obj_t *ui_ManualBrewScreen_timerLabel;
extern lv_obj_t *ui_ManualBrewScreen_weightLabel;
extern void ui_event_ManualBrewScreen_goBtn(lv_event_t *e);
extern lv_obj_t *ui_ManualBrewScreen_goBtn;
// CUSTOM VARIABLES
extern lv_obj_t *uic_ManualBrewScreen_dials_tempGauge;
extern lv_obj_t *uic_ManualBrewScreen_dials_tempTarget;
extern lv_obj_t *uic_ManualBrewScreen_dials_pressureGauge;
extern lv_obj_t *uic_ManualBrewScreen_dials_pressureTarget;
extern lv_obj_t *uic_ManualBrewScreen_dials_pressureText;
extern lv_obj_t *uic_ManualBrewScreen_dials_tempText;

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif
