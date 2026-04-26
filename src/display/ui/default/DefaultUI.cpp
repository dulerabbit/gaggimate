#include "DefaultUI.h"

#include <WiFi.h>
#include <display/core/Controller.h>
#include <display/core/process/BrewProcess.h>
#include <display/core/process/Process.h>
#include <display/core/zones.h>
#include <display/drivers/common/LV_Helper.h>
#include <display/main.h>
#include <display/ui/default/lvgl/ui_theme_manager.h>
#include <display/ui/default/lvgl/ui_themes.h>
#include <display/ui/utils/effects.h>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <unordered_map>
#include <vector>

#include "esp_sntp.h"

static EffectManager effect_mgr;
static std::unordered_map<lv_obj_t *, const void *> baseIconSources;
static std::unordered_map<lv_obj_t *, lv_obj_t *> imgbtnGlowOverlays;
struct DialSegmentRings {
    std::vector<lv_obj_t *> temp;
    std::vector<lv_obj_t *> pressure;
};
static std::unordered_map<lv_obj_t *, DialSegmentRings> dialSegmentCache;
static constexpr unsigned long ADJUSTMENT_PULSE_MS = 130;
static constexpr unsigned long GAUGE_ADJUSTMENT_PULSE_MS = 420;
static constexpr unsigned long ADJUSTMENT_PULSE_RERENDER_MS = 35;

static lv_color_t lerpColor(const lv_color_t &a, const lv_color_t &b, float t) {
    t = std::max(0.0f, std::min(1.0f, t));
    const uint8_t r = static_cast<uint8_t>(a.ch.red + (b.ch.red - a.ch.red) * t);
    const uint8_t g = static_cast<uint8_t>(a.ch.green + (b.ch.green - a.ch.green) * t);
    const uint8_t bl = static_cast<uint8_t>(a.ch.blue + (b.ch.blue - a.ch.blue) * t);
    return lv_color_make(r, g, bl);
}

static int16_t normAngle(float deg) {
    while (deg >= 360.0f) {
        deg -= 360.0f;
    }
    while (deg < 0.0f) {
        deg += 360.0f;
    }
    return static_cast<int16_t>(std::round(deg));
}

