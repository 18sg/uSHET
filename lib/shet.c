#include "shet.h"
#include "util.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdarg.h>

#ifdef DEBUG
#define DPRINTF(...) fprintf(stderr, __VA_ARGS__)
#else
#define DPRINTF(...)
#endif


// Assert the type of a JSON object
void assert_type(struct json_object *obj, enum json_type type)
{
	if (!json_object_is_type(obj, type))
		die("Invalid JSON recieved.\n");
}

// Make a new state.
shet_state *shet_new_state()
{
	shet_state *state = malloc(sizeof(*state));
	state->sockfd = -1;
	state->next_id = 0;
	state->callbacks = NULL;
	state->should_exit = 0;
	state->json_parser = json_tokener_new();
	return state;
}

// Clean up the state.
void shet_free_state(shet_state *state)
{
	if (state->sockfd != -1)
		close(state->sockfd);
	
	json_tokener_free(state->json_parser);
	
	// Free callbacks here.
	
	free(state);
}

// Exit the main loop.
void shet_exit(shet_state *state)
{
	state->should_exit = 1;
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

// Connect to a shet server on a host and port.
void shet_connect(shet_state *state, char *host, char *port)
{
	struct addrinfo hints;
	struct addrinfo *result, *rp;
	int sfd, s;
	
	/* Obtain address(es) matching host/port */
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;    /* Allow IPv4 or IPv6 */
	hints.ai_socktype = SOCK_STREAM; /* Stream socket */
	hints.ai_flags = 0;
	hints.ai_protocol = 0;          /* Any protocol */
	
	s = getaddrinfo(host, port, &hints, &result);
	if (s != 0)
		die("getaddrinfo: %s\n", gai_strerror(s));
	
	/* getaddrinfo() returns a list of address structures.
		 Try each address until we successfully connect(2).
		 If socket(2) (or connect(2)) fails, we (close the socket
		 and) try the next address. */
	
	for (rp = result; rp != NULL; rp = rp->ai_next) {
		sfd = socket(rp->ai_family, rp->ai_socktype,
				rp->ai_protocol);
		if (sfd == -1)
			continue;
		
		if (connect(sfd, rp->ai_addr, rp->ai_addrlen) != -1)
			break;                  /* Success */
		
		close(sfd);
	}
	
	if (rp == NULL)                 /* No address succeeded */
		die("Could not connect\n");
	
	freeaddrinfo(result);           /* No longer needed */
	state->sockfd = sfd;
}

// Connect to the shet server given in the SHET_HOST
// and SHET_PORT environment variables.
void shet_connect_default(shet_state *state)
{
	char *hostname = getenv("SHET_HOST");
	if (hostname == NULL)
		hostname = "localhost";
	
	char *port = getenv("SHET_PORT");
	if (port == NULL)
		port = "11235";
	
	shet_connect(state, hostname, port);
}

// Deal with a shet 'return' command, calling the appropriate callback.
void process_return(shet_state *state, struct json_object *id_json, struct json_object *msg)
{
	if (json_object_array_length(msg) != 4)
		die("Return messages should be of length 4");
	
	// We only send commands with ints.
	assert_type(id_json, json_type_int);
	int id = json_object_get_int(id_json);
	
	// The success/fail value should be an int too.
	struct json_object *success_json = json_object_array_get_idx(msg, 2);
	assert_type(success_json, json_type_int);
	int success = json_object_get_int(success_json);
	
	// The return value can be any JSON object.
	struct json_object *return_val = json_object_array_get_idx(msg, 3);
	
	
	// Find the right callback.
	callback_list **callback = &(state->callbacks);
	for (; *callback != NULL; callback = &(*callback)->next)
		if ((*callback)->type == RETURN_CB && (*callback)->data.return_cb.id == id)
			break;
	
	if (*callback == NULL)
		die("no callback for id %i\n", id);
	
	// We want to leave everything in a consistent state, as the callback might make
	// another call to this lib, so get the info we need, free everything, *then* callback.
	// Ideally, we should 'return' the callback to be ran in the main loop.
	
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
	} else
		die("Bad success/fail value.\n");
	
	// Remove the callback from the list.
	callback_list *next = (*callback)->next;
	free(*callback);
	*callback = next;
	
	// Run the callback if it's not null.
	if (callback_fun != NULL)
		callback_fun(state, return_val, user_data);
}

// Find a callnack for the named event.
// Die if it can't be found.
callback_list **find_event_cb(shet_state *state, const char *name)
{
	callback_list **callback = &(state->callbacks);
	for (; *callback != NULL; callback = &(*callback)->next)
		if ((*callback)->type == EVENT_CB && 
		    strcmp((*callback)->data.event_cb.event_name, name) == 0)
			break;
	
	if (*callback == NULL)
		die("No callback for event %s\n", name);
	
	return callback;
}

