#ifndef SHET_H
#define SHET_H

#include "json.h"

#define BUF_LEN 1024

// #define DEBUG

// Predeclare the shet_state data structure.
struct shet_state;
typedef struct shet_state shet_state;

// All callbacks should be of this type.
typedef void (*callback_t)(shet_state *, struct json_object *, void *);

// Define 4 types of callbacks that we store.
typedef enum {
	RETURN_CB,
	EVENT_CB,
	CALL_CB,
	PROP_CB,
} callback_type;

typedef struct {
	int id;
	callback_t success_callback;
	callback_t error_callback;
	void *user_data;
} return_callback;

typedef struct {
	char *event_name;
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
	int sockfd;
	int next_id;
	callback_list *callbacks;
	int should_exit;
	struct json_tokener *json_parser;
	
	callback_t error_callback;
	void *error_callback_data;
};


// Make a new state.
shet_state *shet_new_state();
// Clean up the state.
void shet_free_state(shet_state *state);
// Exit the main loop.
void shet_exit(shet_state *state);

// Connect to a shet server on a host and port.
void shet_connect(shet_state *state, char *host, char *port);
// Connect to the shet server given in the SHET_HOST
// and SHET_PORT environment variables.
void shet_connect_default(shet_state *state);

// Do a single read call, and process the results incrementaly.
void shet_tick(shet_state *state);
// Process a message from shet.
void shet_loop(shet_state *state);

// Set the error callback.
// The given callback will be called on any unhandled error from shet.
void shet_set_error_callback(shet_state *state,
                             callback_t callback,
                             void *callback_arg);


// Call an action.
void shet_call_action(shet_state *state,
                     char *path,
                     struct json_object *args,
                     callback_t callback,
                     void *callback_arg);

// Get a property.
void shet_get_prop(shet_state *state,
                   char *path,
                   callback_t callback,
                   void *callback_arg);
// Set a property.
void shet_set_prop(shet_state *state,
                   char *path,
                   struct json_object *value,
                   callback_t callback,
                   void *callback_arg);

// Watch an event.
void shet_watch_event(shet_state *state,
                      char *path,
                      callback_t event_callback,
                      callback_t deleted_callback,
                      callback_t created_callback,
                      void *callback_arg);
#endif
