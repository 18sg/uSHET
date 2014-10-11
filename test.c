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

// Given two JSON strings, compare them and return the token which points to the
// first differing componenst in each via differing_{a,b}. Returns a bool which
// is true if the JSON is the same and false if not. If differing_{a,b} are
// NULL, they will not be set.
bool cmp_json(const char *json_a, const char *json_b,
              jsmntok_t *differing_a, jsmntok_t *differing_b) {
	const size_t num_tokens = 100;
	
	jsmn_parser pa;
	jsmn_parser pb;
	jsmn_init(&pa);
	jsmn_init(&pb);
	jsmntok_t tokens_a[num_tokens];
	jsmntok_t tokens_b[num_tokens];
	
	// Attempt to tokenise the strings
	jsmnerr_t ea = jsmn_parse(&pa , json_a , strlen(json_a),
	                          tokens_a , num_tokens);
	jsmnerr_t eb = jsmn_parse(&pb , json_b , strlen(json_b),
	                          tokens_b , num_tokens);
	
	// Make sure the string tokenzied successfully
	if (ea < 0 || eb < 0)
		return false;
	
	for (int i = 0; i < ea && i < eb; i++) {
		if (tokens_a[i].type != tokens_b[i].type ||
		    tokens_a[i].size != tokens_b[i].size ||
		    ((tokens_a[i].type == JSMN_PRIMITIVE ||
		      tokens_a[i].type == JSMN_STRING) &&
		     strncmp(json_a+tokens_a[i].start,
		             json_b+tokens_b[i].start,
		             tokens_a[i].end - tokens_a[i].start))) {
			if (differing_a != NULL) *differing_a = tokens_a[i];
			if (differing_b != NULL) *differing_b = tokens_b[i];
			return false;
		}
	}
	
	// Finally, a sanity check...
	return ea == eb;
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

// Standard "assert true" assertion
#define TASSERT_JSON_EQUAL(a,b) do { \
	const char *sa = (a); \
	const char *sb = (b); \
	jsmntok_t ta; \
	ta.start = 0; \
	jsmntok_t tb; \
	tb.start = 0; \
	if (!cmp_json(sa,sb,&ta,&tb)) { \
		fprintf(stderr, "TASSERT_JSON_EQUAL Failed: %s:%s:%d:\n",\
		        __FILE__,__func__,__LINE__);\
		fprintf(stderr, " > \"%s\"\n", sa);\
		fprintf(stderr, " >  ", sa);\
		if (ta.start == tb.start) { \
			for (int i = 0; i < ta.start; i++) fprintf(stderr, " "); \
			fprintf(stderr, "|\n"); \
		} else if (ta.start < tb.start) { \
			for (int i = 0; i < ta.start; i++) fprintf(stderr, " "); \
			fprintf(stderr, "^"); \
			for (int i = ta.start; i < tb.start-1; i++) fprintf(stderr, "-"); \
			fprintf(stderr, "v\n"); \
		} else { \
			for (int i = 0; i < tb.start; i++) fprintf(stderr, " "); \
			fprintf(stderr, "v"); \
			for (int i = tb.start; i < ta.start-1; i++) fprintf(stderr, "-"); \
			fprintf(stderr, "^\n"); \
		} \
		fprintf(stderr, " > \"%s\"\n", sb);\
		return false; \
	} \
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
	const char *conn_name = "tester";
	
	shet_state_t state;
	RESET_TRANSMIT_CB();
	shet_state_init(&state, conn_name, transmit_cb, (void *)test_shet_state_init);
	
	// Make sure a registration command is sent
	TASSERT(transmit_count == 1);
	TASSERT(transmit_last_user_data == (void *)test_shet_state_init);
	TASSERT(transmit_last_data != NULL);
	
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
