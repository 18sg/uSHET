#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "shet.h"
#include "util.h"

#include "jsmn.h"

#ifdef DEBUG
#define DPRINTF(...) fprintf(stderr, __VA_ARGS__)
#else
#define DPRINTF(...)
#endif


// Assert the type of a JSON object
static bool assert_type(const jsmntok_t *token, jsmntype_t type)
{
	if (token->type != type)
		DPRINTF("Expected token type %d, got %d.\n", type, token->type);
	
	return token->type != type;
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

// Deal with a shet 'return' command, calling the appropriate callback.
static void process_return(shet_state *state, jsmntok_t *tokens)
{
	if (tokens[0].size != 4) {
		DPRINTF("Return messages should be of length 4");
		return;
	}
	
	// Requests sent by uSHET always use integer IDs. Note this string is always a
	// substring surrounded by non-numbers so atoi is safe.
	assert_int(state->line, &(tokens[1]));
	int id = atoi(state->line + tokens[1].start);
	
	// The success/fail value should be an int. Note this string is always a
	// substring surrounded by non-numbers so atoi is safe.
	assert_int(state->line, &(tokens[2]));
	int success = atoi(state->line + tokens[2].start);
	
	// The returned value can be any JSON object
	jsmntok_t *value_token = tokens+3;
	
	// Find the right callback.
	callback_list **callback = &(state->callbacks);
	for (; *callback != NULL; callback = &(*callback)->next)
		if ((*callback)->type == RETURN_CB && (*callback)->data.return_cb.id == id)
			break;
	
	if (*callback == NULL) {
		DPRINTF("no callback for id %d\n", id);
		return;
	}
	
	// We want to leave everything in a consistent state, as the callback might
	// make another call to this lib, so get the info we need, "free" everything,
	// *then* callback.  Ideally, we should 'return' the callback to be ran in the
	// main loop.
	
	// User-supplied data.
	void *user_data = (*callback)->data.return_cb.user_data;
	
	// Either the success or fail callback.
	callback_t callback_fun;
	if (success == 0)
		callback_fun = (*callback)->data.return_cb.success_callback;
	else if (success == 1) {
		callback_fun = (*callback)->data.return_cb.error_callback;
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
	callback_list *next = (*callback)->next;
	(*callback)->next = NULL;
	(*callback)->data.return_cb.success_callback = NULL;
	(*callback)->data.return_cb.error_callback = NULL;
	*callback = next;
	
	// Run the callback if it's not null.
	if (callback_fun != NULL)
		callback_fun(state, value_token, user_data);
}

// Find a callnack for the named event.
// Return NULL if not found.
static callback_list **find_event_cb(shet_state *state, const char *name)
{
	callback_list **callback = &(state->callbacks);
	for (; *callback != NULL; callback = &(*callback)->next)
		if ((*callback)->type == EVENT_CB && 
		    strcmp((*callback)->data.event_cb.event_name, name) == 0)
			break;
	
	if (*callback == NULL) {
		DPRINTF("No callback for event %s\n", name);
		return NULL;
	}
	
	return callback;
}

// Process an evant callback.
// Ad the processing is basically the same.
static void process_event(shet_state *state, jsmntok_t *tokens, event_callback_type type)
{
	if (tokens[0].size < 3) {
		DPRINTF("Event messages should be at least 3 parts.\n");
		return;
	}
	
	// Get the name. It is safe to clobber the char after the name since it will
	// be a pair of quotes as part of the JSON syntax.
	assert_type(&(tokens[3]), JSMN_STRING);
	const char *name = state->line + tokens[3].start;
	state->line[tokens[3].end + 1] = '\0';
	
	// Find the callback for this event.
	callback_list **callback = find_event_cb(state, name);
	if (callback == NULL)
		return;
	
	// Get the callback and user data.
	callback_t callback_fun;
	switch (type) {
		case EVENT_ECB:         callback_fun = (*callback)->data.event_cb.event_callback; break;
		case EVENT_DELETED_ECB: callback_fun = (*callback)->data.event_cb.deleted_callback; break;
		case EVENT_CREATED_ECB: callback_fun = (*callback)->data.event_cb.created_callback; break;
		default: die("This error was put here to make the compiler STFU. "
		             "If you've just seen this message, then [tomn] is an idiot.\n");
	}
	void *user_data = (*callback)->data.event_cb.user_data;
	
	// Extract the arguments. Simply truncate the command array (destroys the
	// preceeding token).
	tokens[3] = tokens[0];
	tokens[3].size -= 2;
	
	if (callback_fun != NULL)
		callback_fun(state, &(tokens[3]), user_data);
}

// Process a message from shet.
static void process_message(shet_state *state, jsmntok_t *tokens)
{
	DPRINTF( "recieved: '%.*s' (%d)\n"
	       , tokens[0].end - tokens[0].start
	       , line + tokens[0].start
	       , tokens[0].type
	       );
	
	assert_type(&(tokens[0]), JSMN_ARRAY);
	if (tokens[0].size < 2) {
		DPRINTF("Command too short.\n");
		return;
	}
	
	// Note that the command is at least followed by the closing bracket and if
	// not a comma and so nulling out the character following it is safe.
	assert_type(&(tokens[2]), JSMN_STRING);
	const char *command = state->line + tokens[1].start;
	state->line[tokens[1].end + 1] = '\0';
	
	// Extract args in indiviaual handling functions -- too much effort to do here.
	if (strcmp(command, "return") == 0)
		process_return(state, tokens);
	else if (strcmp(command, "event") == 0)
		process_event(state, tokens, EVENT_ECB);
	else if (strcmp(command, "eventdeleted") == 0)
		process_event(state, tokens, EVENT_DELETED_ECB);
	else if (strcmp(command, "eventcreated") == 0)
		process_event(state, tokens, EVENT_CREATED_ECB);
	else
		DPRINTF("unsupported command: \"%s\"\n", command);
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
	
	state->line = line;
	
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
				process_message(state, state->tokens);
			} else {
				DPRINTF("No tokens in JSON in shet_process_line.\n");
			}
			break;
	}
}

