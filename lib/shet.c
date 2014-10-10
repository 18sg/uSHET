#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "shet.h"

#include "jsmn.h"

#ifdef DEBUG
#define DPRINTF(...) fprintf(stderr, __VA_ARGS__)
#else
#define DPRINTF(...)
#endif

////////////////////////////////////////////////////////////////////////////////
// Assertions
////////////////////////////////////////////////////////////////////////////////


// Assert the type of a JSON object
static bool assert_type(const jsmntok_t *token, jsmntype_t type)
{
	if (token->type != type)
		DPRINTF("Expected token type %d, got %d.\n", type, token->type);
	
	return token->type == type;
}


// Assert that a JSON primitive is an int
static bool assert_int(const char *line, const jsmntok_t *token)
{
	if (token->type != JSMN_PRIMITIVE) {
		DPRINTF("Expected token type %d, got %d.\n", JSMN_PRIMITIVE, token->type);
		return false;
	}
	
	if (token->end - token->start <= 0) {
		DPRINTF("Integers cannot be zero-length strings.\n");
		return false;
	}
	
	char first_char = line[token->start];
	if (!(first_char == '-' || (first_char >= '0' && first_char <= '9'))) {
		DPRINTF("Integers don't start with '%c'.\n", first_char);
		return false;
	}
	
	return true;
}


////////////////////////////////////////////////////////////////////////////////
// Internal deferred utility functions/macros
////////////////////////////////////////////////////////////////////////////////

// Given shet state, makes sure that the deferred is in the callback list,
// adding it if it isn't already present.
static void add_deferred(shet_state *state, deferred_t *deferred) {
	deferred_t **deferreds = &(state->callbacks);
	
	// Check to see if the deferred is already present
	deferred_t *iter = (*deferreds);
	for (; iter != NULL; iter = iter->next)
		if (iter == deferred)
			break;
	
	// Add the deferred if it wasn't already added
	if (iter == NULL) {
		(deferred)->next = *(deferreds);
		*(deferreds) = (deferred);
	}
}


// Given a shet state make sure that the deferred is not in the callback list,
// removing it if it is.
void remove_deferred(shet_state *state, deferred_t *deferred) {
	deferred_t **deferreds = &(state->callbacks);
	
	// Find the deferred, if present
	deferred_t *iter = (*deferreds);
	for (; iter != NULL; iter = iter->next) {
		if (iter->next == deferred) {
			deferred_t *next = iter->next->next;
			iter->next->next = NULL;
			iter->next = next;
			break;
		}
	}
}


// Find a deferred callback for the return with the given ID.
// Return NULL if not found.
static deferred_t *find_return_cb(shet_state *state, int id)
{
	deferred_t *callback = state->callbacks;
	for (; callback != NULL; callback = callback->next)
		if (callback->type == RETURN_CB && callback->data.return_cb.id == id)
			break;
	
	if (callback == NULL) {
		DPRINTF("No callback for id %d\n", id);
		return NULL;
	}
	
	return callback;
}


// Find a callnack for the named event/property/action.
// Return NULL if not found.
static deferred_t *find_named_cb(shet_state *state, const char *name, deferred_type_t type)
{
	deferred_t *callback = state->callbacks;
	for (; callback != NULL; callback = callback->next)
		if (   ( type == EVENT_CB &&
		         callback->type == EVENT_CB && 
		         strcmp(callback->data.event_cb.event_name, name) == 0)
		    || ( type == PROP_CB &&
		         callback->type == PROP_CB && 
		         strcmp(callback->data.prop_cb.prop_name, name) == 0)
		    || ( type == ACTION_CB &&
		         callback->type == ACTION_CB && 
		         strcmp(callback->data.action_cb.action_name, name) == 0))
			break;
	
	if (callback == NULL) {
		DPRINTF("No callback under name %s with type %d\n", name, type);
		return NULL;
	}
	
	return callback;
}

////////////////////////////////////////////////////////////////////////////////
// Internal command processing functions
////////////////////////////////////////////////////////////////////////////////

