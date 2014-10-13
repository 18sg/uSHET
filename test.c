/**
 * A simple test suite for uSHET.
 */

#include <stdio.h>
#include <stdbool.h>
#include <string.h>

// Include the C files so that static functions can be tested
#include "lib/shet.c"
#include "lib/jsmn.c"


////////////////////////////////////////////////////////////////////////////////
// JSON test utilities
////////////////////////////////////////////////////////////////////////////////

// Given two JSON strings and their tokens return the token which points to the
// first differing componenst in each via differing_{a,b}. Returns a bool which
// is true if the JSON is the same and false if not. If differing_{a,b} are
// NULL, they will not be set. tokens_{a,b} are expected to be double pointers
// to a token pointer initialised to the first token to process.
bool cmp_json_tokens(const char *json_a, const char *json_b,
                     jsmntok_t **tokens_a, jsmntok_t **tokens_b,
                     jsmntok_t *differing_a, jsmntok_t *differing_b) {
	// Initially just check to see if the types are even comparable
	if ((**tokens_a).type != (**tokens_b).type ||
	    (**tokens_a).size != (**tokens_b).size) {
		*differing_a = **tokens_a;
		*differing_b = **tokens_b;
		return false;
	
	// Compare atomic objects
	} else if ((*tokens_a)->type == JSMN_PRIMITIVE ||
	           (*tokens_a)->type == JSMN_STRING) {
		if (strncmp(json_a+(*tokens_a)->start,
		            json_b+(*tokens_b)->start,
		            (*tokens_a)->end - (*tokens_a)->start) != 0) {
			if (differing_a != NULL) *differing_a = **tokens_a;
			if (differing_b != NULL) *differing_b = **tokens_b;
			return false;
		} else {
			(*tokens_a)++;
			(*tokens_b)++;
			return true;
		}
	
	// Iterate over and compare compound objects
	} else {
		int size = (**tokens_a).size;
		(*tokens_a)++;
		(*tokens_b)++;
		for (int i = 0; i < size; i++)
			if (!cmp_json_tokens(json_a, json_b,
			                     tokens_a, tokens_b,
			                     differing_a, differing_b))
				return false;
		return true;
	}
}

////////////////////////////////////////////////////////////////////////////////
// Test suite assertions
////////////////////////////////////////////////////////////////////////////////

// Standard "assert true" assertion
#define TASSERT(x) do { if (!(x)) { \
	fprintf(stderr, "TASSERT Failed: %s:%s:%d: "#x"\n",\
	        __FILE__,__func__,__LINE__);\
	return false; \
} } while (0)

// Assert equal integers (prints the value of the integers on error)
#define TASSERT_INT_EQUAL(a,b) do { if ((a) != (b)) { \
	fprintf(stderr, "TASSERT_INT_EQUAL Failed: %s:%s:%d: "#a" (%d) == "#b" (%d)\n",\
	        __FILE__,__func__,__LINE__, (a), (b));\
	return false; \
} } while (0)

// Compare two JSON strings given as a set of tokens and assert that they are
// equivilent. If they are not, prints the two strings along with an indicator
// pointing at the first non-matching parts.
#define TASSERT_JSON_EQUAL_TOK_TOK(sa,ta,sb,tb) do { \
	TASSERT((sa) != NULL);\
	TASSERT((ta) != NULL);\
	TASSERT((sb) != NULL);\
	TASSERT((tb) != NULL);\
	const char *a = (sa); /* JSON String a */ \
	const char *b = (sb); /* JSON String b */ \
	jsmntok_t *ca = (ta); /* Cur token pointer a */ \
	jsmntok_t *cb = (tb); /* Cur token pointer b */ \
	jsmntok_t da; /* Differing token a */ \
	da.start = 0; \
	jsmntok_t db; /* Differing token b */ \
	db.start = 0; \
	if (!cmp_json_tokens(a,b,&ca,&cb,&da,&db)) { \
		fprintf(stderr, "TASSERT_JSON_EQUAL Failed: %s:%s:%d:\n",\
		        __FILE__,__func__,__LINE__);\
		fprintf(stderr, " > \"%.*s\"\n", (ta)->end-(ta)->start, a+(ta)->start);\
		fprintf(stderr, " >  ", a);\
		if (da.start == db.start) { \
			for (int i = 0; i < da.start; i++) fprintf(stderr, " "); \
			fprintf(stderr, "|\n"); \
		} else if (da.start < db.start) { \
			for (int i = 0; i < da.start; i++) fprintf(stderr, " "); \
			fprintf(stderr, "^"); \
			for (int i = da.start; i < db.start-1; i++) fprintf(stderr, "-"); \
			fprintf(stderr, "v\n"); \
		} else { \
			for (int i = 0; i < db.start; i++) fprintf(stderr, " "); \
			fprintf(stderr, "v"); \
			for (int i = db.start; i < da.start-1; i++) fprintf(stderr, "-"); \
			fprintf(stderr, "^\n"); \
		} \
		fprintf(stderr, " > \"%.*s\"\n", (tb)->end-(tb)->start, b+(tb)->start);\
		return false; \
	} \
} while (0)


// Compare two JSON strings the first given as a string and token and the other
// as a string and assert that they are equivilent.
#define TASSERT_JSON_EQUAL_TOK_STR(sa,ta,sb) do { \
	jsmn_parser p; \
	jsmn_init(&p); \
	jsmntok_t tb[100]; \
	jsmnerr_t e = jsmn_parse(&p, sb, strlen(sb), \
	                         tb, 100); \
	if (e <= 0) { \
		fprintf(stderr, "TASSERT_JSON_EQUAL Could not parse JSON: %s:%s:%d: %s\n",\
		        __FILE__,__func__,__LINE__,sa); \
		return false; \
	} \
	TASSERT_JSON_EQUAL_TOK_TOK(sa, ta, sb, tb);\
} while (0)


// Compare two JSON strings given as strings.
#define TASSERT_JSON_EQUAL_STR_STR(sa,sb) do { \
	jsmn_parser p; \
	jsmn_init(&p); \
	jsmntok_t ta[100]; \
	jsmnerr_t e = jsmn_parse(&p, sa, strlen(sa), \
	                         ta, 100); \
	if (e <= 0) { \
		fprintf(stderr, "TASSERT_JSON_EQUAL Could not parse JSON: %s:%s:%d: %s\n",\
		        __FILE__,__func__,__LINE__,sa); \
		return false; \
	} \
	TASSERT_JSON_EQUAL_TOK_STR(sa, ta, sb);\
} while (0)




