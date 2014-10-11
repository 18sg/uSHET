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
// NULL, they will not be set.
bool cmp_json_tokens(const char *json_a, const char *json_b,
                     jsmntok_t **tokens_a, jsmntok_t **tokens_b,
                     jsmntok_t *differing_a, jsmntok_t *differing_b) {
	// Sanity check the basic object
	if ((**tokens_a).type != (**tokens_b).type ||
	    (**tokens_a).size != (**tokens_b).size) {
		*differing_a = **tokens_a;
		*differing_b = **tokens_b;
		return false;
	} else if ((*tokens_a)->type == JSMN_PRIMITIVE ||
	           (*tokens_a)->type == JSMN_STRING) {
		// Compare atomic objects
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
	} else {
		// Iterate over compound objects
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

// Assert equal integers
#define TASSERT_INT_EQUAL(a,b) do { if ((a) != (b)) { \
	fprintf(stderr, "TASSERT Failed: %s:%s:%d: "#a" (%d) == "#b" (%d)\n",\
	        __FILE__,__func__,__LINE__, (a), (b));\
	return false; \
} } while (0)

// Compare two JSON strings given as a set of tokens and assert that they are
// equivilent
#define TASSERT_JSON_EQUAL_TOK_TOK(sa,ta,sb,tb) do { \
	TASSERT((sa) != NULL);\
	TASSERT((ta) != NULL);\
	TASSERT((sb) != NULL);\
	TASSERT((tb) != NULL);\
	const char *a = (sa); /* JSON String a */ \
	const char *b = (sb); /* JSON String b */ \
	jsmntok_t *ca = (ta); /* Cur token a */ \
	jsmntok_t *cb = (tb); /* Cur token b */ \
	jsmntok_t da; /* Differing token a */ \
	da.start = 0; \
	jsmntok_t db; /* Differing token b */ \
	db.start = 0; \
	if (!cmp_json_tokens(a,b,&ca,&cb,&da,&db)) { \
		fprintf(stderr, "TASSERT_JSON_EQUAL_TOK_TOK Failed: %s:%s:%d:\n",\
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
	TASSERT(e >= 0); \
	TASSERT_JSON_EQUAL_TOK_TOK(sa, ta, sb, tb);\
} while (0)


// Compare two JSON strings the first given as a string and token and the other
// as a string and assert that they are equivilent.
#define TASSERT_JSON_EQUAL_STR_STR(sa,sb) do { \
	jsmn_parser p; \
	jsmn_init(&p); \
	jsmntok_t ta[100]; \
	jsmnerr_t e = jsmn_parse(&p, sa, strlen(sa), \
	                         ta, 100); \
	TASSERT(e >= 0); \
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
} callback_result_t;

static void callback(shet_state_t *state, char *line, jsmntok_t *token, void *user_data) {
	callback_result_t *result = (callback_result_t *)user_data;
	if (result != NULL) {
		result->state = state;
		result->line = line;
		result->token = token;
		result->count++;
	}
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



////////////////////////////////////////////////////////////////////////////////
// World starts here
////////////////////////////////////////////////////////////////////////////////

int main(int argc, char *argv[]) {
	bool (*tests[])(void) = {
		test_assert_int,
		test_deferred_utilities,
		test_shet_state_init,
		test_shet_set_error_callback,
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