// Process an evant callback.
// The type parameter is 0 for 'event', 1 for 'eventdeleted', and 2 for 'eventcreated',
// Ad the processing is basically the same.
void process_event(shet_state *state, struct json_object *id, struct json_object *msg, int type)
{
	if (json_object_array_length(msg) < 3)
		die("Event messages should be at least 4 parts.\n");
		
	// Get the name.
	struct json_object *name_json = json_object_array_get_idx(msg, 2);
	assert_type(name_json, json_type_string);
	const char *name = json_object_get_string(name_json);
	
	// Find the callback for this event.
	callback_list **callback = find_event_cb(state, name);
	
	// Get the callback and user data.
	callback_t callback_fun;
	switch (type) {
		case 0: callback_fun = (*callback)->data.event_cb.event_callback; break;
		case 1: callback_fun = (*callback)->data.event_cb.deleted_callback; break;
		case 2: callback_fun = (*callback)->data.event_cb.created_callback; break;
		default: die("This error was put here to make the compiler STFU. "
		             "If you've just seen this message, then the i'm an idiot.\n");
	}
	void *user_data = (*callback)->data.event_cb.user_data;
	
	// Extract the arguments.
	struct json_object *args = json_object_new_array();
	int len = json_object_array_length(msg);
	for (int i = 3; i < len; i++)
		json_object_array_add(args, json_object_array_get_idx(msg, i));
	
	if (callback_fun != NULL)
		callback_fun(state, args, user_data);
	
	// XXX: Should send return.
	
	json_object_put(args);
}

// Process a message from shet.
void process_message(shet_state *state, struct json_object *msg)
{
	DPRINTF("recieved: %s\n", json_object_get_string(msg));
	
	assert_type(msg, json_type_array);
	if (json_object_array_length(msg) < 2)
		die("Command too short.\n");
	
	struct json_object *id = json_object_array_get_idx(msg, 0);
	struct json_object *command_json = json_object_array_get_idx(msg, 1);
	assert_type(command_json, json_type_string);
	const char *command = json_object_get_string(command_json);
	// Extract args in indiviaual handling functions -- too much effort to do here.
	
	// Call the appropriate message handling function.
	if (strcmp(command, "return") == 0)
		process_return(state, id, msg);
	else if (strcmp(command, "event") == 0)
		process_event(state, id, msg, 0);
	else if (strcmp(command, "eventdeleted") == 0)
		process_event(state, id, msg, 1);
	else if (strcmp(command, "eventcreated") == 0)
		process_event(state, id, msg, 2);
	else
		die("unsupported command: \"%s\"\n", command);
}

// Do a single read call, and process the results incrementaly.
void shet_tick(shet_state *state)
{
	char buf[BUF_LEN];
	int len = read(state->sockfd, buf, BUF_LEN);
	if (len <= 0)
		die("Could not read\n");
	
	// To avoid complexity, just pass characters individually to the json parser,
	// and treat each json object it returns as a message.
	// It ignores whitespace, so the \r\n won't cause troubles.
	// XXX: Assumes json is always valid -- will probably hang if it's not.
	for (int i = 0; i < len; i++) {
		struct json_object *msg = json_tokener_parse_ex(state->json_parser, buf + i, 1);
		if (msg != NULL) {
			// Legit JSON!
			// Clear the parser and process the message.
			json_tokener_reset(state->json_parser);
			process_message(state, msg);
			json_object_put(msg);
		}
	}
}

// Send and recieve data until shet_exit is called.
void shet_loop(shet_state *state)
{
	while (!state->should_exit)
		shet_tick(state);
}

// Send a command, returning the id used.
int send_command(shet_state *state,
                 char *command_name,
                 char *path,
                 struct json_object *args)
{
	int id = state->next_id++;
	
	// Construct the command.
	json_object *command = json_object_new_array();
	json_object_array_add(command, json_object_new_int(id));
	json_object_array_add(command, json_object_new_string(command_name));
	json_object_array_add(command, json_object_new_string(path));
	
	// Possibly add the arguments.
	if (args != NULL) {
		int len = json_object_array_length(args);
		for (int i = 0; i < len; i++) {
			json_object *obj = json_object_array_get_idx(args, i);
			json_object_array_add(command, json_object_get(obj));
		}
	}
	
	// Turn it into a string, and send it.
	const char *command_str = json_object_get_string(command);
	DPRINTF("%s\n", command_str);
	write(state->sockfd, command_str, strlen(command_str));
	write(state->sockfd, "\r\n", 2); // Windows line endings -- ugh!
	
	// Free the command.
	json_object_put(command);
	
	return id;
}

// Send a command, and register a callback for the 'return'.
void send_command_cb(shet_state *state,
                     char *command_name,
                     char *path,
                     struct json_object *args,
                     callback_t callback,
                     void * callback_arg)
{
	// Send the command.
	int id = send_command(state, command_name, path, args);
	
	// Add the callback.
	callback_list *cb_list = malloc(sizeof(*cb_list));
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
                     char *path,
                     struct json_object *args,
                     callback_t callback,
                     void *callback_arg)
{
	send_command_cb(state, "call", path, args, callback, callback_arg);
}

// Get a property.
void shet_get_prop(shet_state *state,
                   char *path,
                   callback_t callback,
                   void *callback_arg)
{
	send_command_cb(state, "get", path, NULL, callback, callback_arg);
}

// Set a property.
void shet_set_prop(shet_state *state,
                   char *path,
                   struct json_object *value,
                   callback_t callback,
                   void *callback_arg)
{
	// Wrap the single argument in a list.
	struct json_object *args = json_object_new_array();
	json_object_array_add(args, json_object_get(value));
	send_command_cb(state, "set", path, args, callback, callback_arg);
	json_object_put(args);
}

// Watch an event.
void shet_watch_event(shet_state *state,
                      char *path,
                      callback_t event_callback,
                      callback_t deleted_callback,
                      callback_t created_callback,
                      void *callback_arg)
{
	// Make a callback.
	callback_list *cb_list = malloc(sizeof(*cb_list));
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
	send_command_cb(state, "watch", path, NULL, NULL, NULL);
}