// Send a command, returning the id used.
static int send_command(shet_state *state,
                        const char *command_name,
                        const char *path,
                        const char *args)
{
	int id = state->next_id++;
	
	// Construct the command...
	snprintf( state->out_buf, SHET_BUF_SIZE-1
	        , "[%d,\"%s\",\"%s\"%s%s]\n"
	        , id
	        , command_name
	        , path
	        , args ? ","  : ""
	        , args ? args : ""
	        );
	
	// ...and send it
	DPRINTF("%s\n", state->out_buf);
	state->transmit(state->out_buf);
	
	return id;
}

// Send a command, and register a callback for the 'return'.
static void send_command_cb(shet_state *state,
                            callback_list *cb_list,
                            const char *command_name,
                            const char *path,
                            const char *args,
                            callback_t callback,
                            void * callback_arg)
{
	if (cb_list->next != NULL) {
		DPRINTF("callback_list already used!\n");
		return;
	}
	
	// Send the command.
	int id = send_command(state, command_name, path, args);
	
	// Add the callback.
	cb_list->type = RETURN_CB;
	cb_list->data.return_cb.id = id;
	cb_list->data.return_cb.success_callback = callback;
	cb_list->data.return_cb.error_callback = NULL;
	cb_list->data.return_cb.user_data = callback_arg;
	
	// Push it onto the list.
	cb_list->next = state->callbacks;
	state->callbacks = cb_list;
}


// Functions that the user can use to interact with SHET.

// Call an action.
void shet_call_action(shet_state *state,
                     callback_list *cb_list,
                     const char *path,
                     const char *args,
                     callback_t callback,
                     void *callback_arg)
{
	send_command_cb(state, cb_list, "call", path, args, callback, callback_arg);
}

// Get a property.
void shet_get_prop(shet_state *state,
                   callback_list *cb_list,
                   const char *path,
                   callback_t callback,
                   void *callback_arg)
{
	send_command_cb(state, cb_list, "get", path, NULL, callback, callback_arg);
}

// Set a property.
void shet_set_prop(shet_state *state,
                   callback_list *cb_list,
                   const char *path,
                   const char *value,
                   callback_t callback,
                   void *callback_arg)
{
	// Wrap the single argument in a list.
	int value_len = strlen(value);
	char wrapped_value[value_len+3];
	wrapped_value[0] = '[';
	strcpy(wrapped_value+1, value);
	wrapped_value[value_len+1] = ']';
	wrapped_value[value_len+2] = '\0';
	
	send_command_cb(state, cb_list, "set", path, wrapped_value, callback, callback_arg);
}

// Watch an event.
void shet_watch_event(shet_state *state,
                      callback_list *cb_list,
                      const char *path,
                      callback_t event_callback,
                      callback_t deleted_callback,
                      callback_t created_callback,
                      void *callback_arg)
{
	if (cb_list->next != NULL) {
		DPRINTF("callback_list already used!\n");
		return;
	}
	
	// Make a callback.
	cb_list->type = EVENT_CB;
	cb_list->data.event_cb.event_name = path;
	cb_list->data.event_cb.event_callback = event_callback;
	cb_list->data.event_cb.deleted_callback = deleted_callback;
	cb_list->data.event_cb.created_callback = created_callback;
	cb_list->data.event_cb.user_data = callback_arg;
	
	// And push it onto the callback list.
	cb_list->next = state->callbacks;
	state->callbacks = cb_list;
	
	// Finally, send the command.
	send_command_cb(state, cb_list, "watch", path, NULL, NULL, NULL);
}