// Deal with a shet 'return' command, calling the appropriate callback.
static void process_return(shet_state *state, char *line, jsmntok_t *tokens)
{
	if (tokens[0].size != 4) {
		DPRINTF("Return messages should be of length 4\n");
		return;
	}
	
	// Requests sent by uSHET always use integer IDs. Note this string is always a
	// substring surrounded by non-numbers so atoi is safe.
	if (!assert_int(line, &(tokens[1]))) return;
	int id = atoi(line + tokens[1].start);
	
	// The success/fail value should be an int. Note this string is always a
	// substring surrounded by non-numbers so atoi is safe.
	if (!assert_int(line, &(tokens[3]))) return;
	int success = atoi(line + tokens[3].start);
	
	// The returned value can be any JSON object
	jsmntok_t *value_token = tokens+4;
	
	// Find the right callback.
	deferred_t *callback = find_return_cb(state, id);
	if (callback == NULL)
		return;
	
	// We want to leave everything in a consistent state, as the callback might
	// make another call to this lib, so get the info we need, "free" everything,
	// *then* callback.  Ideally, we should 'return' the callback to be ran in the
	// main loop.
	
	// User-supplied data.
	void *user_data = callback->data.return_cb.user_data;
	
	// Either the success or fail callback.
	callback_t callback_fun;
	if (success == 0)
		callback_fun = callback->data.return_cb.success_callback;
	else if (success == 1) {
		callback_fun = callback->data.return_cb.error_callback;
		// Fall back to default error callback.
		if (callback_fun == NULL) {
			callback_fun = state->error_callback;
			user_data = state->error_callback_data;
		}
	} else {
		DPRINTF("Bad success/fail value.\n");
		return;
	}
	
	// Remove the callback from the list and clear the fields to indicate that the
	// callback is not registered.
	remove_deferred(state, callback);
	
	// Run the callback if it's not null.
	if (callback_fun != NULL)
		callback_fun(state, line, value_token, user_data);
}


// Process an command from the server
static void process_command(shet_state *state, char *line, jsmntok_t *tokens, command_callback_type_t type)
{
	size_t min_num_parts;
	switch (type) {
		case EVENT_CCB:
		case EVENT_DELETED_CCB:
		case EVENT_CREATED_CCB:
		case GET_PROP_CCB:
			min_num_parts = 3;
			break;
		
		case SET_PROP_CCB:
			min_num_parts = 4;
			break;
		
		default:
			min_num_parts = -1;
	}
	
	if (tokens[0].size < min_num_parts) {
		DPRINTF("Command should be at least %d parts.\n", min_num_parts);
		return;
	}
	
	// Get the name. It is safe to clobber the char after the name since it will
	// be a pair of quotes as part of the JSON syntax.
	if (!assert_type(&(tokens[3]), JSMN_STRING)) return;
	const char *name = line + tokens[3].start;
	line[tokens[3].end] = '\0';
	
	// Find the callback for this event.
	deferred_t *callback = find_named_cb(state, name, EVENT_CB);
	if (callback == NULL)
		return;
	
	// Get the callback and user data.
	callback_t callback_fun;
	switch (type) {
		case EVENT_CCB:         callback_fun = callback->data.event_cb.event_callback; break;
		case EVENT_DELETED_CCB: callback_fun = callback->data.event_cb.deleted_callback; break;
		case EVENT_CREATED_CCB: callback_fun = callback->data.event_cb.created_callback; break;
		case GET_PROP_CCB:      callback_fun = callback->data.prop_cb.get_callback; break;
		case SET_PROP_CCB:      callback_fun = callback->data.prop_cb.set_callback; break;
		default:                callback_fun = NULL; break;
	}
	void *user_data;
	switch (type) {
		case EVENT_CCB:
		case EVENT_CREATED_CCB:
		case EVENT_DELETED_CCB:
			user_data = callback->data.event_cb.user_data;
			break;
		
		case GET_PROP_CCB:
		case SET_PROP_CCB:
			user_data = callback->data.prop_cb.user_data;
			break;
		
		default:
			user_data = NULL;
			break;
	}
	
	// Extract the arguments. Simply truncate the command array (destroys the
	// preceeding token).
	tokens[3] = tokens[0];
	tokens[3].size -= 2;
	
	if (callback_fun != NULL)
		callback_fun(state, line, &(tokens[3]), user_data);
}


