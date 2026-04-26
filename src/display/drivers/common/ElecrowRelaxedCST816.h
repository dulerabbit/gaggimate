#pragma once

#include <TouchDrvInterface.hpp>
#include <SensorCommon.tpp>
#include <SensorLib.h>

class ElecrowRelaxedCST816 : public TouchDrvInterface,
                             public SensorCommon<ElecrowRelaxedCST816> {
    friend class SensorCommon<ElecrowRelaxedCST816>;

  public:
    ElecrowRelaxedCST816();

    bool begin(PLATFORM_WIRE_TYPE &wire, uint8_t address, int sda, int scl) override;

    bool begin(uint8_t addr, iic_fptr_t readRegCallback, iic_fptr_t writeRegCallback) override;

    void reset() override;

    uint8_t getPoint(int16_t *x_array, int16_t *y_array, uint8_t get_point) override;

    bool isPressed() override;

    const char *getModelName() override;

    void sleep() override;

    void wakeup() override;

    void idle() override;

    uint8_t getSupportTouchPoint() override;

    bool getResolution(int16_t *x, int16_t *y) override;

    void setCenterButtonCoordinate(int16_t x, int16_t y);

    void setHomeButtonCallback(home_button_callback_t cb, void *user_data);

    void disableAutoSleep();

    void enableAutoSleep();

    void setGpioCallback(gpio_mode_fptr_t mode_cb,
                         gpio_write_fptr_t write_cb,
                         gpio_read_fptr_t read_cb) override;

  private:
    bool initImpl();
    int getReadMaskImpl();

    int16_t _centerButtonX;
    int16_t _centerButtonY;
};