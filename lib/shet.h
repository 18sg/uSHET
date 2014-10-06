#ifndef SHET_H
#define SHET_H

#include <stdlib.h>
#include <stdbool.h>

#include "jsmn.h"

// The number of JSON tokens to allocate for parsing a single message
#define SHET_NUM_TOKENS 20

// Number of characters in the buffer used to hold outgoing SHET messages
#define SHET_BUF_SIZE 100

// #define DEBUG

// Predeclare the shet_state data structure.
struct shet_state;
typedef struct shet_state shet_state;

// All callbacks should be of this type.
typedef void (*callback_t)(shet_state *, jsmntok_t *, void *);

// Define 4 types of callbacks that we store.
typedef enum {
	RETURN_CB,
	EVENT_CB,
	CALL_CB,
	PROP_CB,
} callback_type;

// Define the types of event callbacks
typedef enum {
	EVENT_ECB,
	EVENT_DELETED_ECB,
	EVENT_CREATED_ECB,
} event_callback_type;

typedef struct {
	int id;
	callback_t success_callback;
	callback_t error_callback;
	void *user_data;
} return_callback;

typedef struct {
	const char *event_name;
	callback_t event_callback;
	callback_t deleted_callback;
	callback_t created_callback;
	void *user_data;
} event_callback;

// Not implemented yet...
typedef struct {
} call_callback;

typedef struct {
} prop_callback;

// A list of callbacks.
typedef struct callback_list {
	callback_type type;
	union {
		return_callback return_cb;
		event_callback event_cb;
		call_callback call_cb;
		prop_callback prop_cb;
	} data;
	struct callback_list *next;
} callback_list;

// The global shet state.
struct shet_state {
	int next_id;
	callback_list *callbacks;
	
	// Last JSON line received
	char *line;
	
	// A buffer of tokens for JSON strings
	jsmntok_t tokens[SHET_NUM_TOKENS];
	
	// Outgoing JSON buffer
	char out_buf[SHET_BUF_SIZE];
	
	// Function to call to be called to transmit (null-terminated) data
	void (*transmit)(const char *data);
	
	callback_t error_callback;
	void *error_callback_data;
};


// Make a new state.
void shet_state_init(shet_state *state, void (*transmit)(const char *data));

// Process a message from shet.
void shet_process_line(shet_state *state, char *line, size_t line_length);

// Set the error callback.
// The given callback will be called on any unhandled error from shet.
void shet_set_error_callback(shet_state *state,
                             callback_t callback,
                             void *callback_arg);


// Call an action.
void shet_call_action(shet_state *state,
                     callback_list *cb_list,
                     const char *path,
                     const char *args,
                     callback_t callback,
                     void *callback_arg);

// Get a property.
void shet_get_prop(shet_state *state,
                   callback_list *cb_list,
                   const char *path,
                   callback_t callback,
                   void *callback_arg);
// Set a property.
void shet_set_prop(shet_state *state,
                   callback_list *cb_list,
                   const char *path,
                   const char *value,
                   callback_t callback,
                   void *callback_arg);

// Watch an event.
void shet_watch_event(shet_state *state,
                      callback_list *cb_list,
                      const char *path,
                      callback_t event_callback,
                      callback_t deleted_callback,
                      callback_t created_callback,
                      void *callback_arg);

#endif