// Process a message from shet.
static void process_message(shet_state *state, char *line, jsmntok_t *tokens)
{
	if (!assert_type(&(tokens[0]), JSMN_ARRAY)) return;
	if (tokens[0].size < 2) {
		DPRINTF("Command too short.\n");
		return;
	}
	
	// Note that the command is at least followed by the closing bracket and if
	// not a comma and so nulling out the character following it is safe.
	if (!assert_type(&(tokens[2]), JSMN_STRING)) return;
	const char *command = line + tokens[2].start;
	line[tokens[2].end] = '\0';
	
	// Extract args in indiviaual handling functions -- too much effort to do here.
	if (strcmp(command, "return") == 0)
		process_return(state, line, tokens);
	else if (strcmp(command, "event") == 0)
		process_command(state, line, tokens, EVENT_CCB);
	else if (strcmp(command, "eventdeleted") == 0)
		process_command(state, line, tokens, EVENT_DELETED_CCB);
	else if (strcmp(command, "eventcreated") == 0)
		process_command(state, line, tokens, EVENT_CREATED_CCB);
	else if (strcmp(command, "getprop") == 0)
		process_command(state, line, tokens, GET_PROP_CCB);
	else if (strcmp(command, "setprop") == 0)
		process_command(state, line, tokens, SET_PROP_CCB);
	else
		DPRINTF("unsupported command: \"%s\"\n", command);
}

////////////////////////////////////////////////////////////////////////////////
// Internal message generating functions
////////////////////////////////////////////////////////////////////////////////

// Send a command, and register a callback for the 'return' (if the deferred is
// not NULL).
static void send_command(shet_state *state,
                         const char *command_name,
                         const char *path,
                         const char *args,
                         deferred_t *deferred,
                         callback_t callback,
                         callback_t err_callback,
                         void * callback_arg)
{
	int id = state->next_id++;
	
	// Construct the command...
	snprintf( state->out_buf, SHET_BUF_SIZE-1
	        , "[%d,\"%s\"%s%s%s%s%s]\n"
	        , id
	        , command_name
	        , path ? ",\"" : ""
	        , path ? path : ""
	        , path ? "\"": ""
	        , args ? ","  : ""
	        , args ? args : ""
	        );
	
	// ...and send it
	DPRINTF("%s\n", state->out_buf);
	state->transmit(state->out_buf);
	
	// Register the callback (if supplied).
	if (deferred != NULL) {
		deferred->type = RETURN_CB;
		deferred->data.return_cb.id = id;
		deferred->data.return_cb.success_callback = callback;
		deferred->data.return_cb.error_callback = err_callback;
		deferred->data.return_cb.user_data = callback_arg;
		
		// Push it onto the list.
		add_deferred(state, deferred);
	}
}

////////////////////////////////////////////////////////////////////////////////
// General Library Functions
////////////////////////////////////////////////////////////////////////////////

// Make a new state.
void shet_state_init(shet_state *state, void (*transmit)(const char *data))
{
	state->next_id = 0;
	state->callbacks = NULL;
	state->transmit = transmit;
}

// Set the error callback.
// The given callback will be called on any unhandled error from shet.
void shet_set_error_callback(shet_state *state,
                             callback_t callback,
                             void *callback_arg)
{
	state->error_callback = callback;
	state->error_callback_data = callback_arg;
}

// Given a single line (expected to be a single JSON message), handle whatever
// it contains. The string passed need only remain valid during the call to this
// function. Note that the string will be corrupted.
void shet_process_line(shet_state *state, char *line, size_t line_length)
{
	if (line_length <= 0) {
		DPRINTF("JSON string is too short!\n");
		return;
	}
	
	jsmn_parser p;
	jsmn_init(&p);
	
	jsmnerr_t e = jsmn_parse( &p
	                        , line
	                        , line_length
	                        , state->tokens
	                        , SHET_NUM_TOKENS
	                        );
	switch (e) {
		case JSMN_ERROR_NOMEM:
			DPRINTF("Out of JSON tokens in shet_process_line.\n");
			break;
		
		case JSMN_ERROR_INVAL:
			DPRINTF("Invalid char in JSON string in shet_process_line.\n");
			break;
		
		case JSMN_ERROR_PART:
			DPRINTF("Incomplete JSON string in shet_process_line.\n");
			break;
		
		default:
			if ((int)e > 0) {
				process_message(state, line, state->tokens);
			} else {
				DPRINTF("No tokens in JSON in shet_process_line.\n");
			}
			break;
	}
}

// Cancel a deferred
void shet_cancel_deferred(shet_state *state, deferred_t *deferred) {
	remove_deferred(state, deferred);
}

// Send a ping
void shet_ping(shet_state *state,
               const char *args,
               deferred_t *deferred,
               callback_t callback,
               callback_t err_callback,
               void *callback_arg)
{
	send_command(state, "ping", NULL, args,
	             deferred,
	             callback, err_callback,
	             callback_arg);
}

