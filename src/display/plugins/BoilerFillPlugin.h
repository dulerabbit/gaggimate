#ifndef BOILERFILLPLUGIN_H
#define BOILERFILLPLUGIN_H

#include <display/core/Plugin.h>
#include <display/core/PluginManager.h>
#include <display/core/Controller.h>
#include <display/core/Settings.h>

struct Event;

class BoilerFillPlugin : public Plugin {
  public:
    void setup(Controller *controller, PluginManager *pluginManager) override;
    void loop() override {};

  private:
    Controller *controller = nullptr;
};

#endif // BOILERFILLPLUGIN_H
