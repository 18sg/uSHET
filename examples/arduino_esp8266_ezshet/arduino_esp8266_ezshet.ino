/**
 * A simple Arduino example for uSHET/EZSHET implementing SHET over WiFi via an
 * ESP8266 chip.
 *
 * This library assumes complete control over the ESP8266 and is known to work
 * with version 0.9.2 of the AT firmware. Since this version of the firmware has
 * known issues with full-duplex communication, this implementation must connect
 * to SHET via the included proxy server `shet_io_arduino_esp8266_proxy.py`.
 * This proxy allows the device to poll for data from the SHET server ensuring
 * half-duplex communication.
 */

#include <SoftwareSerial.h>

#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include "shet.h"
#include "ezshet.h"

#include "shet_io_arduino_esp8266.h"

SoftwareSerial esp8266(2,3);

const char SSID[]       = "Your WiFi SSID here";
const char PASSPHRASE[] = "Your WiFi passphrase here";
const char HOSTNAME[]   = "192.168.1.185";
const int  PORT         = 11236;


const int led_pin = 13;

shet_state shet;
shet_io_arduino_esp8266_t shet_io;


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
	esp8266.begin(9600);
	
	digitalWrite(led_pin, false);
	shet_io_arduino_esp8266_init(&shet_io, &esp8266, SSID, PASSPHRASE, HOSTNAME, PORT);
	
	shet_state_init(&shet, "\"ARDUINO\"", shet_io_arduino_esp8266_tx, &shet_io);
	EZSHET_ADD(&shet, led);
	EZSHET_ADD(&shet, add);
	EZSHET_ADD(&shet, timer_event);
}

void loop() {
	shet_io_arduino_esp8266_rx(&shet_io, &shet);
	
	digitalWrite(led_pin, shet_io.connected);
	
	// Regular timer event
	static int timer = 0;
	static unsigned long long last = 0;
	unsigned long long now = millis();
	if ((now - last) >= 1000) {
		timer_event(&shet, timer++);
		last = now;
	}
}