////////////////////////////////////////////////////////////////////////////////
// Utility callbacks
////////////////////////////////////////////////////////////////////////////////

// A transmit function which simply counts the transmissions and records a
// pointer to the last transmitted value (obviously this pointer only has a
// limited lifetime).
static const char *transmit_last_data = NULL;
static void *transmit_last_user_data = NULL;
static int transmit_count = 0;
static void transmit_cb(const char *data, void *user_data) {
	transmit_last_data = data;
	transmit_last_user_data = user_data;
	transmit_count++;
}

#define RESET_TRANSMIT_CB() do { \
	transmit_last_data = NULL; \
	transmit_last_user_data = NULL; \
	transmit_count = 0; \
} while (0)


// A generic callback which simply places the callback arguments into a
// callback_result_t structure pointed to by the user variable.
typedef struct {
	shet_state_t *state;
	char *line;
	jsmntok_t *token;
	int count;
	const char *return_value;
} callback_result_t;

static void callback(shet_state_t *state, char *line, jsmntok_t *token, void *user_data) {
	callback_result_t *result = (callback_result_t *)user_data;
	if (result != NULL) {
		result->state = state;
		result->line = line;
		result->token = token;
		result->count++;
	} else {
		fprintf(stderr, "TEST ERROR: callback didn't receive result pointer!\n");
	}
}


// A callback like the above but which returns its argument
static void echo_callback(shet_state_t *state, char *line, jsmntok_t *token, void *user_data) {
	callback(state, line, token, user_data);
	
	// Return back the argument
	char response[100];
	strncpy(response, line + token[0].start, token[0].end - token[0].start);
	response[token[0].end - token[0].start] = '\0';
	shet_return(state, 0, response);
}


// A callback like the above but which returns success and returns a null value
static void success_callback(shet_state_t *state, char *line, jsmntok_t *token, void *user_data) {
	callback(state, line, token, user_data);
	
	shet_return(state, 0, "null");
}


// A callback like the above but which returns success and the value in
// the return_value field of callback_result_t.
static void const_callback(shet_state_t *state, char *line, jsmntok_t *token, void *user_data) {
	callback(state, line, token, user_data);
	
	shet_return(state, 0, ((callback_result_t *)user_data)->return_value);
}


////////////////////////////////////////////////////////////////////////////////
// Test token type assertions
////////////////////////////////////////////////////////////////////////////////

bool test_assert_int(void) {
	// Make sure that the function can distinguish between primitives which are
	// and are not integers
	char *strings[] = {
		"0",
		"123",
		"-123",
		"true",
		"false",
		"null",
		"[]",
		"{}",
	};
	size_t num_strings = sizeof(strings)/sizeof(char *);
	size_t num_valid_ints = 3;
	
	for (int i = 0; i < num_strings; i++) {
		jsmn_parser p;
		jsmn_init(&p);
		jsmntok_t tokens[4];
		
		jsmnerr_t e = jsmn_parse( &p
		                        , strings[i]
		                        , strlen(strings[i])
		                        , tokens
		                        , SHET_NUM_TOKENS
		                        );
		// Make sure the string tokenzied successfully
		TASSERT(e >= 1);
		
		// Check the assert
		TASSERT(assert_int(strings[i], tokens) == (i < num_valid_ints));
	}
	
	return true;
}


////////////////////////////////////////////////////////////////////////////////
// Test internal message generation functions
////////////////////////////////////////////////////////////////////////////////

bool test_send_command(void) {
	shet_state_t state;
	RESET_TRANSMIT_CB();
	shet_state_init(&state, NULL, transmit_cb, NULL);
	
	// Test sending with just a command
	send_command(&state, "test1", NULL, NULL,
	             NULL, NULL, NULL, NULL);
	TASSERT(transmit_count == 2);
	TASSERT_JSON_EQUAL_STR_STR(transmit_last_data, "[1, \"test1\"]");
	
	// Test that paths get quoted
	send_command(&state, "test2", "/test", NULL,
	             NULL, NULL, NULL, NULL);
	TASSERT(transmit_count == 3);
	TASSERT_JSON_EQUAL_STR_STR(transmit_last_data, "[2, \"test2\", \"/test\"]");
	
	// Test that arguments don't get quoted
	send_command(&state, "test3", "/test", "[1,2,3], 4",
	             NULL, NULL, NULL, NULL);
	TASSERT(transmit_count == 4);
	TASSERT_JSON_EQUAL_STR_STR(transmit_last_data, "[3, \"test3\", \"/test\", [1,2,3],4]");
	
	// And test that path-less commands still support arguments
	send_command(&state, "test4", NULL, "5, [6,7,8]",
	             NULL, NULL, NULL, NULL);
	TASSERT(transmit_count == 5);
	TASSERT_JSON_EQUAL_STR_STR(transmit_last_data, "[4, \"test4\", 5,[6,7,8]]");
	
	// Make sure no deferreds were created
	TASSERT(state.callbacks == NULL);
	
	// Make sure they are when required!
	deferred_t test_deferred;
	callback_result_t result;
	result.count = 0;
	send_command(&state, "test5", NULL, NULL,
	             &test_deferred, callback, NULL, &result);
	TASSERT(transmit_count == 6);
	TASSERT(result.count == 0);
	TASSERT_JSON_EQUAL_STR_STR(transmit_last_data, "[5, \"test5\"]");
	char response[] = "[5, \"return\", 0, [1,2,3,4]]";
	shet_process_line(&state, response, strlen(response));
	TASSERT(result.count == 1);
	TASSERT_JSON_EQUAL_TOK_STR(result.line, result.token, "[1,2,3,4]");
	
	return true;
}


////////////////////////////////////////////////////////////////////////////////
// Test deferred utility functions
////////////////////////////////////////////////////////////////////////////////

