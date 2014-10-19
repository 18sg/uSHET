#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "shet.h"
#include "shet_json.h"

#include "jsmn.h"

#ifdef SHET_DEBUG
#define DPRINTF(...) fprintf(stderr, __VA_ARGS__)
#else
#define DPRINTF(...)
#endif


////////////////////////////////////////////////////////////////////////////////
// Internal deferred utility functions/macros
////////////////////////////////////////////////////////////////////////////////

// Given shet state, makes sure that the deferred is in the callback list,
// adding it if it isn't already present.
static void add_deferred(shet_state_t *state, shet_deferred_t *deferred) {
	shet_deferred_t **deferreds = &(state->callbacks);
	
	// Check to see if the deferred is already present
	shet_deferred_t *iter = (*deferreds);
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
void remove_deferred(shet_state_t *state, shet_deferred_t *deferred) {
	// Find and remove the deferred, if present.
	shet_deferred_t **iter = &(state->callbacks);
	for (; (*iter) != NULL; iter = &((*iter)->next)) {
		if ((*iter) == deferred) {
			*iter = (*iter)->next;
			break;
		}
	}
}


// Find a deferred callback for the return with the given ID.
// Return NULL if not found.
static shet_deferred_t *find_return_cb(shet_state_t *state, int id)
{
	shet_deferred_t *callback = state->callbacks;
	for (; callback != NULL; callback = callback->next)
		if (callback->type == SHET_RETURN_CB && callback->data.return_cb.id == id)
			break;
	
	if (callback == NULL) {
		DPRINTF("No callback for id %d\n", id);
		return NULL;
	}
	
	return callback;
}


// Find a callnack for the named event/property/action.
// Return NULL if not found.
static shet_deferred_t *find_named_cb(shet_state_t *state, const char *name, shet_deferred_type_t type)
{
	shet_deferred_t *callback = state->callbacks;
	for (; callback != NULL; callback = callback->next)
		if (   ( type == SHET_EVENT_CB &&
		         callback->type == SHET_EVENT_CB && 
		         strcmp(callback->data.event_cb.event_name, name) == 0)
		    || ( type == SHET_PROP_CB &&
		         callback->type == SHET_PROP_CB && 
		         strcmp(callback->data.prop_cb.prop_name, name) == 0)
		    || ( type == SHET_ACTION_CB &&
		         callback->type == SHET_ACTION_CB && 
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
static shet_processing_error_t process_return(shet_state_t *state, shet_json_t json)
{
	if (json.token[0].size != 4) {
		DPRINTF("Return messages should be of length 4\n");
		return SHET_PROC_MALFORMED_RETURN;
	}
	
	// Requests sent by uSHET always use integer IDs.
	shet_json_t id_json;
	id_json.line = json.line;
	id_json.token = json.token + 1;
	if (!SHET_JSON_IS_TYPE(id_json, SHET_INT))
		return SHET_PROC_MALFORMED_RETURN;
	int id = SHET_PARSE_JSON_VALUE(id_json, SHET_INT);
	
	// The success/fail value should be an int. Note this string is always a
	// substring surrounded by non-numbers so atoi is safe.
	shet_json_t success_json;
	success_json.line = json.line;
	success_json.token = json.token + 3;
	if (!SHET_JSON_IS_TYPE(success_json, SHET_INT))
		return SHET_PROC_MALFORMED_RETURN;
	int success = SHET_PARSE_JSON_VALUE(success_json, SHET_INT);
	
	// The returned value can be any JSON object
	shet_json_t value_json;
	value_json.line = json.line;
	value_json.token = json.token+4;
	
	// Find the right callback.
	shet_deferred_t *callback = find_return_cb(state, id);
	
	// We want to leave everything in a consistent state, as the callback might
	// make another call to this lib, so get the info we need, "free" everything,
	// *then* callback.
	
	// Select either the success or fail callback (or fall-back on the 
	shet_callback_t callback_fun = NULL;
	void *user_data = state->error_callback_data;
	if (callback != NULL) {
		if (success == 0)
			callback_fun = callback->data.return_cb.success_callback;
		else if (success != 0)
			callback_fun = callback->data.return_cb.error_callback;
		user_data = callback->data.return_cb.user_data;
		remove_deferred(state, callback);
	}
	
	// Fall back to default error callback.
	if (success != 0 && callback_fun == NULL) {
		callback_fun = state->error_callback;
		user_data = state->error_callback_data;
	}
	
	// Run the callback if it's not null.
	if (callback_fun != NULL)
		callback_fun(state, value_json, user_data);
	
	return SHET_PROC_OK;
}


// Process a command from the server
static shet_processing_error_t process_command(shet_state_t *state, shet_json_t json, command_callback_type_t type)
{
	size_t min_num_parts;
	switch (type) {
		case SHET_EVENT_DELETED_CCB:
		case SHET_EVENT_CREATED_CCB:
		case SHET_GET_PROP_CCB:
			if (json.token[0].size != 3) {
				DPRINTF("Command accepts no arguments.\n");
				return SHET_PROC_MALFORMED_ARGUMENTS;
			} else {
				break;
			}
		
		case SHET_SET_PROP_CCB:
			if (json.token[0].size != 4) {
				DPRINTF("Command accepts exactly one argument.\n");
				return SHET_PROC_MALFORMED_ARGUMENTS;
			} else {
				break;
			}
		
		case SHET_EVENT_CCB:
		case SHET_CALL_CCB:
			if (json.token[0].size < 3) {
				DPRINTF("Commands must have at least 3 components.\n");
				return SHET_PROC_MALFORMED_ARGUMENTS;
			} else {
				break;
			}
		
		default:
			return SHET_PROC_MALFORMED_ARGUMENTS;
	}
	
	// Get the path name.
	shet_json_t name_json = shet_next_token(shet_next_token(state->recv_id));
	if (!SHET_JSON_IS_TYPE(name_json, SHET_STRING))
		return SHET_PROC_MALFORMED_COMMAND;
	const char *name = SHET_PARSE_JSON_VALUE(name_json, SHET_STRING);
	
	// Find the callback for this event.
	shet_deferred_t *callback;
	switch (type) {
		case SHET_EVENT_CCB:
		case SHET_EVENT_DELETED_CCB:
		case SHET_EVENT_CREATED_CCB:
			callback = find_named_cb(state, name, SHET_EVENT_CB);
			break;
		
		case SHET_GET_PROP_CCB:
		case SHET_SET_PROP_CCB:
			callback = find_named_cb(state, name, SHET_PROP_CB);
			break;
		
		case SHET_CALL_CCB:
			callback = find_named_cb(state, name, SHET_ACTION_CB);
			break;
		
		default:
			callback = NULL;
			break;
	}
	
	// Get the deferred callback and user data.
	shet_callback_t callback_fun = NULL;
	if (callback != NULL) {
		switch (type) {
			case SHET_EVENT_CCB:         callback_fun = callback->data.event_cb.event_callback; break;
			case SHET_EVENT_DELETED_CCB: callback_fun = callback->data.event_cb.deleted_callback; break;
			case SHET_EVENT_CREATED_CCB: callback_fun = callback->data.event_cb.created_callback; break;
			case SHET_GET_PROP_CCB:      callback_fun = callback->data.prop_cb.get_callback; break;
			case SHET_SET_PROP_CCB:      callback_fun = callback->data.prop_cb.set_callback; break;
			case SHET_CALL_CCB:          callback_fun = callback->data.action_cb.callback; break;
			default:                callback_fun = NULL; break;
		}
	}
	
	if (callback_fun == NULL) {
		// No callback function specified, generate an appropriate 
		switch (type) {
			case SHET_EVENT_CCB:
			case SHET_EVENT_DELETED_CCB:
			case SHET_EVENT_CREATED_CCB:
				shet_return(state, 0, NULL);
				break;
			
			case SHET_GET_PROP_CCB:
			case SHET_SET_PROP_CCB:
			case SHET_CALL_CCB:
				shet_return(state, 1, "\"No callback handler registered!\"");
				break;
		}
		
		return SHET_PROC_OK;
	} else {
		// Execute the user's callback function
		void *user_data;
		switch (type) {
			case SHET_EVENT_CCB:
			case SHET_EVENT_CREATED_CCB:
			case SHET_EVENT_DELETED_CCB:
				user_data = callback->data.event_cb.user_data;
				break;
			
			case SHET_GET_PROP_CCB:
			case SHET_SET_PROP_CCB:
				user_data = callback->data.prop_cb.user_data;
				break;
			
			case SHET_CALL_CCB:
				user_data = callback->data.action_cb.user_data;
				break;
			
			default:
				user_data = NULL;
				break;
		}
		
		// Find the first argument (if one is present)
		shet_json_t args_json = shet_next_token(name_json);
		jsmntok_t *first_arg_token = args_json.token;
		
		// Truncate the array (remove the ID, command and path)
		args_json.token = first_arg_token - 1;
		*args_json.token = json.token[0];
		args_json.token->size = json.token[0].size - 3;
		if (args_json.token->size > 0) {
			// If the array string now starts with a string, move the start to just
			// before the opening quotes, otherwise move to just before the indicated
			// start of the first element.
			if (first_arg_token->type == JSMN_STRING)
				args_json.token->start = first_arg_token->start - 2;
			else
				args_json.token->start = first_arg_token->start - 1;
		} else {
			// If the new array is empty, the array's first character is just before the
			// closing bracket.
			args_json.token->start = args_json.token->end - 2;
		}
		// Add the opening bracket. Note that this *may* corrupt the path argument but
		// since it won't be used again, this isn't a problem.
		args_json.line[args_json.token->start] = '[';
		
		if (callback_fun != NULL)
			callback_fun(state, args_json, user_data);
		
		return SHET_PROC_OK;
	}
}


// Process a message from shet.
static shet_processing_error_t process_message(shet_state_t *state, shet_json_t json)
{
	
	if (!SHET_JSON_IS_TYPE(json, SHET_ARRAY))
		return SHET_PROC_MALFORMED_COMMAND;
	
	if (json.token[0].size < 2) {
		DPRINTF("Command too short.\n");
		return SHET_PROC_MALFORMED_COMMAND;
	}
	
	// Record the ID token
	state->recv_id.line  = json.line;
	state->recv_id.token = json.token + 1;
	
	// Get the command
	shet_json_t command_json = shet_next_token(state->recv_id);
	if (!SHET_JSON_IS_TYPE(command_json, SHET_STRING))
		return SHET_PROC_MALFORMED_COMMAND;
	const char *command = SHET_PARSE_JSON_VALUE(command_json, SHET_STRING);
	
	// Handle commands separately
	if (strcmp(command, "return") == 0)
		return process_return(state, json);
	else if (strcmp(command, "event") == 0)
		return process_command(state, json, SHET_EVENT_CCB);
	else if (strcmp(command, "eventdeleted") == 0)
		return process_command(state, json, SHET_EVENT_DELETED_CCB);
	else if (strcmp(command, "eventcreated") == 0)
		return process_command(state, json, SHET_EVENT_CREATED_CCB);
	else if (strcmp(command, "getprop") == 0)
		return process_command(state, json, SHET_GET_PROP_CCB);
	else if (strcmp(command, "setprop") == 0)
		return process_command(state, json, SHET_SET_PROP_CCB);
	else if (strcmp(command, "docall") == 0)
		return process_command(state, json, SHET_CALL_CCB);
	else {
		DPRINTF("Unknown command: \"%s\"\n", command);
		shet_return(state, 1, "Unknown command.");
		return SHET_PROC_UNKNOWN_COMMAND;
	}
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
                         shet_deferred_t *deferred,
                         shet_callback_t callback,
                         shet_callback_t err_callback,
                         void * callback_arg)
{
	int id = state->next_id++;
	
	// Construct the command...
	snprintf( state->out_buf, SHET_BUF_SIZE-1
	        , "[%d,\"%s\"%s%s%s%s%s]\r\n"
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
		deferred->type = SHET_RETURN_CB;
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

void shet_state_init(shet_state_t *state, const char *connection_name,
                     void (*transmit)(const char *data, void *user_data),
                     void *transmit_user_data)
{
	state->next_id = 0;
	state->callbacks = NULL;
	state->registered_events = NULL;
	state->connection_name = connection_name;
	state->transmit = transmit;
	state->transmit_user_data = transmit_user_data;
	state->error_callback = NULL;
	state->error_callback_data = NULL;
	
	// Send the initial register command to name this connection
	shet_reregister(state);
}

void shet_set_error_callback(shet_state_t *state,
                             shet_callback_t callback,
                             void *callback_arg)
{
	state->error_callback = callback;
	state->error_callback_data = callback_arg;
}

shet_processing_error_t shet_process_line(shet_state_t *state, char *line, size_t line_length)
{
	if (line_length <= 0) {
		DPRINTF("JSON string is too short!\n");
		return SHET_PROC_INVALID_JSON;
	}
	
	shet_json_t json;
	json.line  = line;
	json.token = state->tokens;
	
	jsmn_parser p;
	jsmn_init(&p);
	
	jsmnerr_t e = jsmn_parse( &p
	                        , json.line
	                        , line_length
	                        , json.token
	                        , SHET_NUM_TOKENS
	                        );
	switch (e) {
		case JSMN_ERROR_NOMEM:
			DPRINTF("Out of JSON tokens in shet_process_line: %.*s\n",
			        line_length, json.line);
			return SHET_PROC_ERR_OUT_OF_TOKENS;
		
		case JSMN_ERROR_INVAL:
			DPRINTF("Invalid char in JSON string in shet_process_line: %.*s\n",
			        line_length, json.line);
			return SHET_PROC_INVALID_JSON;
		
		case JSMN_ERROR_PART:
			DPRINTF("Incomplete JSON string in shet_process_line: %.*s.\n",
			        line_length, json.line);
			return SHET_PROC_INVALID_JSON;
		
		default:
			if ((int)e > 0) {
				return process_message(state, json);
			} else {
				return SHET_PROC_INVALID_JSON;
			}
			break;
	}
}

void shet_reregister(shet_state_t *state) {
	// Cause the server to drop all old objects from this device/application
	send_command(state, "register", NULL, state->connection_name,
	             NULL, NULL, NULL, NULL);
	// Re-send all registration commands for watches, properties and actions
	shet_deferred_t *iter;
	for (iter = state->callbacks; iter != NULL; iter = iter->next) {
		switch(iter->type) {
			case SHET_EVENT_CB: {
				// Find the original watch callback deferred
				shet_deferred_t *watch_deferred = iter->data.event_cb.watch_deferred;
				if (watch_deferred != NULL && watch_deferred->type != SHET_RETURN_CB)
					watch_deferred = NULL;
				
				// Re-register the watch
				send_command(state, "watch", iter->data.event_cb.event_name, NULL,
				             watch_deferred,
				             watch_deferred ? watch_deferred->data.return_cb.success_callback : NULL,
				             watch_deferred ? watch_deferred->data.return_cb.error_callback : NULL,
				             watch_deferred ? watch_deferred->data.return_cb.user_data : NULL);
				break;
			}
			
			case SHET_ACTION_CB: {
				// Find the original make action callback deferred
				shet_deferred_t *mkaction_deferred = iter->data.action_cb.mkaction_deferred;
				if (mkaction_deferred != NULL && mkaction_deferred->type != SHET_RETURN_CB)
					mkaction_deferred = NULL;
				
				// Re-create the action
				send_command(state, "mkaction", iter->data.action_cb.action_name, NULL,
				             mkaction_deferred,
				             mkaction_deferred ? mkaction_deferred->data.return_cb.success_callback : NULL,
				             mkaction_deferred ? mkaction_deferred->data.return_cb.error_callback : NULL,
				             mkaction_deferred ? mkaction_deferred->data.return_cb.user_data : NULL);
				break;
			}
			
			case SHET_PROP_CB: {
				// Find the original make property callback deferred
				shet_deferred_t *mkprop_deferred = iter->data.prop_cb.mkprop_deferred;
				if (mkprop_deferred != NULL && mkprop_deferred->type != SHET_RETURN_CB)
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
	shet_event_t *ev_iter;
	for (ev_iter = state->registered_events; ev_iter != NULL; ev_iter = ev_iter->next) {
			// Find the original make event callback deferred
			shet_deferred_t *mkevent_deferred = ev_iter->mkevent_deferred;
			if (mkevent_deferred != NULL && mkevent_deferred->type != SHET_RETURN_CB)
				mkevent_deferred = NULL;
			
			// Re-register the event
			send_command(state, "mkevent", ev_iter->event_name, NULL,
			             mkevent_deferred,
			             mkevent_deferred ? mkevent_deferred->data.return_cb.success_callback : NULL,
			             mkevent_deferred ? mkevent_deferred->data.return_cb.error_callback : NULL,
			             mkevent_deferred ? mkevent_deferred->data.return_cb.user_data : NULL);
	}
}


void shet_cancel_deferred(shet_state_t *state, shet_deferred_t *deferred) {
	remove_deferred(state, deferred);
}

void shet_ping(shet_state_t *state,
               const char *args,
               shet_deferred_t *deferred,
               shet_callback_t callback,
               shet_callback_t err_callback,
               void *callback_arg)
{
	send_command(state, "ping", NULL, args,
	             deferred,
	             callback, err_callback,
	             callback_arg);
}


const char *shet_get_return_id(shet_state_t *state)
{
	// Null terminate the ID. This is safe since the ID is part of an array and
	// thus there is at least one trailing character which can be clobbered (the
	// comma) with the null. Also, note that string start/ends must be extended to
	// include quotes. Other types start & end already include their surrounding
	// brackets etc.
	if (state->recv_id.token->type == JSMN_STRING) {
		state->recv_id.line[--(state->recv_id.token->start)] = '\"';
		state->recv_id.line[state->recv_id.token->end++] = '\"';
		state->recv_id.line[state->recv_id.token->end] = '\0';
		return state->recv_id.line + state->recv_id.token->start;
	} else {
		state->recv_id.line[state->recv_id.token->end] = '\0';
		return state->recv_id.line + state->recv_id.token->start;
	}
}

void shet_return_with_id(shet_state_t *state,
                         const char *id,
                         int success,
                         const char *value)
{
	// Construct the command...
	snprintf( state->out_buf, SHET_BUF_SIZE-1
	        , "[%s,\"return\",%d,%s]\r\n"
	        , id
	        , success
	        , value ? value : "null"
	        );
	state->out_buf[SHET_BUF_SIZE-1] = '\0';
	
	// ...and send it
	state->transmit(state->out_buf, state->transmit_user_data);
}


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

void shet_make_action(shet_state_t *state,
                      const char *path,
                      shet_deferred_t *action_deferred,
                      shet_callback_t callback,
                      void *action_arg,
                      shet_deferred_t *mkaction_deferred,
                      shet_callback_t mkaction_callback,
                      shet_callback_t mkaction_error_callback,
                      void *mkaction_callback_arg)
{
	// Make a callback for the property.
	action_deferred->type = SHET_ACTION_CB;
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

void shet_remove_action(shet_state_t *state,
                        const char *path,
                        shet_deferred_t *deferred,
                        shet_callback_t callback,
                        shet_callback_t error_callback,
                        void *callback_arg)
{
	// Cancel the action deferred and associated mkaction deferred
	shet_deferred_t *action_deferred = find_named_cb(state, path, SHET_ACTION_CB);
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

void shet_call_action(shet_state_t *state,
                     const char *path,
                     const char *args,
                     shet_deferred_t *deferred,
                     shet_callback_t callback,
                     shet_callback_t err_callback,
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

void shet_make_prop(shet_state_t *state,
                    const char *path,
                    shet_deferred_t *prop_deferred,
                    shet_callback_t get_callback,
                    shet_callback_t set_callback,
                    void *prop_arg,
                    shet_deferred_t *mkprop_deferred,
                    shet_callback_t mkprop_callback,
                    shet_callback_t mkprop_error_callback,
                    void *mkprop_callback_arg)
{
	// Make a callback for the property.
	prop_deferred->type = SHET_PROP_CB;
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

void shet_remove_prop(shet_state_t *state,
                      const char *path,
                      shet_deferred_t *deferred,
                      shet_callback_t callback,
                      shet_callback_t error_callback,
                      void *callback_arg)
{
	// Cancel the property deferred (and that of the mkprop callback)
	shet_deferred_t *prop_deferred = find_named_cb(state, path, SHET_PROP_CB);
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

void shet_get_prop(shet_state_t *state,
                   const char *path,
                   shet_deferred_t *deferred,
                   shet_callback_t callback,
                   shet_callback_t err_callback,
                   void *callback_arg)
{
	send_command(state, "get", path, NULL,
	             deferred,
	             callback, err_callback,
	             callback_arg);
}

void shet_set_prop(shet_state_t *state,
                   const char *path,
                   const char *value,
                   shet_deferred_t *deferred,
                   shet_callback_t callback,
                   shet_callback_t err_callback,
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

void shet_make_event(shet_state_t *state,
                     const char *path,
                     shet_event_t *event,
                     shet_deferred_t *mkevent_deferred,
                     shet_callback_t mkevent_callback,
                     shet_callback_t mkevent_error_callback,
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


void shet_remove_event(shet_state_t *state,
                       const char *path,
                       shet_deferred_t *deferred,
                       shet_callback_t callback,
                       shet_callback_t error_callback,
                       void *callback_arg)
{
	// Remove the event from the list
	shet_event_t **iter = &(state->registered_events);
	for (;*iter != NULL; iter = &((*iter)->next)) {
		if (strcmp((*iter)->event_name, path) == 0) {
			// Also remove the mkevent deferred
			if ((*iter)->mkevent_deferred != NULL)
				remove_deferred(state, (*iter)->mkevent_deferred);
			*iter = (*iter)->next;
			break;
		}
	}
	
	// Finally, send the command
	send_command(state, "rmevent", path, NULL,
	             deferred,
	             callback, error_callback,
	             callback_arg);
}


void shet_raise_event(shet_state_t *state,
                      const char *path,
                      const char *value,
                      shet_deferred_t *deferred,
                      shet_callback_t callback,
                      shet_callback_t err_callback,
                      void *callback_arg)
{
	send_command(state, "raise", path, value,
	             deferred,
	             callback, err_callback,
	             callback_arg);
}


void shet_watch_event(shet_state_t *state,
                      const char *path,
                      shet_deferred_t *event_deferred,
                      shet_callback_t event_callback,
                      shet_callback_t created_callback,
                      shet_callback_t deleted_callback,
                      void *event_arg,
                      shet_deferred_t *watch_deferred,
                      shet_callback_t watch_callback,
                      shet_callback_t watch_error_callback,
                      void *watch_callback_arg)
{
	// Make a callback for the event.
	event_deferred->type = SHET_EVENT_CB;
	event_deferred->data.event_cb.watch_deferred = watch_deferred;
	event_deferred->data.event_cb.event_name = path;
	event_deferred->data.event_cb.event_callback = event_callback;
	event_deferred->data.event_cb.created_callback = created_callback;
	event_deferred->data.event_cb.deleted_callback = deleted_callback;
	event_deferred->data.event_cb.user_data = event_arg;
	
	// And push it onto the callback list.
	add_deferred(state, event_deferred);
	
	// Finally, send the command
	send_command(state, "watch", path, NULL,
	             watch_deferred,
	             watch_callback, watch_error_callback,
	             watch_callback_arg);
}


void shet_ignore_event(shet_state_t *state,
                       const char *path,
                       shet_deferred_t *deferred,
                       shet_callback_t callback,
                       shet_callback_t error_callback,
                       void *callback_arg)
{
	// Cancel the watch deferred and associated event deferred
	shet_deferred_t *event_deferred = find_named_cb(state, path, SHET_EVENT_CB);
	if (event_deferred != NULL) {
		shet_deferred_t *watch_deferred = event_deferred->data.event_cb.watch_deferred;
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
