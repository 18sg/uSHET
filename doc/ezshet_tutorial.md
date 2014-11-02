EZSHET/uSHET Tutorial
=====================

This document provides an introduction to using uSHET with the EZSHET wrapper
library.

uSHET's native API can be quite verbose at times, particularly due to the need
to type-check all incoming JSON and manually convert between C types and JSON.
EZSHET is a library of C-preprocessor macros which generate this type-checking
code automatically and allow you to simply write callback functions with vanilla
C types as arguments.

It is intended that readers use this tutorial (and its appendices) as
the general reference for EZSHET as it covers most of its functionality. The
EZSHET header file (`ezshet.h`) also contains further details if required.

In this tutorial we'll walk through the code of the example client in
`examples/ardiuno_serial_ezshet`. This Arduino sketch implements a very simple
SHET client with the following SHET nodes:

* `/arduino/led`: A *property* containing a boolean which allows the control of
  the Arduino's onboard LED.
* `/arduino/add`: An *action* which takes two integers as arguments and returns
  their sum.
* `/arduino/timer`: An *event* which occurs every second and gives the
  time since the Arduino was reset as its value.

This sketch communicates with the SHET server over the hardware serial port.
Since the SHET server only listens for network connections; you must use
something like the command below to forward the data to/from the serial port
to/from SHET:

	SERIAL_PORT=/dev/ttyUSB0
	SHET_HOST=127.0.0.1
	socat file:$SERIAL_PORT,b115200,raw,echo=0 tcp:$SHET_HOST:11235,reuseaddr


Health Warnings
---------------

Before we begin it is worth highlighting the following health warnings.

Though EZSHET can vastly reduce the complexity of many applications, it does not
expose the same level of flexibility offered by the native API. As a result,
advanced features such as non-blocking, varadic or overloaded callbacks must be
implemented by hand using the normal uSHET API. Since EZSHET is simply a wrapper
around the uSHET API, normal uSHET code can happily coexist with EZSHET code if
desired.

Behind-the-scenes EZSHET uses some fairly advanced macros to implement its
functionality. The most significant side-effect of this is that the error
messages produced when the library is misused can be very unhelpful and
extremely long. As a result, users should take greater-than-usual care when
interacting with the EZSHET API to avoid errors.


Communications
--------------

uSHET natively does not know how to communicate with the SHET server since this
process differs wildly between platforms. As a result, users must supply a
"transmit" function to uSHET and manually feed the library lines of JSON
received from the SHET server.

In this tutorial we'll use the ready-made routines from
`io_libs/shet_io_arduino_serial.h` to implement these tasks. Other libraries in
this directory implement the boilerplate required for other communication
mechanisms or platforms. See the associated source code or the uSHET tutorial
(`doc/tutorial.md`) for details of how to create these yourself.

These I/O libraries generally define the following:

* A state-holding struct used internally by the library. (e.g.
  `shet_io_arduino_serial_t`)
* An initialisation function which configures the library (e.g.
  `shet_io_arduino_serial_init`)
* A transmit callback function (e.g. `shet_io_arduino_serial_tx`)
* A receive function to be placed in your application's main-loop which feeds
  uSHET data (e.g. `shet_io_arduino_serial_rx`).

In our example program we create a global variable to hold the state of the I/O
library like so:

	shet_io_arduino_serial_t shet_io;

In our setup code we initialise the serial port and pass it to the I/O
libraries' init function:

	Serial.begin(115200);
	shet_io_arduino_serial_init(&shet_io, &Serial);

The uSHET Event Loop
--------------------

Every uSHET application must contain a `shet_state_t` struct which holds uSHET's
state. Typically this will be created as a global variable.

	shet_state_t shet;

This state struct is initialised on startup using `shet_state_init`:

	shet_state_init(&shet, "\"my app\"", shet_io_arduino_serial_tx, &shet_io);

This function takes a JSON value to use to uniquely identify the client
within the SHET network (here that value is a string: note the quotes). This is
followed by a callback function which is called whenever uSHET wants to transmit
some data along with user-data which will be passed to the callback. In this
case we're using the callback provided by the I/O library which takes a pointer
to its state as its user-data.

In our main-loop we simply include:

	shet_io_arduino_serial_rx(&shet_io, &shet);

This function receives data from the serial port and passes it into uSHET for
processing.

At this point we now have a complete, if rather dull, uSHET client running. It
should also be noted we've not used any of the EZSHET functions yet!


