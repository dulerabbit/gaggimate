// Handcrafted — GaggiMate Manual Pressure Mode screen
// LVGL version: 8.3.11

#include "../ui.h"

lv_obj_t *uic_ManualBrewScreen_dials_tempText;
lv_obj_t *uic_ManualBrewScreen_dials_pressureText;
lv_obj_t *uic_ManualBrewScreen_dials_pressureTarget;
lv_obj_t *uic_ManualBrewScreen_dials_pressureGauge;
lv_obj_t *uic_ManualBrewScreen_dials_tempTarget;
lv_obj_t *uic_ManualBrewScreen_dials_tempGauge;
lv_obj_t *ui_ManualBrewScreen = NULL;
lv_obj_t *ui_ManualBrewScreen_dials = NULL;
lv_obj_t *ui_ManualBrewScreen_backBtn = NULL;
lv_obj_t *ui_ManualBrewScreen_contentPanel = NULL;
lv_obj_t *ui_ManualBrewScreen_titleLabel = NULL;
lv_obj_t *ui_ManualBrewScreen_liveLabel = NULL;
lv_obj_t *ui_ManualBrewScreen_tempReadoutLabel = NULL;
lv_obj_t *ui_ManualBrewScreen_flowReadoutLabel = NULL;
lv_obj_t *ui_ManualBrewScreen_chart = NULL;
lv_chart_series_t *ui_ManualBrewScreen_chartPressureSeries = NULL;
lv_chart_series_t *ui_ManualBrewScreen_chartFlowSeries = NULL;
lv_chart_series_t *ui_ManualBrewScreen_chartTempSeries = NULL;
lv_obj_t *ui_ManualBrewScreen_downTempBtn = NULL;
lv_obj_t *ui_ManualBrewScreen_tempLabel = NULL;
lv_obj_t *ui_ManualBrewScreen_upTempBtn = NULL;
lv_obj_t *ui_ManualBrewScreen_timerLabel = NULL;
lv_obj_t *ui_ManualBrewScreen_weightLabel = NULL;
lv_obj_t *ui_ManualBrewScreen_goBtn = NULL;

// ---- Outer ring swipe handler: right side = pressure, left side = temp ----
// Detect touches in the bezel ring band so users can swipe directly on the arcs.
static void ui_event_ManualBrewScreen_ringControl(lv_event_t *e) {
    enum { RING_ZONE_NONE = 0, RING_ZONE_PRESSURE = 1, RING_ZONE_TEMP = 2 };
    lv_event_code_t code = lv_event_get_code(e);
    lv_indev_t *indev = lv_indev_get_act();
    lv_point_t point;
    static int16_t last_y = 0;
    static int16_t accum = 0;
    static int zone = RING_ZONE_NONE;
    const int16_t pressure_pixels_per_tenth = 5;
    const int16_t temp_pixels_per_step = 16;

    if (!indev) {
        if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
            zone = RING_ZONE_NONE;
            accum = 0;
        }
        return;
    }

    if (code == LV_EVENT_PRESSED) {
        lv_indev_get_point(indev, &point);
        const int16_t center_x = lv_obj_get_width(ui_ManualBrewScreen) / 2;
        const int16_t center_y = lv_obj_get_height(ui_ManualBrewScreen) / 2;
        const int32_t dx = point.x - center_x;
        const int32_t dy = point.y - center_y;
        const int32_t r2 = dx * dx + dy * dy;
        const int32_t inner_r = 132; // wider touch ring for easier control
        const int32_t outer_r = 252; // near the edge of 480x480 screen
        const bool side_band = (abs(dx) >= 118 && dy > -210 && dy < 188);
        const int16_t split_deadzone = 10; // keep center neutral so both halves stay mirrored

        zone = RING_ZONE_NONE;
        // Exclude the very bottom strip — back button lives there at dy≈210.
        if (((r2 >= inner_r * inner_r && r2 <= outer_r * outer_r) || side_band) && dy < 188) {
            if (dx > split_deadzone) {
                zone = RING_ZONE_PRESSURE;
            } else if (dx < -split_deadzone) {
                zone = RING_ZONE_TEMP;
            }
        }

        last_y = point.y;
        accum = 0;
        return;
    }

    if (code == LV_EVENT_PRESSING && zone != RING_ZONE_NONE) {
        lv_indev_get_point(indev, &point);
        accum += last_y - point.y; // preserve original swipe-to-step behavior
        last_y = point.y;

        if (zone == RING_ZONE_PRESSURE) {
            while (accum >= pressure_pixels_per_tenth) {
                onManualPressureAdjust(e, 0.1f);
                accum -= pressure_pixels_per_tenth;
            }
            while (accum <= -pressure_pixels_per_tenth) {
                onManualPressureAdjust(e, -0.1f);
                accum += pressure_pixels_per_tenth;
            }
        } else if (zone == RING_ZONE_TEMP) {
            while (accum >= temp_pixels_per_step) {
                onManualTempRaise(e);
                accum -= temp_pixels_per_step;
            }
            while (accum <= -temp_pixels_per_step) {
                onManualTempLower(e);
                accum += temp_pixels_per_step;
            }
        }
        return;
    }

    if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        zone = RING_ZONE_NONE;
        accum = 0;
    }
}