////////////////////////////////////////////////////////////////////////////////
// Public Functions for actions
////////////////////////////////////////////////////////////////////////////////

// Call an action.
void shet_call_action(shet_state *state,
                     const char *path,
                     const char *args,
                     deferred_t *deferred,
                     callback_t callback,
                     callback_t err_callback,
                     void *callback_arg)
{
	send_command(state, "call", path, args,
	             deferred,
	             callback, err_callback,
	             callback_arg);
}

////////////////////////////////////////////////////////////////////////////////
// Public Functions for properties
////////////////////////////////////////////////////////////////////////////////

// Create a property
void shet_make_prop(shet_state *state,
                    const char *path,
                    deferred_t *prop_deferred,
                    callback_t get_callback,
                    callback_t set_callback,
                    void *prop_arg,
                    deferred_t *mkprop_deferred,
                    callback_t mkprop_callback,
                    callback_t mkprop_error_callback,
                    void *mkprop_callback_arg)
{
	// Make a callback for the property.
	prop_deferred->type = PROP_CB;
	prop_deferred->data.prop_cb.prop_name = path;
	prop_deferred->data.prop_cb.get_callback = get_callback;
	prop_deferred->data.prop_cb.set_callback = set_callback;
	prop_deferred->data.prop_cb.user_data = prop_arg;
	
	// And push it onto the callback list.
	add_deferred(state, prop_deferred);
	
	// Finally, send the command
	send_command(state, "mkprop", path, NULL,
	             mkprop_deferred,
	             mkprop_callback, mkprop_error_callback,
	             mkprop_callback_arg);
}

// Remove a property
void shet_remove_prop(shet_state *state,
                      const char *path,
                      deferred_t *deferred,
                      callback_t callback,
                      callback_t error_callback,
                      void *callback_arg)
{
	// Cancel the property deferred
	deferred_t *prop_deferred = find_named_cb(state, path, PROP_CB);
	if (prop_deferred != NULL)
		remove_deferred(state, prop_deferred);
	
	// Finally, send the command
	send_command(state, "rmprop", path, NULL,
	             deferred,
	             callback, error_callback,
	             callback_arg);
}

// Get a property.
void shet_get_prop(shet_state *state,
                   const char *path,
                   deferred_t *deferred,
                   callback_t callback,
                   callback_t err_callback,
                   void *callback_arg)
{
	send_command(state, "get", path, NULL,
	             deferred,
	             callback, err_callback,
	             callback_arg);
}

// Set a property.
void shet_set_prop(shet_state *state,
                   const char *path,
                   const char *value,
                   deferred_t *deferred,
                   callback_t callback,
                   callback_t err_callback,
                   void *callback_arg)
{
	// Wrap the single argument in a list.
	int value_len = strlen(value);
	char wrapped_value[value_len+3];
	wrapped_value[0] = '[';
	strcpy(wrapped_value+1, value);
	wrapped_value[value_len+1] = ']';
	wrapped_value[value_len+2] = '\0';
	
	send_command(state, "set", path, wrapped_value,
	             deferred,
	             callback, err_callback,
	             callback_arg);
}

////////////////////////////////////////////////////////////////////////////////
// Public Functions for events
////////////////////////////////////////////////////////////////////////////////

// Watch an event.
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
                      void *watch_callback_arg)
{
	// Make a callback for the event.
	event_deferred->type = EVENT_CB;
	event_deferred->data.event_cb.event_name = path;
	event_deferred->data.event_cb.event_callback = event_callback;
	event_deferred->data.event_cb.deleted_callback = deleted_callback;
	event_deferred->data.event_cb.created_callback = created_callback;
	event_deferred->data.event_cb.user_data = callback_arg;
	
	// And push it onto the callback list.
	add_deferred(state, event_deferred);
	
	// Finally, send the command
	send_command(state, "watch", path, NULL,
	             watch_deferred,
	             watch_callback, watch_error_callback,
	             watch_callback_arg);
}


// Ignore an event.
void shet_ignore_event(shet_state *state,
                       const char *path,
                       deferred_t *deferred,
                       callback_t callback,
                       callback_t error_callback,
                       void *callback_arg)
{
	// Cancel the event deferred
	deferred_t *event_deferred = find_named_cb(state, path, EVENT_CB);
	if (event_deferred != NULL)
		remove_deferred(state, event_deferred);
	
	// Finally, send the command
	send_command(state, "ignore", path, NULL,
	             deferred,
	             callback, error_callback,
	             callback_arg);
}
