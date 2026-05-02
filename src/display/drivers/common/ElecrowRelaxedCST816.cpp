#include "ElecrowRelaxedCST816.h"

#define CST8XX_REG_STATUS 0x00
#define CST8XX_REG_XPOS_HIGH 0x03
#define CST8XX_REG_XPOS_LOW 0x04
#define CST8XX_REG_YPOS_HIGH 0x05
#define CST8XX_REG_YPOS_LOW 0x06
#define CST8XX_REG_DIS_AUTOSLEEP 0xFE
#define CST8XX_REG_CHIP_ID 0xA7
#define CST8XX_REG_FW_VERSION 0xA9
#define CST8XX_REG_SLEEP 0xE5

#define CST816S_CHIP_ID 0xB4
#define CST816T_CHIP_ID 0xB5
#define CST716_CHIP_ID 0x20
#define CST820_CHIP_ID 0xB7
#define CST816D_CHIP_ID 0xB6

ElecrowRelaxedCST816::ElecrowRelaxedCST816() : _centerButtonX(0), _centerButtonY(0) {}

bool ElecrowRelaxedCST816::begin(PLATFORM_WIRE_TYPE &wire, uint8_t address, int sda, int scl) {
    return SensorCommon::begin(wire, address, sda, scl);
}

bool ElecrowRelaxedCST816::begin(uint8_t addr, iic_fptr_t readRegCallback, iic_fptr_t writeRegCallback) {
    return SensorCommon::begin(addr, readRegCallback, writeRegCallback);
}

void ElecrowRelaxedCST816::reset() {
    if (__rst != SENSOR_PIN_NONE) {
        this->setGpioMode(__rst, OUTPUT);
        this->setGpioLevel(__rst, LOW);
        delay(30);
        this->setGpioLevel(__rst, HIGH);
        delay(50);
    }
}

uint8_t ElecrowRelaxedCST816::getPoint(int16_t *x_array, int16_t *y_array, uint8_t get_point) {
    uint8_t buffer[13];
    if (readRegister(CST8XX_REG_STATUS, buffer, 13) == DEV_WIRE_ERR) {
        return 0;
    }

    if (!buffer[2] || !x_array || !y_array || !get_point) {
        return 0;
    }

    if (buffer[2] == 0xFF) {
        return 0;
    }

    uint8_t point = buffer[2] & 0x0F;
    if (point > 1) {
        return 0;
    }

    int16_t tmpX = ((buffer[CST8XX_REG_XPOS_HIGH] & 0x0F) << 8) | buffer[CST8XX_REG_XPOS_LOW];
    int16_t tmpY = ((buffer[CST8XX_REG_YPOS_HIGH] & 0x0F) << 8) | buffer[CST8XX_REG_YPOS_LOW];

    if (tmpX == _centerButtonX && tmpY == _centerButtonY && __homeButtonCb) {
        __homeButtonCb(__userData);
        return 0;
    }

    x_array[0] = tmpX;
    y_array[0] = tmpY;
    updateXY(point, x_array, y_array);
    return point;
}

bool ElecrowRelaxedCST816::isPressed() {
    if (__irq != SENSOR_PIN_NONE) {
        return this->getGpioLevel(__irq) == LOW;
    }

    int16_t x = 0;
    int16_t y = 0;
    return getPoint(&x, &y, 1) > 0;
}

const char *ElecrowRelaxedCST816::getModelName() {
    switch (__chipID) {
    case CST816S_CHIP_ID:
        return "CST816S";
    case CST816T_CHIP_ID:
        return "CST816T";
    case CST716_CHIP_ID:
        return "CST716";
    case CST820_CHIP_ID:
        return "CST820";
    case CST816D_CHIP_ID:
        return "CST816D";
    default:
        return "CST820";
    }
}

void ElecrowRelaxedCST816::sleep() {
    writeRegister(CST8XX_REG_SLEEP, 0x03);
}

void ElecrowRelaxedCST816::wakeup() { reset(); }

void ElecrowRelaxedCST816::idle() {}

uint8_t ElecrowRelaxedCST816::getSupportTouchPoint() { return 1; }

bool ElecrowRelaxedCST816::getResolution(int16_t *x, int16_t *y) {
    (void)x;
    (void)y;
    return false;
}

void ElecrowRelaxedCST816::setCenterButtonCoordinate(int16_t x, int16_t y) {
    _centerButtonX = x;
    _centerButtonY = y;
}

void ElecrowRelaxedCST816::setHomeButtonCallback(home_button_callback_t cb, void *user_data) {
    __homeButtonCb = cb;
    __userData = user_data;
}

void ElecrowRelaxedCST816::disableAutoSleep() {
    reset();
    delay(50);
    writeRegister(CST8XX_REG_DIS_AUTOSLEEP, 0x01);
}

void ElecrowRelaxedCST816::enableAutoSleep() {
    reset();
    delay(50);
    writeRegister(CST8XX_REG_DIS_AUTOSLEEP, 0x00);
}

void ElecrowRelaxedCST816::setGpioCallback(gpio_mode_fptr_t mode_cb,
                                           gpio_write_fptr_t write_cb,
                                           gpio_read_fptr_t read_cb) {
    SensorCommon::setGpioModeCallback(mode_cb);
    SensorCommon::setGpioWriteCallback(write_cb);
    SensorCommon::setGpioReadCallback(read_cb);
}

bool ElecrowRelaxedCST816::initImpl() {
    if (__rst != SENSOR_PIN_NONE) {
        this->setGpioMode(__rst, OUTPUT);
    }

    if (__irq != SENSOR_PIN_NONE) {
        this->setGpioMode(__irq, INPUT);
    }

    reset();

    int chipId = readRegister(CST8XX_REG_CHIP_ID);
    int version = readRegister(CST8XX_REG_FW_VERSION);
    log_i("Chip ID:0x%x", chipId);
    log_i("Version :0x%x", version);

    if (chipId == 0x00 && version == 0x01) {
        __chipID = CST820_CHIP_ID;
        log_w("[ELECROW] Accepting CST touch with chip_id=0x0 and version=0x1");
        return true;
    }

    if (chipId != CST816S_CHIP_ID && chipId != CST816T_CHIP_ID && chipId != CST820_CHIP_ID &&
        chipId != CST816D_CHIP_ID && (chipId != CST716_CHIP_ID || version == 0)) {
        log_e("Chip ID does not match, should be CST816S:0X%02X , CST816T:0X%02X , CST816D:0X%02X , CST820:0X%02X , CST716:0X%02X",
              CST816S_CHIP_ID, CST816T_CHIP_ID, CST816D_CHIP_ID, CST820_CHIP_ID, CST716_CHIP_ID);
        return false;
    }

    __chipID = chipId;
    log_i("Touch type:%s", getModelName());
    return true;
}

int ElecrowRelaxedCST816::getReadMaskImpl() { return -1; }