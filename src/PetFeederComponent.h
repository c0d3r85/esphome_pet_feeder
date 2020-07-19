#include "esphome.h"
#include "esphome/components/pulse_counter/pulse_counter_sensor.h"

using esphome::gpio::GPIOBinaryOutput;
using esphome::pulse_counter::PulseCounterSensor;

static const char *TAG = "pet_feeder";

static const float start_power = 0.4f;

class FeederController {
public:
  virtual void setup() {};

  virtual void set_power(float power) = 0;
  virtual float get_power() = 0;
  virtual void toggle_direction() = 0;
};

class SimpleFeederController: public FeederController {
public:
  SimpleFeederController(output::FloatOutput *driver_pwm, GPIOBinaryOutput *driver_direction_output): driver_pwm(driver_pwm), driver_direction_output(driver_direction_output), power(0) {};

  virtual void setup() {
    driver_direction_output->turn_off();
    driver_pwm->set_level(0.0f);
    sync_inversion();
  };

  void set_power(float power) override {
    ESP_LOGD(TAG, "Set power: %f", power);
    this->power = power;
    driver_pwm->set_level(power);
  };

  float get_power() override {
    return power;
  };

  void toggle_direction() override {
    ESP_LOGD(TAG, "Toggle direction");
    driver_pwm->set_inverted(!driver_pwm->is_inverted());
    sync_inversion();
  };

protected:
  void sync_inversion() {
    driver_direction_output->set_inverted(driver_pwm->is_inverted());
  };

private:
  output::FloatOutput * const driver_pwm;
  GPIOBinaryOutput * const driver_direction_output;
  float power;
};



class Watchdog : public PollingComponent {

public:
  Watchdog(GPIOBinaryOutput *driver_speed_power, PulseCounterSensor *pulse_counter_sensor, FeederController *controller):
  driver_speed_power(driver_speed_power),
  pulse_counter_sensor(pulse_counter_sensor),
  controller(controller),
  amount(0),
  amountLimit(100),
  anti_jam(false) {};

  void setup() {
    ESP_LOGI(TAG, "Watchdog setup %d", pulse_counter_sensor->get_update_interval());
    driver_speed_power->turn_on();
    set_update_interval(pulse_counter_sensor->get_update_interval());
  };

  float get_setup_priority() const override { return setup_priority::PROCESSOR; }

  void reset() {
    controller->set_power(0.0f);
    anti_jam = false;
    amount = 0;
  }

  void set_amount_limit(float amount) {
    amountLimit = amount;
  }

  void update() override {
    const auto power = controller->get_power();
    const bool isFeeding = power > 0.0f;

    if (!isFeeding) return;

    const auto state = pulse_counter_sensor->get_state();
    const auto amountTick = (state != state) ? 0.0f : state / 60000.0f * float(pulse_counter_sensor->get_update_interval());
    amount += amountTick;
    ESP_LOGD(TAG, "amountTick %f %f %f", amountTick, amount, power);
    if (anti_jam) {
      anti_jam = false;
      if (amountTick > 0) {
        controller->toggle_direction();
        controller->set_power(start_power);
        ESP_LOGI(TAG, "Anti-jam mode succseed");
        return;
      }
      controller->set_power(0.0f);
      ESP_LOGE(TAG, "Anti-jam mode failed");
      
      return;
    };


    if (amount >= amountLimit) {
      controller->set_power(0.0f);
      return;
    }

    const bool isSlowSpeed = amountTick <= 0.05;
    const bool isMaxPower = power >= 1.0f;

    if (isFeeding && isSlowSpeed && !isMaxPower) {
      controller->set_power(power + 0.02f);
      return;
    };

    const bool isZeroSpeed = amountTick <= 0.01f;

    if (isFeeding && isZeroSpeed && isMaxPower) {
      anti_jam = true;
      controller->toggle_direction();
      controller->set_power(1.0f);
      ESP_LOGI(TAG, "Anti-jam mode started");
    };
  };

private:
  GPIOBinaryOutput * const driver_speed_power;
  PulseCounterSensor * const pulse_counter_sensor;
  FeederController * const controller;
  float amount;
  float amountLimit;
  bool anti_jam;
};

class PetFeederComponent : public Component, public CustomAPIDevice {
 public:
  PetFeederComponent(Watchdog *watchdog, FeederController *controller):
  controller(controller),
  watchdog(watchdog) {

  };

  float get_setup_priority() const override { return setup_priority::PROCESSOR; }

  void setup() override {
    controller->setup();
    // watchdog->setup();

    register_service(&PetFeederComponent::on_start_feed, "start_feed",
                     {"power", "amount"});

  }

  void on_start_feed(float power, float amount) {
    ESP_LOGD("custom", "Starting pet feeder! (power: %f, amount: %f)", power, amount);
    watchdog->reset();
    watchdog->set_amount_limit(amount);
    controller->set_power(power);
    // do something with arguments

    // Call a homeassistant service
    // call_homeassistant_service("homeassistant.service");
  }

  // void loop() override {
  //   // This will be called very often after setup time.
  //   // think of it as the loop() call in Arduino
  //   // if (digitalRead(5)) {
  //   //   digitalWrite(6, HIGH);

  //   //   // You can also log messages
  //     // ESP_LOGD("custom", "The GPIO pin 5 is HIGH!");
  //   // }
  // }

 private:
  FeederController * const controller;
  Watchdog * const watchdog;
  // FeederState state;
};
