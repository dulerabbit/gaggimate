#ifndef DEFAULTUI_H
#define DEFAULTUI_H

#include "lvgl.h"
#include <display/core/PluginManager.h>
#include <display/core/ProfileManager.h>
#include <display/core/constants.h>
#include <display/drivers/Driver.h>
#include <display/models/profile.h>

#include "./lvgl/ui.h"

class Controller;

constexpr int RERENDER_INTERVAL_IDLE = 2500;
constexpr int RERENDER_INTERVAL_ACTIVE = 100;

constexpr int TEMP_HISTORY_INTERVAL = 250;
constexpr int TEMP_HISTORY_LENGTH = 20 * 1000 / TEMP_HISTORY_INTERVAL;

int16_t calculate_angle(int set_temp, int range, int offset);

enum class BrewScreenState { Brew, Settings };

class DefaultUI {
  public:
    DefaultUI(Controller *controller, Driver *driver, PluginManager *pluginManager);

    // Default work methods
    void init();
    void loop();
    void loopProfiles();

    // Interface methods
    void changeScreen(lv_obj_t **screen, void (*target_init)(void));

    void changeBrewScreenMode(BrewScreenState state);
    void onProfileSwitch();
    void onNextProfile();
    void onPreviousProfile();
    void onProfileSelect();
    bool isProfileScreenActive() const;
    bool isMenuScreenActive() const;
    bool isStandbyScreenActive() const;
    bool isBrewScreenActive() const;
    bool isBrewSettingsActive() const;
    bool isSimpleProcessScreenActive() const;
    bool isManualScreenActive() const;
    bool isStatusScreenActive() const;
    bool isBackNavigableScreenActive() const;
    void onNextMenuItem();
    void onPreviousMenuItem();
    void onMenuItemSelect();
    void onBrewScreenRotate(int delta);
    void onBrewScreenSelect();
    void onSimpleProcessRotate(int delta);
    void onSimpleProcessPress();
    void onManualScreenRotate(int delta);
    void onManualPressureTouch(float deltaBar);
    void onManualScreenPress();
    void onStatusScreenSelect();
    void onBackNavigate();
    void onBrewSettingsRotate(int delta);
    void onBrewSettingsPress();
    void onBrewSettingsToggleMode();
    void focusBrewSettingsTemp();
    void focusBrewSettingsTarget();
    void wakeToMenu();
    void setBrightness(int brightness) {
        if (panelDriver) {
            panelDriver->setBrightness(brightness);
        }
    };

    void onVolumetricDelete();

    void markDirty() { rerender = true; }
    void markProfileDirty() { profileDirty = true; }
    void markProfileClean() { profileDirty = false; }

    void applyTheme();

  private:
    void setupPanel();
    void setupState();
    void setupReactive();

    void handleScreenChange();

    void updateStandbyScreen();
    void updateStatusScreen() const;

    void adjustDials(lv_obj_t *dials);
    void adjustTempTarget(lv_obj_t *dials);
    void adjustTarget(lv_obj_t *obj, double percentage, double start, double range) const;
    void applyDialClusterTheme(lv_obj_t *dials, int tempC, float pressureBar) const;
    lv_color_t getTempGradientColor(int tempC) const;      // Gradient: cool→yellow→orange→red for temperature
    lv_color_t getPressureGradientColor(float pressureBar) const;  // Gradient: blue→purple for pressure

    int tempHistory[TEMP_HISTORY_LENGTH] = {0};
    int tempHistoryIndex = 0;
    int prevTargetTemp = 0;
    bool isTempHistoryInitialized = false;
    int isTemperatureStable = false;
    unsigned long lastTempLog = 0;