bool test_deferred_utilities(void) {
	shet_state_t state;
	RESET_TRANSMIT_CB();
	shet_state_init(&state, NULL, transmit_cb, NULL);
	
	// Make sure that the deferred list is initially empty
	TASSERT(state.callbacks == NULL);
	
	// Make sure searches don't find stuff or crash on empty lists
	TASSERT(find_return_cb(&state, 0) == NULL);
	TASSERT(find_named_cb(&state, "/", EVENT_CCB) == NULL);
	TASSERT(find_named_cb(&state, "/", EVENT_DELETED_CCB) == NULL);
	TASSERT(find_named_cb(&state, "/", EVENT_CREATED_CCB) == NULL);
	TASSERT(find_named_cb(&state, "/", GET_PROP_CCB) == NULL);
	TASSERT(find_named_cb(&state, "/", SET_PROP_CCB) == NULL);
	TASSERT(find_named_cb(&state, "/", CALL_CCB) == NULL);
	
	// ...and that searching doesn't add stuff
	TASSERT(state.callbacks == NULL);
	
	// Add a return
	deferred_t d1;
	d1.type = RETURN_CB;
	d1.data.return_cb.id = 0;
	add_deferred(&state, &d1);
	
	// Make sure it is there the hard way
	TASSERT(state.callbacks == &d1);
	TASSERT(state.callbacks->next == NULL);
	
	// Make sure it is found (but not found by anything else)
	TASSERT(find_return_cb(&state, 0) == &d1);
	TASSERT(find_return_cb(&state, 1) == NULL);
	TASSERT(find_named_cb(&state, "/", EVENT_CCB) == NULL);
	TASSERT(find_named_cb(&state, "/", EVENT_DELETED_CCB) == NULL);
	TASSERT(find_named_cb(&state, "/", EVENT_CREATED_CCB) == NULL);
	TASSERT(find_named_cb(&state, "/", GET_PROP_CCB) == NULL);
	TASSERT(find_named_cb(&state, "/", SET_PROP_CCB) == NULL);
	TASSERT(find_named_cb(&state, "/", CALL_CCB) == NULL);
	
	// Make sure it can be added a second time without duplicating
	add_deferred(&state, &d1);
	
	// Make sure it is there the hard way
	TASSERT(state.callbacks == &d1);
	TASSERT(state.callbacks->next == NULL);
	
	// Make sure it is found (but not found by anything else)
	TASSERT(find_return_cb(&state, 0) == &d1);
	TASSERT(find_return_cb(&state, 1) == NULL);
	TASSERT(find_named_cb(&state, "/", EVENT_CCB) == NULL);
	TASSERT(find_named_cb(&state, "/", EVENT_DELETED_CCB) == NULL);
	TASSERT(find_named_cb(&state, "/", EVENT_CREATED_CCB) == NULL);
	TASSERT(find_named_cb(&state, "/", GET_PROP_CCB) == NULL);
	TASSERT(find_named_cb(&state, "/", SET_PROP_CCB) == NULL);
	TASSERT(find_named_cb(&state, "/", CALL_CCB) == NULL);
	
	// Make sure we can remove it
	remove_deferred(&state, &d1);
	
	// Make sure it is gone
	TASSERT(state.callbacks == NULL);
	TASSERT(find_return_cb(&state, 0) == NULL);
	
	// Add more than one item
	deferred_t d2;
	d2.type = RETURN_CB;
	d2.data.return_cb.id = 1;
	add_deferred(&state, &d2);
	add_deferred(&state, &d1);
	
	// Make sure they get in the hard way (Note: test will fail if the ordering is
	// different, even if the list is technically correct)
	TASSERT(state.callbacks == &d1);
	TASSERT(state.callbacks->next == &d2);
	TASSERT(state.callbacks->next->next == NULL);
	
	// Test both can be found
	TASSERT(find_return_cb(&state, 0) == &d1);
	TASSERT(find_return_cb(&state, 1) == &d2);
	
	// Ensure both can be re-added to no ill effect
	add_deferred(&state, &d1);
	add_deferred(&state, &d2);
	TASSERT(state.callbacks == &d1);
	TASSERT(state.callbacks->next == &d2);
	TASSERT(state.callbacks->next->next == NULL);
	
	// Ensure we can remove the tail
	remove_deferred(&state, &d2);
	TASSERT(state.callbacks == &d1);
	TASSERT(state.callbacks->next == NULL);
	
	// ...and add again
	add_deferred(&state, &d2);
	TASSERT(state.callbacks == &d2);
	TASSERT(state.callbacks->next == &d1);
	TASSERT(state.callbacks->next->next == NULL);
	
	// Ensure we can remove the head (again, this test will fail if insertion
	// order changes!)
	remove_deferred(&state, &d2);
	TASSERT(state.callbacks == &d1);
	TASSERT(state.callbacks->next == NULL);
	
	// Leave us with an empty list again
	remove_deferred(&state, &d1);
	TASSERT(state.callbacks == NULL);
	
	// Test that we can find event callbacks
	d1.type = EVENT_CB;
	d1.data.event_cb.event_name = "/event/d1";
	add_deferred(&state, &d1);
	d2.type = EVENT_CB;
	d2.data.event_cb.event_name = "/event/d2";
	add_deferred(&state, &d2);
	
	TASSERT(find_named_cb(&state, "/event/d1", EVENT_CB) == &d1);
	TASSERT(find_named_cb(&state, "/event/d2", EVENT_CB) == &d2);
	
	// And that non-existant stuff doesn't get found
	TASSERT(find_named_cb(&state, "/non_existant", EVENT_CB) == NULL);
	TASSERT(find_named_cb(&state, "/event/d1", ACTION_CB) == NULL);
	TASSERT(find_named_cb(&state, "/event/d1", PROP_CB) == NULL);
	TASSERT(find_named_cb(&state, "/event/d1", RETURN_CB) == NULL);
	
	// Try actions...
	remove_deferred(&state, &d1);
	remove_deferred(&state, &d2);
	d1.type = ACTION_CB;
	d1.data.action_cb.action_name = "/action/d1";
	d2.type = ACTION_CB;
	d2.data.action_cb.action_name = "/action/d2";
	add_deferred(&state, &d1);
	add_deferred(&state, &d2);
	
	TASSERT(find_named_cb(&state, "/action/d1", ACTION_CB) == &d1);
	TASSERT(find_named_cb(&state, "/action/d2", ACTION_CB) == &d2);
	
	TASSERT(find_named_cb(&state, "/action/d1", EVENT_CB) == NULL);
	TASSERT(find_named_cb(&state, "/action/d1", PROP_CB) == NULL);
	TASSERT(find_named_cb(&state, "/action/d1", RETURN_CB) == NULL);
	
	// Try properties...
	remove_deferred(&state, &d1);
	remove_deferred(&state, &d2);
	d1.type = PROP_CB;
	d1.data.prop_cb.prop_name = "/prop/d1";
	d2.type = PROP_CB;
	d2.data.prop_cb.prop_name = "/prop/d2";
	add_deferred(&state, &d1);
	add_deferred(&state, &d2);
	
	TASSERT(find_named_cb(&state, "/prop/d1", PROP_CB) == &d1);
	TASSERT(find_named_cb(&state, "/prop/d2", PROP_CB) == &d2);
	
	TASSERT(find_named_cb(&state, "/prop/d1", EVENT_CB) == NULL);
	TASSERT(find_named_cb(&state, "/prop/d1", ACTION_CB) == NULL);
	TASSERT(find_named_cb(&state, "/prop/d1", RETURN_CB) == NULL);
	
	return true;
}



