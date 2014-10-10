#ifndef SHET_H
#define SHET_H

#include <stdlib.h>
#include <stdbool.h>

#include "jsmn.h"

#ifdef __cplusplus
extern "C" {
#endif

// The number of JSON tokens to allocate for parsing a single message
#define SHET_NUM_TOKENS 20

// Number of characters in the buffer used to hold outgoing SHET messages
#define SHET_BUF_SIZE 100

// #define DEBUG

// Predeclare the shet_state data structure.
struct shet_state;
typedef struct shet_state shet_state;

// All callbacks should be of this type.
// Arguments are:
// * SHET state object to enable sending responses
// * A string containing (mangled) JSON sent to the callback
// * An array of jsmn tokens breaking down the JSON
// * A user-defined pointer chosen when the callback was defined
typedef void (*callback_t)(shet_state *, char *, jsmntok_t *, void *);

// Define 4 types of callbacks that we store.
typedef enum {
	RETURN_CB,
	EVENT_CB,
	CALL_CB,
	PROP_CB,
} deferred_type_t;

// Define the types of event callbacks
typedef enum {
	EVENT_ECB,
	EVENT_DELETED_ECB,
	EVENT_CREATED_ECB,
} event_callback_type_t;

typedef struct {
	int id;
	callback_t success_callback;
	callback_t error_callback;
	void *user_data;
} return_callback_t;

typedef struct {
	const char *event_name;
	callback_t event_callback;
	callback_t deleted_callback;
	callback_t created_callback;
	void *user_data;
} event_callback_t;

// Not implemented yet...
typedef struct {
} call_callback_t;

typedef struct {
} prop_callback_t;

// A list of callbacks.
typedef struct deferred {
	deferred_type_t type;
	union {
		return_callback_t return_cb;
		event_callback_t event_cb;
		call_callback_t call_cb;
		prop_callback_t prop_cb;
	} data;
	struct deferred *next;
} deferred_t;

// The global shet state.
struct shet_state {
	int next_id;
	deferred_t *callbacks;
	
	// A buffer of tokens for JSON strings
	jsmntok_t tokens[SHET_NUM_TOKENS];
	
	// Outgoing JSON buffer
	char out_buf[SHET_BUF_SIZE];
	
	// Function to call to be called to transmit (null-terminated) data
	void (*transmit)(const char *data);
	
	callback_t error_callback;
	void *error_callback_data;
};


// Initialise the SHET state variable
void shet_state_init(shet_state *state, void (*transmit)(const char *data));

// Process a message from shet.
void shet_process_line(shet_state *state, char *line, size_t line_length);

// Set the error callback.
// The given callback will be called on any unhandled error from shet.
void shet_set_error_callback(shet_state *state,
                             callback_t callback,
                             void *callback_arg);

// Un-register a given deferred. This is intended for use in timeout-like
// scenarios or in the event of an action failing resulting a deferred being
// redundant. It does not cause the underlying event to be unregistered.
void shet_cancel_deferred(shet_state *state, deferred_t *deferred);

// Ping the server
void shet_ping(shet_state *state,
               const char *args,
               deferred_t *deferred,
               callback_t callback,
               callback_t err_callback,
               void *callback_arg);

// Call an action.
void shet_call_action(shet_state *state,
                     const char *path,
                     const char *args,
                     deferred_t *deferred,
                     callback_t callback,
                     callback_t err_callback,
                     void *callback_arg);

// Get a property.
void shet_get_prop(shet_state *state,
                   const char *path,
                   deferred_t *deferred,
                   callback_t callback,
                   callback_t err_callback,
                   void *callback_arg);
// Set a property.
void shet_set_prop(shet_state *state,
                   const char *path,
                   const char *value,
                   deferred_t *deferred,
                   callback_t callback,
                   callback_t err_callback,
                   void *callback_arg);

// Watch an event. The watch callbacks are optional, the event ones are not!
void shet_watch_event(shet_state *state,
                      const char *path,
                      deferred_t *event_deferred,
                      callback_t event_callback,
                      callback_t deleted_callback,
                      callback_t created_callback,
                      void *callback_arg,
                      deferred_t *watch_deferred,
                      callback_t watch_callback,
                      callback_t watch_error_callback,
                      void *watch_callback_arg);

#ifdef __cplusplus
}
#endif

#endif