// event functions
void ui_event_ManualBrewScreen(lv_event_t *e) {
    lv_event_code_t event_code = lv_event_get_code(e);
    // Swipe gesture intentionally removed — swiping up on ring was triggering back navigation.
    if (event_code == LV_EVENT_SCREEN_LOADED) {
        onManualScreenLoad(e);
    }
}

void ui_event_ManualBrewScreen_backBtn(lv_event_t *e) {
    lv_event_code_t event_code = lv_event_get_code(e);

    if (event_code == LV_EVENT_CLICKED) {
        onMenuClick(e);
    }
}

void ui_event_ManualBrewScreen_downTempBtn(lv_event_t *e) {
    lv_event_code_t event_code = lv_event_get_code(e);

    if (event_code == LV_EVENT_CLICKED) {
        onManualTempLower(e);
    }
}

void ui_event_ManualBrewScreen_upTempBtn(lv_event_t *e) {
    lv_event_code_t event_code = lv_event_get_code(e);

    if (event_code == LV_EVENT_CLICKED) {
        onManualTempRaise(e);
    }
}

void ui_event_ManualBrewScreen_goBtn(lv_event_t *e) {
    lv_event_code_t event_code = lv_event_get_code(e);

    if (event_code == LV_EVENT_CLICKED) {
        onManualProcessToggle(e);
    }
}

// build functions