    void updateTempHistory();
    void updateTempStableFlag();
    void adjustHeatingIndicator(lv_obj_t *contentPanel);
    void reloadProfiles();
    void triggerAdjustmentPulse(bool increase);
    void triggerGaugeAdjustmentPulse(bool isTempGauge);
    bool hasActiveAdjustmentPulse(unsigned long now) const;
    void showManualSavePrompt(const String &shotId);
    void hideManualSavePrompt();
    void updateManualSavePromptSelection();
    void onManualSavePromptConfirm();
    void saveManualShotAsProfile(const String &shotId);
    String getNextManualProfileLabel() const;
    static void onManualSavePromptButton(lv_event_t *e);

    Driver *panelDriver = nullptr;
    Controller *controller;
    PluginManager *pluginManager;
    ProfileManager *profileManager;

    // Screen state
    String selectedProfileId = "";
    Profile selectedProfile{};
    int updateAvailable = false;
    int updateActive = false;
    int apActive = false;
    int error = false;
    int autotuning = false;
    int waitingForController = false;
    int volumetricAvailable = false;
    int bluetoothScales = false;
    int volumetricMode = false;
    int brewVolumetric = false;
    int profileVolumetric = false;
    int grindActive = false;
    int active = false;
    int smartGrindActive = false;
    int grindAvailable = false;
    int initialized = false;

    // Seasonal flags
    int christmasMode = false;

    bool rerender = false;
    unsigned long lastRender = 0;

    int mode = MODE_STANDBY;
    int currentTemp = 0;
    int targetTemp = 0;
    float targetDuration = 0;
    float targetVolume = 0;
    int grindDuration = 0;
    float grindVolume = 0.0f;
    int pressureAvailable = 1;
    float pressure = 0.0f;
    int pressureScaling = DEFAULT_PRESSURE_SCALING;
    int heatingFlash = 0;
    double bluetoothWeight = 0.0;
    BrewScreenState brewScreenState = BrewScreenState::Brew;

    int profileDirty = 0;
    int currentProfileIdx;
    int currentMenuIdx = 0;
    int currentBrewTopIdx = 2;
    int currentBrewSettingsIdx = 0;
    int currentBrewActionIdx = 1;
    int currentSimpleProcessIdx = 0;
    int currentManualIdx = 0;  // 0 = pressure, 1 = temp, 2 = go
    bool brewSettingsActionMode = false;
    unsigned long plusPulseUntil = 0;
    unsigned long minusPulseUntil = 0;
    unsigned long tempAdjustPulseUntil = 0;
    unsigned long pressureAdjustPulseUntil = 0;
    int profileLoaded = 0;
    float manualTargetPressure = 6.0f;
    int manualDisplayTemp = 0;
    float manualDisplayPressure = 0.0f;
    float manualDisplayFlow = 0.0f;
    double manualDisplayWeight = 0.0;
    int manualDisplayElapsedSeconds = 0;
    unsigned long manualStartMs = 0;
    bool manualWasActive = false;
    bool manualFreezeData = false;
    bool manualPromptRequested = false;
    bool manualSavePromptVisible = false;
    bool manualSavePromptSelectionYes = true;
    String manualPromptShotId;
    float manualPromptShotDurationSec = 0.0f;
    float manualPromptShotVolume = 0.0f;
    lv_obj_t *manualSavePrompt = nullptr;
    lv_obj_t *manualSavePromptYesBtn = nullptr;
    lv_obj_t *manualSavePromptNoBtn = nullptr;
    std::vector<String> favoritedProfileIds;
    std::vector<Profile> favoritedProfiles;
    int currentThemeMode = -1; // Force applyTheme on first loop

    // Screen change
    lv_obj_t **targetScreen = &ui_StandbyScreen;
    lv_obj_t *currentScreen = ui_StandbyScreen;
    void (*targetScreenInit)(void) = &ui_StandbyScreen_screen_init;

    // Standby brightness control
    unsigned long standbyEnterTime = 0;

    xTaskHandle taskHandle;
    static void loopTask(void *arg);
    xTaskHandle profileTaskHandle;
    static void profileLoopTask(void *arg);
};

#endif // DEFAULTUI_H
