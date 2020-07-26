#pragma once

#include "esphome.h"
#include "FeederController.h"
#include "Watchdog.h"


class PetFeederComponent : public Component, public CustomAPIDevice {
public:
  PetFeederComponent(Watchdog *watchdog, FeederController *controller):
    controller(controller),
    watchdog(watchdog) {

  };

  float get_setup_priority() const override { return setup_priority::PROCESSOR; }

  void setup() override {
    controller->setup();

    register_service(&PetFeederComponent::on_start_feed, "start_feed",
    {"power", "amount"});

  }

  void on_start_feed(float power, float amount) {
    ESP_LOGD(TAG, "Starting pet feeder! (power: %f, amount: %f)", power, amount);
    watchdog->start_feeding(power, amount);
  }

private:
  FeederController * const controller;
  Watchdog * const watchdog;
};
