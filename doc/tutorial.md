uSHET Tutorial
==============

This document is a very brief tutorial which introduces the basic concepts of
the uSHET API. The C header file (`shet.h`) contains more complete documentation
and should be used as a reference. This API is fairly low-level and while it
offers the complete flexibility it can require a fair amount of boilerplate
code. Most users will prefer to use the EZSHET library (see
`doc/ezshet_tutorial.md`) which is simply a wrapper around uSHET which covers
the common cases with far less code.

In this tutorial we'll work through the key parts of the example in
`examples/arduino_serial/arduino_serial.ino`. This Arduino sketch implements a
very simple SHET client with the following SHET nodes:

* `/arduino/led`: A *property* containing a boolean which allows the control of
  the Arduino's onboard LED.
* `/arduino/sum`: An *action* which takes an arbitrary number of (integer)
  arguments and returns their sum.
* `/arduino/timer`: An *event* which occurs every second and gives the
  time since the Arduino was reset as its value.

This sketch communicates with the SHET servier over the hardware serial port.
Since the SHET server only listens for network connections; you must use
something like the command below to forward the data to/from the serial port
to/from SHET:

	SERIAL_PORT=/dev/ttyUSB0
	SHET_HOST=127.0.0.1
	socat file:$SERIAL_PORT,b115200,raw,echo=0 tcp:$SHET_HOST:11235,reuseaddr


The uSHET Event Loop
--------------------

Every uSHET application must contain a `shet_state_t` struct which holds uSHET's
state. Typically this will be created as a global variable.

	shet_state_t shet;

This state struct is initialised on startup using:

	shet_state_init(&shet, "\"my app\"", transmit, NULL);

This function takes a JSON value to use to uniquely identify the client
within the SHET network (here that value is a string: note the quotes). This is
followed by a callback function which is called whenever uSHET wants to transmit
some data. This callback is passed the last argument as user-data which in this
case we set to NULL.

The transmit callback function is simply passed a null-terminated string to be
transmitted. In our example, this is then sent straight out of the serial port:

	void transmit(const char *data, void *user_data) {
		Serial.print(data);
	}

uSHET expects to be fed individual lines of JSON (i.e. SHET protocol data). In
our example, the main-loop reads characters from the serial port and, whenever a
newline is received, feeds them to uSHET using `shet_process_line`. The relevent
snippet from the main loop is below:

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

With the above alone we now have a complete, if rather dull, uSHET client
running.

Adding a Property
-----------------

Lets make a property which controls the Arduino's LED.

### Registration

First we must create a `shet_deferred_t` struct to maintain uSHET's state
relating to the property's callback functions:

	shet_deferred_t led_deferred;

The property must then be registered in our setup code using `shet_make_prop`:

	shet_make_prop(&shet, "/arduino/led",
	               &led_deferred, led_get_cb, led_set_cb, NULL,
	               NULL, NULL, NULL, NULL);

This causes a property to be registered at the SHET path `/arduino/led`.

The second line of arguments defines a pair of callbacks for when the value of
the property is read or set via SHET. `led_deferred` is a `shet_deferred_t`
struct which will hold state information used by uSHET to implement the get/set
callbacks. The `NULL` at the end of the line is a piece of user data which will
be passed to the get/set callbacks.

The third line optionally takes another `shet_deferred_t` followed by two
callback functions and a piece of user data. This pair of callbacks are called
upon the success or failure (e.g. when the path was already in use) of property
creation respectively. Since most applications probably don't have any sensible
way of coping with a registration failure, nor any reason to wait for successful
registration, these callbacks are often left unused.

### Getter

We define a callback function for the "getter" which returns the current LED
state. All callbacks in uSHET are passed the following arguments: a pointer to
the current `shet_state_t`, a tokenised JSON value and the user-data specified
during registration.

	void led_get_cb(shet_state_t *shet, shet_json_t json, void *user_data) {
		shet_return(shet, 0, digitalRead(led) ? "true" : "false");
	}

In the case of property getter callbacks, the JSON value passed is undefined and
so we simply ignore it. The callback function simply calls `shet_return` with a
status of 0 (success) and the state of the LED expressed as JSON value.

### Setter

