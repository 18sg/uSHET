#ifndef SHET_H
#define SHET_H

#include <stdlib.h>
#include <stdbool.h>

#include "jsmn.h"

#ifdef __cplusplus
extern "C" {
#endif

////////////////////////////////////////////////////////////////////////////////
// Resource allocation constants
////////////////////////////////////////////////////////////////////////////////

// The number of JSON tokens to allocate for parsing a single message
#define SHET_NUM_TOKENS 20

// Number of characters in the buffer used to hold outgoing SHET messages
#define SHET_BUF_SIZE 100

// #define DEBUG


////////////////////////////////////////////////////////////////////////////////
// Types
////////////////////////////////////////////////////////////////////////////////

// Predeclare the shet_state data structure.
struct shet_state;
typedef struct shet_state shet_state_t;

// All callbacks should be of this type.
// Arguments are:
// * SHET state object to enable sending responses
// * A string containing (mangled) JSON sent to the callback
// * An array of jsmn tokens breaking down the JSON
// * A user-defined pointer chosen when the callback was defined
typedef void (*callback_t)(shet_state_t *, char *, jsmntok_t *, void *);

// Define 4 types of callbacks that we store.
typedef enum {
	RETURN_CB,
	EVENT_CB,
	ACTION_CB,
	PROP_CB,
} deferred_type_t;

// Predeclare the deferred struct
struct deferred;

// Define the types of (from) server command callbacks
typedef enum {
	EVENT_CCB,
	EVENT_DELETED_CCB,
	EVENT_CREATED_CCB,
	GET_PROP_CCB,
	SET_PROP_CCB,
	CALL_CCB,
} command_callback_type_t;

typedef struct {
	int id;
	callback_t success_callback;
	callback_t error_callback;
	void *user_data;
} return_callback_t;

typedef struct {
	struct deferred *watch_deferred;
	const char *event_name;
	callback_t event_callback;
	callback_t deleted_callback;
	callback_t created_callback;
	void *user_data;
} event_callback_t;

typedef struct {
	struct deferred *mkprop_deferred;
	const char *prop_name;
	callback_t get_callback;
	callback_t set_callback;
	void *user_data;
} prop_callback_t;


typedef struct {
	struct deferred *mkaction_deferred;
	const char *action_name;
	callback_t callback;
	void *user_data;
} action_callback_t;

// A list of callbacks.
typedef struct deferred {
	deferred_type_t type;
	union {
		return_callback_t return_cb;
		event_callback_t event_cb;
		action_callback_t action_cb;
		prop_callback_t prop_cb;
	} data;
	struct deferred *next;
} deferred_t;

// A list of registered events
typedef struct event {
	const char *event_name;
	struct deferred *mkevent_deferred;
	struct event *next;
} event_t;

// The global shet state.
struct shet_state {
	// Next ID to use when sending a command
	int next_id;
	
	// Pointer to the string containing the last command received
	char *line;
	
	// The token defining the ID of the last command received. (Used for
	// returning).
	jsmntok_t *recv_id;
	
	// Linked lists of registered callback deferreds and event registrations
	deferred_t *callbacks;
	event_t *registered_events;
	
	// A buffer of tokens for JSON strings
	jsmntok_t tokens[SHET_NUM_TOKENS];
	
	// Outgoing JSON buffer
	char out_buf[SHET_BUF_SIZE];
	
	// Unique identifier for the connection
	const char *connection_name;
	
	// Function to call to be called to transmit (null-terminated) data
	void (*transmit)(const char *data, void *user_data);
	void *transmit_user_data;
	
	callback_t error_callback;
	void *error_callback_data;
};


////////////////////////////////////////////////////////////////////////////////
// General Library Functions
////////////////////////////////////////////////////////////////////////////////

// Initialise the SHET state variable. Takes a connection name which should be
// some unique JSON structure (as a string) which uniquely identifies this
// device and application. This value must be valid for the full lifetime of the
// SHET library. Also takes a function which can be used to transmit data.
void shet_state_init(shet_state_t *state, const char *connection_name,
                     void (*transmit)(const char *data, void *user_data),
                     void *transmit_user_data);

// Process a message from shet.
void shet_process_line(shet_state_t *state, char *line, size_t line_length);

// Set the error callback.
// The given callback will be called on any unhandled error from shet.
void shet_set_error_callback(shet_state_t *state,
                             callback_t callback,
                             void *callback_arg);

// Un-register a given deferred. This is intended for use in timeout-like
// scenarios or in the event of an action failing resulting a deferred being
// redundant. It does not cause the underlying event to be unregistered.
void shet_cancel_deferred(shet_state_t *state, deferred_t *deferred);

// Ping the server
void shet_ping(shet_state_t *state,
               const char *args,
               deferred_t *deferred,
               callback_t callback,
               callback_t err_callback,
               void *callback_arg);

// Return a response to the last command received. This should be called within
// the callback functions registered for actions and properties.
void shet_return(shet_state_t *state,
                 int success,
                 const char *value);

// Like shet_return except the ID used is that specified rather than the last
// command executed. This function can thus be used outside a SHET callback
// function and thus be used for asynchronous responses. The caller is
// responsible for storing the return ID during the original callback using
// shet_get_return_id.
void shet_return_with_id(shet_state_t *state,
                         const char *id,
                         int success,
                         const char *value);

// Get the string containing the return ID for the last command received. The
// pointer returned is valid until the next command is processed, i.e. the
// string is safe for the duration of a callback and should be copied if
// required afterward.
char *shet_get_return_id(shet_state_t *state);

////////////////////////////////////////////////////////////////////////////////
// Action Functions
////////////////////////////////////////////////////////////////////////////////

// Create an action. The deferred and callbacks for calling the action is
// mandatory, those for the mkaction are not.
void shet_make_action(shet_state_t *state,
                      const char *path,
                      deferred_t *action_deferred,
                      callback_t callback,
                      void *action_arg,
                      deferred_t *mkaction_deferred,
                      callback_t mkaction_callback,
                      callback_t mkaction_err_callback,
                      void *mkaction_callback_arg);

// Remove an action. This will cancel the action deferred set up when making the
// action but will not cancel the deferred for making the action. Callback
// optional.
void shet_remove_action(shet_state_t *state,
                        const char *path,
                        deferred_t *deferred,
                        callback_t callback,
                        callback_t err_callback,
                        void *callback_arg);


// Call an action.
void shet_call_action(shet_state_t *state,
                     const char *path,
                     const char *args,
                     deferred_t *deferred,
                     callback_t callback,
                     callback_t err_callback,
                     void *callback_arg);

////////////////////////////////////////////////////////////////////////////////
// Property Functions
////////////////////////////////////////////////////////////////////////////////

// Create a property. The deferred and callbacks for getting/setting the property
// are mandatory, those for the mkprop are not.
void shet_make_prop(shet_state_t *state,
                    const char *path,
                    deferred_t *prop_deferred,
                    callback_t get_callback,
                    callback_t set_callback,
                    void *prop_arg,
                    deferred_t *mkprop_deferred,
                    callback_t mkprop_callback,
                    callback_t mkprop_err_callback,
                    void *mkprop_callback_arg);

// Remove a property. This will cancel the property deferred set up when making
// the property but will not cancel the deferred for making the property.
// Callback optional.
void shet_remove_prop(shet_state_t *state,
                      const char *path,
                      deferred_t *deferred,
                      callback_t callback,
                      callback_t err_callback,
                      void *callback_arg);

// Get a property.
void shet_get_prop(shet_state_t *state,
                   const char *path,
                   deferred_t *deferred,
                   callback_t callback,
                   callback_t err_callback,
                   void *callback_arg);
// Set a property.
void shet_set_prop(shet_state_t *state,
                   const char *path,
                   const char *value,
                   deferred_t *deferred,
                   callback_t callback,
                   callback_t err_callback,
                   void *callback_arg);


////////////////////////////////////////////////////////////////////////////////
// Event Functions
////////////////////////////////////////////////////////////////////////////////

// Make a new event. Must be provided with an unused "event_t" which represents
// the registration of the event. Optionally accepts a callback for the success
// of the creation of the event.
void shet_make_event(shet_state_t *state,
                     const char *path,
                     event_t *event,
                     deferred_t *mkevent_deferred,
                     callback_t mkevent_callback,
                     callback_t mkevent_error_callback,
                     void *mkevent_callback_arg);

// Remove (unregister) an event. Optionally accepts a callback for the success
// of the unregistration of the event.
void shet_remove_event(shet_state_t *state,
                       const char *path,
                       deferred_t *deferred,
                       callback_t callback,
                       callback_t error_callback,
                       void *callback_arg);

// Watch an event. The watch callbacks are optional, the event ones are not!
void shet_watch_event(shet_state_t *state,
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

// Ignore an event which has previously been watched. This will cancel the event
// deferred set up when watching but will not cancel the deferred for watching
// the event. The callbacks are optional.
void shet_ignore_event(shet_state_t *state,
                       const char *path,
                       deferred_t *deferred,
                       callback_t callback,
                       callback_t error_callback,
                       void *callback_arg);

#ifdef __cplusplus
}
#endif

#endif