void ui_ManualBrewScreen_screen_init(void) {
    ui_ManualBrewScreen = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_ManualBrewScreen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(ui_ManualBrewScreen, scr_unloaded_delete_cb, LV_EVENT_SCREEN_UNLOADED,
                        ui_ManualBrewScreen_screen_destroy);
    ui_object_set_themeable_style_property(ui_ManualBrewScreen, LV_PART_MAIN | LV_STATE_DEFAULT, LV_STYLE_BG_COLOR,
                                           _ui_theme_color_Dark);
    ui_object_set_themeable_style_property(ui_ManualBrewScreen, LV_PART_MAIN | LV_STATE_DEFAULT, LV_STYLE_BG_OPA,
                                           _ui_theme_alpha_Dark);

    // Background dials (arcs)
    ui_ManualBrewScreen_dials = ui_dials_create(ui_ManualBrewScreen);
    lv_obj_set_x(ui_ManualBrewScreen_dials, 0);
    lv_obj_set_y(ui_ManualBrewScreen_dials, 0);
    // Make arc children non-clickable so the parent ring handler receives touch events
    {
        uint32_t n = lv_obj_get_child_cnt(ui_ManualBrewScreen_dials);
        for (uint32_t i = 0; i < n; i++) {
            lv_obj_clear_flag(lv_obj_get_child(ui_ManualBrewScreen_dials, i), LV_OBJ_FLAG_CLICKABLE);
        }
    }

    // Back button — bottom-center of circular screen
    ui_ManualBrewScreen_backBtn = lv_imgbtn_create(ui_ManualBrewScreen);
    lv_imgbtn_set_src(ui_ManualBrewScreen_backBtn, LV_IMGBTN_STATE_RELEASED, NULL, &ui_img_295763949, NULL);
    lv_obj_set_width(ui_ManualBrewScreen_backBtn, 40);
    lv_obj_set_height(ui_ManualBrewScreen_backBtn, 40);
    lv_obj_set_x(ui_ManualBrewScreen_backBtn, 0);
    lv_obj_set_y(ui_ManualBrewScreen_backBtn, 210);
    lv_obj_set_align(ui_ManualBrewScreen_backBtn, LV_ALIGN_CENTER);
    ui_object_set_themeable_style_property(ui_ManualBrewScreen_backBtn, LV_PART_MAIN | LV_STATE_DEFAULT, LV_STYLE_IMG_RECOLOR,
                                           _ui_theme_color_NiceWhite);
    ui_object_set_themeable_style_property(ui_ManualBrewScreen_backBtn, LV_PART_MAIN | LV_STATE_DEFAULT,
                                           LV_STYLE_IMG_RECOLOR_OPA, _ui_theme_alpha_NiceWhite);

    // Content panel — transparent circular area for all widgets
    ui_ManualBrewScreen_contentPanel = lv_obj_create(ui_ManualBrewScreen);
    lv_obj_set_width(ui_ManualBrewScreen_contentPanel, 360);
    lv_obj_set_height(ui_ManualBrewScreen_contentPanel, 360);
    lv_obj_set_align(ui_ManualBrewScreen_contentPanel, LV_ALIGN_CENTER);
    lv_obj_clear_flag(ui_ManualBrewScreen_contentPanel, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_radius(ui_ManualBrewScreen_contentPanel, 180, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_ManualBrewScreen_contentPanel, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_ManualBrewScreen_contentPanel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_ManualBrewScreen_contentPanel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    // Title
    ui_ManualBrewScreen_titleLabel = lv_label_create(ui_ManualBrewScreen_contentPanel);
    lv_obj_set_width(ui_ManualBrewScreen_titleLabel, LV_SIZE_CONTENT);
    lv_obj_set_height(ui_ManualBrewScreen_titleLabel, LV_SIZE_CONTENT);
    lv_obj_set_x(ui_ManualBrewScreen_titleLabel, 0);
    lv_obj_set_y(ui_ManualBrewScreen_titleLabel, -140);
    lv_obj_set_align(ui_ManualBrewScreen_titleLabel, LV_ALIGN_CENTER);
    lv_label_set_text(ui_ManualBrewScreen_titleLabel, "Manual");
    ui_object_set_themeable_style_property(ui_ManualBrewScreen_titleLabel, LV_PART_MAIN | LV_STATE_DEFAULT,
                                           LV_STYLE_TEXT_COLOR, _ui_theme_color_NiceWhite);
    ui_object_set_themeable_style_property(ui_ManualBrewScreen_titleLabel, LV_PART_MAIN | LV_STATE_DEFAULT, LV_STYLE_TEXT_OPA,
                                           _ui_theme_alpha_NiceWhite);
    lv_obj_set_style_text_font(ui_ManualBrewScreen_titleLabel, &lv_font_montserrat_24, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_add_flag(ui_ManualBrewScreen_titleLabel, LV_OBJ_FLAG_HIDDEN);

    // Live chart — pressure (blue) and subtle temperature (amber) over time
    ui_ManualBrewScreen_chart = lv_chart_create(ui_ManualBrewScreen_contentPanel);
    lv_obj_set_size(ui_ManualBrewScreen_chart, 204, 186);
    lv_obj_set_x(ui_ManualBrewScreen_chart, 0);
    lv_obj_set_y(ui_ManualBrewScreen_chart, -68);
    lv_obj_set_align(ui_ManualBrewScreen_chart, LV_ALIGN_CENTER);
    lv_obj_clear_flag(ui_ManualBrewScreen_chart, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_chart_set_type(ui_ManualBrewScreen_chart, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(ui_ManualBrewScreen_chart, 60);
    lv_chart_set_update_mode(ui_ManualBrewScreen_chart, LV_CHART_UPDATE_MODE_SHIFT);
    lv_chart_set_range(ui_ManualBrewScreen_chart, LV_CHART_AXIS_PRIMARY_Y, 0, 120);    // 0-12 bar × 10
    lv_chart_set_range(ui_ManualBrewScreen_chart, LV_CHART_AXIS_SECONDARY_Y, 0, 50);   // 0-5 ml/s × 10
    lv_chart_set_div_line_count(ui_ManualBrewScreen_chart, 6, 5);
    lv_obj_set_style_bg_opa(ui_ManualBrewScreen_chart, 6, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_ManualBrewScreen_chart, lv_color_hex(0x0F1418), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_ManualBrewScreen_chart, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui_ManualBrewScreen_chart, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_line_opa(ui_ManualBrewScreen_chart, LV_OPA_20, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_line_color(ui_ManualBrewScreen_chart, lv_color_hex(0xC9D4DD), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_size(ui_ManualBrewScreen_chart, 0, LV_PART_INDICATOR);  // hide data-point dots
    lv_obj_set_style_line_width(ui_ManualBrewScreen_chart, 2, LV_PART_ITEMS | LV_STATE_DEFAULT);
    // Live chart series: red for temp, blue for pressure, green for flow
    ui_ManualBrewScreen_chartTempSeries =
        lv_chart_add_series(ui_ManualBrewScreen_chart, lv_color_hex(0xFF3333), LV_CHART_AXIS_PRIMARY_Y);  // red for temperature
    ui_ManualBrewScreen_chartPressureSeries =
        lv_chart_add_series(ui_ManualBrewScreen_chart, lv_color_hex(0x3366FF), LV_CHART_AXIS_PRIMARY_Y);  // blue for pressure
    ui_ManualBrewScreen_chartFlowSeries = 
        lv_chart_add_series(ui_ManualBrewScreen_chart, lv_color_hex(0x00DD88), LV_CHART_AXIS_SECONDARY_Y); // green for flow
    lv_chart_set_all_value(ui_ManualBrewScreen_chart, ui_ManualBrewScreen_chartTempSeries, LV_CHART_POINT_NONE);
    lv_chart_set_all_value(ui_ManualBrewScreen_chart, ui_ManualBrewScreen_chartPressureSeries, 0);
    lv_chart_set_all_value(ui_ManualBrewScreen_chart, ui_ManualBrewScreen_chartFlowSeries, 0);

    // Readout row: temp (left), pressure (center), flow (right)
    ui_ManualBrewScreen_liveLabel = lv_label_create(ui_ManualBrewScreen_contentPanel);
    lv_obj_set_width(ui_ManualBrewScreen_liveLabel, LV_SIZE_CONTENT);
    lv_obj_set_height(ui_ManualBrewScreen_liveLabel, LV_SIZE_CONTENT);
    lv_obj_set_x(ui_ManualBrewScreen_liveLabel, 0);
    lv_obj_set_y(ui_ManualBrewScreen_liveLabel, 66);
    lv_obj_set_align(ui_ManualBrewScreen_liveLabel, LV_ALIGN_CENTER);
    lv_label_set_text(ui_ManualBrewScreen_liveLabel, "0.0 bar");
    lv_obj_set_style_text_align(ui_ManualBrewScreen_liveLabel, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    ui_object_set_themeable_style_property(ui_ManualBrewScreen_liveLabel, LV_PART_MAIN | LV_STATE_DEFAULT, LV_STYLE_TEXT_COLOR, _ui_theme_color_NiceWhite);
    ui_object_set_themeable_style_property(ui_ManualBrewScreen_liveLabel, LV_PART_MAIN | LV_STATE_DEFAULT, LV_STYLE_TEXT_OPA, _ui_theme_alpha_NiceWhite);
    lv_obj_set_style_text_font(ui_ManualBrewScreen_liveLabel, &lv_font_montserrat_42, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_ManualBrewScreen_tempReadoutLabel = lv_label_create(ui_ManualBrewScreen_contentPanel);
    lv_obj_set_width(ui_ManualBrewScreen_tempReadoutLabel, LV_SIZE_CONTENT);
    lv_obj_set_height(ui_ManualBrewScreen_tempReadoutLabel, LV_SIZE_CONTENT);
    lv_obj_set_x(ui_ManualBrewScreen_tempReadoutLabel, -110);
    lv_obj_set_y(ui_ManualBrewScreen_tempReadoutLabel, 67);
    lv_obj_set_align(ui_ManualBrewScreen_tempReadoutLabel, LV_ALIGN_CENTER);
    lv_label_set_text(ui_ManualBrewScreen_tempReadoutLabel, "93°C");
    lv_obj_set_style_text_align(ui_ManualBrewScreen_tempReadoutLabel, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    ui_object_set_themeable_style_property(ui_ManualBrewScreen_tempReadoutLabel, LV_PART_MAIN | LV_STATE_DEFAULT, LV_STYLE_TEXT_COLOR, _ui_theme_color_NiceWhite);
    ui_object_set_themeable_style_property(ui_ManualBrewScreen_tempReadoutLabel, LV_PART_MAIN | LV_STATE_DEFAULT, LV_STYLE_TEXT_OPA, _ui_theme_alpha_NiceWhite);
    lv_obj_set_style_text_font(ui_ManualBrewScreen_tempReadoutLabel, &lv_font_montserrat_18, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_ManualBrewScreen_flowReadoutLabel = lv_label_create(ui_ManualBrewScreen_contentPanel);
    lv_obj_set_width(ui_ManualBrewScreen_flowReadoutLabel, LV_SIZE_CONTENT);
    lv_obj_set_height(ui_ManualBrewScreen_flowReadoutLabel, LV_SIZE_CONTENT);
    lv_obj_set_x(ui_ManualBrewScreen_flowReadoutLabel, 112);
    lv_obj_set_y(ui_ManualBrewScreen_flowReadoutLabel, 67);
    lv_obj_set_align(ui_ManualBrewScreen_flowReadoutLabel, LV_ALIGN_CENTER);
    lv_label_set_text(ui_ManualBrewScreen_flowReadoutLabel, "0.0 ml/s");
    lv_obj_set_style_text_align(ui_ManualBrewScreen_flowReadoutLabel, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    ui_object_set_themeable_style_property(ui_ManualBrewScreen_flowReadoutLabel, LV_PART_MAIN | LV_STATE_DEFAULT, LV_STYLE_TEXT_COLOR, _ui_theme_color_NiceWhite);
    ui_object_set_themeable_style_property(ui_ManualBrewScreen_flowReadoutLabel, LV_PART_MAIN | LV_STATE_DEFAULT, LV_STYLE_TEXT_OPA, _ui_theme_alpha_NiceWhite);
    lv_obj_set_style_text_font(ui_ManualBrewScreen_flowReadoutLabel, &lv_font_montserrat_18, LV_PART_MAIN | LV_STATE_DEFAULT);

    // Temp down button
    ui_ManualBrewScreen_downTempBtn = lv_imgbtn_create(ui_ManualBrewScreen_contentPanel);
    lv_imgbtn_set_src(ui_ManualBrewScreen_downTempBtn, LV_IMGBTN_STATE_RELEASED, NULL, &ui_img_834125362, NULL);
    lv_obj_set_width(ui_ManualBrewScreen_downTempBtn, 40);
    lv_obj_set_height(ui_ManualBrewScreen_downTempBtn, 40);
    lv_obj_set_x(ui_ManualBrewScreen_downTempBtn, -70);
    lv_obj_set_y(ui_ManualBrewScreen_downTempBtn, 96);
    lv_obj_set_align(ui_ManualBrewScreen_downTempBtn, LV_ALIGN_CENTER);
    ui_object_set_themeable_style_property(ui_ManualBrewScreen_downTempBtn, LV_PART_MAIN | LV_STATE_DEFAULT,
                                           LV_STYLE_IMG_RECOLOR, _ui_theme_color_NiceWhite);
    ui_object_set_themeable_style_property(ui_ManualBrewScreen_downTempBtn, LV_PART_MAIN | LV_STATE_DEFAULT,
                                           LV_STYLE_IMG_RECOLOR_OPA, _ui_theme_alpha_NiceWhite);

    // Temperature label
    ui_ManualBrewScreen_tempLabel = lv_label_create(ui_ManualBrewScreen_contentPanel);
    lv_obj_set_width(ui_ManualBrewScreen_tempLabel, 90);
    lv_obj_set_height(ui_ManualBrewScreen_tempLabel, LV_SIZE_CONTENT);
    lv_obj_set_x(ui_ManualBrewScreen_tempLabel, 0);
    lv_obj_set_y(ui_ManualBrewScreen_tempLabel, 96);
    lv_obj_set_align(ui_ManualBrewScreen_tempLabel, LV_ALIGN_CENTER);
    lv_label_set_text(ui_ManualBrewScreen_tempLabel, "93\xc2\xb0" "C");
    lv_obj_set_style_text_align(ui_ManualBrewScreen_tempLabel, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    ui_object_set_themeable_style_property(ui_ManualBrewScreen_tempLabel, LV_PART_MAIN | LV_STATE_DEFAULT,
                                           LV_STYLE_TEXT_COLOR, _ui_theme_color_NiceWhite);
    ui_object_set_themeable_style_property(ui_ManualBrewScreen_tempLabel, LV_PART_MAIN | LV_STATE_DEFAULT, LV_STYLE_TEXT_OPA,
                                           _ui_theme_alpha_NiceWhite);
    lv_obj_set_style_text_font(ui_ManualBrewScreen_tempLabel, &lv_font_montserrat_20, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_add_flag(ui_ManualBrewScreen_tempLabel, LV_OBJ_FLAG_HIDDEN);

    // Temp up button
    ui_ManualBrewScreen_upTempBtn = lv_imgbtn_create(ui_ManualBrewScreen_contentPanel);
    lv_imgbtn_set_src(ui_ManualBrewScreen_upTempBtn, LV_IMGBTN_STATE_RELEASED, NULL, &ui_img_390988422, NULL);
    lv_obj_set_width(ui_ManualBrewScreen_upTempBtn, 40);
    lv_obj_set_height(ui_ManualBrewScreen_upTempBtn, 40);
    lv_obj_set_x(ui_ManualBrewScreen_upTempBtn, 70);
    lv_obj_set_y(ui_ManualBrewScreen_upTempBtn, 96);
    lv_obj_set_align(ui_ManualBrewScreen_upTempBtn, LV_ALIGN_CENTER);
    ui_object_set_themeable_style_property(ui_ManualBrewScreen_upTempBtn, LV_PART_MAIN | LV_STATE_DEFAULT,
                                           LV_STYLE_IMG_RECOLOR, _ui_theme_color_NiceWhite);
    ui_object_set_themeable_style_property(ui_ManualBrewScreen_upTempBtn, LV_PART_MAIN | LV_STATE_DEFAULT,
                                           LV_STYLE_IMG_RECOLOR_OPA, _ui_theme_alpha_NiceWhite);
    lv_obj_add_flag(ui_ManualBrewScreen_downTempBtn, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_ManualBrewScreen_upTempBtn, LV_OBJ_FLAG_HIDDEN);

    // Timer label (elapsed seconds)
    ui_ManualBrewScreen_timerLabel = lv_label_create(ui_ManualBrewScreen_contentPanel);
    lv_obj_set_width(ui_ManualBrewScreen_timerLabel, LV_SIZE_CONTENT);
    lv_obj_set_height(ui_ManualBrewScreen_timerLabel, LV_SIZE_CONTENT);
    lv_obj_set_x(ui_ManualBrewScreen_timerLabel, -78);
    lv_obj_set_y(ui_ManualBrewScreen_timerLabel, 126);
    lv_obj_set_align(ui_ManualBrewScreen_timerLabel, LV_ALIGN_CENTER);
    lv_label_set_text(ui_ManualBrewScreen_timerLabel, "0s");
    ui_object_set_themeable_style_property(ui_ManualBrewScreen_timerLabel, LV_PART_MAIN | LV_STATE_DEFAULT,
                                           LV_STYLE_TEXT_COLOR, _ui_theme_color_NiceWhite);
    ui_object_set_themeable_style_property(ui_ManualBrewScreen_timerLabel, LV_PART_MAIN | LV_STATE_DEFAULT, LV_STYLE_TEXT_OPA,
                                           _ui_theme_alpha_NiceWhite);
    lv_obj_set_style_text_font(ui_ManualBrewScreen_timerLabel, &lv_font_montserrat_24, LV_PART_MAIN | LV_STATE_DEFAULT);

    // BT scale weight label
    ui_ManualBrewScreen_weightLabel = lv_label_create(ui_ManualBrewScreen_contentPanel);
    lv_obj_set_width(ui_ManualBrewScreen_weightLabel, LV_SIZE_CONTENT);
    lv_obj_set_height(ui_ManualBrewScreen_weightLabel, LV_SIZE_CONTENT);
    lv_obj_set_x(ui_ManualBrewScreen_weightLabel, 78);
    lv_obj_set_y(ui_ManualBrewScreen_weightLabel, 126);
    lv_obj_set_align(ui_ManualBrewScreen_weightLabel, LV_ALIGN_CENTER);
    lv_label_set_text(ui_ManualBrewScreen_weightLabel, "--.-g");
    ui_object_set_themeable_style_property(ui_ManualBrewScreen_weightLabel, LV_PART_MAIN | LV_STATE_DEFAULT,
                                           LV_STYLE_TEXT_COLOR, _ui_theme_color_NiceWhite);
    ui_object_set_themeable_style_property(ui_ManualBrewScreen_weightLabel, LV_PART_MAIN | LV_STATE_DEFAULT, LV_STYLE_TEXT_OPA,
                                           _ui_theme_alpha_NiceWhite);
    lv_obj_set_style_text_font(ui_ManualBrewScreen_weightLabel, &lv_font_montserrat_24, LV_PART_MAIN | LV_STATE_DEFAULT);

    // Go/Stop button — moved up to give clearance from the back button below
    ui_ManualBrewScreen_goBtn = lv_imgbtn_create(ui_ManualBrewScreen_contentPanel);
    lv_imgbtn_set_src(ui_ManualBrewScreen_goBtn, LV_IMGBTN_STATE_RELEASED, NULL, &ui_img_445946954, NULL);
    lv_obj_set_width(ui_ManualBrewScreen_goBtn, 40);
    lv_obj_set_height(ui_ManualBrewScreen_goBtn, 40);
    lv_obj_set_x(ui_ManualBrewScreen_goBtn, 0);
    lv_obj_set_y(ui_ManualBrewScreen_goBtn, 135);
    lv_obj_set_align(ui_ManualBrewScreen_goBtn, LV_ALIGN_CENTER);
    ui_object_set_themeable_style_property(ui_ManualBrewScreen_goBtn, LV_PART_MAIN | LV_STATE_DEFAULT, LV_STYLE_IMG_RECOLOR,
                                           _ui_theme_color_NiceWhite);
    ui_object_set_themeable_style_property(ui_ManualBrewScreen_goBtn, LV_PART_MAIN | LV_STATE_DEFAULT,
                                           LV_STYLE_IMG_RECOLOR_OPA, _ui_theme_alpha_NiceWhite);

    // Register event callbacks
    lv_obj_add_flag(ui_ManualBrewScreen_dials, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_PRESS_LOCK);
    lv_obj_add_event_cb(ui_ManualBrewScreen_dials, ui_event_ManualBrewScreen_ringControl, LV_EVENT_ALL, NULL);
    lv_obj_add_event_cb(ui_ManualBrewScreen_backBtn, ui_event_ManualBrewScreen_backBtn, LV_EVENT_ALL, NULL);
    lv_obj_add_event_cb(ui_ManualBrewScreen_downTempBtn, ui_event_ManualBrewScreen_downTempBtn, LV_EVENT_ALL, NULL);
    lv_obj_add_event_cb(ui_ManualBrewScreen_upTempBtn, ui_event_ManualBrewScreen_upTempBtn, LV_EVENT_ALL, NULL);
    lv_obj_add_event_cb(ui_ManualBrewScreen_goBtn, ui_event_ManualBrewScreen_goBtn, LV_EVENT_ALL, NULL);
    lv_obj_add_event_cb(ui_ManualBrewScreen, ui_event_ManualBrewScreen, LV_EVENT_ALL, NULL);

    // Cache dial child references
    uic_ManualBrewScreen_dials_tempGauge = ui_comp_get_child(ui_ManualBrewScreen_dials, UI_COMP_DIALS_TEMPGAUGE);
    uic_ManualBrewScreen_dials_tempTarget = ui_comp_get_child(ui_ManualBrewScreen_dials, UI_COMP_DIALS_TEMPTARGET);
    uic_ManualBrewScreen_dials_pressureGauge = ui_comp_get_child(ui_ManualBrewScreen_dials, UI_COMP_DIALS_PRESSUREGAUGE);
    uic_ManualBrewScreen_dials_pressureTarget = ui_comp_get_child(ui_ManualBrewScreen_dials, UI_COMP_DIALS_PRESSURETARGET);
    uic_ManualBrewScreen_dials_pressureText = ui_comp_get_child(ui_ManualBrewScreen_dials, UI_COMP_DIALS_PRESSURETEXT);
    uic_ManualBrewScreen_dials_tempText = ui_comp_get_child(ui_ManualBrewScreen_dials, UI_COMP_DIALS_TEMPTEXT);
    // Manual mode uses reverse arc direction so upward pressure progression fills upward from bottom.
    lv_arc_set_mode(uic_ManualBrewScreen_dials_pressureGauge, LV_ARC_MODE_REVERSE);
}

void ui_ManualBrewScreen_screen_destroy(void) {
    if (ui_ManualBrewScreen)
        lv_obj_del(ui_ManualBrewScreen);

    ui_ManualBrewScreen = NULL;
    ui_ManualBrewScreen_dials = NULL;
    uic_ManualBrewScreen_dials_tempGauge = NULL;
    uic_ManualBrewScreen_dials_tempTarget = NULL;
    uic_ManualBrewScreen_dials_pressureGauge = NULL;
    uic_ManualBrewScreen_dials_pressureTarget = NULL;
    uic_ManualBrewScreen_dials_pressureText = NULL;
    uic_ManualBrewScreen_dials_tempText = NULL;
    ui_ManualBrewScreen_backBtn = NULL;
    ui_ManualBrewScreen_contentPanel = NULL;
    ui_ManualBrewScreen_titleLabel = NULL;
    ui_ManualBrewScreen_chart = NULL;
    ui_ManualBrewScreen_chartPressureSeries = NULL;
    ui_ManualBrewScreen_chartFlowSeries = NULL;
    ui_ManualBrewScreen_chartTempSeries = NULL;
    ui_ManualBrewScreen_liveLabel = NULL;
    ui_ManualBrewScreen_tempReadoutLabel = NULL;
    ui_ManualBrewScreen_flowReadoutLabel = NULL;
    ui_ManualBrewScreen_downTempBtn = NULL;
    ui_ManualBrewScreen_tempLabel = NULL;
    ui_ManualBrewScreen_upTempBtn = NULL;
    ui_ManualBrewScreen_timerLabel = NULL;
    ui_ManualBrewScreen_weightLabel = NULL;
    ui_ManualBrewScreen_goBtn = NULL;
}