////////////////////////////////////////////////////////////////////////////////
// Test general library functions
////////////////////////////////////////////////////////////////////////////////


bool test_shet_state_init(void) {
	RESET_TRANSMIT_CB();
	shet_state_t state;
	shet_state_init(&state, "\"tester\"", transmit_cb, (void *)test_shet_state_init);
	
	// Make sure a registration command is sent and that the transmit callback
	// gets the right data
	TASSERT(transmit_count == 1);
	TASSERT(transmit_last_user_data == (void *)test_shet_state_init);
	TASSERT_JSON_EQUAL_STR_STR(transmit_last_data, "[0, \"register\", \"tester\"]");
	
	return true;
}



bool test_shet_set_error_callback(void) {
	RESET_TRANSMIT_CB();
	shet_state_t state;
	shet_state_init(&state, "\"tester\"", transmit_cb, NULL);
	
	// Make sure nothing happens when an unknown return arrives without an error
	// callback setup
	char line1[] = "[0,\"return\",0,[1,2,3]]";
	shet_process_line(&state, line1, strlen(line1));
	
	// Set up the error callback
	callback_result_t result;
	result.count = 0;
	shet_set_error_callback(&state, callback, &result);
	
	// Make sure the error came through
	char line2[] = "[1,\"return\",0,[1,2,3]]";
	shet_process_line(&state, line2, strlen(line2));
	TASSERT_INT_EQUAL(result.count, 1);
	TASSERT_JSON_EQUAL_TOK_STR(result.line, result.token, "[1,2,3]");
	
	return true;
}

#define TYPE_EVENT 1
#define TYPE_ACTION 2
#define TYPE_PROPERTY 3
#define TYPE_WATCH 4

