#ifndef MANUALPRESSUREPROCESS_H
#define MANUALPRESSUREPROCESS_H

#include <display/core/constants.h>
#include <display/core/process/Process.h>

// Safety: 5 minutes max for manual mode (same as brew safety duration)
constexpr int MANUAL_SAFETY_DURATION_MS = 300000;

class ManualPressureProcess : public Process {
  public:
    unsigned long started;

    explicit ManualPressureProcess() { started = millis(); }

    // Solenoid (3-way valve) ON: directs water to group head
    bool isRelayActive() override { return true; }

    bool isAltRelayActive() override { return false; }

    // Pump value not used — pressure is controlled via sendAdvancedOutputControl
    float getPumpValue() override { return isActive() ? 100.f : 0.f; }

    void progress() override {}

    bool isActive() override { return millis() - started < MANUAL_SAFETY_DURATION_MS; }

    bool isComplete() override { return !isActive(); }

    int getType() override { return MODE_MANUAL; }

    void updateVolume(double volume) override {}
};

#endif // MANUALPRESSUREPROCESS_H
