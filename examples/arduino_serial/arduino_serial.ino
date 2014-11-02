/**
 * A simple Arduino example for uSHET implementing SHET over UART.
 *
 * To "make it real" you can use socat to connect the serial port with the SHET:
 *
 *     socat file:/dev/ttyUSB0,b115200,raw,echo=0 tcp:127.0.0.1:11235,reuseaddr
 */

#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include "shet.h"
#include "jsmn.h"

const int led = 13;

shet_state shet;


/**
 * A property which controls the onboard LED state.
 */
void led_set_cb(shet_state_t *shet, shet_json_t json, void *user_data) {
	if (json.token->type == JSMN_PRIMITIVE && 
	     (json.line[json.token->start] == 't' ||
	      json.line[json.token->start] == 'f')) {
		digitalWrite(led, json.line[json.token->start] == 't');
		shet_return(shet, 0, NULL);
	} else {
		shet_return(shet, 1, "\"Expected a single boolean!\"");
	}
}
void led_get_cb(shet_state_t *shet, shet_json_t json, void *user_data) {
	shet_return(shet, 0, digitalRead(led) ? "true" : "false");
}
shet_deferred_t led_deferred;


/**
 * An action which performs summation.
 */
void sum_cb(shet_state_t *shet, shet_json_t json, void *user_data) {
	int sum = 0;
	if (json.token[0].type == JSMN_ARRAY && json.token[0].size >= 1) {
		for (int i = 0; i < json.token[0].size; i++) {
			if (json.token[i+1].type == JSMN_PRIMITIVE &&
			    ((json.line[json.token[i+1].start] >= '0' &&
			      json.line[json.token[i+1].start] <= '9') ||
			     json.line[json.token[i+1].start] == '-')) {
				sum += atoi(json.line + json.token[i+1].start);
			} else {
				shet_return(shet, 1, "\"All arguments must be integers.\"");
				return;
			}
		}
		
		// Return the sum
		char num[10];
		sprintf(num, "%d", sum);
		shet_return(shet, 0, num);
	} else {
		shet_return(shet, 1, "\"Must have at least 1 argument.\"");
	}
}
shet_deferred_t sum_deferred;


/**
 * An event which occurrs every second.
 */
shet_event_t timer_event;


void setup() {
	Serial.begin(115200);
	pinMode(led, OUTPUT);
	
	delay(1000);
	
	shet_state_init(&shet, "\"ARDUINO\"", transmit, NULL);
	
	// LED control
	shet_make_prop(&shet, "/arduino/led",
	               &led_deferred, led_get_cb, led_set_cb, NULL,
	               NULL, NULL, NULL, NULL);
	
	// Summation
	shet_make_action(&shet, "/arduino/sum",
	                 &sum_deferred, sum_cb, NULL,
	                 NULL, NULL, NULL, NULL);
	
	// Timer event
	shet_make_event(&shet, "/arduino/timer",
	                &timer_event,
	                NULL, NULL, NULL, NULL);
}

// Callback to handle transmission of SHET data
void transmit(const char *data, void *user_data) {
	Serial.print(data);
}

void loop() {
	// Process shet commands from the host
	static char line[SHET_BUF_SIZE];
	static size_t len = 0;
	if (Serial.available()) {
		// If a buffer overrun ocurrs just wrap around and let uSHET reject the data
		if (len >= SHET_BUF_SIZE)
			len = 0;
		
		line[len++] = Serial.read();
		if (line[len-1] == '\n') {
			shet_process_line(&shet, line, len);
			len = 0;
		}
	}
	
	// Regular timer event
	static int timer = 0;
	static unsigned long long last = 0;
	unsigned long long now = millis();
	if ((now - last) >= 1000) {
		char time_str[10];
		sprintf(time_str, "%d", timer++);
		shet_raise_event(&shet, "/arduino/timer", time_str, NULL, NULL, NULL, NULL);
		last = now;
	}
}