bool test_shet_register(void) {
	// Paths of the things which will be registered
	const char *paths[] = {
		"/event/e1",
		"/event/e2",
		"/action/a1",
		"/action/a2",
		"/property/p1",
		"/property/p2",
		"/watch/w1",
		"/watch/w2",
	};
	// Types of the above paths
	const int types[] = {TYPE_EVENT,TYPE_EVENT,
	                     TYPE_ACTION,TYPE_ACTION,
	                     TYPE_PROPERTY,TYPE_PROPERTY,
	                     TYPE_WATCH,TYPE_WATCH};
	// Number of times the above paths have been registered (or unregistered)
	int reg_counts[] = {0,0,0,0,0,0,0,0};
	// Number of times the above paths have been registered with the wrong command
	int wrong_reg_counts[] = {0,0,0,0,0,0,0,0};
	// Return IDs for the most recent reg command for the above paths
	int reg_return_ids[] = {0,0,0,0,0,0,0,0};
	// Number of times the above paths' registration callbacks have been called
	int cb_counts[] = {0,0,0,0,0,0,0,0};
	// Number of paths
	const int num = sizeof(paths)/sizeof(const char *);
	
	// Deferreds for the "make" parts of the element
	deferred_t make_deferreds[num];
	// Deferreds for the live part of the element (e.g. event, call, set, get).
	deferred_t deferreds[num];
	// Event objects for any events
	event_t events[num];
	
	// Number of "register" commands received
	int register_count = 0;
	
	// Unexpected commands received to transmit
	int bad_tx_count = 0;
	
	// Unknown paths requested
	int bad_path_count = 0;
	
	// Transmit callback. Simply returns the number of registrations which have
	// occurred. Note: this function assumes only register and
	// event/action/property/watch calls.
	void transmit(const char *data, void *user_data) {
		jsmn_parser p;
		jsmn_init(&p);
		jsmntok_t tokens[100];
		jsmnerr_t e = jsmn_parse(&p, data, strlen(data),
	                           tokens, 100);
		
		// Die if something strange is passed rather than crashing reading funny
		// memory locations...
		if (e < 3 || e > 4 || tokens[0].type != JSMN_ARRAY) {
			bad_tx_count++;
			return;
		}
		
		// Is this a register?
		if (strncmp("register", data + tokens[2].start, tokens[2].end - tokens[2].start) == 0) {
			register_count++;
		} else {
			// Check through the paths for a match
			int i;
			for (i = 0; i < num; i++) {
				if (strncmp(paths[i], data + tokens[3].start, tokens[3].end - tokens[3].start) == 0) {
					// Check that creationg commands are correct
					const char *str = data + tokens[2].start;
					size_t len = tokens[2].end - tokens[2].start;
					if ((strncmp("mkevent", str, len) == 0  && types[i] == TYPE_EVENT) ||
					    (strncmp("mkprop", str, len) == 0   && types[i] == TYPE_PROPERTY) ||
					    (strncmp("mkaction", str, len) == 0 && types[i] == TYPE_ACTION) ||
					    (strncmp("watch", str, len) == 0    && types[i] == TYPE_WATCH) ||
					    (strncmp("rmevent", str, len) == 0  && types[i] == TYPE_EVENT) ||
					    (strncmp("rmprop", str, len) == 0   && types[i] == TYPE_PROPERTY) ||
					    (strncmp("rmaction", str, len) == 0 && types[i] == TYPE_ACTION) ||
					    (strncmp("ignore", str, len) == 0   && types[i] == TYPE_WATCH))
						reg_counts[i]++;
					else {
						wrong_reg_counts[i]++;
					}
					
					reg_return_ids[i] = atoi(data + tokens[1].start);
					break;
				}
			}
			
			if (i == num)
				bad_path_count++;
		}
	}
	
	// Callback for "make" functions and the like
	void make(shet_state_t *state, char *data, jsmntok_t *token, void *user_data) {
		size_t i = (const char **)user_data - paths;
		cb_counts[i]++;
	}
	
	shet_state_t state;
	shet_state_init(&state, NULL, transmit, NULL);
	TASSERT_INT_EQUAL(register_count, 1);
	for (int i = 0; i < num; i++) {
		TASSERT_INT_EQUAL(reg_counts[i], 0);
		TASSERT_INT_EQUAL(wrong_reg_counts[i], 0);
	}
	TASSERT_INT_EQUAL(bad_path_count, 0);
	TASSERT_INT_EQUAL(bad_tx_count, 0);
	
	// Test that re-registering an empty system does no damage
	shet_reregister(&state);
	TASSERT_INT_EQUAL(register_count, 2);
	for (int i = 0; i < num; i++) {
		TASSERT_INT_EQUAL(reg_counts[i], 0);
		TASSERT_INT_EQUAL(wrong_reg_counts[i], 0);
	}
	TASSERT_INT_EQUAL(bad_path_count, 0);
	TASSERT_INT_EQUAL(bad_tx_count, 0);
	
	// Test registering all the test paths
	for (int i = 0; i < num; i++) {
		switch (types[i]) {
			case TYPE_EVENT:
				shet_make_event(&state, paths[i], &(events[i]),
				                &(make_deferreds[i]), make, NULL, &(paths[i]));
				break;
			case TYPE_ACTION:
				shet_make_action(&state, paths[i],
				                 &(deferreds[i]), NULL, NULL,
				                 &(make_deferreds[i]), make, NULL, &(paths[i]));
				break;
			case TYPE_PROPERTY:
				shet_make_prop(&state, paths[i],
				               &(deferreds[i]), NULL, NULL, NULL,
				               &(make_deferreds[i]), make, NULL, &(paths[i]));
				break;
			case TYPE_WATCH:
				shet_watch_event(&state, paths[i],
				                 &(deferreds[i]), NULL, NULL, NULL, NULL,
				                 &(make_deferreds[i]), make, NULL, &(paths[i]));
				break;
			default:
				TASSERT(false);
				break;
		}
	}
	TASSERT_INT_EQUAL(register_count, 2);
	for (int i = 0; i < num; i++) {
		TASSERT_INT_EQUAL(reg_counts[i], 1);
		TASSERT_INT_EQUAL(cb_counts[i], 0);
		TASSERT_INT_EQUAL(wrong_reg_counts[i], 0);
	}
	TASSERT_INT_EQUAL(bad_path_count, 0);
	TASSERT_INT_EQUAL(bad_tx_count, 0);
	
	// Send callbacks
	for (int i = 0; i < num; i++) {
		char msg[100];
		sprintf(msg, "[%d,\"return\",0,null]", reg_return_ids[i]);
		shet_process_line(&state, msg, strlen(msg));
	}
	for (int i = 0; i < num; i++) {
		TASSERT_INT_EQUAL(cb_counts[i], 1);
	}
	
	// Test re-registering makes everything come back
	shet_reregister(&state);
	TASSERT_INT_EQUAL(register_count, 3);
	for (int i = 0; i < num; i++) {
		TASSERT_INT_EQUAL(reg_counts[i], 2);
		TASSERT_INT_EQUAL(cb_counts[i], 1);
		TASSERT_INT_EQUAL(wrong_reg_counts[i], 0);
	}
	TASSERT_INT_EQUAL(bad_path_count, 0);
	TASSERT_INT_EQUAL(bad_tx_count, 0);
	
	// And that all the callbacks work again
	for (int i = 0; i < num; i++) {
		char msg[100];
		sprintf(msg, "[%d,\"return\",0,null]", reg_return_ids[i]);
		shet_process_line(&state, msg, strlen(msg));
	}
	for (int i = 0; i < num; i++) {
		TASSERT_INT_EQUAL(cb_counts[i], 2);
	}
	
	// Remove everything and make sture everything returns to normal
	for (int i = 0; i < num; i++) {
		switch (types[i]) {
			case TYPE_EVENT:
				shet_remove_event(&state, paths[i],
				                  NULL, NULL, NULL, NULL);
				break;
			case TYPE_ACTION:
				shet_remove_action(&state, paths[i],
				                   NULL, NULL, NULL, NULL);
				break;
			case TYPE_PROPERTY:
				shet_remove_prop(&state, paths[i],
				                 NULL, NULL, NULL, NULL);
				break;
			case TYPE_WATCH:
				shet_ignore_event(&state, paths[i],
				                  NULL, NULL, NULL, NULL);
				break;
			default:
				TASSERT(false);
				break;
		}
	}
	TASSERT_INT_EQUAL(register_count, 3);
	for (int i = 0; i < num; i++) {
		TASSERT_INT_EQUAL(reg_counts[i], 3);
		TASSERT_INT_EQUAL(cb_counts[i], 2);
		TASSERT_INT_EQUAL(wrong_reg_counts[i], 0);
	}
	TASSERT_INT_EQUAL(bad_path_count, 0);
	TASSERT_INT_EQUAL(bad_tx_count, 0);
	
	// Test re-registering doesn't re-introduce anything
	shet_reregister(&state);
	TASSERT_INT_EQUAL(register_count, 4);
	for (int i = 0; i < num; i++) {
		TASSERT_INT_EQUAL(reg_counts[i], 3);
		TASSERT_INT_EQUAL(cb_counts[i], 2);
		TASSERT_INT_EQUAL(wrong_reg_counts[i], 0);
	}
	TASSERT_INT_EQUAL(bad_path_count, 0);
	TASSERT_INT_EQUAL(bad_tx_count, 0);
}


bool test_shet_cancel_deferred_and_shet_ping(void) {
	RESET_TRANSMIT_CB();
	shet_state_t state;
	shet_state_init(&state, "\"tester\"", transmit_cb, NULL);
	
	// Send a ping event
	deferred_t deferred;
	callback_result_t result;
	result.count = 0;
	shet_ping(&state, "[1,2,3,{1:2,3:4}]",
	          &deferred, callback, NULL, &result);
	TASSERT_INT_EQUAL(transmit_count, 2);
	TASSERT_JSON_EQUAL_STR_STR(transmit_last_data, "[1,\"ping\",[1,2,3,{1:2,3:4}]]");
	
	// Send response
	char response1[] = "[1,\"return\",0,[1,2,3,{1:2,3:4}]]";
	shet_process_line(&state, response1, strlen(response1));
	TASSERT_INT_EQUAL(result.count, 1);
	TASSERT_JSON_EQUAL_TOK_STR(result.line, result.token, "[1,2,3,{1:2,3:4}]");
	
	// Send a second ping
	shet_ping(&state, "[3,2,1]",
	          &deferred, callback, NULL, &result);
	TASSERT_INT_EQUAL(transmit_count, 3);
	TASSERT_JSON_EQUAL_STR_STR(transmit_last_data, "[2,\"ping\",[3,2,1]]");
	
	// Cancel the response
	shet_cancel_deferred(&state, &deferred);
	
	// Check that the response now doesn't trigger a callback
	char response2[] = "[2,\"return\",0,[3,2,1]]";
	shet_process_line(&state, response2, strlen(response2));
	TASSERT_INT_EQUAL(result.count, 1);
	
	return true;
}


