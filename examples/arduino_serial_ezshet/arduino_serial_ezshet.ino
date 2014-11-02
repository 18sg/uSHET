/**
 * A simple Arduino example for uSHET/EZSHET implementing SHET over UART using
 * the Arduino Serial IO wrapper.
 *
 * To "make it real" you can use socat to connect the serial port with the SHET:
 *
 *     socat file:/dev/ttyUSB0,b115200,raw,echo=0 tcp:127.0.0.1:11235,reuseaddr
 */

#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include "shet.h"
#include "ezshet.h"

#include "shet_io_arduino_serial.h"

const int led_pin = 13;

shet_state shet;
shet_io_arduino_serial_t shet_io;


/**
 * A property which controls the onboard LED state.
 */
void set_led(shet_state_t *shet, bool state) {
	digitalWrite(led_pin, state);
}
bool get_led(shet_state_t *shet) {
	return digitalRead(led_pin);
}
EZSHET_PROP("/arduino/led", led, SHET_BOOL);


/**
 * An action which performs addition.
 */
int add(shet_state_t *shet, int a, int b) {
	return a + b;
}
EZSHET_ACTION("/arduino/add", add, SHET_INT, SHET_INT, SHET_INT);


/**
 * An event which occurrs every second.
 */
EZSHET_EVENT("/arduino/timer", timer_event, SHET_INT);


void setup() {
	pinMode(led_pin, OUTPUT);
	
	Serial.begin(115200);
	shet_io_arduino_serial_init(&shet_io, &Serial);
	
	delay(1000);
	shet_state_init(&shet, "\"ARDUINO\"", shet_io_arduino_serial_tx, &shet_io);
	EZSHET_ADD(&shet, led);
	EZSHET_ADD(&shet, add);
	EZSHET_ADD(&shet, timer_event);
}

void loop() {
	shet_io_arduino_serial_rx(&shet_io, &shet);
	
	// Regular timer event
	static int timer = 0;
	static unsigned long long last = 0;
	unsigned long long now = millis();
	if ((now - last) >= 1000) {
		timer_event(&shet, timer++);
		last = now;
	}
}
