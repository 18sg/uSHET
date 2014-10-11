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
static void add_deferred(shet_state_t *state, deferred_t *deferred) {
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
void remove_deferred(shet_state_t *state, deferred_t *deferred) {
	// Find the deferred, if present
	deferred_t **iter = &(state->callbacks);
	for (; (*iter) != NULL; iter = &((*iter)->next)) {
		if ((*iter) == deferred) {
			*iter = (*iter)->next;
			break;
		}
	}
}


// Find a deferred callback for the return with the given ID.
// Return NULL if not found.
static deferred_t *find_return_cb(shet_state_t *state, int id)
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
static deferred_t *find_named_cb(shet_state_t *state, const char *name, deferred_type_t type)
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
static void process_return(shet_state_t *state, jsmntok_t *tokens)
{
	if (tokens[0].size != 4) {
		DPRINTF("Return messages should be of length 4\n");
		return;
	}
	
	// Requests sent by uSHET always use integer IDs. Note this string is always a
	// substring surrounded by non-numbers so atoi is safe.
	if (!assert_int(state->line, &(tokens[1]))) return;
	int id = atoi(state->line + tokens[1].start);
	
	// The success/fail value should be an int. Note this string is always a
	// substring surrounded by non-numbers so atoi is safe.
	if (!assert_int(state->line, &(tokens[3]))) return;
	int success = atoi(state->line + tokens[3].start);
	
	// The returned value can be any JSON object
	jsmntok_t *value_token = tokens+4;
	
	// Find the right callback.
	deferred_t *callback = find_return_cb(state, id);
	
	// We want to leave everything in a consistent state, as the callback might
	// make another call to this lib, so get the info we need, "free" everything,
	// *then* callback.  Ideally, we should 'return' the callback to be ran in the
	// main loop.
	
	// Select either the success or fail callback (or fall-back on the 
	callback_t callback_fun = state->error_callback;
	void *user_data = state->error_callback_data;
	if (callback != NULL) {
		user_data = callback->data.return_cb.user_data;
		if (success == 0)
			callback_fun = callback->data.return_cb.success_callback;
		else if (success != 0) {
			callback_fun = callback->data.return_cb.error_callback;
			// Fall back to default error callback.
			if (callback_fun == NULL) {
				callback_fun = state->error_callback;
				user_data = state->error_callback_data;
			}
		}
		remove_deferred(state, callback);
	}
	
	// Run the callback if it's not null.
	if (callback_fun != NULL)
		callback_fun(state, state->line, value_token, user_data);
}


// Process an command from the server
static void process_command(shet_state_t *state, jsmntok_t *tokens, command_callback_type_t type)
{
	size_t min_num_parts;
	switch (type) {
		case EVENT_CCB:
		case EVENT_DELETED_CCB:
		case EVENT_CREATED_CCB:
		case GET_PROP_CCB:
		case CALL_CCB:
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
	const char *name = state->line + tokens[3].start;
	state->line[tokens[3].end] = '\0';
	
	// Find the callback for this event.
	deferred_t *callback = find_named_cb(state, name, type);
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
		case CALL_CCB:          callback_fun = callback->data.action_cb.callback; break;
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
		
		case CALL_CCB:
			user_data = callback->data.action_cb.user_data;
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
		callback_fun(state, state->line, &(tokens[3]), user_data);
}


// Process a message from shet.
static void process_message(shet_state_t *state, jsmntok_t *tokens)
{
	if (!assert_type(&(tokens[0]), JSMN_ARRAY)) return;
	if (tokens[0].size < 2) {
		DPRINTF("Command too short.\n");
		return;
	}
	
	// Note that the command is at least followed by the closing bracket and if
	// not a comma and so nulling out the character following it is safe.
	if (!assert_type(&(tokens[2]), JSMN_STRING)) return;
	const char *command = state->line + tokens[2].start;
	state->line[tokens[2].end] = '\0';
	
	// Extract args in indiviaual handling functions -- too much effort to do here.
	if (strcmp(command, "return") == 0)
		process_return(state, tokens);
	else if (strcmp(command, "event") == 0)
		process_command(state, tokens, EVENT_CCB);
	else if (strcmp(command, "eventdeleted") == 0)
		process_command(state, tokens, EVENT_DELETED_CCB);
	else if (strcmp(command, "eventcreated") == 0)
		process_command(state, tokens, EVENT_CREATED_CCB);
	else if (strcmp(command, "getprop") == 0)
		process_command(state, tokens, GET_PROP_CCB);
	else if (strcmp(command, "setprop") == 0)
		process_command(state, tokens, SET_PROP_CCB);
	else if (strcmp(command, "docall") == 0)
		process_command(state, tokens, CALL_CCB);
	else
		DPRINTF("unsupported command: \"%s\"\n", command);
}

////////////////////////////////////////////////////////////////////////////////
// Internal message generating functions
////////////////////////////////////////////////////////////////////////////////

// Send a command, and register a callback for the 'return' (if the deferred is
// not NULL).
static void send_command(shet_state_t *state,
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
	state->out_buf[SHET_BUF_SIZE-1] = '\0';
	
	// ...and send it
	state->transmit(state->out_buf, state->transmit_user_data);
	
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
void shet_state_init(shet_state_t *state, const char *connection_name,
                     void (*transmit)(const char *data, void *user_data),
                     void *transmit_user_data)
{
	state->next_id = 0;
	state->callbacks = NULL;
	state->connection_name = connection_name;
	state->transmit = transmit;
	state->transmit_user_data = transmit_user_data;
	state->error_callback = NULL;
	state->error_callback_data = NULL;
	
	// Send the initial register command to name this connection
	send_command(state, "register", NULL, state->connection_name,
	             NULL, NULL, NULL, NULL);
}

// Set the error callback.
// The given callback will be called on any unhandled error from shet.
void shet_set_error_callback(shet_state_t *state,
                             callback_t callback,
                             void *callback_arg)
{
	state->error_callback = callback;
	state->error_callback_data = callback_arg;
}

// Given a single line (expected to be a single JSON message), handle whatever
// it contains. The string passed need only remain valid during the call to this
// function. Note that the string will be corrupted.
void shet_process_line(shet_state_t *state, char *line, size_t line_length)
{
	if (line_length <= 0) {
		DPRINTF("JSON string is too short!\n");
		return;
	}
	
	state->line = line;
	
	jsmn_parser p;
	jsmn_init(&p);
	
	jsmnerr_t e = jsmn_parse( &p
	                        , state->line
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
				process_message(state, state->tokens);
			} else {
				DPRINTF("No tokens in JSON in shet_process_line.\n");
			}
			break;
	}
}

// Re-register all actions/properties/events/watches with the remote server.
// This should be called after reconnection.
void shet_reregister(shet_state_t *state) {
	// Cause the server to drop all old objects from this device/application
	send_command(state, "register", NULL, state->connection_name,
	             NULL, NULL, NULL, NULL);
	
	// Cancel any outstanding returns
	deferred_t *iter;
	for (iter = state->callbacks; iter != NULL; iter = iter->next) {
		if (iter->type == RETURN_CB) {
			remove_deferred(state, iter);
			break;
		}
	}
	
	// Re-send all registration commands for watches, properties and actions
	for (iter = state->callbacks; iter != NULL; iter = iter->next) {
		switch(iter->type) {
			case EVENT_CB: {
				// Find the original watch callback deferred
				deferred_t *watch_deferred = iter->data.event_cb.watch_deferred;
				if (watch_deferred->type != RETURN_CB)
					watch_deferred = NULL;
				
				// Re-register the watch
				send_command(state, "watch", iter->data.event_cb.event_name, NULL,
				             watch_deferred,
				             watch_deferred ? watch_deferred->data.return_cb.success_callback : NULL,
				             watch_deferred ? watch_deferred->data.return_cb.error_callback : NULL,
				             watch_deferred ? watch_deferred->data.return_cb.user_data : NULL);
				break;
			}
			
			case ACTION_CB: {
				// Find the original make action callback deferred
				deferred_t *mkaction_deferred = iter->data.action_cb.mkaction_deferred;
				if (mkaction_deferred->type != RETURN_CB)
					mkaction_deferred = NULL;
				
				// Re-create the action
				send_command(state, "mkaction", iter->data.action_cb.action_name, NULL,
				             mkaction_deferred,
				             mkaction_deferred ? mkaction_deferred->data.return_cb.success_callback : NULL,
				             mkaction_deferred ? mkaction_deferred->data.return_cb.error_callback : NULL,
				             mkaction_deferred ? mkaction_deferred->data.return_cb.user_data : NULL);
				break;
			}
			
			case PROP_CB: {
				// Find the original make property callback deferred
				deferred_t *mkprop_deferred = iter->data.prop_cb.mkprop_deferred;
				if (mkprop_deferred->type != RETURN_CB)
					mkprop_deferred = NULL;
				
				// Re-create the property
				send_command(state, "mkprop", iter->data.prop_cb.prop_name, NULL,
				             mkprop_deferred,
				             mkprop_deferred ? mkprop_deferred->data.return_cb.success_callback : NULL,
				             mkprop_deferred ? mkprop_deferred->data.return_cb.error_callback : NULL,
				             mkprop_deferred ? mkprop_deferred->data.return_cb.user_data : NULL);
				break;
			}
		}
	}
	
	// Re-register any events
	event_t *ev_iter;
	for (ev_iter = state->registered_events; ev_iter != NULL; ev_iter = ev_iter->next) {
			// Find the original make event callback deferred
			deferred_t *mkevent_deferred = ev_iter->mkevent_deferred;
			if (mkevent_deferred->type != RETURN_CB)
				mkevent_deferred = NULL;
			
			// Re-register the event
			send_command(state, "mkevent", ev_iter->event_name, NULL,
			             mkevent_deferred,
			             mkevent_deferred ? mkevent_deferred->data.return_cb.success_callback : NULL,
			             mkevent_deferred ? mkevent_deferred->data.return_cb.error_callback : NULL,
			             mkevent_deferred ? mkevent_deferred->data.return_cb.user_data : NULL);
	}
}


// Cancel a deferred
void shet_cancel_deferred(shet_state_t *state, deferred_t *deferred) {
	remove_deferred(state, deferred);
}

// Send a ping
void shet_ping(shet_state_t *state,
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


// Isolate the string containing the return ID.
char *shet_get_return_id(shet_state_t *state)
{
	// Add the appropriate prefix/postfix to the incoming ID based on the type and
	// null terminate it. This is safe since the ID is part of an array and thus
	// there is at least one trailing character which can be clobbered (the comma)
	// with the null.
	switch (state->recv_id->type) {
		case JSMN_STRING:
			state->line[state->recv_id->start - 1] = '\"';
			state->line[state->recv_id->end]       = '\"';
			state->line[state->recv_id->end + 1]   = '\0';
			return state->line + state->recv_id->start - 1;
		
		case JSMN_ARRAY:
			state->line[state->recv_id->start - 1] = '[';
			state->line[state->recv_id->end]       = ']';
			state->line[state->recv_id->end + 1]   = '\0';
			return state->line + state->recv_id->start - 1;
		
		case JSMN_OBJECT:
			state->line[state->recv_id->start - 1] = '{';
			state->line[state->recv_id->end]       = '}';
			state->line[state->recv_id->end + 1]   = '\0';
			return state->line + state->recv_id->start - 1;
		
		default:
		case JSMN_PRIMITIVE:
			state->line[state->recv_id->end] = '\0';
			return state->line + state->recv_id->start;
	}
}


// Return a response to the last command received
void shet_return_with_id(shet_state_t *state,
                         const char *id,
                         int success,
                         const char *value)
{
	// Construct the command...
	snprintf( state->out_buf, SHET_BUF_SIZE-1
	        , "[%s,\"return\",%d,%s]\n"
	        , id
	        , success
	        , value ? value : "[]"
	        );
	state->out_buf[SHET_BUF_SIZE-1] = '\0';
	
	// ...and send it
	state->transmit(state->out_buf, state->transmit_user_data);
}


// Return a response to the last command received
void shet_return(shet_state_t *state,
                 int success,
                 const char *value)
{
	shet_return_with_id(state,
	                    shet_get_return_id(state),
	                    success,
	                    value);
}

////////////////////////////////////////////////////////////////////////////////
// Public Functions for actions
////////////////////////////////////////////////////////////////////////////////

// Create an action
void shet_make_action(shet_state_t *state,
                      const char *path,
                      deferred_t *action_deferred,
                      callback_t callback,
                      void *action_arg,
                      deferred_t *mkaction_deferred,
                      callback_t mkaction_callback,
                      callback_t mkaction_error_callback,
                      void *mkaction_callback_arg)
{
	// Make a callback for the property.
	action_deferred->type = ACTION_CB;
	action_deferred->data.action_cb.mkaction_deferred = mkaction_deferred;
	action_deferred->data.action_cb.action_name = path;
	action_deferred->data.action_cb.callback = callback;
	action_deferred->data.action_cb.user_data = action_arg;
	
	// And push it onto the callback list.
	add_deferred(state, action_deferred);
	
	// Finally, send the command
	send_command(state, "mkaction", path, NULL,
	             mkaction_deferred,
	             mkaction_callback, mkaction_error_callback,
	             mkaction_callback_arg);
}

// Remove an action
void shet_remove_action(shet_state_t *state,
                        const char *path,
                        deferred_t *deferred,
                        callback_t callback,
                        callback_t error_callback,
                        void *callback_arg)
{
	// Cancel the action deferred and associated mkaction deferred
	deferred_t *action_deferred = find_named_cb(state, path, ACTION_CB);
	if (action_deferred != NULL) {
		remove_deferred(state, action_deferred);
		if (action_deferred->data.action_cb.mkaction_deferred != NULL)
			remove_deferred(state, action_deferred->data.action_cb.mkaction_deferred);
	}
	
	// Finally, send the command
	send_command(state, "rmaction", path, NULL,
	             deferred,
	             callback, error_callback,
	             callback_arg);
}

// Call an action.
void shet_call_action(shet_state_t *state,
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
void shet_make_prop(shet_state_t *state,
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
	prop_deferred->data.prop_cb.mkprop_deferred = mkprop_deferred;
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
void shet_remove_prop(shet_state_t *state,
                      const char *path,
                      deferred_t *deferred,
                      callback_t callback,
                      callback_t error_callback,
                      void *callback_arg)
{
	// Cancel the property deferred (and that of the mkprop callback)
	deferred_t *prop_deferred = find_named_cb(state, path, PROP_CB);
	if (prop_deferred != NULL) {
		remove_deferred(state, prop_deferred);
		if (prop_deferred->data.prop_cb.mkprop_deferred != NULL)
			remove_deferred(state, prop_deferred->data.prop_cb.mkprop_deferred);
	}
	
	// Finally, send the command
	send_command(state, "rmprop", path, NULL,
	             deferred,
	             callback, error_callback,
	             callback_arg);
}

// Get a property.
void shet_get_prop(shet_state_t *state,
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
void shet_set_prop(shet_state_t *state,
                   const char *path,
                   const char *value,
                   deferred_t *deferred,
                   callback_t callback,
                   callback_t err_callback,
                   void *callback_arg)
{
	send_command(state, "set", path, value,
	             deferred,
	             callback, err_callback,
	             callback_arg);
}

////////////////////////////////////////////////////////////////////////////////
// Public Functions for events
////////////////////////////////////////////////////////////////////////////////

// Create an event
void shet_make_event(shet_state_t *state,
                     const char *path,
                     event_t *event,
                     deferred_t *mkevent_deferred,
                     callback_t mkevent_callback,
                     callback_t mkevent_error_callback,
                     void *mkevent_callback_arg)
{
	// Setup the event and push it into the callback list.
	event->event_name = path;
	event->mkevent_deferred = mkevent_deferred;
	event->next = state->registered_events;
	state->registered_events = event;
	
	// Finally, send the command
	send_command(state, "mkevent", path, NULL,
	             mkevent_deferred,
	             mkevent_callback, mkevent_error_callback,
	             mkevent_callback_arg);
}


// Remove an event
void shet_remove_event(shet_state_t *state,
                       const char *path,
                       deferred_t *deferred,
                       callback_t callback,
                       callback_t error_callback,
                       void *callback_arg)
{
	// Remove the event from the list
	event_t **iter = &(state->registered_events);
	for (;*iter != NULL; iter = &((*iter)->next)) {
		if (strcmp((*iter)->event_name, path) == 0) {
			*iter = (*iter)->next;
			// Also remove the mkevent deferred
			if ((*iter)->mkevent_deferred != NULL)
				remove_deferred(state, (*iter)->mkevent_deferred);
			break;
		}
	}
	
	// Finally, send the command
	send_command(state, "rmevent", path, NULL,
	             deferred,
	             callback, error_callback,
	             callback_arg);
}


// Raise an event
void shet_raise_event(shet_state_t *state,
                      const char *path,
                      const char *value,
                      deferred_t *deferred,
                      callback_t callback,
                      callback_t err_callback,
                      void *callback_arg)
{
	send_command(state, "raise", path, value,
	             deferred,
	             callback, err_callback,
	             callback_arg);
}


// Watch an event.
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
                      void *watch_callback_arg)
{
	// Make a callback for the event.
	event_deferred->type = EVENT_CB;
	event_deferred->data.event_cb.watch_deferred = watch_deferred;
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
void shet_ignore_event(shet_state_t *state,
                       const char *path,
                       deferred_t *deferred,
                       callback_t callback,
                       callback_t error_callback,
                       void *callback_arg)
{
	// Cancel the watch deferred and associated event deferred
	deferred_t *event_deferred = find_named_cb(state, path, EVENT_CB);
	if (event_deferred != NULL) {
		deferred_t *watch_deferred = event_deferred->data.event_cb.watch_deferred;
		if (watch_deferred != NULL) {
			remove_deferred(state, event_deferred);
		}
		remove_deferred(state, event_deferred);
	}
	
	// Finally, send the command
	send_command(state, "ignore", path, NULL,
	             deferred,
	             callback, error_callback,
	             callback_arg);
}