bool test_return(void) {
	RESET_TRANSMIT_CB();
	shet_state_t state;
	shet_state_init(&state, "\"tester\"", transmit_cb, NULL);
	
	// A function which immediately returns nothing
	void return_null(shet_state_t *state, char *line, jsmntok_t *tokens, void *user_data) {
		shet_return(state, 0, NULL);
	}
	
	// A function which immediately returns a value
	void return_value(shet_state_t *state, char *line, jsmntok_t *tokens, void *user_data) {
		shet_return(state, 0, "[1,2,3]");
	}
	
	// A function which doesn't respond but simply copies down the ID
	char return_id[100];
	void return_later(shet_state_t *state, char *line, jsmntok_t *tokens, void *user_data) {
		strcpy(return_id, shet_get_return_id(state));
	}
	
	deferred_t deferred;
	
	// Test an action which returns nothing (also tests objects as IDs)
	shet_make_action(&state, "/test/action",
	                 &deferred, return_null, NULL,
	                 NULL, NULL, NULL, NULL);
	char line1[] = "[{\"whacky\":\"id\"}, \"docall\", \"/test/action\", null]";
	shet_process_line(&state, line1, strlen(line1));
	TASSERT_INT_EQUAL(transmit_count, 3);
	TASSERT_JSON_EQUAL_STR_STR(transmit_last_data, "[{\"whacky\":\"id\"}, \"return\", 0, null]");
	shet_remove_action(&state, "/test/action", NULL, NULL, NULL, NULL);
	
	// Test an action which returns some value (also tests arrays as IDs)
	shet_make_action(&state, "/test/action",
	                 &deferred, return_value, NULL,
	                 NULL, NULL, NULL, NULL);
	char line2[] = "[[\"volume\",11], \"docall\", \"/test/action\", null]";
	shet_process_line(&state, line2, strlen(line2));
	TASSERT_INT_EQUAL(transmit_count, 6);
	TASSERT_JSON_EQUAL_STR_STR(transmit_last_data, "[[\"volume\":11], \"return\", 0, [1,2,3]]");
	shet_remove_action(&state, "/test/action", NULL, NULL, NULL, NULL);
	
	// Test an action which doesn't return immediately (also tests strings as IDs)
	shet_make_action(&state, "/test/action",
	                 &deferred, return_later, NULL,
	                 NULL, NULL, NULL, NULL);
	char line3[] = "[\"eye-dee\", \"docall\", \"/test/action\", null]";
	shet_process_line(&state, line3, strlen(line3));
	TASSERT_INT_EQUAL(transmit_count, 8);
	
	// Attempt to return using the stored ID
	shet_return_with_id(&state, return_id, 0, "\"woah\"");
	TASSERT_INT_EQUAL(transmit_count, 9);
	TASSERT_JSON_EQUAL_STR_STR(transmit_last_data, "[\"eye-dee\", \"return\", 0, \"woah\"]");
	shet_remove_action(&state, "/test/action", NULL, NULL, NULL, NULL);
	
	// Finally test primitives as IDs
	shet_make_action(&state, "/test/action",
	                 &deferred, return_null, NULL,
	                 NULL, NULL, NULL, NULL);
	char line4[] = "[null, \"docall\", \"/test/action\", null]";
	shet_process_line(&state, line4, strlen(line4));
	TASSERT_INT_EQUAL(transmit_count, 12);
	TASSERT_JSON_EQUAL_STR_STR(transmit_last_data, "[null, \"return\", 0, null]");
	shet_remove_action(&state, "/test/action", NULL, NULL, NULL, NULL);
	
	return true;
}


////////////////////////////////////////////////////////////////////////////////
// Test actions
////////////////////////////////////////////////////////////////////////////////