static lv_obj_t *createDialSegment(lv_obj_t *parent, float startDeg, float endDeg, lv_color_t color) {
    lv_obj_t *seg = lv_arc_create(parent);
    lv_obj_set_size(seg, 480, 480);
    lv_obj_align(seg, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_state(seg, LV_STATE_DISABLED);
    lv_obj_clear_flag(seg, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

    lv_arc_set_range(seg, 0, 100);
    lv_arc_set_value(seg, 100);
    lv_arc_set_bg_angles(seg, normAngle(startDeg), normAngle(endDeg));

    lv_obj_set_style_arc_width(seg, 20, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_arc_rounded(seg, false, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_arc_color(seg, color, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_arc_opa(seg, LV_OPA_COVER, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_arc_img_src(seg, nullptr, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(seg, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_spread(seg, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_opa(seg, LV_OPA_0, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_set_style_arc_width(seg, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_arc_opa(seg, LV_OPA_0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_arc_img_src(seg, nullptr, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(seg, LV_OPA_0, LV_PART_KNOB | LV_STATE_DEFAULT);

    return seg;
}

static void pruneIconGlowState() {
    for (auto it = baseIconSources.begin(); it != baseIconSources.end();) {
        if (!it->first || !lv_obj_is_valid(it->first)) {
            it = baseIconSources.erase(it);
        } else {
            ++it;
        }
    }

    for (auto it = imgbtnGlowOverlays.begin(); it != imgbtnGlowOverlays.end();) {
        const bool buttonInvalid = !it->first || !lv_obj_is_valid(it->first);
        const bool overlayInvalid = !it->second || !lv_obj_is_valid(it->second);
        if (buttonInvalid || overlayInvalid) {
            it = imgbtnGlowOverlays.erase(it);
        } else {
            ++it;
        }
    }
}

static const void *getIconSource(lv_obj_t *btn) {
    if (!btn) {
        return nullptr;
    }

    if (lv_obj_check_type(btn, &lv_imgbtn_class)) {
        const void *src = lv_imgbtn_get_src_middle(btn, LV_IMGBTN_STATE_RELEASED);
        if (!src) {
            src = lv_imgbtn_get_src_middle(btn, LV_IMGBTN_STATE_CHECKED_RELEASED);
        }
        if (!src) {
            src = lv_imgbtn_get_src_middle(btn, LV_IMGBTN_STATE_PRESSED);
        }
        return src;
    }

    if (lv_obj_check_type(btn, &lv_img_class)) {
        return lv_img_get_src(btn);
    }

    return lv_obj_get_style_bg_img_src(btn, LV_PART_MAIN | LV_STATE_DEFAULT);
}

static const void *getGlowVariant(const void *baseSrc) {
    if (baseSrc == &ui_img_445946954)
        return &ui_img_glow_play_40x40;
    if (baseSrc == &ui_img_340148213)
        return &ui_img_glow_settings_40x40;
    if (baseSrc == &ui_img_332059803)
        return &ui_img_glow_dropdown_bar_40x40;
    if (baseSrc == &ui_img_979979123)
        return &ui_img_glow_mug_hot_alt_80x80;
    if (baseSrc == &ui_img_783005998)
        return &ui_img_glow_wind_80x80;
    if (baseSrc == &ui_img_545340440)
        return &ui_img_glow_raindrops_80x80;
    if (baseSrc == &ui_img_363557387)
        return &ui_img_glow_coffee_bean_80x80;
    if (baseSrc == &ui_img_631115820)
        return &ui_img_glow_check_40x40;
    if (baseSrc == &ui_img_1456692430)
        return &ui_img_glow_pause_40x40;
    if (baseSrc == &ui_img_1594943393)
        return &ui_img_glow_disk_30x30;
    if (baseSrc == &ui_img_1464184441)
        return &ui_img_glow_floppy_disks_30x30;
    if (baseSrc == &ui_img_390988422)
        return &ui_img_glow_plus_small_40x40;
    if (baseSrc == &ui_img_834125362)
        return &ui_img_glow_minus_small_40x40;
    if (baseSrc == &ui_img_1220767159)
        return &ui_img_glow_tachometer_fast_40x40;
    if (baseSrc == &ui_img_manual_pressure_80x80)
        return &ui_img_glow_manual_pressure_80x80;
    return nullptr;
}

static void setButtonIconSource(lv_obj_t *btn, const void *src) {
    if (!btn || !src) {
        return;
    }
    if (lv_obj_check_type(btn, &lv_imgbtn_class)) {
        lv_imgbtn_set_src(btn, LV_IMGBTN_STATE_RELEASED, nullptr, src, nullptr);
        lv_imgbtn_set_src(btn, LV_IMGBTN_STATE_PRESSED, nullptr, src, nullptr);
        lv_imgbtn_set_src(btn, LV_IMGBTN_STATE_CHECKED_RELEASED, nullptr, src, nullptr);
        lv_imgbtn_set_src(btn, LV_IMGBTN_STATE_CHECKED_PRESSED, nullptr, src, nullptr);
    } else if (lv_obj_check_type(btn, &lv_img_class)) {
        lv_img_set_src(btn, src);
    } else {
        lv_obj_set_style_bg_img_src(btn, src, LV_PART_MAIN | LV_STATE_DEFAULT);
    }
}

static lv_obj_t *ensureImgbtnGlowOverlay(lv_obj_t *btn) {
    if (!btn || !lv_obj_check_type(btn, &lv_imgbtn_class)) {
        return nullptr;
    }

    auto it = imgbtnGlowOverlays.find(btn);
    if (it != imgbtnGlowOverlays.end() && it->second && lv_obj_is_valid(it->second)) {
        return it->second;
    }

    lv_obj_t *parent = lv_obj_get_parent(btn);
    if (!parent) {
        return nullptr;
    }

    lv_obj_add_flag(parent, LV_OBJ_FLAG_OVERFLOW_VISIBLE);

    lv_obj_t *overlay = lv_img_create(parent);
    lv_obj_add_flag(overlay, LV_OBJ_FLAG_IGNORE_LAYOUT);
    lv_obj_add_flag(overlay, LV_OBJ_FLAG_FLOATING);
    lv_obj_add_flag(overlay, LV_OBJ_FLAG_ADV_HITTEST);
    lv_obj_clear_flag(overlay, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(overlay, LV_OBJ_FLAG_HIDDEN);
    imgbtnGlowOverlays[btn] = overlay;
    return overlay;
}

static void setIconFocusStyle(lv_obj_t *btn, bool focused, bool emphasize = false, bool iconEdgeGlow = false) {
    if (!btn) {
        return;
    }

    pruneIconGlowState();

    const bool isImgbtn = lv_obj_check_type(btn, &lv_imgbtn_class);

    // Imgbtn icons can change source dynamically (e.g., play/pause/wind), so always refresh base.
    if (isImgbtn) {
        const void *liveBase = getIconSource(btn);
        if (liveBase) {
            baseIconSources[btn] = liveBase;
        }
    }

    if (!isImgbtn && baseIconSources.find(btn) == baseIconSources.end()) {
        const void *base = getIconSource(btn);
        if (base) {
            baseIconSources[btn] = base;
        }
    }

    const void *baseSrc = nullptr;
    auto it = baseIconSources.find(btn);
    if (it != baseIconSources.end()) {
        baseSrc = it->second;
    }

    const void *glowSrc = getGlowVariant(baseSrc);

    if (isImgbtn) {
        // Keep base icon on the button and render glow in a dedicated overlay to avoid 40x40 clipping.
        if (baseSrc) {
            setButtonIconSource(btn, baseSrc);
        }
        lv_obj_t *overlay = ensureImgbtnGlowOverlay(btn);
        if (overlay) {
            if (glowSrc) {
                lv_img_set_src(overlay, glowSrc);
                lv_obj_align_to(overlay, btn, LV_ALIGN_CENTER, 0, 0);
                lv_obj_move_foreground(overlay);
                lv_obj_set_style_opa(overlay, focused ? (emphasize ? LV_OPA_100 : LV_OPA_80) : LV_OPA_TRANSP,
                                     LV_PART_MAIN | LV_STATE_DEFAULT);
                if (focused) {
                    lv_obj_clear_flag(overlay, LV_OBJ_FLAG_HIDDEN);
                } else {
                    lv_obj_add_flag(overlay, LV_OBJ_FLAG_HIDDEN);
                }
            } else {
                lv_obj_add_flag(overlay, LV_OBJ_FLAG_HIDDEN);
            }
        }
    } else {
        const void *targetSrc = focused && glowSrc ? glowSrc : baseSrc;
        if (targetSrc) {
            setButtonIconSource(btn, targetSrc);
        }
    }

    lv_obj_set_style_radius(btn, LV_RADIUS_CIRCLE, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(btn, LV_OPA_TRANSP, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(btn, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_transform_zoom(btn, 256, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(btn, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_spread(btn, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_opa(btn, LV_OPA_TRANSP, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_outline_width(btn, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_outline_opa(btn, LV_OPA_TRANSP, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_outline_pad(btn, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    if (focused && !glowSrc && iconEdgeGlow) {
        lv_obj_set_style_transform_zoom(btn, emphasize ? 270 : 264, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_shadow_color(btn, lv_color_white(), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_shadow_width(btn, emphasize ? 22 : 16, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_shadow_spread(btn, emphasize ? 2 : 1, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_shadow_opa(btn, emphasize ? LV_OPA_60 : LV_OPA_40, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_outline_color(btn, lv_color_white(), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_outline_width(btn, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_outline_opa(btn, LV_OPA_40, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_outline_pad(btn, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    const bool usingGlow = focused && glowSrc;
    if (lv_obj_check_type(btn, &lv_imgbtn_class) || lv_obj_check_type(btn, &lv_img_class)) {
        lv_obj_set_style_img_recolor(btn, lv_color_white(), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_img_recolor_opa(btn, usingGlow ? LV_OPA_TRANSP : LV_OPA_100, LV_PART_MAIN | LV_STATE_DEFAULT);
    } else {
        lv_obj_set_style_bg_img_recolor(btn, lv_color_white(), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_img_recolor_opa(btn, usingGlow ? LV_OPA_TRANSP : LV_OPA_100, LV_PART_MAIN | LV_STATE_DEFAULT);
    }
}

static std::vector<lv_obj_t *> getVisibleMenuButtons() {
    std::vector<lv_obj_t *> buttons;

    // Deterministic circular traversal for 2x2 layout: Brew -> Steam -> Manual -> Water.
    if (ui_MenuScreen_btnBrew && !lv_obj_has_flag(ui_MenuScreen_btnBrew, LV_OBJ_FLAG_HIDDEN)) {
        buttons.emplace_back(ui_MenuScreen_btnBrew);
    }
    if (ui_MenuScreen_btnSteam && !lv_obj_has_flag(ui_MenuScreen_btnSteam, LV_OBJ_FLAG_HIDDEN)) {
        buttons.emplace_back(ui_MenuScreen_btnSteam);
    }
    if (ui_MenuScreen_btnManual && !lv_obj_has_flag(ui_MenuScreen_btnManual, LV_OBJ_FLAG_HIDDEN)) {
        buttons.emplace_back(ui_MenuScreen_btnManual);
    }
    if (ui_MenuScreen_waterBtn && !lv_obj_has_flag(ui_MenuScreen_waterBtn, LV_OBJ_FLAG_HIDDEN)) {
        buttons.emplace_back(ui_MenuScreen_waterBtn);
    }
    if (ui_MenuScreen_grindBtn && !lv_obj_has_flag(ui_MenuScreen_grindBtn, LV_OBJ_FLAG_HIDDEN)) {
        buttons.emplace_back(ui_MenuScreen_grindBtn);
    }

    return buttons;
}

static std::vector<lv_obj_t *> getBrewSettingsActionButtons() {
    std::vector<lv_obj_t *> buttons;
    if (ui_BrewScreen_saveButton) {
        buttons.emplace_back(ui_BrewScreen_saveButton);
    }
    if (ui_BrewScreen_acceptButton) {
        buttons.emplace_back(ui_BrewScreen_acceptButton);
    }
    if (ui_BrewScreen_saveAsNewButton) {
        buttons.emplace_back(ui_BrewScreen_saveAsNewButton);
    }
    return buttons;
}

static std::vector<lv_obj_t *> getBrewTopButtons() {
    std::vector<lv_obj_t *> buttons;
    if (ui_BrewScreen_profileSelectBtn) {
        buttons.emplace_back(ui_BrewScreen_profileSelectBtn);
    }
    if (ui_BrewScreen_settingsButton) {
        buttons.emplace_back(ui_BrewScreen_settingsButton);
    }
    if (ui_BrewScreen_startButton) {
        buttons.emplace_back(ui_BrewScreen_startButton);
    }
    return buttons;
}

int16_t calculate_angle(int set_temp, int range, int offset) {
    const double percentage = static_cast<double>(set_temp) / static_cast<double>(MAX_TEMP);
    return (percentage * ((double)range)) - range / 2 - offset;
}

void DefaultUI::updateTempHistory() {
    if (currentTemp > 0) {
        tempHistory[tempHistoryIndex] = currentTemp;
        tempHistoryIndex += 1;
    }

    if (tempHistoryIndex > TEMP_HISTORY_LENGTH) {
        tempHistoryIndex = 0;
        isTempHistoryInitialized = true;
    }

    if (tempHistoryIndex % 4 == 0) {
        heatingFlash = !heatingFlash;
        rerender = true;
    }
}

void DefaultUI::updateTempStableFlag() {
    if (isTempHistoryInitialized) {
        float totalError = 0.0f;
        float maxError = 0.0f;
        for (uint16_t i = 0; i < TEMP_HISTORY_LENGTH; i++) {
            float error = abs(tempHistory[i] - targetTemp);
            totalError += error;
            maxError = error > maxError ? error : maxError;
        }

        const float avgError = totalError / TEMP_HISTORY_LENGTH;
        const float errorMargin = max(2.0f, static_cast<float>(targetTemp) * 0.02f);

        isTemperatureStable = avgError < errorMargin && maxError <= errorMargin;
    }

    // instantly reset stability if setpoint has changed
    if (prevTargetTemp != targetTemp) {
        isTemperatureStable = false;
    }

    prevTargetTemp = targetTemp;
}

void DefaultUI::adjustHeatingIndicator(lv_obj_t *dials) {
    lv_obj_t *heatingIcon = ui_comp_get_child(dials, UI_COMP_DIALS_TEMPICON);
    lv_obj_set_style_img_recolor(heatingIcon, lv_color_hex(isTemperatureStable ? 0x00D100 : 0xF62C2C),
                                 LV_PART_MAIN | LV_STATE_DEFAULT);
    if (!isTemperatureStable) {
        lv_obj_set_style_opa(heatingIcon, heatingFlash ? LV_OPA_50 : LV_OPA_100, LV_PART_MAIN | LV_STATE_DEFAULT);
    }
}

void DefaultUI::reloadProfiles() { profileLoaded = 0; }

DefaultUI::DefaultUI(Controller *controller, Driver *driver, PluginManager *pluginManager)
    : controller(controller), panelDriver(driver), pluginManager(pluginManager) {
    setupPanel();
}

void DefaultUI::init() {
    profileManager = controller->getProfileManager();
    auto triggerRender = [this](Event const &) { rerender = true; };
    pluginManager->on("boiler:currentTemperature:change", [=](Event const &event) {
        int newTemp = static_cast<int>(event.getFloat("value"));
        if (newTemp != currentTemp) {
            currentTemp = newTemp;
            rerender = true;
        }
    });
    pluginManager->on("boiler:pressure:change", [=](Event const &event) {
        float newPressure = event.getFloat("value");
        if (round(newPressure * 10.0f) != round(pressure * 10.0f)) {
            pressure = newPressure;
            rerender = true;
        }
    });
    pluginManager->on("boiler:targetTemperature:change", [=](Event const &event) {
        int newTemp = static_cast<int>(event.getFloat("value"));
        if (newTemp != targetTemp) {
            targetTemp = newTemp;
            rerender = true;
        }
    });
    pluginManager->on("controller:targetVolume:change", [=](Event const &event) {
        targetVolume = event.getFloat("value");
        rerender = true;
    });
    pluginManager->on("controller:targetDuration:change", [=](Event const &event) {
        targetDuration = event.getFloat("value");
        rerender = true;
    });
    pluginManager->on("controller:grindDuration:change", [=](Event const &event) {
        grindDuration = event.getInt("value");
        rerender = true;
    });
    pluginManager->on("controller:grindVolume:change", [=](Event const &event) {
        grindVolume = event.getFloat("value");
        rerender = true;
    });
    pluginManager->on("controller:process:end", triggerRender);
    pluginManager->on("controller:process:start", triggerRender);
    pluginManager->on("controller:mode:change", [this](Event const &event) {
        mode = event.getInt("value");
        switch (mode) {
        case MODE_STANDBY:
            changeScreen(&ui_StandbyScreen, &ui_StandbyScreen_screen_init);
            break;
        case MODE_BREW:
            changeScreen(&ui_BrewScreen, &ui_BrewScreen_screen_init);
            break;
        case MODE_GRIND:
            changeScreen(&ui_GrindScreen, &ui_GrindScreen_screen_init);
            break;
        case MODE_STEAM:
            changeScreen(&ui_SimpleProcessScreen, &ui_SimpleProcessScreen_screen_init);
            break;
        case MODE_WATER:
            changeScreen(&ui_SimpleProcessScreen, &ui_SimpleProcessScreen_screen_init);
            break;
        case MODE_MANUAL:
            changeScreen(&ui_ManualBrewScreen, &ui_ManualBrewScreen_screen_init);
            break;
        default:
            break;
        };
    });
    pluginManager->on("controller:brew:start",
                      [this](Event const &event) { changeScreen(&ui_StatusScreen, &ui_StatusScreen_screen_init); });
    pluginManager->on("controller:brew:clear", [this](Event const &event) {
        if (lv_scr_act() == ui_StatusScreen) {
            changeScreen(&ui_BrewScreen, &ui_BrewScreen_screen_init);
        }
    });
    pluginManager->on("controller:bluetooth:waiting", [this](Event const &) {
        waitingForController = true;
        rerender = true;
    });
    pluginManager->on("controller:bluetooth:connect", [this](Event const &) {
        waitingForController = false;
        rerender = true;
        initialized = true;
        if (lv_scr_act() == ui_StandbyScreen) {
            Settings &settings = controller->getSettings();
            if (settings.getStartupMode() == MODE_BREW) {
                changeScreen(&ui_BrewScreen, &ui_BrewScreen_screen_init);
            } else {
                standbyEnterTime = millis();
            }
        }
        if (controller->getSystemInfo().capabilities.pressure) {
            pressureAvailable = 1;
        }
    });
    pluginManager->on("controller:bluetooth:disconnect", [this](Event const &) {
        waitingForController = true;
        rerender = true;
    });
    pluginManager->on("controller:wifi:connect", [this](Event const &event) {
        rerender = true;
        apActive = event.getInt("AP");
    });
    pluginManager->on("ota:update:start", [this](Event const &) {
        updateActive = true;
        rerender = true;
        changeScreen(&ui_StandbyScreen, &ui_StandbyScreen_screen_init);
    });
    pluginManager->on("ota:update:end", [this](Event const &) {
        updateActive = false;
        rerender = true;
        changeScreen(&ui_StandbyScreen, &ui_StandbyScreen_screen_init);
    });
    pluginManager->on("ota:update:status", [this](Event const &event) {
        rerender = true;
        updateAvailable = event.getInt("value");
    });
    pluginManager->on("controller:error", [this](Event const &) {
        rerender = true;
        changeScreen(&ui_StandbyScreen, &ui_StandbyScreen_screen_init);
    });
    pluginManager->on("controller:autotune:start",
                      [this](Event const &) { changeScreen(&ui_StandbyScreen, &ui_StandbyScreen_screen_init); });
    pluginManager->on("controller:autotune:result",
                      [this](Event const &) { changeScreen(&ui_StandbyScreen, &ui_StandbyScreen_screen_init); });

    pluginManager->on("profiles:profile:select", [this](Event const &event) {
        profileManager->loadSelectedProfile(selectedProfile);
        selectedProfileId = event.getString("id");
        targetDuration = profileManager->getSelectedProfile().getTotalDuration();
        targetVolume = profileManager->getSelectedProfile().getTotalVolume();
        profileVolumetric = profileManager->getSelectedProfile().isVolumetric();
        reloadProfiles();
        rerender = true;
    });
    pluginManager->on("profiles:profile:favorite", [this](Event const &event) { reloadProfiles(); });
    pluginManager->on("profiles:profile:unfavorite", [this](Event const &event) { reloadProfiles(); });
    pluginManager->on("profiles:profile:save", [this](Event const &event) { reloadProfiles(); });
    pluginManager->on("controller:volumetric-measurement:bluetooth:change", [=](Event const &event) {
        double newWeight = event.getFloat("value");
        if (round(newWeight * 10.0) != round(bluetoothWeight * 10.0)) {
            bluetoothWeight = newWeight;
            rerender = true;
        }
    });
    pluginManager->on("shot-history:manual:recorded", [this](Event const &event) {
        manualPromptShotId = event.getString("id");
        manualPromptShotDurationSec = event.getFloat("duration");
        manualPromptShotVolume = event.getFloat("volume");
        manualPromptRequested = !manualPromptShotId.isEmpty();
        rerender = true;
    });
    setupState();
    setupReactive();
    xTaskCreatePinnedToCore(loopTask, "DefaultUI::loop", configMINIMAL_STACK_SIZE * 6, this, 1, &taskHandle, 1);
    xTaskCreatePinnedToCore(profileLoopTask, "DefaultUI::loopProfiles", configMINIMAL_STACK_SIZE * 4, this, 1, &profileTaskHandle,
                            0);
}

void DefaultUI::loop() {
    const unsigned long now = millis();
    const unsigned long diff = now - lastRender;

    if (now - lastTempLog > TEMP_HISTORY_INTERVAL) {
        updateTempHistory();
        lastTempLog = now;
    }

    if ((controller->isActive() && diff > RERENDER_INTERVAL_ACTIVE) || diff > RERENDER_INTERVAL_IDLE) {
        rerender = true;
    }

    if (hasActiveAdjustmentPulse(now) && diff > ADJUSTMENT_PULSE_RERENDER_MS) {
        rerender = true;
    }

    if (rerender) {
        rerender = false;
        lastRender = now;
        error = controller->isErrorState();
        autotuning = controller->isAutotuning();
        const Settings &settings = controller->getSettings();
        volumetricAvailable = controller->isVolumetricAvailable();
        bluetoothScales = controller->isBluetoothScaleHealthy();
        volumetricMode = volumetricAvailable && settings.isVolumetricTarget();
        brewVolumetric = volumetricAvailable && profileVolumetric;
        grindActive = controller->isGrindActive();
        active = controller->isActive();
        smartGrindActive = settings.isSmartGrindActive();
        grindAvailable = smartGrindActive || settings.getAltRelayFunction() == ALT_RELAY_GRIND;

        // Manual mode state tracking
        if (mode == MODE_MANUAL) {
            manualTargetPressure = controller->getSettings().getTargetManualPressure();
            if (active) {
                if (manualStartMs == 0) {
                    manualStartMs = now;
                }
                manualWasActive = true;
                manualFreezeData = false;
                int elapsed = (int)((now - manualStartMs) / 1000UL);
                if (elapsed != manualDisplayElapsedSeconds) {
                    manualDisplayElapsedSeconds = elapsed;
                }
                manualDisplayTemp = currentTemp;
                manualDisplayPressure = pressure;
                manualDisplayFlow = controller->getCurrentPumpFlow();
                manualDisplayWeight = bluetoothWeight;
            } else {
                if (manualWasActive) {
                    // Preserve the last shot values on screen after stop.
                    manualFreezeData = true;
                    manualWasActive = false;
                }

                if (!manualFreezeData) {
                    manualDisplayElapsedSeconds = 0;
                    manualDisplayTemp = currentTemp;
                    manualDisplayPressure = pressure;
                    manualDisplayFlow = controller->getCurrentPumpFlow();
                    manualDisplayWeight = bluetoothWeight;
                }

                manualStartMs = 0;
            }
        }
        applyTheme();
        if (controller->isErrorState()) {
            changeScreen(&ui_StandbyScreen, &ui_StandbyScreen_screen_init);
        }
        updateTempStableFlag();
        handleScreenChange();
        currentScreen = lv_scr_act();
        if (lv_scr_act() == ui_StandbyScreen)
            updateStandbyScreen();
        if (lv_scr_act() == ui_StatusScreen)
            updateStatusScreen();

        if (manualPromptRequested && !manualSavePromptVisible) {
            showManualSavePrompt(manualPromptShotId);
            manualPromptRequested = false;
        }

        effect_mgr.evaluate_all();
    }

    lv_task_handler();
}

void DefaultUI::loopProfiles() {
    if (!profileLoaded) {
        favoritedProfileIds.clear();
        favoritedProfiles.clear();
        favoritedProfileIds.emplace_back(controller->getSettings().getSelectedProfile());
        for (auto &id : profileManager->getFavoritedProfiles()) {
            if (std::find(favoritedProfileIds.begin(), favoritedProfileIds.end(), id) == favoritedProfileIds.end())
                favoritedProfileIds.emplace_back(id);
        }
        for (const auto &profileId : favoritedProfileIds) {
            Profile profile{};
            profileManager->loadProfile(profileId, profile);
            favoritedProfiles.emplace_back(profile);
        }
        profileLoaded = 1;
    }
}

void DefaultUI::changeScreen(lv_obj_t **screen, void (*target_init)()) {
    targetScreen = screen;
    targetScreenInit = target_init;
    rerender = true;

    if (screen == &ui_ManualBrewScreen) {
        manualFreezeData = false;
        manualWasActive = false;
        manualStartMs = 0;
        manualDisplayElapsedSeconds = 0;
        manualDisplayTemp = currentTemp;
        manualDisplayPressure = pressure;
        manualDisplayFlow = controller->getCurrentPumpFlow();
        manualDisplayWeight = bluetoothWeight;
        // Manual screen should default focus to brew button (no pressure/temp focus stepping).
        currentManualIdx = 2;
    }

    // Reset some submenus
    brewScreenState = BrewScreenState::Brew;
    currentBrewTopIdx = 2;
    currentSimpleProcessIdx = 0;
    if (screen != &ui_ManualBrewScreen)
        currentManualIdx = 0;
}

void DefaultUI::changeBrewScreenMode(BrewScreenState state) {
    brewScreenState = state;
    if (state == BrewScreenState::Settings) {
        currentBrewSettingsIdx = 0;
        currentBrewActionIdx = 1;
        brewSettingsActionMode = false;
    } else {
        currentBrewTopIdx = 2;
        brewSettingsActionMode = false;
    }
    rerender = true;
}

void DefaultUI::onProfileSwitch() {
    currentProfileIdx = 0;
    changeScreen(&ui_ProfileScreen, ui_ProfileScreen_screen_init);
}

void DefaultUI::onNextProfile() {
    if (currentProfileIdx < favoritedProfileIds.size() - 1) {
        currentProfileIdx++;
    }
    rerender = true;
}

void DefaultUI::onPreviousProfile() {
    if (currentProfileIdx > 0) {
        currentProfileIdx--;
    }
    rerender = true;
}

void DefaultUI::onProfileSelect() {
    profileManager->selectProfile(favoritedProfileIds[currentProfileIdx]);
    profileDirty = false;
    changeScreen(&ui_BrewScreen, ui_BrewScreen_screen_init);
}

bool DefaultUI::isProfileScreenActive() const { return lv_scr_act() == ui_ProfileScreen; }

bool DefaultUI::isMenuScreenActive() const { return lv_scr_act() == ui_MenuScreen; }

bool DefaultUI::isStandbyScreenActive() const { return lv_scr_act() == ui_StandbyScreen; }

bool DefaultUI::isBrewScreenActive() const { return lv_scr_act() == ui_BrewScreen && brewScreenState == BrewScreenState::Brew; }

bool DefaultUI::isBrewSettingsActive() const { return lv_scr_act() == ui_BrewScreen && brewScreenState == BrewScreenState::Settings; }

bool DefaultUI::isSimpleProcessScreenActive() const { return lv_scr_act() == ui_SimpleProcessScreen; }

bool DefaultUI::isManualScreenActive() const { return lv_scr_act() == ui_ManualBrewScreen; }

bool DefaultUI::isStatusScreenActive() const { return lv_scr_act() == ui_StatusScreen; }

bool DefaultUI::isBackNavigableScreenActive() const {
    lv_obj_t *screen = lv_scr_act();
    return screen == ui_BrewScreen || screen == ui_StatusScreen || screen == ui_GrindScreen ||
           screen == ui_SimpleProcessScreen || screen == ui_ProfileScreen || screen == ui_ManualBrewScreen;
}

void DefaultUI::onNextMenuItem() {
    const auto buttons = getVisibleMenuButtons();
    if (buttons.empty()) {
        return;
    }

    currentMenuIdx = (currentMenuIdx + 1) % buttons.size();
    rerender = true;
}

void DefaultUI::onPreviousMenuItem() {
    const auto buttons = getVisibleMenuButtons();
    if (buttons.empty()) {
        return;
    }

    currentMenuIdx = (currentMenuIdx + buttons.size() - 1) % buttons.size();
    rerender = true;
}

void DefaultUI::onMenuItemSelect() {
    const auto buttons = getVisibleMenuButtons();
    if (buttons.empty()) {
        return;
    }

    if (currentMenuIdx >= buttons.size()) {
        currentMenuIdx = 0;
    }

    lv_event_send(buttons[currentMenuIdx], LV_EVENT_CLICKED, nullptr);
}

void DefaultUI::onBrewScreenRotate(int delta) {
    const auto buttons = getBrewTopButtons();
    if (buttons.empty()) {
        return;
    }

    currentBrewTopIdx = (currentBrewTopIdx + delta) % static_cast<int>(buttons.size());
    if (currentBrewTopIdx < 0) {
        currentBrewTopIdx += buttons.size();
    }
    rerender = true;
}

void DefaultUI::onBrewScreenSelect() {
    const auto buttons = getBrewTopButtons();
    if (buttons.empty()) {
        return;
    }

    if (currentBrewTopIdx < 0 || currentBrewTopIdx >= static_cast<int>(buttons.size())) {
        currentBrewTopIdx = 0;
    }

    lv_event_send(buttons[currentBrewTopIdx], LV_EVENT_CLICKED, nullptr);
}

void DefaultUI::onSimpleProcessRotate(int delta) {
    if (currentSimpleProcessIdx != 0) {
        return;
    }

    for (int step = 0; step < abs(delta); step++) {
        if (delta > 0) {
            controller->raiseTemp();
            triggerAdjustmentPulse(true);
        } else {
            controller->lowerTemp();
            triggerAdjustmentPulse(false);
        }
    }

    rerender = true;
}

void DefaultUI::onSimpleProcessPress() {
    // Water mode should dispense immediately on dial press.
    if (mode == MODE_WATER) {
        if (ui_SimpleProcessScreen_goButton && !lv_obj_has_flag(ui_SimpleProcessScreen_goButton, LV_OBJ_FLAG_HIDDEN)) {
            lv_event_send(ui_SimpleProcessScreen_goButton, LV_EVENT_CLICKED, nullptr);
        }
        return;
    }

    // Two-step flow: temp focus -> play focus -> execute.
    if (currentSimpleProcessIdx == 0) {
        currentSimpleProcessIdx = 1;
        rerender = true;
        return;
    }

    if (ui_SimpleProcessScreen_goButton && !lv_obj_has_flag(ui_SimpleProcessScreen_goButton, LV_OBJ_FLAG_HIDDEN)) {
        lv_event_send(ui_SimpleProcessScreen_goButton, LV_EVENT_CLICKED, nullptr);
    }
}

void DefaultUI::onManualScreenRotate(int delta) {
    if (manualSavePromptVisible) {
        if (delta != 0) {
            manualSavePromptSelectionYes = !manualSavePromptSelectionYes;
            updateManualSavePromptSelection();
            rerender = true;
        }
        return;
    }

    // Manual encoder controls pressure only.
    // Direction mapping: CW increases pressure, CCW decreases pressure.
    const int magnitude = abs(delta);

    // Time-based velocity: track interval between calls so rapid spinning ramps step size.
    static uint32_t lastManualRotateMs = 0;
    const uint32_t now = millis();
    const uint32_t interval = now - lastManualRotateMs;
    lastManualRotateMs = now;

    float perClickStep;
    if (interval < 85)       perClickStep = 1.2f;   // very fast
    else if (interval < 150) perClickStep = 0.7f;   // fast
    else if (interval < 260) perClickStep = 0.35f;  // medium
    else                     perClickStep = 0.2f;   // slow/deliberate

    const bool increase = delta < 0;  // CW increase, CCW decrease (manual screen mapping)
    const float signedDelta = (increase ? 1.0f : -1.0f) * perClickStep * static_cast<float>(magnitude);
    controller->adjustManualPressure(signedDelta);
    triggerAdjustmentPulse(increase);

    rerender = true;
}

void DefaultUI::onManualPressureTouch(float deltaBar) {
    if (manualSavePromptVisible) {
        return;
    }

    if (std::abs(deltaBar) < 0.05f) {
        return;
    }

    currentManualIdx = 0;
    controller->adjustManualPressure(deltaBar);
    triggerAdjustmentPulse(deltaBar > 0.0f);
    rerender = true;
}

void DefaultUI::onManualScreenPress() {
    if (manualSavePromptVisible) {
        onManualSavePromptConfirm();
        rerender = true;
        return;
    }

    // Manual screen click should always toggle brew (no focus cycling).
    if (ui_ManualBrewScreen_goBtn) {
        lv_event_send(ui_ManualBrewScreen_goBtn, LV_EVENT_CLICKED, nullptr);
    }
    currentManualIdx = 2;
    rerender = true;
}

void DefaultUI::onStatusScreenSelect() {
    if (!ui_StatusScreen_pauseButton) {
        return;
    }

    lv_event_send(ui_StatusScreen_pauseButton, LV_EVENT_CLICKED, nullptr);
}

void DefaultUI::onBackNavigate() {
    controller->deactivate();
    controller->setMode(MODE_BREW);
    changeScreen(&ui_MenuScreen, ui_MenuScreen_screen_init);
}

void DefaultUI::onBrewSettingsRotate(int delta) {
    if (brewSettingsActionMode) {
        const auto actionButtons = getBrewSettingsActionButtons();
        if (actionButtons.empty()) {
            return;
        }
        currentBrewActionIdx = (currentBrewActionIdx + delta) % static_cast<int>(actionButtons.size());
        if (currentBrewActionIdx < 0) {
            currentBrewActionIdx += actionButtons.size();
        }
        rerender = true;
        return;
    }

    for (int step = 0; step < abs(delta); step++) {
        const bool increase = delta > 0;
        if (currentBrewSettingsIdx == 0) {
            increase ? controller->raiseTemp() : controller->lowerTemp();
        } else {
            increase ? controller->raiseBrewTarget() : controller->lowerBrewTarget();
        }
        triggerAdjustmentPulse(increase);
    }
    markProfileDirty();
    rerender = true;
}

void DefaultUI::triggerAdjustmentPulse(bool increase) {
    const unsigned long now = millis();
    if (increase) {
        plusPulseUntil = now + ADJUSTMENT_PULSE_MS;
    } else {
        minusPulseUntil = now + ADJUSTMENT_PULSE_MS;
    }
}

bool DefaultUI::hasActiveAdjustmentPulse(unsigned long now) const {
    return now < plusPulseUntil || now < minusPulseUntil;
}

void DefaultUI::showManualSavePrompt(const String &shotId) {
    if (manualSavePromptVisible) {
        return;
    }

    manualPromptShotId = shotId;
    manualSavePromptSelectionYes = true;

    lv_obj_t *overlay = lv_obj_create(lv_layer_top());
    manualSavePrompt = overlay;
    lv_obj_set_size(overlay, 340, 340);
    lv_obj_center(overlay);
    lv_obj_set_style_radius(overlay, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(overlay, lv_color_hex(0x151515), 0);
    lv_obj_set_style_bg_opa(overlay, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(overlay, lv_color_hex(0x3A3A3A), 0);
    lv_obj_set_style_border_width(overlay, 2, 0);
    lv_obj_set_style_pad_all(overlay, 20, 0);
    lv_obj_clear_flag(overlay, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(overlay);
    lv_label_set_text(title, "Save as profile?");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 52);

    lv_obj_t *namePreview = lv_label_create(overlay);
    lv_label_set_text(namePreview, getNextManualProfileLabel().c_str());
    lv_obj_set_style_text_font(namePreview, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(namePreview, lv_color_hex(0xBDBDBD), 0);
    lv_obj_align(namePreview, LV_ALIGN_TOP_MID, 0, 94);

    manualSavePromptNoBtn = lv_btn_create(overlay);
    lv_obj_set_size(manualSavePromptNoBtn, 108, 108);
    lv_obj_set_style_radius(manualSavePromptNoBtn, LV_RADIUS_CIRCLE, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_align(manualSavePromptNoBtn, LV_ALIGN_BOTTOM_MID, -70, -44);
    lv_obj_add_event_cb(manualSavePromptNoBtn, DefaultUI::onManualSavePromptButton, LV_EVENT_CLICKED, this);
    lv_obj_t *noLabel = lv_label_create(manualSavePromptNoBtn);
    lv_label_set_text(noLabel, "No");
    lv_obj_set_style_text_font(noLabel, &lv_font_montserrat_24, 0);
    lv_obj_center(noLabel);

    manualSavePromptYesBtn = lv_btn_create(overlay);
    lv_obj_set_size(manualSavePromptYesBtn, 108, 108);
    lv_obj_set_style_radius(manualSavePromptYesBtn, LV_RADIUS_CIRCLE, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_align(manualSavePromptYesBtn, LV_ALIGN_BOTTOM_MID, 70, -44);
    lv_obj_add_event_cb(manualSavePromptYesBtn, DefaultUI::onManualSavePromptButton, LV_EVENT_CLICKED, this);
    lv_obj_t *yesLabel = lv_label_create(manualSavePromptYesBtn);
    lv_label_set_text(yesLabel, "Yes");
    lv_obj_set_style_text_font(yesLabel, &lv_font_montserrat_24, 0);
    lv_obj_center(yesLabel);

    manualSavePromptVisible = true;
    updateManualSavePromptSelection();
}

void DefaultUI::hideManualSavePrompt() {
    if (manualSavePrompt && lv_obj_is_valid(manualSavePrompt)) {
        lv_obj_del(manualSavePrompt);
    }
    manualSavePrompt = nullptr;
    manualSavePromptYesBtn = nullptr;
    manualSavePromptNoBtn = nullptr;
    manualSavePromptVisible = false;
    manualPromptRequested = false;
    manualPromptShotId = "";
    manualPromptShotDurationSec = 0.0f;
    manualPromptShotVolume = 0.0f;
}

void DefaultUI::updateManualSavePromptSelection() {
    if (!manualSavePromptVisible) {
        return;
    }

    auto applySelectionStyle = [](lv_obj_t *btn, bool selected) {
        if (!btn || !lv_obj_is_valid(btn)) {
            return;
        }
        lv_obj_set_style_bg_color(btn, selected ? lv_color_hex(0x2A8E42) : lv_color_hex(0x3A3A3A), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(btn, selected ? 3 : 1, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(btn, selected ? lv_color_hex(0xB9E9C5) : lv_color_hex(0x5A5A5A),
                                      LV_PART_MAIN | LV_STATE_DEFAULT);
    };

    applySelectionStyle(manualSavePromptYesBtn, manualSavePromptSelectionYes);
    applySelectionStyle(manualSavePromptNoBtn, !manualSavePromptSelectionYes);
}

void DefaultUI::onManualSavePromptConfirm() {
    if (!manualSavePromptVisible) {
        return;
    }

    if (manualSavePromptSelectionYes) {
        saveManualShotAsProfile(manualPromptShotId);
    }

    hideManualSavePrompt();
}

void DefaultUI::saveManualShotAsProfile(const String &shotId) {
    Profile profile{};
    profile.label = getNextManualProfileLabel();
    profile.description = "Saved from manual shot " + shotId;
    profile.type = "standard";
    profile.utility = false;
    profile.temperature = static_cast<float>(controller->getTargetTemp());

    Phase brewPhase{};
    brewPhase.name = "Brew";
    brewPhase.phase = PhaseType::PHASE_TYPE_BREW;
    brewPhase.valve = 1;
    const float shotDurationSeconds = manualPromptShotDurationSec > 0.0f ? manualPromptShotDurationSec
                                                                          : static_cast<float>(manualDisplayElapsedSeconds);
    brewPhase.duration = std::max(1.0f, shotDurationSeconds);
    brewPhase.pumpIsSimple = false;
    brewPhase.temperature = profile.temperature;
    brewPhase.transition = Transition{.type = TransitionType::INSTANT, .duration = 0.0f, .adaptive = false};
    brewPhase.pumpAdvanced.target = PumpTarget::PUMP_TARGET_PRESSURE;
    brewPhase.pumpAdvanced.pressure = std::max(0.0f, controller->getSettings().getTargetManualPressure());
    brewPhase.pumpAdvanced.flow = 0.0f;

    const float volume = manualPromptShotVolume > 0.0f ? manualPromptShotVolume : static_cast<float>(manualDisplayWeight);
    if (volume > 0.5f) {
        Target volumeTarget{};
        volumeTarget.type = TargetType::TARGET_TYPE_VOLUMETRIC;
        volumeTarget.operator_ = TargetOperator::GTE;
        volumeTarget.value = volume;
        brewPhase.targets.push_back(volumeTarget);
    }

    profile.phases.push_back(brewPhase);
    profileManager->saveProfile(profile);
}

String DefaultUI::getNextManualProfileLabel() const {
    int maxManualNumber = 0;
    const std::vector<String> profileIds = profileManager->listProfiles();

    for (const String &id : profileIds) {
        Profile profile{};
        if (!profileManager->loadProfile(id, profile)) {
            continue;
        }
        if (!profile.label.startsWith("Manual ")) {
            continue;
        }

        String suffix = profile.label.substring(7);
        if (suffix.isEmpty()) {
            continue;
        }

        bool numeric = true;
        for (size_t i = 0; i < suffix.length(); i++) {
            if (!std::isdigit(static_cast<unsigned char>(suffix.charAt(i)))) {
                numeric = false;
                break;
            }
        }

        if (!numeric) {
            continue;
        }

        int value = suffix.toInt();
        if (value > maxManualNumber) {
            maxManualNumber = value;
        }
    }

    return "Manual " + String(maxManualNumber + 1);
}

void DefaultUI::onManualSavePromptButton(lv_event_t *e) {
    DefaultUI *ui = static_cast<DefaultUI *>(lv_event_get_user_data(e));
    if (!ui) {
        return;
    }

    lv_obj_t *target = lv_event_get_target(e);
    if (target == ui->manualSavePromptYesBtn) {
        ui->manualSavePromptSelectionYes = true;
    } else if (target == ui->manualSavePromptNoBtn) {
        ui->manualSavePromptSelectionYes = false;
    }

    ui->onManualSavePromptConfirm();
    ui->rerender = true;
}

void DefaultUI::onBrewSettingsPress() {
    if (brewSettingsActionMode) {
        const auto actionButtons = getBrewSettingsActionButtons();
        if (actionButtons.empty()) {
            return;
        }
        if (currentBrewActionIdx < 0 || currentBrewActionIdx >= static_cast<int>(actionButtons.size())) {
            currentBrewActionIdx = 1;
        }
        lv_event_send(actionButtons[currentBrewActionIdx], LV_EVENT_CLICKED, nullptr);
        return;
    }

    currentBrewSettingsIdx = (currentBrewSettingsIdx + 1) % 2;
    rerender = true;
}

void DefaultUI::onBrewSettingsToggleMode() {
    brewSettingsActionMode = !brewSettingsActionMode;
    if (!brewSettingsActionMode) {
        currentBrewSettingsIdx = 0;
    }
    rerender = true;
}

void DefaultUI::focusBrewSettingsTemp() {
    if (lv_scr_act() != ui_BrewScreen || brewScreenState != BrewScreenState::Settings) {
        return;
    }
    brewSettingsActionMode = false;
    currentBrewSettingsIdx = 0;
    rerender = true;
}

void DefaultUI::focusBrewSettingsTarget() {
    if (lv_scr_act() != ui_BrewScreen || brewScreenState != BrewScreenState::Settings) {
        return;
    }
    brewSettingsActionMode = false;
    currentBrewSettingsIdx = 1;
    rerender = true;
}

void DefaultUI::wakeToMenu() {
    currentMenuIdx = 0;
    controller->deactivateStandby();
    changeScreen(&ui_MenuScreen, ui_MenuScreen_screen_init);
}

void DefaultUI::onVolumetricDelete() {
    controller->onVolumetricDelete();
    profileVolumetric = profileManager->getSelectedProfile().isVolumetric();
    profileDirty = true;
}

void DefaultUI::setupPanel() {
    ui_init();
    lv_task_handler();

    delay(100);
    // Set initial brightness based on settings
    const Settings &settings = controller->getSettings();
    setBrightness(settings.getMainBrightness());
}

void DefaultUI::setupState() {
    error = controller->isErrorState();
    autotuning = controller->isAutotuning();
    const Settings &settings = controller->getSettings();
    volumetricAvailable = controller->isVolumetricAvailable();
    volumetricMode = volumetricAvailable && settings.isVolumetricTarget();
    grindActive = controller->isGrindActive();
    active = controller->isActive();
    smartGrindActive = settings.isSmartGrindActive();
    grindAvailable = smartGrindActive || settings.getAltRelayFunction() == ALT_RELAY_GRIND;
    mode = controller->getMode();
    currentTemp = static_cast<int>(controller->getCurrentTemp());
    targetTemp = static_cast<int>(controller->getTargetTemp());
    targetDuration = profileManager->getSelectedProfile().getTotalDuration();
    targetVolume = profileManager->getSelectedProfile().getTotalVolume();
    grindDuration = settings.getTargetGrindDuration();
    grindVolume = settings.getTargetGrindVolume();
    if (controller->getSystemInfo().capabilities.pressure) {
        pressureAvailable = 1;
    }
    pressureScaling = std::ceil(settings.getPressureScaling());
    selectedProfileId = settings.getSelectedProfile();
    profileManager->loadSelectedProfile(selectedProfile);
    profileVolumetric = selectedProfile.isVolumetric();
}

void DefaultUI::setupReactive() {
    effect_mgr.use_effect([=] { return currentScreen == ui_MenuScreen; }, [=]() { adjustDials(ui_MenuScreen_dials); },
                          &pressureAvailable);
    effect_mgr.use_effect([=] { return currentScreen == ui_StatusScreen; }, [=]() { adjustDials(ui_StatusScreen_dials); },
                          &pressureAvailable);
    effect_mgr.use_effect([=] { return currentScreen == ui_BrewScreen; }, [=]() { adjustDials(ui_BrewScreen_dials); },
                          &pressureAvailable);
    effect_mgr.use_effect([=] { return currentScreen == ui_GrindScreen; }, [=]() { adjustDials(ui_GrindScreen_dials); },
                          &pressureAvailable);
    effect_mgr.use_effect([=] { return currentScreen == ui_SimpleProcessScreen; },
                          [=]() { adjustDials(ui_SimpleProcessScreen_dials); }, &pressureAvailable);
    effect_mgr.use_effect([=] { return currentScreen == ui_ProfileScreen; }, [=]() { adjustDials(ui_ProfileScreen_dials); },
                          &pressureAvailable);
    effect_mgr.use_effect([=] { return currentScreen == ui_BrewScreen; }, [=]() { adjustHeatingIndicator(ui_BrewScreen_dials); },
                          &isTemperatureStable, &heatingFlash);
    effect_mgr.use_effect([=] { return currentScreen == ui_SimpleProcessScreen; },
                          [=]() { adjustHeatingIndicator(ui_SimpleProcessScreen_dials); }, &isTemperatureStable, &heatingFlash);
    effect_mgr.use_effect([=] { return currentScreen == ui_MenuScreen; }, [=]() { adjustHeatingIndicator(ui_MenuScreen_dials); },
                          &isTemperatureStable, &heatingFlash);
    effect_mgr.use_effect([=] { return currentScreen == ui_ProfileScreen; },
                          [=]() { adjustHeatingIndicator(ui_ProfileScreen_dials); }, &isTemperatureStable, &heatingFlash);
    effect_mgr.use_effect([=] { return currentScreen == ui_GrindScreen; },
                          [=]() { adjustHeatingIndicator(ui_GrindScreen_dials); }, &isTemperatureStable, &heatingFlash);
    effect_mgr.use_effect([=] { return currentScreen == ui_StatusScreen; },
                          [=]() { adjustHeatingIndicator(ui_StatusScreen_dials); }, &isTemperatureStable, &heatingFlash);
    effect_mgr.use_effect([=] { return currentScreen == ui_SimpleProcessScreen; },
                          [=]() { lv_label_set_text(ui_SimpleProcessScreen_mainLabel5, mode == MODE_STEAM ? "Steam" : "Water"); },
                          &mode);
    effect_mgr.use_effect([=] { return currentScreen == ui_MenuScreen; },
                          [=]() {
                              lv_arc_set_value(uic_MenuScreen_dials_tempGauge, currentTemp);
                              lv_label_set_text_fmt(uic_MenuScreen_dials_tempText, "%d°C", currentTemp);
                              applyDialClusterTheme(ui_MenuScreen_dials, currentTemp, pressure);
                          },
                          &currentTemp);
    effect_mgr.use_effect([=] { return currentScreen == ui_StatusScreen; },
                          [=]() {
                              lv_arc_set_value(uic_StatusScreen_dials_tempGauge, currentTemp);
                              lv_label_set_text_fmt(uic_StatusScreen_dials_tempText, "%d°C", currentTemp);
                              applyDialClusterTheme(ui_StatusScreen_dials, currentTemp, pressure);
                          },
                          &currentTemp);
    effect_mgr.use_effect([=] { return currentScreen == ui_BrewScreen; },
                          [=]() {
                              lv_arc_set_value(uic_BrewScreen_dials_tempGauge, currentTemp);
                              lv_label_set_text_fmt(uic_BrewScreen_dials_tempText, "%d°C", currentTemp);
                              applyDialClusterTheme(ui_BrewScreen_dials, currentTemp, pressure);
                          },
                          &currentTemp);
    effect_mgr.use_effect([=] { return currentScreen == ui_GrindScreen; },
                          [=]() {
                              lv_arc_set_value(uic_GrindScreen_dials_tempGauge, currentTemp);
                              lv_label_set_text_fmt(uic_GrindScreen_dials_tempText, "%d°C", currentTemp);
                              applyDialClusterTheme(ui_GrindScreen_dials, currentTemp, pressure);
                          },
                          &currentTemp);
    effect_mgr.use_effect([=] { return currentScreen == ui_SimpleProcessScreen; },
                          [=]() {
                              lv_arc_set_value(uic_SimpleProcessScreen_dials_tempGauge, currentTemp);
                              lv_label_set_text_fmt(uic_SimpleProcessScreen_dials_tempText, "%d°C", currentTemp);
                              applyDialClusterTheme(ui_SimpleProcessScreen_dials, currentTemp, pressure);
                          },
                          &currentTemp);
    effect_mgr.use_effect([=] { return currentScreen == ui_ProfileScreen; },
                          [=]() {
                              lv_arc_set_value(uic_ProfileScreen_dials_tempGauge, currentTemp);
                              lv_label_set_text_fmt(uic_ProfileScreen_dials_tempText, "%d°C", currentTemp);
                              applyDialClusterTheme(ui_ProfileScreen_dials, currentTemp, pressure);
                          },
                          &currentTemp);
    effect_mgr.use_effect([=] { return currentScreen == ui_MenuScreen; }, [=]() { adjustTempTarget(ui_MenuScreen_dials); },
                          &targetTemp);
    effect_mgr.use_effect([=] { return currentScreen == ui_StatusScreen; },
                          [=]() {
                              lv_label_set_text_fmt(ui_StatusScreen_targetTemp, "%d°C", targetTemp);
                              adjustTempTarget(ui_StatusScreen_dials);
                          },
                          &targetTemp);
    effect_mgr.use_effect([=] { return currentScreen == ui_BrewScreen; },
                          [=]() {
                              lv_label_set_text_fmt(ui_BrewScreen_targetTemp, "%d°C", targetTemp);
                              adjustTempTarget(ui_BrewScreen_dials);
                          },
                          &targetTemp);
    effect_mgr.use_effect([=] { return currentScreen == ui_GrindScreen; }, [=]() { adjustTempTarget(ui_GrindScreen_dials); },
                          &targetTemp);
    effect_mgr.use_effect([=] { return currentScreen == ui_SimpleProcessScreen; },
                          [=]() {
                              lv_label_set_text_fmt(ui_SimpleProcessScreen_targetTemp, "%d°C", targetTemp);
                              adjustTempTarget(ui_SimpleProcessScreen_dials);
                          },
                          &targetTemp);
    effect_mgr.use_effect([=] { return currentScreen == ui_ProfileScreen; }, [=]() { adjustTempTarget(ui_ProfileScreen_dials); },
                          &targetTemp);
    effect_mgr.use_effect([=] { return currentScreen == ui_MenuScreen; },
                          [=]() {
                              lv_arc_set_value(uic_MenuScreen_dials_pressureGauge, pressure * 10.0f);
                              lv_label_set_text_fmt(uic_MenuScreen_dials_pressureText, "%.1f bar", pressure);
                              applyDialClusterTheme(ui_MenuScreen_dials, currentTemp, pressure);
                          },
                          &pressure);
    effect_mgr.use_effect([=] { return currentScreen == ui_StatusScreen; },
                          [=]() {
                              lv_arc_set_value(uic_StatusScreen_dials_pressureGauge, pressure * 10.0f);
                              lv_label_set_text_fmt(uic_StatusScreen_dials_pressureText, "%.1f bar", pressure);
                              applyDialClusterTheme(ui_StatusScreen_dials, currentTemp, pressure);
                          },
                          &pressure);
    effect_mgr.use_effect([=] { return currentScreen == ui_BrewScreen; },
                          [=]() {
                              lv_arc_set_value(uic_BrewScreen_dials_pressureGauge, pressure * 10.0f);
                              lv_label_set_text_fmt(uic_BrewScreen_dials_pressureText, "%.1f bar", pressure);
                              applyDialClusterTheme(ui_BrewScreen_dials, currentTemp, pressure);
                          },
                          &pressure);
    effect_mgr.use_effect([=] { return currentScreen == ui_GrindScreen; },
                          [=]() {
                              lv_arc_set_value(uic_GrindScreen_dials_pressureGauge, pressure * 10.0f);
                              lv_label_set_text_fmt(uic_GrindScreen_dials_pressureText, "%.1f bar", pressure);
                              applyDialClusterTheme(ui_GrindScreen_dials, currentTemp, pressure);
                          },
                          &pressure);
    effect_mgr.use_effect([=] { return currentScreen == ui_SimpleProcessScreen; },
                          [=]() {
                              lv_arc_set_value(uic_SimpleProcessScreen_dials_pressureGauge, pressure * 10.0f);
                              lv_label_set_text_fmt(uic_SimpleProcessScreen_dials_pressureText, "%.1f bar", pressure);
                              applyDialClusterTheme(ui_SimpleProcessScreen_dials, currentTemp, pressure);
                          },
                          &pressure);
    effect_mgr.use_effect([=] { return currentScreen == ui_ProfileScreen; },
                          [=]() {
                              lv_arc_set_value(uic_ProfileScreen_dials_pressureGauge, pressure * 10.0f);
                              lv_label_set_text_fmt(uic_ProfileScreen_dials_pressureText, "%.1f bar", pressure);
                              applyDialClusterTheme(ui_ProfileScreen_dials, currentTemp, pressure);
                          },
                          &pressure);
    // ---- ManualBrewScreen effects ----
    effect_mgr.use_effect([=] { return currentScreen == ui_ManualBrewScreen; },
                          [=]() { adjustHeatingIndicator(ui_ManualBrewScreen_dials); },
                          &isTemperatureStable, &heatingFlash);
    effect_mgr.use_effect([=] { return currentScreen == ui_ManualBrewScreen; },
                          [=]() {
                              lv_arc_set_value(uic_ManualBrewScreen_dials_tempGauge, manualDisplayTemp);
                              lv_label_set_text_fmt(uic_ManualBrewScreen_dials_tempText, "%d°C", manualDisplayTemp);
                          },
                          &manualDisplayTemp);
    effect_mgr.use_effect([=] { return currentScreen == ui_ManualBrewScreen; },
                          [=]() {
                              lv_label_set_text_fmt(ui_ManualBrewScreen_tempLabel, "%d\xc2\xb0" "C", targetTemp);
                              adjustTempTarget(ui_ManualBrewScreen_dials);
                          },
                          &targetTemp);
    effect_mgr.use_effect([=] { return currentScreen == ui_ManualBrewScreen; },
                          [=]() {
                              // Move the pressure target needle to show the setpoint
                              {
                                  const double maxP = 9.0;
                                  const double pct = std::min(1.0, std::max(0.0, static_cast<double>(manualTargetPressure) / maxP));
                                  // Manual pressure arc uses reverse mode, so invert needle percentage.
                                  adjustTarget(ui_comp_get_child(ui_ManualBrewScreen_dials, UI_COMP_DIALS_PRESSURETARGET),
                                               1.0 - pct, 298.0, 124.0);
                              }

                              // Arc gauge: show actual while brewing, target while idle
                              const float shownPressure = active ? manualDisplayPressure : manualTargetPressure;
                              lv_arc_set_range(uic_ManualBrewScreen_dials_pressureGauge, 0, 90);
                              lv_arc_set_value(uic_ManualBrewScreen_dials_pressureGauge,
                                               (lv_coord_t)std::min(90.0f, std::max(0.0f, shownPressure * 10.0f)));
                              lv_label_set_text_fmt(uic_ManualBrewScreen_dials_pressureText, "%.1f", manualDisplayPressure);

                              // Bottom readouts: temp (left), pressure target (center), flow (right)
                              if (ui_ManualBrewScreen_liveLabel) {
                                  lv_label_set_text_fmt(ui_ManualBrewScreen_liveLabel, "%.1f", manualTargetPressure);
                                  lv_obj_set_style_border_color(ui_ManualBrewScreen_liveLabel, lv_color_hex(0x7FDBFF),
                                                                LV_PART_MAIN | LV_STATE_DEFAULT);
                                  lv_obj_set_style_border_width(ui_ManualBrewScreen_liveLabel, 2,
                                                                LV_PART_MAIN | LV_STATE_DEFAULT);
                                  lv_obj_set_style_border_opa(ui_ManualBrewScreen_liveLabel, LV_OPA_70,
                                                              LV_PART_MAIN | LV_STATE_DEFAULT);
                                  lv_obj_set_style_bg_color(ui_ManualBrewScreen_liveLabel, lv_color_black(),
                                                            LV_PART_MAIN | LV_STATE_DEFAULT);
                                  lv_obj_set_style_bg_opa(ui_ManualBrewScreen_liveLabel, LV_OPA_20,
                                                          LV_PART_MAIN | LV_STATE_DEFAULT);
                                  lv_obj_set_style_pad_left(ui_ManualBrewScreen_liveLabel, 12,
                                                            LV_PART_MAIN | LV_STATE_DEFAULT);
                                  lv_obj_set_style_pad_right(ui_ManualBrewScreen_liveLabel, 12,
                                                             LV_PART_MAIN | LV_STATE_DEFAULT);
                                  lv_obj_set_style_pad_top(ui_ManualBrewScreen_liveLabel, 6,
                                                           LV_PART_MAIN | LV_STATE_DEFAULT);
                                  lv_obj_set_style_pad_bottom(ui_ManualBrewScreen_liveLabel, 6,
                                                              LV_PART_MAIN | LV_STATE_DEFAULT);
                                  lv_obj_set_style_radius(ui_ManualBrewScreen_liveLabel, LV_RADIUS_CIRCLE,
                                                          LV_PART_MAIN | LV_STATE_DEFAULT);
                              }
                              if (ui_ManualBrewScreen_tempReadoutLabel) {
                                  lv_label_set_text_fmt(ui_ManualBrewScreen_tempReadoutLabel, "%d°C", targetTemp);
                              }
                              if (ui_ManualBrewScreen_flowReadoutLabel) {
                                  lv_label_set_text_fmt(ui_ManualBrewScreen_flowReadoutLabel, "%.1f ml/s", manualDisplayFlow);
                              }

                              // Push chart data at ~5 Hz for smoother shot curves.
                              static uint32_t lastChartPush = 0;
                              static bool pressureSeriesVisible = false;
                              static bool tempSeriesVisible = false;
                              static bool flowSeriesVisible = false;
                              const uint32_t nowMs = millis();
                              if (nowMs - lastChartPush >= 200 && ui_ManualBrewScreen_chart) {
                                  lastChartPush = nowMs;
                                  if (ui_ManualBrewScreen_chartPressureSeries) {
                                      if (active) {
                                          if (!pressureSeriesVisible) {
                                              lv_chart_set_all_value(ui_ManualBrewScreen_chart,
                                                                     ui_ManualBrewScreen_chartPressureSeries, 0);
                                              pressureSeriesVisible = true;
                                          }
                                          lv_chart_set_next_value(ui_ManualBrewScreen_chart,
                                                                  ui_ManualBrewScreen_chartPressureSeries,
                                                                  (lv_coord_t)(manualDisplayPressure * 10.0f));
                                      } else if (pressureSeriesVisible) {
                                          lv_chart_set_all_value(ui_ManualBrewScreen_chart,
                                                                 ui_ManualBrewScreen_chartPressureSeries,
                                                                 LV_CHART_POINT_NONE);
                                          pressureSeriesVisible = false;
                                      }
                                  }

                                  if (ui_ManualBrewScreen_chartTempSeries) {
                                      if (active) {
                                          if (!tempSeriesVisible) {
                                              lv_chart_set_all_value(ui_ManualBrewScreen_chart,
                                                                     ui_ManualBrewScreen_chartTempSeries,
                                                                     (lv_coord_t)manualDisplayTemp);
                                              tempSeriesVisible = true;
                                          }
                                          lv_chart_set_next_value(ui_ManualBrewScreen_chart,
                                                                  ui_ManualBrewScreen_chartTempSeries,
                                                                  (lv_coord_t)manualDisplayTemp);
                                      } else if (tempSeriesVisible) {
                                          lv_chart_set_all_value(ui_ManualBrewScreen_chart,
                                                                 ui_ManualBrewScreen_chartTempSeries,
                                                                 LV_CHART_POINT_NONE);
                                          tempSeriesVisible = false;
                                      }
                                  }

                                  if (ui_ManualBrewScreen_chartFlowSeries) {
                                      if (active) {
                                          if (!flowSeriesVisible) {
                                              lv_chart_set_all_value(ui_ManualBrewScreen_chart,
                                                                     ui_ManualBrewScreen_chartFlowSeries, 0);
                                              flowSeriesVisible = true;
                                          }
                                          lv_chart_set_next_value(ui_ManualBrewScreen_chart,
                                                                  ui_ManualBrewScreen_chartFlowSeries,
                                                                  (lv_coord_t)(manualDisplayFlow * 10.0f));
                                      } else if (flowSeriesVisible) {
                                          lv_chart_set_all_value(ui_ManualBrewScreen_chart,
                                                                 ui_ManualBrewScreen_chartFlowSeries,
                                                                 LV_CHART_POINT_NONE);
                                          flowSeriesVisible = false;
                                      }
                                  }
                                  lv_chart_refresh(ui_ManualBrewScreen_chart);
                              }
                          },
                          &manualDisplayPressure, &manualTargetPressure, &active, &targetTemp, &manualDisplayFlow);
    effect_mgr.use_effect([=] { return currentScreen == ui_ManualBrewScreen; },
                          [=]() {
                              // Timer
                              lv_label_set_text_fmt(ui_ManualBrewScreen_timerLabel, "%ds", manualDisplayElapsedSeconds);
                              // Go button icon
                              lv_imgbtn_set_src(ui_ManualBrewScreen_goBtn, LV_IMGBTN_STATE_RELEASED, nullptr,
                                                active ? &ui_img_1456692430 : &ui_img_445946954, nullptr);
                              // Focus highlights
                              const bool pressureFocused = currentManualIdx == 0;
                              const bool tempFocused = currentManualIdx == 1;
                              const bool goFocused = currentManualIdx == 2;
                              const unsigned long tNow = millis();
                              const bool plusPulse = (pressureFocused || tempFocused) && tNow < plusPulseUntil;
                              const bool minusPulse = (pressureFocused || tempFocused) && tNow < minusPulseUntil;

                              // Ensure target labels remain white and visible.
                              lv_obj_set_style_text_color(ui_ManualBrewScreen_tempLabel, lv_color_white(),
                                                          LV_PART_MAIN | LV_STATE_DEFAULT);
                              lv_obj_set_style_text_opa(ui_ManualBrewScreen_tempLabel, LV_OPA_100,
                                                        LV_PART_MAIN | LV_STATE_DEFAULT);
                              lv_obj_set_style_text_color(ui_ManualBrewScreen_timerLabel, lv_color_white(),
                                                          LV_PART_MAIN | LV_STATE_DEFAULT);
                              lv_obj_set_style_text_opa(ui_ManualBrewScreen_timerLabel, LV_OPA_100,
                                                        LV_PART_MAIN | LV_STATE_DEFAULT);
                              lv_obj_set_style_text_color(ui_ManualBrewScreen_weightLabel, lv_color_white(),
                                                          LV_PART_MAIN | LV_STATE_DEFAULT);
                              lv_obj_set_style_text_opa(ui_ManualBrewScreen_weightLabel, LV_OPA_100,
                                                        LV_PART_MAIN | LV_STATE_DEFAULT);

                              const lv_color_t pressureAccent = plusPulse ? lv_color_hex(0xB9FFCB)
                                                                          : minusPulse ? lv_color_hex(0xFFD3A1)
                                                                                       : lv_color_hex(0x7FDBFF);
                              (void)pressureAccent; // used below for chart border / tempLabel highlight

                              lv_obj_set_style_bg_color(ui_ManualBrewScreen_tempLabel, lv_color_white(),
                                                        LV_PART_MAIN | LV_STATE_DEFAULT);
                              lv_obj_set_style_bg_opa(ui_ManualBrewScreen_tempLabel,
                                                      tempFocused ? LV_OPA_10 : LV_OPA_TRANSP,
                                                      LV_PART_MAIN | LV_STATE_DEFAULT);
                              lv_obj_set_style_radius(ui_ManualBrewScreen_tempLabel, 14,
                                                      LV_PART_MAIN | LV_STATE_DEFAULT);
                              lv_obj_set_style_shadow_color(ui_ManualBrewScreen_tempLabel, lv_color_white(),
                                                            LV_PART_MAIN | LV_STATE_DEFAULT);
                              lv_obj_set_style_shadow_width(ui_ManualBrewScreen_tempLabel, tempFocused ? 16 : 0,
                                                            LV_PART_MAIN | LV_STATE_DEFAULT);
                              lv_obj_set_style_shadow_opa(ui_ManualBrewScreen_tempLabel,
                                                          tempFocused ? LV_OPA_50 : LV_OPA_TRANSP,
                                                          LV_PART_MAIN | LV_STATE_DEFAULT);
                              lv_obj_set_style_pad_left(ui_ManualBrewScreen_tempLabel, tempFocused ? 8 : 0,
                                                        LV_PART_MAIN | LV_STATE_DEFAULT);
                              lv_obj_set_style_pad_right(ui_ManualBrewScreen_tempLabel, tempFocused ? 8 : 0,
                                                         LV_PART_MAIN | LV_STATE_DEFAULT);
                              lv_obj_set_style_pad_top(ui_ManualBrewScreen_tempLabel, tempFocused ? 2 : 0,
                                                       LV_PART_MAIN | LV_STATE_DEFAULT);
                              lv_obj_set_style_pad_bottom(ui_ManualBrewScreen_tempLabel, tempFocused ? 2 : 0,
                                                          LV_PART_MAIN | LV_STATE_DEFAULT);
                              setIconFocusStyle(ui_ManualBrewScreen_goBtn, goFocused, true, true);
                              // Keep weight visible; sanitize invalid values to avoid garbled text.
                              lv_obj_clear_flag(ui_ManualBrewScreen_weightLabel, LV_OBJ_FLAG_HIDDEN);
                              if (bluetoothScales && std::isfinite(manualDisplayWeight) && manualDisplayWeight > -5000.0 &&
                                  manualDisplayWeight < 5000.0) {
                                  lv_label_set_text_fmt(ui_ManualBrewScreen_weightLabel, "%.1fg", manualDisplayWeight);
                              } else {
                                  lv_label_set_text(ui_ManualBrewScreen_weightLabel, "--.-g");
                              }
                          },
                          &active, &currentManualIdx, &manualDisplayElapsedSeconds, &plusPulseUntil, &minusPulseUntil,
                          &bluetoothScales, &manualDisplayWeight);
    effect_mgr.use_effect([=] { return currentScreen == ui_StandbyScreen; },
                          [=]() {
                              updateAvailable ? lv_obj_clear_flag(ui_StandbyScreen_updateIcon, LV_OBJ_FLAG_HIDDEN)
                                              : lv_obj_add_flag(ui_StandbyScreen_updateIcon, LV_OBJ_FLAG_HIDDEN);
                          },
                          &updateAvailable);
    effect_mgr.use_effect([=] { return currentScreen == ui_StandbyScreen; },
                          [=]() {
                              bool deactivated = true;
                              if (updateActive) {
                                  lv_label_set_text_fmt(ui_StandbyScreen_mainLabel, "Updating...");
                              } else if (error) {
                                  if (controller->getError() == ERROR_CODE_RUNAWAY) {
                                      lv_label_set_text_fmt(ui_StandbyScreen_mainLabel, "Temperature error, please restart");
                                  }
                              } else if (autotuning) {
                                  lv_label_set_text_fmt(ui_StandbyScreen_mainLabel, "Autotuning...");
                              } else if (waitingForController) {
                                  lv_label_set_text_fmt(ui_StandbyScreen_mainLabel, "Waiting for controller...");
                              } else {
                                  lv_label_set_text_fmt(ui_StandbyScreen_mainLabel, "Booting display...");
                                  deactivated = false;
                              }
                              _ui_flag_modify(ui_StandbyScreen_mainLabel, LV_OBJ_FLAG_HIDDEN, deactivated);
                              _ui_flag_modify(ui_StandbyScreen_touchIcon, LV_OBJ_FLAG_HIDDEN, !deactivated);
                              _ui_flag_modify(ui_StandbyScreen_statusContainer, LV_OBJ_FLAG_HIDDEN, !deactivated);
                          },
                          &updateAvailable, &error, &autotuning, &waitingForController, &initialized);
    effect_mgr.use_effect([=] { return currentScreen == ui_BrewScreen; },
                          [=]() {
                              if (brewVolumetric) {
                                  lv_label_set_text_fmt(ui_BrewScreen_targetDuration, "%.1fg", targetVolume);
                              } else {
                                  const double secondsDouble = targetDuration;
                                  const auto minutes = static_cast<int>(secondsDouble / 60.0);
                                  const auto seconds = static_cast<int>(secondsDouble) % 60;
                                  lv_label_set_text_fmt(ui_BrewScreen_targetDuration, "%2d:%02d", minutes, seconds);
                              }
                          },
                          &targetDuration, &targetVolume, &brewVolumetric);
    effect_mgr.use_effect([=] { return currentScreen == ui_GrindScreen; },
                          [=]() {
                              if (volumetricMode) {
                                  lv_label_set_text_fmt(ui_GrindScreen_targetDuration, "%.1fg", grindVolume);
                              } else {
                                  const double secondsDouble = grindDuration / 1000.0;
                                  const auto minutes = static_cast<int>(secondsDouble / 60.0);
                                  const auto seconds = static_cast<int>(secondsDouble) % 60;
                                  lv_label_set_text_fmt(ui_GrindScreen_targetDuration, "%2d:%02d", minutes, seconds);
                              }
                          },
                          &grindDuration, &grindVolume, &volumetricMode);
    effect_mgr.use_effect([=] { return currentScreen == ui_BrewScreen; },
                          [=]() {
                              lv_img_set_src(ui_BrewScreen_Image4, brewVolumetric ? &ui_img_1424216268 : &ui_img_360122106);
                              _ui_flag_modify(ui_BrewScreen_byTimeButton, LV_OBJ_FLAG_HIDDEN, brewVolumetric);
                          },
                          &brewVolumetric);
    effect_mgr.use_effect(
        [=] { return currentScreen == ui_GrindScreen; },
        [=]() {
            lv_img_set_src(ui_GrindScreen_targetSymbol, volumetricMode ? &ui_img_1424216268 : &ui_img_360122106);
            ui_object_set_themeable_style_property(ui_GrindScreen_weightLabel, LV_PART_MAIN | LV_STATE_DEFAULT,
                                                   LV_STYLE_TEXT_COLOR,
                                                   volumetricMode ? _ui_theme_color_Dark : _ui_theme_color_NiceWhite);
            ui_object_set_themeable_style_property(ui_GrindScreen_volumetricButton, LV_PART_MAIN | LV_STATE_DEFAULT,
                                                   LV_STYLE_IMG_RECOLOR,
                                                   volumetricMode ? _ui_theme_color_Dark : _ui_theme_color_NiceWhite);
            ui_object_set_themeable_style_property(ui_GrindScreen_modeSwitch, LV_PART_MAIN | LV_STATE_DEFAULT, LV_STYLE_BG_COLOR,
                                                   volumetricMode ? _ui_theme_color_NiceWhite : _ui_theme_color_Dark);
        },
        &volumetricMode);
    effect_mgr.use_effect([=] { return currentScreen == ui_GrindScreen; },
                          [=]() { _ui_flag_modify(ui_GrindScreen_modeSwitch, LV_OBJ_FLAG_HIDDEN, volumetricAvailable); },
                          &volumetricAvailable);
    effect_mgr.use_effect([=] { return currentScreen == ui_SimpleProcessScreen; },
                          [=]() {
                              if (currentSimpleProcessIdx == 1 &&
                                  lv_obj_has_flag(ui_SimpleProcessScreen_goButton, LV_OBJ_FLAG_HIDDEN)) {
                                  currentSimpleProcessIdx = 0;
                              }

                              if (mode == MODE_STEAM) {
                                  _ui_flag_modify(ui_SimpleProcessScreen_goButton, LV_OBJ_FLAG_HIDDEN, active);
                                  lv_imgbtn_set_src(ui_SimpleProcessScreen_goButton, LV_IMGBTN_STATE_RELEASED, nullptr,
                                                    &ui_img_691326438, nullptr);
                              } else {
                                  lv_imgbtn_set_src(ui_SimpleProcessScreen_goButton, LV_IMGBTN_STATE_RELEASED, nullptr,
                                                    active ? &ui_img_1456692430 : &ui_img_445946954, nullptr);
                              }

                              const bool tempFocused = currentSimpleProcessIdx == 0;
                              const bool goFocused = currentSimpleProcessIdx == 1 || mode == MODE_WATER;
                              const unsigned long now = millis();
                              const bool plusPulse = tempFocused && now < plusPulseUntil;
                              const bool minusPulse = tempFocused && now < minusPulseUntil;

                              // Highlight the number itself, matching profile-settings style intent.
                              lv_obj_set_style_bg_color(ui_SimpleProcessScreen_targetTemp, lv_color_white(),
                                                        LV_PART_MAIN | LV_STATE_DEFAULT);
                              lv_obj_set_style_bg_opa(ui_SimpleProcessScreen_targetTemp,
                                                    tempFocused ? LV_OPA_10 : LV_OPA_TRANSP,
                                                      LV_PART_MAIN | LV_STATE_DEFAULT);
                              lv_obj_set_style_radius(ui_SimpleProcessScreen_targetTemp, 14,
                                                      LV_PART_MAIN | LV_STATE_DEFAULT);
                              lv_obj_set_style_pad_left(ui_SimpleProcessScreen_targetTemp, tempFocused ? 8 : 0,
                                                        LV_PART_MAIN | LV_STATE_DEFAULT);
                              lv_obj_set_style_pad_right(ui_SimpleProcessScreen_targetTemp, tempFocused ? 8 : 0,
                                                         LV_PART_MAIN | LV_STATE_DEFAULT);
                              lv_obj_set_style_text_opa(ui_SimpleProcessScreen_targetTemp,
                                                        tempFocused ? LV_OPA_100 : LV_OPA_70,
                                                        LV_PART_MAIN | LV_STATE_DEFAULT);

                              setIconFocusStyle(ui_SimpleProcessScreen_upTempButton, plusPulse, false, true);
                              setIconFocusStyle(ui_SimpleProcessScreen_downTempButton, minusPulse, false, true);
                              setIconFocusStyle(ui_SimpleProcessScreen_goButton, goFocused, true, true);
                          },
                          &active, &mode, &currentSimpleProcessIdx, &plusPulseUntil, &minusPulseUntil);
    effect_mgr.use_effect([=] { return currentScreen == ui_GrindScreen; },
                          [=]() {
                              lv_imgbtn_set_src(ui_GrindScreen_startButton, LV_IMGBTN_STATE_RELEASED, nullptr,
                                                grindActive ? &ui_img_1456692430 : &ui_img_445946954, nullptr);
                          },
                          &grindActive);
    effect_mgr.use_effect([=] { return currentScreen == ui_StatusScreen; },
                          [=]() { setIconFocusStyle(ui_StatusScreen_pauseButton, true, true); },
                          &active, &mode);
    effect_mgr.use_effect([=] { return currentScreen == ui_BrewScreen; },
                          [=] { lv_label_set_text(ui_BrewScreen_profileName, selectedProfile.label.c_str()); },
                          &selectedProfileId);

    effect_mgr.use_effect(
        [=] { return currentScreen == ui_ProfileScreen; },
        [=] {
            if (profileLoaded) {
                _ui_flag_modify(ui_ProfileScreen_profileDetails, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_REMOVE);
                _ui_flag_modify(ui_ProfileScreen_loadingSpinner, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_ADD);
                lv_label_set_text(ui_ProfileScreen_profileName, favoritedProfiles[currentProfileIdx].label.c_str());
                lv_label_set_text(ui_ProfileScreen_mainLabel, currentProfileIdx == 0 ? "Current profile" : "Select profile");

                const auto minutes = static_cast<int>(favoritedProfiles[currentProfileIdx].getTotalDuration() / 60.0 - 0.5);
                const auto seconds = static_cast<int>(favoritedProfiles[currentProfileIdx].getTotalDuration()) % 60;
                lv_label_set_text_fmt(ui_ProfileScreen_targetDuration2, "%2d:%02d", minutes, seconds);
                lv_label_set_text_fmt(ui_ProfileScreen_targetTemp2, "%d°C",
                                      static_cast<int>(favoritedProfiles[currentProfileIdx].temperature));
                unsigned int phaseCount = favoritedProfiles[currentProfileIdx].getPhaseCount();
                unsigned int stepCount = favoritedProfiles[currentProfileIdx].phases.size();
                lv_label_set_text_fmt(ui_ProfileScreen_stepsLabel, "%d step%s", stepCount, stepCount > 1 ? "s" : "");
                lv_label_set_text_fmt(ui_ProfileScreen_phasesLabel, "%d phase%s", phaseCount, phaseCount > 1 ? "s" : "");
            } else {
                _ui_flag_modify(ui_ProfileScreen_profileDetails, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_ADD);
                _ui_flag_modify(ui_ProfileScreen_loadingSpinner, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_REMOVE);
            }

            ui_object_set_themeable_style_property(ui_ProfileScreen_previousProfileBtn, LV_PART_MAIN | LV_STATE_DEFAULT,
                                                   LV_STYLE_IMG_RECOLOR,
                                                   currentProfileIdx > 0 ? _ui_theme_color_NiceWhite : _ui_theme_color_SemiDark);
            ui_object_set_themeable_style_property(ui_ProfileScreen_previousProfileBtn, LV_PART_MAIN | LV_STATE_DEFAULT,
                                                   LV_STYLE_IMG_RECOLOR_OPA,
                                                   currentProfileIdx > 0 ? _ui_theme_alpha_NiceWhite : _ui_theme_alpha_SemiDark);
            ui_object_set_themeable_style_property(
                ui_ProfileScreen_nextProfileBtn, LV_PART_MAIN | LV_STATE_DEFAULT, LV_STYLE_IMG_RECOLOR,
                currentProfileIdx < favoritedProfiles.size() - 1 ? _ui_theme_color_NiceWhite : _ui_theme_color_SemiDark);
            ui_object_set_themeable_style_property(
                ui_ProfileScreen_nextProfileBtn, LV_PART_MAIN | LV_STATE_DEFAULT, LV_STYLE_IMG_RECOLOR_OPA,
                currentProfileIdx < favoritedProfiles.size() - 1 ? _ui_theme_alpha_NiceWhite : _ui_theme_alpha_SemiDark);

            const bool canSelectProfile = profileLoaded && !favoritedProfiles.empty();
            if (canSelectProfile) {
                setIconFocusStyle(ui_ProfileScreen_chooseButton, true, false);
            } else {
                setIconFocusStyle(ui_ProfileScreen_chooseButton, false, false);
            }
        },
        &currentProfileIdx, &profileLoaded);

    // Show/hide grind button based on SmartGrind setting or Alt Relay function
    effect_mgr.use_effect([=] { return currentScreen == ui_MenuScreen; },
                          [=]() {
                              grindAvailable ? lv_obj_clear_flag(ui_MenuScreen_grindBtn, LV_OBJ_FLAG_HIDDEN)
                                             : lv_obj_add_flag(ui_MenuScreen_grindBtn, LV_OBJ_FLAG_HIDDEN);
                          },
                          &grindAvailable);
    effect_mgr.use_effect([=] { return currentScreen == ui_MenuScreen; },
                          [=]() {
                              const auto buttons = getVisibleMenuButtons();
                              if (buttons.empty()) {
                                  currentMenuIdx = 0;
                                  return;
                              }

                              if (currentMenuIdx >= buttons.size()) {
                                  currentMenuIdx = 0;
                              }

                              for (size_t index = 0; index < buttons.size(); index++) {
                                  lv_obj_t *button = buttons[index];
                                  const bool isFocused = index == currentMenuIdx;
                                  setIconFocusStyle(button, isFocused, false, true);
                              }
                          },
                          &currentMenuIdx, &grindAvailable);
    effect_mgr.use_effect([=] { return currentScreen == ui_BrewScreen; },
                          [=]() {
                              if (volumetricAvailable && bluetoothScales) {
                                  lv_label_set_text_fmt(ui_BrewScreen_weightLabel, "%.1fg", bluetoothWeight);
                              } else {
                                  lv_label_set_text(ui_BrewScreen_weightLabel, "-");
                              }
                          },
                          &bluetoothWeight, &volumetricAvailable, &bluetoothScales);
    effect_mgr.use_effect([=] { return currentScreen == ui_GrindScreen; },
                          [=]() {
                              if (volumetricAvailable && bluetoothScales) {
                                  lv_label_set_text_fmt(ui_GrindScreen_weightLabel, "%.1fg", bluetoothWeight);
                              } else {
                                  lv_label_set_text(ui_GrindScreen_weightLabel, "-");
                              }
                          },
                          &bluetoothWeight, &volumetricAvailable, &bluetoothScales);
    effect_mgr.use_effect(
        [=] { return currentScreen == ui_BrewScreen; },
        [=]() {
            _ui_flag_modify(ui_BrewScreen_adjustments, LV_OBJ_FLAG_HIDDEN, brewScreenState == BrewScreenState::Settings);
            _ui_flag_modify(ui_BrewScreen_acceptButton, LV_OBJ_FLAG_HIDDEN, brewScreenState == BrewScreenState::Settings);
            _ui_flag_modify(ui_BrewScreen_saveButton, LV_OBJ_FLAG_HIDDEN, brewScreenState == BrewScreenState::Settings);
            _ui_flag_modify(ui_BrewScreen_saveAsNewButton, LV_OBJ_FLAG_HIDDEN, brewScreenState == BrewScreenState::Settings);
            _ui_flag_modify(ui_BrewScreen_startButton, LV_OBJ_FLAG_HIDDEN, brewScreenState == BrewScreenState::Brew);
            _ui_flag_modify(ui_BrewScreen_profileInfo, LV_OBJ_FLAG_HIDDEN, brewScreenState == BrewScreenState::Brew);
            _ui_flag_modify(ui_BrewScreen_modeSwitch, LV_OBJ_FLAG_HIDDEN,
                            brewScreenState == BrewScreenState::Brew && volumetricAvailable);
            if (volumetricAvailable) {
                lv_img_set_src(ui_BrewScreen_volumetricButton, bluetoothScales ? &ui_img_1424216268 : &ui_img_flowmeter_png);
            }
        },
        &brewScreenState, &volumetricAvailable, &bluetoothScales);
    effect_mgr.use_effect(
        [=] { return currentScreen == ui_BrewScreen; },
        [=]() {
            const bool tempFocused = brewScreenState == BrewScreenState::Settings && !brewSettingsActionMode &&
                                     currentBrewSettingsIdx == 0;
            const bool durationFocused = brewScreenState == BrewScreenState::Settings && !brewSettingsActionMode &&
                                         currentBrewSettingsIdx == 1;

            lv_obj_set_style_bg_color(ui_BrewScreen_tempContainer, lv_color_white(), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(ui_BrewScreen_tempContainer, tempFocused ? LV_OPA_10 : LV_OPA_TRANSP,
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_radius(ui_BrewScreen_tempContainer, 18, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_color(ui_BrewScreen_tempContainer, lv_color_white(), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(ui_BrewScreen_tempContainer, tempFocused ? 1 : 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_opa(ui_BrewScreen_tempContainer, tempFocused ? LV_OPA_40 : LV_OPA_TRANSP,
                                        LV_PART_MAIN | LV_STATE_DEFAULT);

            lv_obj_set_style_bg_color(ui_BrewScreen_targetContainer, lv_color_white(), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(ui_BrewScreen_targetContainer, durationFocused ? LV_OPA_10 : LV_OPA_TRANSP,
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_radius(ui_BrewScreen_targetContainer, 18, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_color(ui_BrewScreen_targetContainer, lv_color_white(), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(ui_BrewScreen_targetContainer, durationFocused ? 1 : 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_opa(ui_BrewScreen_targetContainer, durationFocused ? LV_OPA_40 : LV_OPA_TRANSP,
                                        LV_PART_MAIN | LV_STATE_DEFAULT);

            const auto actionButtons = getBrewSettingsActionButtons();
            for (size_t i = 0; i < actionButtons.size(); i++) {
                lv_obj_t *btn = actionButtons[i];
                const bool focused = brewScreenState == BrewScreenState::Settings && brewSettingsActionMode &&
                                     static_cast<int>(i) == currentBrewActionIdx;
                const bool emphasize = btn == ui_BrewScreen_acceptButton;
                setIconFocusStyle(btn, focused, emphasize);
            }

            const unsigned long now = millis();
            const bool inSettingsAdjustMode = brewScreenState == BrewScreenState::Settings && !brewSettingsActionMode;
            const bool plusPulse = inSettingsAdjustMode && now < plusPulseUntil;
            const bool minusPulse = inSettingsAdjustMode && now < minusPulseUntil;
            const bool tempAdjust = inSettingsAdjustMode && currentBrewSettingsIdx == 0;
            const bool durationAdjust = inSettingsAdjustMode && currentBrewSettingsIdx == 1;

            setIconFocusStyle(ui_BrewScreen_upTempButton, tempAdjust && plusPulse, false, true);
            setIconFocusStyle(ui_BrewScreen_downTempButton, tempAdjust && minusPulse, false, true);
            setIconFocusStyle(ui_BrewScreen_upDurationButton, durationAdjust && plusPulse, false, true);
            setIconFocusStyle(ui_BrewScreen_downDurationButton, durationAdjust && minusPulse, false, true);

            const auto topButtons = getBrewTopButtons();
            for (size_t i = 0; i < topButtons.size(); i++) {
                lv_obj_t *btn = topButtons[i];
                const bool focused = brewScreenState == BrewScreenState::Brew && static_cast<int>(i) == currentBrewTopIdx;
                const bool isPlay = btn == ui_BrewScreen_startButton;
                setIconFocusStyle(btn, focused, isPlay, isPlay);
            }
        },
        &brewScreenState, &currentBrewTopIdx, &currentBrewSettingsIdx, &currentBrewActionIdx, &brewSettingsActionMode,
        &plusPulseUntil, &minusPulseUntil);
    effect_mgr.use_effect(
        [=] { return currentScreen == ui_BrewScreen; },
        [=]() {
            ui_object_set_themeable_style_property(ui_BrewScreen_saveButton, LV_PART_MAIN | LV_STATE_DEFAULT,
                                                   LV_STYLE_IMG_RECOLOR,
                                                   profileDirty ? _ui_theme_color_NiceWhite : _ui_theme_color_SemiDark);
            ui_object_set_themeable_style_property(ui_BrewScreen_saveButton, LV_PART_MAIN | LV_STATE_DEFAULT,
                                                   LV_STYLE_IMG_RECOLOR_OPA,
                                                   profileDirty ? _ui_theme_alpha_NiceWhite : _ui_theme_alpha_SemiDark);
            ui_object_set_themeable_style_property(ui_BrewScreen_saveAsNewButton, LV_PART_MAIN | LV_STATE_DEFAULT,
                                                   LV_STYLE_IMG_RECOLOR,
                                                   profileDirty ? _ui_theme_color_NiceWhite : _ui_theme_color_SemiDark);
            ui_object_set_themeable_style_property(ui_BrewScreen_saveAsNewButton, LV_PART_MAIN | LV_STATE_DEFAULT,
                                                   LV_STYLE_IMG_RECOLOR_OPA,
                                                   profileDirty ? _ui_theme_alpha_NiceWhite : _ui_theme_alpha_SemiDark);
        },
        &brewScreenState, &profileDirty);
    effect_mgr.use_effect([=] { return currentScreen == ui_StandbyScreen; },
                          [=]() { lv_img_set_src(ui_StandbyScreen_logo, christmasMode ? &ui_img_1510335 : &ui_img_logo_png); },
                          &christmasMode);
}

void DefaultUI::handleScreenChange() {
    lv_obj_t *current = lv_scr_act();

    if (current != *targetScreen) {
        if (*targetScreen == ui_StandbyScreen) {
            standbyEnterTime = millis();
        } else if (current == ui_StandbyScreen) {
            const Settings &settings = controller->getSettings();
            setBrightness(settings.getMainBrightness());
        }

        _ui_screen_change(targetScreen, LV_SCR_LOAD_ANIM_NONE, 0, 0, targetScreenInit);
        lv_obj_del(current);
        rerender = true;
    }
}

void DefaultUI::updateStandbyScreen() {
    if (standbyEnterTime > 0) {
        const Settings &settings = controller->getSettings();
        const unsigned long now = millis();
        if (now - standbyEnterTime >= settings.getStandbyBrightnessTimeout()) {
            setBrightness(settings.getStandbyBrightness());
        }
    }

    if (!apActive && WiFi.status() == WL_CONNECTED && !updateActive && !error && !autotuning && !waitingForController &&
        initialized) {
        time_t now;
        struct tm timeinfo;

        localtime_r(&now, &timeinfo);
        // allocate enough space for both 12h/24h time formats
        if (getLocalTime(&timeinfo, 500)) {
            char time[9];
            Settings &settings = controller->getSettings();
            const char *format = settings.isClock24hFormat() ? "%H:%M" : "%I:%M %p";
            strftime(time, sizeof(time), format, &timeinfo);
            lv_label_set_text(ui_StandbyScreen_time, time);
            lv_obj_clear_flag(ui_StandbyScreen_time, LV_OBJ_FLAG_HIDDEN);

            christmasMode = (timeinfo.tm_mon == 11 && timeinfo.tm_mday < 27) || (timeinfo.tm_mon == 0 && timeinfo.tm_mday < 6);
        }
    } else {
        lv_obj_add_flag(ui_StandbyScreen_time, LV_OBJ_FLAG_HIDDEN);
    }
    controller->getClientController()->isConnected() ? lv_obj_clear_flag(ui_StandbyScreen_bluetoothIcon, LV_OBJ_FLAG_HIDDEN)
                                                     : lv_obj_add_flag(ui_StandbyScreen_bluetoothIcon, LV_OBJ_FLAG_HIDDEN);
    !apActive &&WiFi.status() == WL_CONNECTED ? lv_obj_clear_flag(ui_StandbyScreen_wifiIcon, LV_OBJ_FLAG_HIDDEN)
                                              : lv_obj_add_flag(ui_StandbyScreen_wifiIcon, LV_OBJ_FLAG_HIDDEN);
}

void DefaultUI::updateStatusScreen() const {
    // Copy process pointers to avoid race conditions with controller thread
    Process *process = controller->getProcess();
    Process *lastProcess = controller->getLastProcess();

    if (process == nullptr) {
        process = lastProcess;
    }
    if (process == nullptr || process->getType() != MODE_BREW) {
        return;
    }

    // Additional safety: Validate that the process pointer is still valid
    // by checking if it matches either current or last process
    if (process != controller->getProcess() && process != controller->getLastProcess()) {
        ESP_LOGW("DefaultUI", "Process pointer became invalid during access, skipping update");
        return;
    }

    auto *brewProcess = static_cast<BrewProcess *>(process);
    if (brewProcess == nullptr) {
        ESP_LOGE("DefaultUI", "brewProcess is null after cast");
        return;
    }

    // Validate the brewProcess object before accessing its members
    // Check if the object is in a reasonable state by validating key fields
    if (brewProcess->profile.phases.empty() || brewProcess->phaseIndex >= brewProcess->profile.phases.size()) {
        ESP_LOGE("DefaultUI", "brewProcess phaseIndex out of bounds: %u >= %zu", brewProcess->phaseIndex,
                 brewProcess->profile.phases.size());
        return;
    }

    // Final safety check before accessing brewProcess members
    if (!brewProcess) {
        ESP_LOGE("DefaultUI", "brewProcess became null after validation");
        return;
    }

    const auto phase = brewProcess->currentPhase;

    unsigned long now = millis();
    if (!process->isActive()) {
        // Add bounds check for finished timestamp
        if (brewProcess && brewProcess->finished > 0) {
            now = brewProcess->finished;
        }
    }

    lv_label_set_text(ui_StatusScreen_stepLabel, phase.phase == PhaseType::PHASE_TYPE_BREW ? "BREW" : "INFUSION");
    String phaseText = "Finished";
    if (process->isActive()) {
        phaseText = phase.name;
    } else if (controller->getSettings().isDelayAdjust() && !process->isComplete()) {
        phaseText = "Calibrating...";
    }
    lv_label_set_text(ui_StatusScreen_phaseLabel, phaseText.c_str());

    // Add bounds check for processStarted timestamp
    if (brewProcess && brewProcess->processStarted > 0 && now >= brewProcess->processStarted) {
        const unsigned long processDuration = now - brewProcess->processStarted;
        const double processSecondsDouble = processDuration / 1000.0;
        const auto processMinutes = static_cast<int>(processSecondsDouble / 60.0);
        const auto processSeconds = static_cast<int>(processSecondsDouble) % 60;
        lv_label_set_text_fmt(ui_StatusScreen_currentDuration, "%2d:%02d", processMinutes, processSeconds);
    } else {
        lv_label_set_text_fmt(ui_StatusScreen_currentDuration, "00:00");
    }

    if (brewProcess && brewProcess->target == ProcessTarget::VOLUMETRIC && phase.hasVolumetricTarget()) {
        Target target = phase.getVolumetricTarget();
        lv_bar_set_value(ui_StatusScreen_brewBar, brewProcess->currentVolume * 10.0, LV_ANIM_OFF);
        lv_bar_set_range(ui_StatusScreen_brewBar, 0, target.value * 10.0 + 1.0);
        lv_label_set_text_fmt(ui_StatusScreen_brewLabel, "%.1f / %.1fg", brewProcess->currentVolume, target.value);
    } else if (brewProcess) {
        // Add bounds check for currentPhaseStarted timestamp
        if (brewProcess->currentPhaseStarted > 0 && now >= brewProcess->currentPhaseStarted) {
            const unsigned long progress = now - brewProcess->currentPhaseStarted;
            lv_bar_set_value(ui_StatusScreen_brewBar, progress, LV_ANIM_OFF);
            lv_bar_set_range(ui_StatusScreen_brewBar, 0, std::max(static_cast<int>(brewProcess->getPhaseDuration()), 1));
            lv_label_set_text_fmt(ui_StatusScreen_brewLabel, "%d / %ds", progress / 1000, brewProcess->getPhaseDuration() / 1000);
        } else {
            lv_bar_set_value(ui_StatusScreen_brewBar, 0, LV_ANIM_OFF);
            lv_bar_set_range(ui_StatusScreen_brewBar, 0, 1);
            lv_label_set_text(ui_StatusScreen_brewLabel, "0s");
        }
    }

    if (brewProcess && brewProcess->target == ProcessTarget::TIME) {
        const unsigned long targetDuration = brewProcess->getTotalDuration();
        const double targetSecondsDouble = targetDuration / 1000.0;
        const auto targetMinutes = static_cast<int>(targetSecondsDouble / 60.0);
        const auto targetSeconds = static_cast<int>(targetSecondsDouble) % 60;
        lv_label_set_text_fmt(ui_StatusScreen_targetDuration, "%2d:%02d", targetMinutes, targetSeconds);
    } else if (brewProcess) {
        lv_label_set_text_fmt(ui_StatusScreen_targetDuration, "%.1fg", brewProcess->getBrewVolume());
    }
    if (brewProcess) {
        lv_img_set_src(ui_StatusScreen_Image8,
                       brewProcess->target == ProcessTarget::TIME ? &ui_img_360122106 : &ui_img_1424216268);
    }

    if (brewProcess && brewProcess->isAdvancedPump()) {
        float pressure = brewProcess->getPumpPressure();
        const double percentage = 1.0 - static_cast<double>(pressure) / static_cast<double>(pressureScaling);
        adjustTarget(uic_StatusScreen_dials_pressureTarget, percentage, -62.0, 124.0);
    } else {
        const double percentage = 1.0 - 0.5;
        adjustTarget(uic_StatusScreen_dials_pressureTarget, percentage, -62.0, 124.0);
    }

    // Brew finished adjustments
    if (process->isActive()) {
        lv_obj_add_flag(ui_StatusScreen_brewVolume, LV_OBJ_FLAG_HIDDEN);
    } else {
        // Re-validate brewProcess pointer before accessing members
        if (brewProcess && brewProcess->target == ProcessTarget::VOLUMETRIC) {
            lv_obj_clear_flag(ui_StatusScreen_brewVolume, LV_OBJ_FLAG_HIDDEN);
        }
        lv_obj_add_flag(ui_StatusScreen_barContainer, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_StatusScreen_labelContainer, LV_OBJ_FLAG_HIDDEN);
        if (brewProcess) {
            lv_label_set_text_fmt(ui_StatusScreen_brewVolume, "%.1lfg", brewProcess->currentVolume);
        }
        lv_imgbtn_set_src(ui_StatusScreen_pauseButton, LV_IMGBTN_STATE_RELEASED, nullptr, &ui_img_631115820, nullptr);
    }
}

void DefaultUI::adjustDials(lv_obj_t *dials) {
    lv_obj_t *tempGauge = ui_comp_get_child(dials, UI_COMP_DIALS_TEMPGAUGE);
    lv_obj_t *tempText = ui_comp_get_child(dials, UI_COMP_DIALS_TEMPTEXT);
    lv_obj_t *pressureTarget = ui_comp_get_child(dials, UI_COMP_DIALS_PRESSURETARGET);
    lv_obj_t *pressureGauge = ui_comp_get_child(dials, UI_COMP_DIALS_PRESSUREGAUGE);
    lv_obj_t *pressureText = ui_comp_get_child(dials, UI_COMP_DIALS_PRESSURETEXT);
    lv_obj_t *pressureSymbol = ui_comp_get_child(dials, UI_COMP_DIALS_IMAGE6);
    const int pressureHidden = pressureAvailable ? _UI_MODIFY_FLAG_REMOVE : _UI_MODIFY_FLAG_ADD;
    _ui_flag_modify(pressureTarget, LV_OBJ_FLAG_HIDDEN, pressureHidden);
    _ui_flag_modify(pressureGauge, LV_OBJ_FLAG_HIDDEN, pressureHidden);
    _ui_flag_modify(pressureText, LV_OBJ_FLAG_HIDDEN, pressureHidden);
    _ui_flag_modify(pressureSymbol, LV_OBJ_FLAG_HIDDEN, pressureHidden);
    lv_obj_set_x(tempText, pressureAvailable ? -50 : 0);
    lv_obj_set_y(tempText, pressureAvailable ? -205 : -180);
    lv_arc_set_bg_angles(tempGauge, 118, 242);
    lv_arc_set_range(pressureGauge, 0, pressureScaling * 10);
}

inline void DefaultUI::adjustTempTarget(lv_obj_t *dials) {
    constexpr double gaugeAngle = 124.0;
    constexpr double gaugeStart = 118.0;
    const int clampedTemp = std::clamp(targetTemp, MIN_TEMP, MAX_TEMP);
    const double percentage = static_cast<double>(clampedTemp - MIN_TEMP) /
                              static_cast<double>(MAX_TEMP - MIN_TEMP);
    lv_obj_t *tempTarget = ui_comp_get_child(dials, UI_COMP_DIALS_TEMPTARGET);
    adjustTarget(tempTarget, percentage, gaugeStart, gaugeAngle);
}

void DefaultUI::applyTheme() {
    const Settings &settings = controller->getSettings();
    int newThemeMode = settings.getThemeMode();

    if (newThemeMode != currentThemeMode) {
        currentThemeMode = newThemeMode;
        ui_theme_set(currentThemeMode);
    }
}

void DefaultUI::adjustTarget(lv_obj_t *obj, double percentage, double start, double range) const {
    double angle = start + range - range * percentage;

    lv_img_set_angle(obj, angle * -10);
    int x = static_cast<int>(std::cos(angle * M_PI / 180.0f) * 235.0);
    int y = static_cast<int>(std::sin(angle * M_PI / 180.0f) * -235.0);
    lv_obj_set_pos(obj, x, y);
}

void DefaultUI::applyDialClusterTheme(lv_obj_t *dials, int tempC, float pressureBar) const {
    (void)dials;
    (void)tempC;
    (void)pressureBar;
}

// Get gradient color for temperature (yellow→orange→red: coolwarm to brewing to steam)
// 87-94°C (optimal brewing), 140+°C (steam)
inline lv_color_t DefaultUI::getTempGradientColor(int tempC) const {
    // Requested palette: yellow -> orange -> red
    const int clamped = std::clamp(tempC, MIN_TEMP, 145);

    if (clamped <= 94) {
        // Espresso band: warm yellow to orange-red
        const int ratio = (clamped - MIN_TEMP) * 100 / std::max(1, (94 - MIN_TEMP));
        const int r = 255;
        const int g = 228 - (ratio * 108) / 100; // 228 -> 120
        const int b = 40 - (ratio * 40) / 100;   // 40  -> 0
        return lv_color_make(r, std::max(0, g), std::max(0, b));
    }

    // Steam band: orange-red to deep red
    const int ratio = (clamped - 94) * 100 / std::max(1, (145 - 94));
    const int r = 255;
    const int g = 120 - (ratio * 120) / 100; // 120 -> 0
    return lv_color_make(r, std::max(0, g), 0);
}

// Get gradient color for pressure (blue → purple)
inline lv_color_t DefaultUI::getPressureGradientColor(float pressureBar) const {
    // 0 bar: blue (0x3366FF)
    // 9 bar: purple (0x9933FF)
    float ratio = std::min(1.0f, pressureBar / 9.0f);
    int r = static_cast<int>(51.0f + (ratio * (153.0f - 51.0f)));
    int g = static_cast<int>(102.0f - (ratio * 102.0f));
    int b = 255;
    return lv_color_make(r, g, b);
}

void DefaultUI::loopTask(void *arg) {
    auto *ui = static_cast<DefaultUI *>(arg);
    while (true) {
        ui->loop();
        vTaskDelay(25 / portTICK_PERIOD_MS);
    }
}

void DefaultUI::profileLoopTask(void *arg) {
    auto *ui = static_cast<DefaultUI *>(arg);
    while (true) {
        ui->loopProfiles();
        vTaskDelay(25 / portTICK_PERIOD_MS);
    }
}
