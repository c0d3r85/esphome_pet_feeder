#pragma once

#include "esphome.h"

using esphome::gpio::GPIOBinaryOutput;
using esphome::pulse_counter::PulseCounterSensor;

static const char *TAG = "pet_feeder";

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
		sync_inversion();
		driver_pwm->set_level(0.0f);
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
		driver_direction_output->turn_off();
	};

private:
	output::FloatOutput * const driver_pwm;
	GPIOBinaryOutput * const driver_direction_output;
	float power;
};