Adding a Property
-----------------

Lets make a property which controls the Arduino's LED using EZSHET. This is as
simple as:

	void set_led(shet_state_t *shet, bool state) {
		digitalWrite(led_pin, state);
	}
	bool get_led(shet_state_t *shet) {
		return digitalRead(led_pin);
	}
	EZSHET_PROP("/arduino/led", led, SHET_BOOL);

The `EZSHET_PROP` line defines a property at the path `/arduino/led` which in
our program is referred to by the C identifier `led`. The final argument is
`SHET_BOOL` which states that we're expecting our property to be a boolean
value.

EZSHET expects two callback functions named `get_` and `set_` followed by the
identifier we chose, in this case `get_led` and `set_led`.

`set_led` is called when the property is set and is passed the boolean value as
a normal C bool-type argument. Once the function returns, EZSHET automatically
returns success to the SHET server. If a non-boolean value is written to the
property via SHET, the EZSHET wrapper automatically returns an error status to
SHET along with a helpful error message and the `set_led` callback is not
called.

`get_led` is called whenever the property is read via SHET should return the
value of the property is a normal C bool. This value is translated into JSON and
returned with a success status automatically by EZSHET.

As a final detail, in our setup code we also need to add the following line to
cause the property to be registered with SHET:

	EZSHET_ADD(&shet, led);


Take a look at the Appendix A on supported types for details of what types
EZSHET supports.

Take a look at the Appendix C on variables-as-properties for an even easier way
of defining properties that simply get or set a C variable in your application.


Adding an Action
----------------

Lets make an action which adds two numbers together.

	int add(shet_state_t *shet, int a, int b) {
		return a + b;
	}
	EZSHET_ACTION("/arduino/add", add, SHET_INT, SHET_INT, SHET_INT);

Our callback function is very straight-forward simply accepting two integers and
returning the sum.

The `EZSHET_ACTION` macro takes the path of the action, the name of the callback
function, a return type followed by the argument types.

When the action is called by SHET, the JSON arguments are type-checked by EZSHET
and converted into C types. If they are of the correct types our callback
function is called and the value it returns is automatically converted into JSON
and returned via SHET with a successful return status. If the argument types are
incorrect, EZSHET automatically responds with a helpful error message with an
error status and the callback is not called.

As with properties, the action must be registered in the setup code like so:

	EZSHET_ADD(&shet, add);

If the action should not return a value it should return `SHET_NULL`. See
Appendix B for how to make an action which returns multiple values.


Adding an Event
---------------

Lets make an event which occurs every second and simply gives the number of
seconds since reset.

	EZSHET_EVENT("/arduino/timer", timer_event, SHET_INT);

This creates an event at the given path and defines a new function called
`timer_event` which will raise the event. This function takes a `shet_state_t *`
and whatever types are given (in this case just an integer though any number of
arguments, or none at all, may be specified).

In our main-loop we add some simple timer logic which calls `timer_event` with
the time since reset once per second:

	static int timer = 0;
	static unsigned long long last = 0;
	unsigned long long now = millis();
	if ((now - last) >= 1000) {
		timer_event(&shet, timer++);
		last = now;
	}

As always, the action must be registered in the setup code like so:

	EZSHET_ADD(&shet, timer_event);

If you want to watch an event rather than create one, see Appendix D.


Appendix A: Supported Types
---------------------------

EZSHET is able to check for, and pack/unpack, all JSON types.

### Basic Types

The table below gives an overview of the supported types and their corresponding
JSON type.

	Name          C-type (Argument)   C-type (Return)   Notes
	-----------   -----------------   ---------------   -----
	SHET_INT      int                 int
	SHET_FLOAT    double              double            1
	SHET_BOOL     bool                bool
	SHET_NULL     n/a                 n/a               2
	SHET_STRING   const char *        const char *      3
	SHET_ARRAY    shet_json_t         const char *      4
	SHET_OBJECT   shet_json_t         const char *      4

Note that each JSON type will be converted into one C type when given as an
argument but may be expected as a different C type when returned by a callback.

1. EZSHET currently converts doubles into JSON strings with six decimal
   digits. Additionally, very large values are not converted into scientific
   notation and may cause a buffer overrun. Please use with caution!

2. The NULL type has no sensible C equivalent and so will not actually get
   passed into callbacks nor expected as a returned value.