The setter callback is a little more complex. This callback receives a JSON
value from SHET which will be used to change the LED's state. Since SHET does
not have a way to express that a property takes a certain type, our callback
function must check the JSON type passed.

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

uSHET uses the compact [JSMN](http://zserge.com/jsmn.html) library as the
underlying JSON parser. As a result, the JSON passed to callbacks comes in the
form of a `shet_json_t` which is a simple struct containing two fields:

* `token`: A pointer to a JSMN token (`jsmntok_t`)
* `line`: A string containing the line of JSON which the token referrs to.

Readers should refer the JSMN documentation for details on how to use JSMN's
output. In brief, the code above checks that the JSON token is a boolean which
in JSMN means a primitive value whose first character is either 't' or 'f'.

If the value passed to the setter is found to be a boolean, the LED is set
accordingly and success is returned to SHET (note that `NULL` is given as the
returned value since there is no need to return a value. If the value is of the
wrong type, an error condition is reported along with a human-readable JSON
string (notice the quotes) describing what went wrong.


Adding an Action
----------------

As you'll see, adding an action is very similar to adding a property. Our action
will take an arbitrary number of (integer) arguments and sum them to produce an
integral result.


### Registration

Once again we must allocate a `shet_deferred_t` struct to hold uSHET's state
relating to the action's callback:

	shet_deferred_t sum_deferred;

Then in our setup code we must register the action:

	shet_make_action(&shet, "/arduino/sum",
	                 &sum_deferred, sum_cb, NULL,
	                 NULL, NULL, NULL, NULL);

As before, we first specify the path of our action followed by the callback
function which implements the action. As before this requires a
`shet_deferred_t` and a user value but this time there is just one callback
function. On the last line we again get the option to add a callback for
registration success/failure but once again we've left these as NULL.

### Callback

Our callback function is defined as below.

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

As before we must manually type-check the supplied JSON argument. Actions are
given their arguments as an array and so we check that we've received an array
and that it is non-empty. We then iterate over the array's contents and check
that each value is an integer and sum them up.

Finally the sum is convert to a JSON integer (i.e. placed in a string) and this
is then returned to SHET.


Adding an Event
---------------

We'll finally create an event which will be raised once per second containing
the current uptime of the Arduino in seconds.

### Registration

SHET requires some state to be maintained for all registered events and so we
must create a `shet_event_t` struct like so:

	shet_event_t timer_event;

We then register the event in our setup code:

	shet_make_event(&shet, "/arduino/timer",
	                &timer_event,
	                NULL, NULL, NULL, NULL);

As usual, this function takes the path at which the event will reside. On the
second line we specify the `shet_event_t` struct we allocated. On the final line
we have the usual optional registration success failure callbacks which we've
left as NULL here.

### Raising the Event

In our main loop we implement a simple timer which simply polls the timer until
1 second has elapsed and then increments a counter and raises the event in SHET.

	static int timer = 0;
	static unsigned long long last = 0;
	unsigned long long now = millis();
	if ((now - last) >= 1000) {
		char time_str[10];
		sprintf(time_str, "%d", timer++);
		shet_raise_event(&shet, "/arduino/timer", time_str, NULL, NULL, NULL, NULL);
		last = now;
	}

The function `shet_raise_event` is the one which actually raises the event
within SHET. This function takes the path of the event we registered followed by
a JSON string giving the value of the event. In this case this is a simple
integer written into a string. The final four arguments specify an optional
callback for getting the success/failure of raising the event (as always this
includes a `shet_deferred_t` and a user argument, hence the four arguments).

Going Further
-------------

This tutorial has given only a brief introduction to a subset of uSHET's
features which gives a feel for the style of the API. Other basic SHET features,
such as the ability to call actions, get/set properties or watch events are not
covered here. The uSHET header file `shet.h` includes (fairly) detailed
documentation for the uSHET API which should be easy to navigate for users
familiar with SHET.

A lot of boiler-plate code was required in this example in order to type-check
the JSON being passed to our callbacks and produce valid JSON strings. The
EZSHET wrapper library is designed to eliminate much of this code for "common
case" uses. This library makes use of uSHET's library of JSON utility functions
and macros in `shet_json.h` and advanced users may prefer to use these directly.