bool test_shet_make_action(void) {
	RESET_TRANSMIT_CB();
	shet_state_t state;
	shet_state_init(&state, "\"tester\"", transmit_cb, NULL);
	
	deferred_t deferred1;
	deferred_t deferred2;
	callback_result_t result1;
	callback_result_t result2;
	result1.count = 0;
	result2.count = 0;
	
	// Test that two actions can be independently called
	shet_make_action(&state, "/test/action1",
	                 &deferred1, echo_callback, &result1,
	                 NULL, NULL, NULL, NULL);
	TASSERT_INT_EQUAL(transmit_count, 2);
	TASSERT_JSON_EQUAL_STR_STR(transmit_last_data, "[1,\"mkaction\",\"/test/action1\"]");
	shet_make_action(&state, "/test/action2",
	                 &deferred2, echo_callback, &result2,
	                 NULL, NULL, NULL, NULL);
	TASSERT_INT_EQUAL(transmit_count, 3);
	TASSERT_JSON_EQUAL_STR_STR(transmit_last_data, "[2,\"mkaction\",\"/test/action2\"]");
	
	// Call each action with a null argument to ensure both can act independently
	// and that null arguments work
	char line1[] = "[0,\"docall\",\"/test/action1\"]";
	shet_process_line(&state, line1, strlen(line1));
	TASSERT_INT_EQUAL(result1.count, 1);
	TASSERT_JSON_EQUAL_TOK_STR(result1.line,result1.token, "[]");
	TASSERT_INT_EQUAL(result2.count, 0);
	TASSERT_INT_EQUAL(transmit_count, 4);
	TASSERT_JSON_EQUAL_STR_STR(transmit_last_data, "[0,\"return\",0,[]]");
	
	char line2[] = "[1,\"docall\",\"/test/action2\"]";
	shet_process_line(&state, line2, strlen(line2));
	TASSERT_INT_EQUAL(result1.count, 1);
	TASSERT_INT_EQUAL(result2.count, 1);
	TASSERT_JSON_EQUAL_TOK_STR(result2.line,result2.token, "[]");
	TASSERT_INT_EQUAL(transmit_count, 5);
	TASSERT_JSON_EQUAL_STR_STR(transmit_last_data, "[1,\"return\",0,[]]");
	
	// Make sure actions can be removed
	shet_remove_action(&state, "/test/action2", NULL, NULL, NULL, NULL);
	char line3[] = "[2,\"docall\",\"/test/action2\"]";
	shet_process_line(&state, line3, strlen(line3));
	TASSERT_INT_EQUAL(result1.count, 1);
	TASSERT_INT_EQUAL(result2.count, 1);
	TASSERT_INT_EQUAL(transmit_count, 6);
	
	// Call with a single string argument
	char line4[] = "[3,\"docall\",\"/test/action1\", \"just me\"]";
	shet_process_line(&state, line4, strlen(line4));
	TASSERT_INT_EQUAL(result1.count, 2);
	TASSERT_JSON_EQUAL_TOK_STR(result1.line,result1.token, "[\"just me\"]");
	TASSERT_INT_EQUAL(transmit_count, 7);
	TASSERT_JSON_EQUAL_STR_STR(transmit_last_data, "[3,\"return\",0,[\"just me\"]]");
	
	// Call with a single non-string primitive argument
	char line5[] = "[4,\"docall\",\"/test/action1\", true]";
	shet_process_line(&state, line5, strlen(line5));
	TASSERT_INT_EQUAL(result1.count, 3);
	TASSERT_JSON_EQUAL_TOK_STR(result1.line,result1.token, "[true]");
	TASSERT_INT_EQUAL(transmit_count, 8);
	TASSERT_JSON_EQUAL_STR_STR(transmit_last_data, "[4,\"return\",0,[true]]");
	
	// Call with a single compound argument
	char line6[] = "[5,\"docall\",\"/test/action1\", [1,2,3]]";
	shet_process_line(&state, line6, strlen(line6));
	TASSERT_INT_EQUAL(result1.count, 4);
	TASSERT_JSON_EQUAL_TOK_STR(result1.line,result1.token, "[[1,2,3]]");
	TASSERT_INT_EQUAL(transmit_count, 9);
	TASSERT_JSON_EQUAL_STR_STR(transmit_last_data, "[5,\"return\",0,[[1,2,3]]]");
	
	// Call with multiple arguments
	char line7[] = "[6,\"docall\",\"/test/action1\", 1,2,3]";
	shet_process_line(&state, line7, strlen(line7));
	TASSERT_INT_EQUAL(result1.count, 5);
	TASSERT_JSON_EQUAL_TOK_STR(result1.line,result1.token, "[1,2,3]");
	TASSERT_INT_EQUAL(transmit_count, 10);
	TASSERT_JSON_EQUAL_STR_STR(transmit_last_data, "[6,\"return\",0,[1,2,3]]");
	
	return true;
}


bool test_shet_call_action(void) {
	RESET_TRANSMIT_CB();
	shet_state_t state;
	shet_state_init(&state, "\"tester\"", transmit_cb, NULL);
	
	// Test a call with no argument
	deferred_t deferred;
	callback_result_t result;
	result.count = 0;
	shet_call_action(&state, "/test/action", NULL,
	                 &deferred, callback, NULL, &result);
	TASSERT_INT_EQUAL(transmit_count, 2);
	TASSERT_JSON_EQUAL_STR_STR(transmit_last_data, "[1,\"call\",\"/test/action\"]");
	
	// Test the return
	char line1[] = "[1,\"return\",0,null]";
	shet_process_line(&state, line1, strlen(line1));
	TASSERT_INT_EQUAL(result.count, 1);
	
	// Test a call with an argument
	shet_call_action(&state, "/test/action", "\"magic\"",
	                 &deferred, callback, NULL, &result);
	TASSERT_INT_EQUAL(transmit_count, 3);
	TASSERT_JSON_EQUAL_STR_STR(transmit_last_data, "[2,\"call\",\"/test/action\", \"magic\"]");
	
	// Test the return
	char line2[] = "[2,\"return\",0,null]";
	shet_process_line(&state, line2, strlen(line2));
	TASSERT_INT_EQUAL(result.count, 2);
	
	// Test a call with a failed response
	shet_call_action(&state, "/test/action", NULL,
	                 &deferred, NULL, callback, &result);
	TASSERT_INT_EQUAL(transmit_count, 4);
	TASSERT_JSON_EQUAL_STR_STR(transmit_last_data, "[3,\"call\",\"/test/action\"]");
	
	// Test the return with an error
	char line3[] = "[3,\"return\",1,\"fail\"]";
	shet_process_line(&state, line3, strlen(line3));
	TASSERT_INT_EQUAL(result.count, 3);
	
	return true;
}


////////////////////////////////////////////////////////////////////////////////
// Test properties
////////////////////////////////////////////////////////////////////////////////

