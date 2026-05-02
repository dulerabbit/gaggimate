#include "main.h"
#ifdef ELECROW_ROTARY_21
#include <Adafruit_PCF8574.h>
#endif

#ifndef GAGGIMATE_HEADLESS
#include <lvgl.h>
#endif

#ifdef ELECROW_ROTARY_21
#include "drivers/Waveshare/utilities.h"
#include <display/ui/default/DefaultUI.h>

// Encoder state — written in ISR, read in loop
static volatile int  enc_delta    = 0;  // accumulated rotation steps
static volatile bool enc_clk_last = false;

static void IRAM_ATTR encoderISR() {
    bool clk = digitalRead(ELECROW_ENC_CLK_PIN);
    if (clk == enc_clk_last) {
        return;
    }

    bool dt  = digitalRead(ELECROW_ENC_DT_PIN);
    // Count both CLK edges for snappier one-click profile moves.
    enc_delta += (clk == dt) ? 1 : -1;
    enc_clk_last = clk;
}

// Button debounce state (PCF8574 P5, active LOW)
static bool     enc_btn_raw_last = true;
static bool     enc_btn_stable_state = true;  // HIGH = released (INPUT_PULLUP)
static uint32_t enc_btn_raw_change_ms = 0;
static uint32_t enc_btn_press_start_ms = 0;
static bool     enc_btn_long_handled = false;
static constexpr uint32_t ENC_BTN_DEBOUNCE_MS = 30;
static constexpr uint32_t ENC_BTN_LONG_PRESS_MS = 700;
#endif

Controller &controller = *Controller::getInstance();

#ifdef ELECROW_ROTARY_21
Adafruit_PCF8574 pcf;
#endif

void setup() {
    Serial.begin(115200);
#ifdef ELECROW_ROTARY_21
    Wire.begin(38, 39);
    Wire.setClock(400000UL);
    uint8_t pcfAddr = 0x21;
    bool pcfOk = pcf.begin(pcfAddr, &Wire);
    if (!pcfOk) {
        pcfAddr = 0x20;
        pcfOk = pcf.begin(pcfAddr, &Wire);
    }

    if (pcfOk) {
        Serial.printf("[ELECROW] PCF8574 detected at 0x%02X\n", pcfAddr);
        pcf.pinMode(0, OUTPUT);  // P0 = touch chip reset
        pcf.pinMode(2, INPUT_PULLUP); // P2 = touch IRQ (idle high)
        pcf.pinMode(3, OUTPUT);  // P3 = LCD power
        pcf.pinMode(4, OUTPUT);  // P4 = LCD reset
        pcf.pinMode(ELECROW_ENC_SW_BIT, INPUT_PULLUP); // P5 = encoder button
        pcf.digitalWrite(0, LOW);   // Assert touch reset
        pcf.digitalWrite(3, HIGH);  // LCD power on
        pcf.digitalWrite(4, HIGH);  // LCD reset released
        delay(120);
        pcf.digitalWrite(0, HIGH);  // Release touch chip reset
        delay(250);                  // Allow touch controller firmware to boot
    } else {
        Serial.println(F("[ELECROW] PCF8574 not detected at 0x21 or 0x20"));
    }

    // Encoder quadrature input (CLK + DT are direct GPIOs)
    pinMode(ELECROW_ENC_CLK_PIN, INPUT_PULLUP);
    pinMode(ELECROW_ENC_DT_PIN,  INPUT_PULLUP);
    enc_clk_last = digitalRead(ELECROW_ENC_CLK_PIN);
    attachInterrupt(digitalPinToInterrupt(ELECROW_ENC_CLK_PIN), encoderISR, CHANGE);
#endif
    delay(100);
    Controller::getInstance()->begin();
}

void loop() {
    Controller::getInstance()->loop();

#ifdef ELECROW_ROTARY_21
    // --- Rotary encoder rotation ---
    int delta = 0;
    noInterrupts();
    delta = enc_delta;
    enc_delta = 0;
    interrupts();

    if (delta != 0) {
        DefaultUI *ui = controller.getUI();
        if (ui) {
            if (ui->isProfileScreenActive()) {
                if (delta > 0) {
                    for (int i = 0; i < delta; i++) ui->onNextProfile();
                } else {
                    for (int i = 0; i < -delta; i++) ui->onPreviousProfile();
                }
            } else if (ui->isSimpleProcessScreenActive()) {
                ui->onSimpleProcessRotate(delta);
            } else if (ui->isBrewSettingsActive()) {
                ui->onBrewSettingsRotate(delta);
            } else if (ui->isBrewScreenActive()) {
                ui->onBrewScreenRotate(delta);
            } else if (ui->isMenuScreenActive()) {
                if (delta > 0) {
                    for (int i = 0; i < delta; i++) ui->onNextMenuItem();
                } else {
                    for (int i = 0; i < -delta; i++) ui->onPreviousMenuItem();
                }
            }
        }
    }

    // --- Encoder button (PCF8574 P5, polled every 50 ms) ---
    static uint32_t enc_poll_ms = 0;
    uint32_t now = millis();
    if (now - enc_poll_ms >= 20) {
        enc_poll_ms = now;
        bool rawBtn = pcf.digitalRead(ELECROW_ENC_SW_BIT);  // LOW = pressed
        DefaultUI *ui = controller.getUI();

        if (rawBtn != enc_btn_raw_last) {
            enc_btn_raw_last = rawBtn;
            enc_btn_raw_change_ms = now;
        }

        if (!enc_btn_stable_state && !enc_btn_long_handled && ui &&
            now - enc_btn_press_start_ms >= ENC_BTN_LONG_PRESS_MS) {
            if (ui->isMenuScreenActive()) {
                controller.activateStandby();
                enc_btn_long_handled = true;
            } else if (ui->isBrewSettingsActive()) {
                ui->onBrewSettingsToggleMode();
                enc_btn_long_handled = true;
            } else if (ui->isBackNavigableScreenActive()) {
                ui->onBackNavigate();
                enc_btn_long_handled = true;
            }
        }

        if (now - enc_btn_raw_change_ms >= ENC_BTN_DEBOUNCE_MS && enc_btn_stable_state != enc_btn_raw_last) {
            enc_btn_stable_state = enc_btn_raw_last;

            if (!enc_btn_stable_state) {
                // Pressed (falling edge)
                enc_btn_press_start_ms = now;
                enc_btn_long_handled = false;
            } else {
                // Released (rising edge)
                const uint32_t pressDuration = now - enc_btn_press_start_ms;
                if (!enc_btn_long_handled && ui) {
                    if (ui->isStandbyScreenActive()) {
                        ui->wakeToMenu();
                    } else if (ui->isProfileScreenActive()) {
                        ui->onProfileSelect();
                    } else if (ui->isSimpleProcessScreenActive()) {
                        ui->onSimpleProcessPress();
                    } else if (ui->isStatusScreenActive()) {
                        ui->onStatusScreenSelect();
                    } else if (ui->isBrewSettingsActive()) {
                        ui->onBrewSettingsPress();
                    } else if (ui->isBrewScreenActive()) {
                        ui->onBrewScreenSelect();
                    } else if (ui->isMenuScreenActive() && pressDuration < ENC_BTN_LONG_PRESS_MS) {
                        ui->onMenuItemSelect();
                    }
                }
            }
        }
    }
#endif

    delay(1);
}
