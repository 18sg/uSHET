#ifndef SHET_INTERNAL_H
#define SHET_INTERNAL_H

#ifdef __cplusplus
extern "C" {
#endif


// Define 4 types of callbacks that we store.
typedef enum {
	SHET_RETURN_CB,
	SHET_EVENT_CB,
	SHET_ACTION_CB,
	SHET_PROP_CB,
} shet_deferred_type_t;

// Define the types of (from) server command callbacks
typedef enum {
	SHET_EVENT_CCB,
	SHET_EVENT_DELETED_CCB,
	SHET_EVENT_CREATED_CCB,
	SHET_GET_PROP_CCB,
	SHET_SET_PROP_CCB,
	SHET_CALL_CCB,
} command_callback_type_t;

typedef struct {
	int id;
	shet_callback_t success_callback;
	shet_callback_t error_callback;
	void *user_data;
} shet_return_callback_t;

typedef struct {
	struct shet_deferred *watch_deferred;
	const char *event_name;
	shet_callback_t event_callback;
	shet_callback_t deleted_callback;
	shet_callback_t created_callback;
	void *user_data;
} shet_event_callback_t;

typedef struct {
	struct shet_deferred *mkprop_deferred;
	const char *prop_name;
	shet_callback_t get_callback;
	shet_callback_t set_callback;
	void *user_data;
} shet_prop_callback_t;


typedef struct {
	struct shet_deferred *mkaction_deferred;
	const char *action_name;
	shet_callback_t callback;
	void *user_data;
} shet_action_callback_t;

// A list of callbacks.
struct shet_deferred {
	shet_deferred_type_t type;
	union {
		shet_return_callback_t return_cb;
		shet_event_callback_t event_cb;
		shet_action_callback_t action_cb;
		shet_prop_callback_t prop_cb;
	} data;
	struct shet_deferred *next;
};

// A list of registered events
struct shet_event {
	const char *event_name;
	struct shet_deferred *mkevent_deferred;
	struct shet_event *next;
};

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
	shet_deferred_t *callbacks;
	shet_event_t *registered_events;
	
	// A buffer of tokens for JSON strings
	jsmntok_t tokens[SHET_NUM_TOKENS];
	
	// Outgoing JSON buffer
	char out_buf[SHET_BUF_SIZE];
	
	// Unique identifier for the connection
	const char *connection_name;
	
	// Function to call to be called to transmit (null-terminated) data
	void (*transmit)(const char *data, void *user_data);
	void *transmit_user_data;
	
	shet_callback_t error_callback;
	void *error_callback_data;
};



#ifdef __cplusplus
}
#endif

#endif