3. Strings are not currently automatically escaped. As a result, incoming
   strings must have their escape characters expanded by the user and all
   outgoing strings must be manually escaped by the user. Failing to do this
   will result in undefined behaviour.

4. JSON arrays and objects are given ready-parsed output from the
   [JSMN](http://zserge.com/jsmn.html) JSON parsing library. Since JSMN doesn't
   have any JSON generation capabilities, arrays and objects should be returned
   as valid JSON strings.

### Unpacking/Packing JSON Arrays

Say you have a property or action which is expected to take an array `[r,g,b]`
where `r`, `g` and `b` are integers. You could set up your property or action to
accept a `SHET_ARRAY` and then manually type-check and parse the array's
contents but this would be tiresome and is essentially equivalent resorting to
the raw uSHET API.

To avoid this pain EZSHET can unpack/pack values from/into arrays with known
lengths and contents using the two special types: `SHET_ARRAY_BEGIN` and
`SHET_ARRAY_END`. Matched pairs of these can be used to surround multiple types
as described previously. They can also be arbitrarily nested.

#### Properties

In this example we can define our property like so:
	
	void set_rgb_prop(shet_state_t *shet, int r, int g, int b) {
		// ...do something...
	}
	void get_rgb_prop(shet_state_t *shet, int *r, int *g, int *b) {
		// ...get the values...
		*r = my_r;
		*g = my_g;
		*b = my_b;
	}
	EZSHET_PROP("/arduino/rgb_prop", rgb_prop,
		SHET_ARRAY_BEGIN,
			SHET_INT,
			SHET_INT,
			SHET_INT,
		SHET_ARRAY_END);

Our getter/setter callbacks now receive/produce multiple arguments/results.

Further, note that the getter now returns via pointer-arguments since C functions cannot
return multiple values. This is true of all unpacked arrays even if there is
only one value in the array.


#### Variables-as-Properties

When using variables as properties (See Appendix C), things work similarly:
	
	int r;
	int g;
	int b;
	EZSHET_VAR_PROP("/arduino/rgb_var_prop",
		rgb_var_prop, SHET_ARRAY_BEGIN,
			r, SHET_INT,
			g, SHET_INT,
			b, SHET_INT,
		_, SHET_ARRAY_END);

Note that the identifier used by EZSHET is that of the `SHET_ARRAY_BEGIN` (i.e.
the first type) and this need not correspond with any existing C variable. Also
note that the `SHET_ARRAY_END` gets an arbitrary name (`_`) which need not
exist.


#### Events

Events can also be defined similarly.

	EZSHET_EVENT("/arduino/rgb_event", rgb_event,
		SHET_ARRAY_BEGIN,
			SHET_INT,
			SHET_INT,
			SHET_INT,
		SHET_ARRAY_END);

And thus called like:

	rgb_event(&shet, my_r, my_g, my_b);

#### Watched Events

Watched events (See Appendix D) with array-values can also be watched similarly.

	void some_event(shet_state_t *shet, int r, int g, int b) {
		// ...do something...
	}
	EZSHET_WATCH("/some/event", some_event,
		SHET_ARRAY_BEGIN,
			SHET_INT,
			SHET_INT,
			SHET_INT,
		SHET_ARRAY_END);


#### Actions

Actions can also be defined similarly.

	void rgb_action(shet_state_t *shet, int r, int g, int b) {
		// ...do something...
	}
	EZSHET_ACTION("/arduino/rgb_action", rgb_action,
		SHET_NULL,
		SHET_ARRAY_BEGIN,
			SHET_INT,
			SHET_INT,
			SHET_INT,
		SHET_ARRAY_END);

Alternatively an action may return a packed array.

	void rgb_action2(shet_state_t *shet, int *r, int *g, int *b) {
		// ...do something...
		*r = my_r;
		*g = my_g;
		*b = my_b;
	}
	EZSHET_ACTION("/arduino/rgb_action2", rgb_action2,
		EZSHET_RETURN_ARGS_BEGIN,
			SHET_ARRAY_BEGIN,
				SHET_INT,
				SHET_INT,
				SHET_INT,
			SHET_ARRAY_END,
		EZSHET_RETURN_ARGS_END);

Note that actions returning a packed array are treated as returning multiple
values, even if there is only one value in the packed array. See Appendix B for
details of how to return multiple values from an action are handled.


Appendix B: Returning Multiple Values from an Action
----------------------------------------------------

To define an action which returns multiple values it must be defined as below:

	EZSHET_ACTION("/path/to/action", name,
	              EZSHET_RETURN_ARGS_BEGIN,
	                // ...list of return types here...
	              EZSHET_RETURN_ARGS_END,
	              // ...list of argument types here...
	              );

That is, the return arguments must be surrounded by a
`EZSHET_RETURN_ARGS_BEGIN`/`EZSHET_RETURN_ARGS_END` pair. As the name suggests,
these values are returned via pointer arguments to the action (since C does not
support returning multiple values).

The callback for the action will be passed the return arguments followed by the
input arguments.

To give a complete example of a "swap" action which swaps the order of its
arguments:

	void swap(shet_state_t *shet, int *a_out, int *b_out, int a_in, int b_in) {
		*a_out = b_in;
		*b_out = a_in;
	}
	EZSHET_ACTION("/arduino/swap", swap,
		EZSHET_RETURN_ARGS_BEGIN,
			SHET_INT,
			SHET_INT,
		EZSHET_RETURN_ARGS_END,
		SHET_INT,
		SHET_INT);


Appendix C: Values-as-Properties
--------------------------------

A special case of properties is when the property simply gets/sets a C variable.
For example, say there is a variable which holds a PIN number we can expose this
as a SHET property like so:

	int pin = 1234; // Highly secure.
	EZSHET_VAR_PROP("/arduino/pin", pin, SHET_INT);

This must be registered as usual in your setup code:

	EZSHET_ADD(pin);

The variable can now be changed by setting the value of `/arduino/pin` or read
by getting `/ardiuno/pin`.

Note that the underlying C variables the property gets/sets should be of the
return type defined in the table of Appendix A.


Appendix D: Watching Events
---------------------------

Events can be watched as follows:

	void some_event(shet_state_t *shet, int a, int b) {
		// ...do something...
	}
	EZSHET_WATCH("/some/event", some_event, SHET_INT, SHET_INT);

The `EZSHET_WATCH` macro takes the path of the event to watch, the name of a
callback function to call when the event is raised and the types of values
passed with the event (if any). In this case the event is expected to come with
a pair of integers.

When an event is raised, EZSHET type-checks the associated JSON values and
converts them into C types. If the types are correct, the callback is called
with the event's values as arguments and success is automatically returned by
EZSHET. If the types are incorrect the callback is not called and failure is
returned to the SHET server.

Watches must be registered as usual in your setup code:

	EZSHET_ADD(some_event);


Appendix E: Detecting failures
------------------------------

EZSHET generally errs in favour of assuming things usually work correctly and
thus things such as type errors and registration failures are not as obviously
exposed as in the native uSHET API. However, EZSHET does allow very basic access
to this information through the following macros:

* `EZSHET_IS_REGISTERED(name)`: Returns a boolean indicating if the given EZSHET
	action/property/event is currently successfully registered. In the case of
	watches, this value is true only if the watch is successfully registered and
	the event currently exists.

* `EZSHET_ERROR_COUNT(name)`: Returns a counter indicating the number of times a
  given action/property-getter/watch was called with arguments of the wrong
  type.


Appendix F: Unregistration
--------------------------

To unregister SHET nodes added with `EZSHET_ADD(name)` there is a corresponding
function `EZSHET_REMOVE(name)`.


Appendix G: Declaring EZSHET Wrappers In Headers
------------------------------------------------

All the EZSHET definition macros (e.g. `EZSHET_ACTION`) have a corresponding
`EZSHET_DECLARE_` version which will declare everything required in a manner
suitable for inclusion in a header file.

With the exception of `EZSHET_DECLARE_EVENT` these all just take the C
identifier name associated with the action/property/event, for example
`EZSHET_DECLARE_ACTION(add)`.

`EZSHET_DECLARE_EVENT`, however, also requires that the types passed with the
event are included, for example `EZSHET_DECLARE_EVENT(timer_event, SHET_INT,
SHET_INT)`.


Appendix H: Peeking Behind The Scenes
-------------------------------------

The EZSHET library is based on macros which generate code for wrapper functions
that take care of all the dull type-checking duties for you.

You can see what code is being generated by EZSHET compiling with the `-E`
option. However, to try and dispel the magic for the more casual observer, lets
look at the following example: 

	EZSHET_ACTION("/arduino/add", SHET_INT, SHET_INT, SHET_INT);

Below is the (annotated and pretty-printed) wrapper function generated by EZSHET
as it was in the commit `d3eca4dbf94d507a2ad62db3d3a45b05c8ffed23`. The
type-checking code is written in a slightly unusual style which is easier to
generate than a more conventional style.

```c
void _ezshet_wrapper_add(shet_state_t *shet, shet_json_t json, void *data) {
	// These variables will hold the two parsed integer arguments (if the types
	// check out OK!)
	int II;
	int III;
	
	// Has there been a type error?
	bool error = false;
	
	{
		// Make a copy of the shet_json_t which we can iterate with
		shet_json_t _json = json;
		
		// Record the parent shet_json_t (used when unpacking arrays).  We're at
		// the root so this is NULL.
		shet_json_t _parent;
		_parent.token = NULL;
		
		// Count the number of values unpacked (more obviously useful when
		// unpacking arrays)
		int _num_unpacked = 0;
		
		// Check the first value we've received. (We're using the "do {} while(0)"
		// idiom to allow the block to exit early.)
		do {
			// Check that we've received array (uSHET action callbacks receive their
			// arguments in an array).
			if ( (_parent.token != NULL && _num_unpacked >= _parent.token->size) ||
			     (!(_json.token->type == JSMN_ARRAY))) {
				error = true;
				break;
			}
			
			// Unpack the values in the array
			{
				// A counter which ensures we don't unpack more values than the array
				// contains.
				int _num_unpacked = 0;
				
				// Remember the parent token (i.e. the array's head)
				shet_json_t _parent = _json;
				
				// Advance to the token of the first element of the array
				_json.token++;
				
				// Check that the array has at least this many values and that the
				// current value is an integer.
				if ( ( _parent.token != NULL &&
				       _num_unpacked >= _parent.token->size) ||
				     (!( _json.token->type == JSMN_PRIMITIVE &&
				         ( _json.line[_json.token->start] == '+' ||
				           _json.line[_json.token->start] == '-' ||
				           ( _json.line[_json.token->start] >= '0' &&
				             _json.line[_json.token->start] <= '9'))))) {
					error = true;
					break;
				}
				// Convert the JSON integer to a C integer
				II = atoi(_json.line + _json.token->start);
				
				// Advance to the next element in the array
				_num_unpacked++;
				_json.token++;
				
				// Check that the next element exists and is an integer too.
				if ( ( _parent.token != NULL &&
				       _num_unpacked >= _parent.token->size) ||
				     (!( _json.token->type == JSMN_PRIMITIVE &&
				         ( _json.line[_json.token->start] == '+' ||
				           _json.line[_json.token->start] == '-' ||
				           ( _json.line[_json.token->start] >= '0' &&
				             _json.line[_json.token->start] <= '9'))))) {
					error = true;
					break;
				}
				// Convert from JSON to a C integer
				III = atoi(_json.line + _json.token->start);
				
				// Advance to the next element in the array (though this is never
				// accessed)
				_num_unpacked++;
				_json.token++;
				
				// Check that we unpacked exactly the number of entries that exist in
				// the array (i.e. that there wasn't a 3rd or 4th entry).
				if (_num_unpacked != _parent.token->size) {
					error = true;
					break;
				}
			}
			
			// Count that we have unpacked the array (note that this is not the same
			// _num_unpacked as above because that was in a different scope).
			_num_unpacked++;
		} while (0);
		
		// Check that the current token pointer (_json) is pointing immediately
		// after the supplied token (i.e. sanity-check we've not missed any tokens
		// belonging to the supplied value). Also checks that we've only processed
		// exactly one token (not counting any nested tokens).
		if (_json.token != shet_next_token(json).token || _num_unpacked != 1) {
			error = true;
		}
	}
	
	if (!error) {
		// If everything worked, call the callback function
		int ret_val = add(shet, II, III);
		
		// Allocate a string long enough to store a printed integer (and a null
		// terminator) and print the JSON representation of the returned integer
		char packed_ret_val[((sizeof(int) <= 2) ? 6 : (sizeof(int) <= 4) ? 11 : 20) + 1];
		sprintf(packed_ret_val, "%d", ret_val);
		
		// Return success and the returned integer
		shet_return(shet, 0, packed_ret_val);
	} else {
		// If the types weren't correct, return failiure with a helpful error
		// message.
		static const char err_message[] = "\"Expected int, int\"";
		shet_return(shet, 1, err_message);
		
		// Increment the error count
		EZSHET_ERROR_COUNT(add)++;
	}
}
```
