#pragma once

#include "esphome.h"
#include "esphome/components/pulse_counter/pulse_counter_sensor.h"

using esphome::gpio::GPIOBinaryOutput;
using esphome::pulse_counter::PulseCounterSensor;

class BehaviorChanger {
public:
	virtual void set_idle() = 0;

	virtual void start_feeding(float power, float amount) = 0;

	virtual void continue_feeding() = 0;

	virtual void start_anti_jam() = 0;
};

class Behavior {
public:

	virtual void tick(float amount) = 0;

};

class IdleBehavior : public Behavior {
public:
	void tick(float amount) override {};
};

class FeedingBehavior : public Behavior {
public:
	FeedingBehavior(BehaviorChanger *behaviorChanger, FeederController *controller) : behaviorChanger(behaviorChanger), controller(controller) {}

	void setup(float power, float amount) {
		amountLimit = amount;
		initialPower = power;
	}

	void tick(float amount) override {
		const auto currentPower = controller->get_power();
		amountLimit -= amount;

		if (currentPower < initialPower) {
			controller->set_power(initialPower);
		}

		if (amountLimit <= 0) {
			behaviorChanger->set_idle();
			return;
		}

		const bool isSlowSpeed = amount <= 0.05;
		const bool isMaxPower = currentPower >= 1.0f;

		if (isSlowSpeed && !isMaxPower) {
			controller->set_power(currentPower + 0.02f);
			return;
		};

		const bool isZeroSpeed = amount <= 0.01f;

		if (isZeroSpeed && isMaxPower) {
			behaviorChanger->start_anti_jam();
		};
	};

private:
	BehaviorChanger * const behaviorChanger;
	FeederController * const controller;
	float amountLimit;
	float initialPower;
};

class AntiJamBehavior : public Behavior {
public:
	AntiJamBehavior(BehaviorChanger *behaviorChanger, FeederController *controller) :
		behaviorChanger(behaviorChanger), controller(controller) {}

	void start(float power, int ticks) {
		totalAmount = 0;
		this->power = power;
		ticksAmount = ticks;
		controller->toggle_direction();
		controller->set_power(power);
	}

	void tick(float amount) override {
		totalAmount += amount;
		ticksAmount--;
		const bool timeIsUp = ticksAmount <= 0;
		if (timeIsUp) {
			controller->set_power(0);
			controller->toggle_direction();
		}

		if (timeIsUp && totalAmount > 0) {
			behaviorChanger->continue_feeding();
			return;
		}

		if (timeIsUp) {
			behaviorChanger->set_idle();
			return;
		}
	};
private:
	BehaviorChanger * const behaviorChanger;
	FeederController * const controller;
	float power;
	int ticksAmount;
	float totalAmount;
};


class Watchdog : public BehaviorChanger, PollingComponent {

public:
	Watchdog(GPIOBinaryOutput *driver_speed_power, PulseCounterSensor *pulse_counter_sensor, FeederController *controller):
		driver_speed_power(driver_speed_power),
		pulse_counter_sensor(pulse_counter_sensor),
		controller(controller),
		feedingBehavior(FeedingBehavior(this, controller)),
		antiJamBehavior(AntiJamBehavior(this, controller)),
		currentBehavior(&idleBehavior) {
	};

	void setup() {
		driver_speed_power->turn_on();
		set_update_interval(pulse_counter_sensor->get_update_interval());
	};

	float get_setup_priority() const override { return setup_priority::PROCESSOR; }


	void start_feeding(float power, float amount) override {
		ESP_LOGI(TAG, "start_feeding");
		feedingBehavior.setup(power, amount);
		continue_feeding();
	}

	void continue_feeding() override {
		ESP_LOGI(TAG, "continue_feeding");
		currentBehavior = &feedingBehavior;
	}

	void start_anti_jam() override {
		ESP_LOGI(TAG, "start_anti_jam");
		auto ticksAmount = 600 / pulse_counter_sensor->get_update_interval();
		if (ticksAmount <= 0) ticksAmount = 1;
		antiJamBehavior.start(1.0f, ticksAmount);
		currentBehavior = &antiJamBehavior;
	}

	void set_idle() override {
		ESP_LOGI(TAG, "set_idle");
		controller->set_power(0);
		currentBehavior = &idleBehavior;
	}

	void update() override {
		const auto state = pulse_counter_sensor->get_state();
		const auto amountTick = (state != state) ? 0.0f : state / 60000.0f * float(pulse_counter_sensor->get_update_interval());
		currentBehavior->tick(amountTick);
	};

private:
	GPIOBinaryOutput * const driver_speed_power;
	PulseCounterSensor * const pulse_counter_sensor;
	FeederController * const controller;
	IdleBehavior idleBehavior;
	FeedingBehavior feedingBehavior;
	AntiJamBehavior antiJamBehavior;
	Behavior * currentBehavior;
};