bool test_shet_make_prop(void) {
	RESET_TRANSMIT_CB();
	shet_state_t state;
	shet_state_init(&state, "\"tester\"", transmit_cb, NULL);
	
	deferred_t deferred1;
	deferred_t deferred2;
	callback_result_t result1;
	callback_result_t result2;
	result1.count = 0;
	result2.count = 0;
	char response1[] = "[1,2,3]";
	char response2[] = "[3,2,1]";
	result1.return_value = response1;
	result2.return_value = response2;
	
	// Make two properties
	shet_make_prop(&state, "/test/prop1",
	               &deferred1, const_callback, success_callback, &result1,
	               NULL, NULL, NULL, NULL);
	TASSERT_INT_EQUAL(transmit_count, 2);
	TASSERT_JSON_EQUAL_STR_STR(transmit_last_data, "[1,\"mkprop\",\"/test/prop1\"]");
	shet_make_prop(&state, "/test/prop2",
	               &deferred2, const_callback, success_callback, &result2,
	               NULL, NULL, NULL, NULL);
	TASSERT_INT_EQUAL(transmit_count, 3);
	TASSERT_JSON_EQUAL_STR_STR(transmit_last_data, "[2,\"mkprop\",\"/test/prop2\"]");
	
	// Make sure they can be set independently
	char line1[] = "[0,\"setprop\",\"/test/prop1\",123]";
	shet_process_line(&state, line1, strlen(line1));
	TASSERT_INT_EQUAL(result1.count, 1);
	TASSERT_JSON_EQUAL_TOK_STR(result1.line,result1.token, "[123]");
	TASSERT_INT_EQUAL(result2.count, 0);
	TASSERT_INT_EQUAL(transmit_count, 4);
	TASSERT_JSON_EQUAL_STR_STR(transmit_last_data, "[0,\"return\",0,null]");
	
	char line2[] = "[1,\"setprop\",\"/test/prop2\",321]";
	shet_process_line(&state, line2, strlen(line2));
	TASSERT_INT_EQUAL(result1.count, 1);
	TASSERT_INT_EQUAL(result2.count, 1);
	TASSERT_JSON_EQUAL_TOK_STR(result2.line,result2.token, "[321]");
	TASSERT_INT_EQUAL(transmit_count, 5);
	TASSERT_JSON_EQUAL_STR_STR(transmit_last_data, "[1,\"return\",0,null]");
	
	// Make sure they can be got independently
	char line3[] = "[0,\"getprop\",\"/test/prop1\"]";
	shet_process_line(&state, line3, strlen(line3));
	TASSERT_INT_EQUAL(result1.count, 2);
	TASSERT_JSON_EQUAL_TOK_STR(result1.line,result1.token, "[]");
	TASSERT_INT_EQUAL(result2.count, 1);
	TASSERT_INT_EQUAL(transmit_count, 6);
	TASSERT_JSON_EQUAL_STR_STR(transmit_last_data, "[0,\"return\",0,[1,2,3]]");
	
	char line4[] = "[1,\"getprop\",\"/test/prop2\"]";
	shet_process_line(&state, line4, strlen(line4));
	TASSERT_INT_EQUAL(result1.count, 2);
	TASSERT_INT_EQUAL(result2.count, 2);
	TASSERT_JSON_EQUAL_TOK_STR(result2.line,result2.token, "[]");
	TASSERT_INT_EQUAL(transmit_count, 7);
	TASSERT_JSON_EQUAL_STR_STR(transmit_last_data, "[1,\"return\",0,[3,2,1]]");
	
	// Make sure properties can be removed independently
	shet_remove_prop(&state, "/test/prop2", NULL, NULL, NULL, NULL);
	
	// Prop 1 should remain
	char line5[] = "[0,\"getprop\",\"/test/prop1\"]";
	shet_process_line(&state, line5, strlen(line5));
	TASSERT_INT_EQUAL(result1.count, 3);
	TASSERT_JSON_EQUAL_TOK_STR(result1.line,result1.token, "[]");
	TASSERT_INT_EQUAL(result2.count, 2);
	TASSERT_INT_EQUAL(transmit_count, 9);
	TASSERT_JSON_EQUAL_STR_STR(transmit_last_data, "[0,\"return\",0,[1,2,3]]");
	
	// Prop 2 should nolonger exist
	char line6[] = "[1,\"getprop\",\"/test/prop2\"]";
	shet_process_line(&state, line6, strlen(line6));
	TASSERT_INT_EQUAL(result1.count, 3);
	TASSERT_INT_EQUAL(result2.count, 2);
	TASSERT_INT_EQUAL(transmit_count, 9);
	
	return true;
}


bool test_shet_set_prop_and_shet_get_prop(void) {
	RESET_TRANSMIT_CB();
	shet_state_t state;
	shet_state_init(&state, "\"tester\"", transmit_cb, NULL);
	
	deferred_t deferred;
	callback_result_t result;
	result.count = 0;
	
	// Test get
	shet_get_prop(&state, "/test/get",
	              &deferred, callback, NULL, &result);
	TASSERT_INT_EQUAL(transmit_count, 2);
	TASSERT_JSON_EQUAL_STR_STR(transmit_last_data, "[1,\"get\",\"/test/get\"]");
	
	// Test the return
	char line1[] = "[1,\"return\",0,[1,2,3]]";
	shet_process_line(&state, line1, strlen(line1));
	TASSERT_INT_EQUAL(result.count, 1);
	TASSERT_JSON_EQUAL_TOK_STR(result.line,result.token, "[1,2,3]");
	
	// Test set
	shet_set_prop(&state, "/test/set", "[3,2,1]",
	              &deferred, callback, NULL, &result);
	TASSERT_INT_EQUAL(transmit_count, 3);
	TASSERT_JSON_EQUAL_STR_STR(transmit_last_data, "[2,\"set\",\"/test/set\", [3,2,1]]");
	
	// Test the return
	char line2[] = "[2,\"return\",0,null]";
	shet_process_line(&state, line2, strlen(line2));
	TASSERT_INT_EQUAL(result.count, 2);
	TASSERT_JSON_EQUAL_TOK_STR(result.line,result.token, "null");
	
	// Test that error conditions work
	shet_set_prop(&state, "/test/set", "[9,9,9]",
	              &deferred, NULL, callback, &result);
	TASSERT_INT_EQUAL(transmit_count, 4);
	TASSERT_JSON_EQUAL_STR_STR(transmit_last_data, "[3,\"set\",\"/test/set\", [9,9,9]]");
	char line3[] = "[3,\"return\",1,\"fail\"]";
	shet_process_line(&state, line3, strlen(line3));
	TASSERT_INT_EQUAL(result.count, 3);
	TASSERT_JSON_EQUAL_TOK_STR(result.line,result.token, "\"fail\"");
	
	return true;
}

////////////////////////////////////////////////////////////////////////////////
// World starts here
////////////////////////////////////////////////////////////////////////////////

int main(int argc, char *argv[]) {
	bool (*tests[])(void) = {
		test_assert_int,
		test_deferred_utilities,
		test_shet_state_init,
		test_shet_set_error_callback,
		test_send_command,
		test_shet_register,
		test_shet_cancel_deferred_and_shet_ping,
		test_return,
		test_shet_make_action,
		test_shet_call_action,
		test_shet_make_prop,
		test_shet_set_prop_and_shet_get_prop,
	};
	size_t num_tests = sizeof(tests)/sizeof(tests[0]);
	
	size_t num_passes = 0;
	for (int i = 0; i < num_tests; i++) {
		bool result = tests[i]();
		if (result)
			fprintf(stderr, ".");
		else
			fprintf(stderr, "F");
		
		num_passes += result ? 1 : 0;
	}
	
	fprintf(stderr, "\n%s: %d of %d tests passed!\n",
	        (num_passes == num_tests) ? "PASS" : "FAIL",
	        num_passes,
	        num_tests);
	return (num_passes == num_tests) ? 0 : -1;
}
