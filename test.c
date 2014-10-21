/**
 * A test suite for uSHET. Only tests functionality of underlying protocol
 * implementation. Does not test any wrappers.
 */

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <limits.h>

// Include the C files so that static functions can be tested
#include "lib/jsmn.c"
#include "lib/shet.c"
#include "lib/shet_json.c"
#include "lib/ezshet.c"

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
		if ( ((*tokens_a)->end - (*tokens_a)->start) !=
		     ((*tokens_b)->end - (*tokens_b)->start) ||
		    strncmp(json_a+(*tokens_a)->start,
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
#define TASSERT_INT_EQUAL(a,b) do { \
  int va = (a); \
  int vb = (b); \
  if ((va) != (vb)) { \
	fprintf(stderr, "TASSERT_INT_EQUAL Failed: %s:%s:%d: "#a" (%d) == "#b" (%d)\n",\
	        __FILE__,__func__,__LINE__, (va), (vb));\
	return false; \
} } while (0)

// Compare two JSON strings given as a set of tokens and assert that they are
// equivalent. If they are not, prints the two strings along with an indicator
// pointing at the first non-matching parts.
#define TASSERT_JSON_EQUAL_TOK_TOK(ja,jb) do { \
	char *sa = ja.line;\
	jsmntok_t *ta = ja.token;\
	char *sb = jb.line;\
	jsmntok_t *tb = jb.token;\
	TASSERT((sa) != NULL);\
	TASSERT((ta) != NULL);\
	TASSERT((sb) != NULL);\
	TASSERT((tb) != NULL);\
	const char *a = (sa); /* JSON String a */ \
	const char *b = (sb); /* JSON String b */ \
	jsmntok_t *ca = (ta); /* Cur token pointer a */ \
	jsmntok_t *cb = (tb); /* Cur token pointer b */ \
	jsmntok_t da; /* Differing token a */ \
	da.start = (ta)->start; \
	jsmntok_t db; /* Differing token b */ \
	db.start = (tb)->start; \
	if (!cmp_json_tokens(a,b,&ca,&cb,&da,&db)) { \
		fprintf(stderr, "TASSERT_JSON_EQUAL Failed: %s:%s:%d:\n",\
		        __FILE__,__func__,__LINE__);\
		/* Print strings with their quotes */ \
		if ((ta)->type == JSMN_STRING) { \
			(ta)->start--; \
			(ta)->end++; \
		} \
		if ((tb)->type == JSMN_STRING) { \
			(tb)->start--; \
			(tb)->end++; \
		} \
		/* Print the difference */ \
		fprintf(stderr, " > \"%.*s\"\n", (ta)->end-(ta)->start, a+(ta)->start);\
		fprintf(stderr, " >  ", a);\
		if (da.start-(ta)->start == db.start-(tb)->start) { \
			for (int i = (ta)->start; i < da.start-(ta)->start; i++) fprintf(stderr, " "); \
			fprintf(stderr, "|\n"); \
		} else if (da.start-(ta)->start < db.start-(tb)->start) { \
			for (int i = (ta)->start; i < da.start-(ta)->start; i++) fprintf(stderr, " "); \
			fprintf(stderr, "^"); \
			for (int i = da.start-(ta)->start; i < db.start-(tb)->start-1; i++) fprintf(stderr, "-"); \
			fprintf(stderr, "v\n"); \
		} else { \
			for (int i = (tb)->start; i < db.start-(tb)->start; i++) fprintf(stderr, " "); \
			fprintf(stderr, "v"); \
			for (int i = db.start-(tb)->start; i < da.start-(ta)->start-1; i++) fprintf(stderr, "-"); \
			fprintf(stderr, "^\n"); \
		} \
		fprintf(stderr, " > \"%.*s\"\n", (tb)->end-(tb)->start, b+(tb)->start);\
		return false; \
	} \
} while (0)


// Compare two JSON strings the first given as a string and token and the other
// as a string and assert that they are equivilent.
#define TASSERT_JSON_EQUAL_TOK_STR(ja,sb) do { \
	jsmn_parser p; \
	jsmn_init(&p); \
	jsmntok_t tb[100]; \
	jsmnerr_t e = jsmn_parse(&p, (sb), strlen((sb)), \
	                         tb, 100); \
	if (e <= 0) { \
		fprintf(stderr, "TASSERT_JSON_EQUAL Could not parse JSON: %s:%s:%d: %s\n",\
		        __FILE__,__func__,__LINE__,(sb)); \
		return false; \
	} \
	shet_json_t jb; \
	/* XXX: Cast off the const for shet_json_t. This macro is known not to modify
	 * the string. */ \
	jb.line = (char *)(sb); \
	jb.token = tb; \
	TASSERT_JSON_EQUAL_TOK_TOK((ja), jb);\
} while (0)


// Compare two JSON strings given as strings.
#define TASSERT_JSON_EQUAL_STR_STR(sa,sb) do { \
	jsmn_parser p; \
	jsmn_init(&p); \
	jsmntok_t ta[100]; \
	jsmnerr_t e = jsmn_parse(&p, (sa), strlen((sa)), \
	                         ta, 100); \
	if (e <= 0) { \
		fprintf(stderr, "TASSERT_JSON_EQUAL Could not parse JSON: %s:%s:%d: %s\n",\
		        __FILE__,__func__,__LINE__,(sa)); \
		return false; \
	} \
	shet_json_t ja; \
	/* XXX: Cast off the const for shet_json_t. This macro is known not to modify
	 * the string. */ \
	ja.line = (char *)(sa); \
	ja.token = ta; \
	TASSERT_JSON_EQUAL_TOK_STR(ja, (sb));\
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
	shet_json_t json;
	int count;
	const char *return_value;
} callback_result_t;

static void callback(shet_state_t *state, shet_json_t json, void *user_data) {
	callback_result_t *result = (callback_result_t *)user_data;
	if (result != NULL) {
		result->state = state;
		result->json = json;
		result->count++;
	} else {
		fprintf(stderr, "TEST ERROR: callback didn't receive result pointer!\n");
	}
}


// A callback like the above but which returns its argument
static void echo_callback(shet_state_t *state, shet_json_t json, void *user_data) {
	callback(state, json, user_data);
	
	// Return back the argument
	char response[100];
	strncpy(response, json.line + json.token[0].start, json.token[0].end - json.token[0].start);
	response[json.token[0].end - json.token[0].start] = '\0';
	shet_return(state, 0, response);
}


// A callback like the above but which returns success and returns a null value
static void success_callback(shet_state_t *state, shet_json_t json, void *user_data) {
	callback(state, json, user_data);
	
	shet_return(state, 0, "null");
}


// A callback like the above but which returns success and the value in
// the return_value field of callback_result_t.
static void const_callback(shet_state_t *state, shet_json_t json, void *user_data) {
	callback(state, json, user_data);
	
	shet_return(state, 0, ((callback_result_t *)user_data)->return_value);
}


////////////////////////////////////////////////////////////////////////////////
// Test token type checking
////////////////////////////////////////////////////////////////////////////////

bool test_SHET_JSON_IS_TYPE(void) {
	// Make sure that the function can distinguish between primitives which are
	// and are not integers
	char *strings[] = {
		"0",
		"123",
		"-123",
		"true",
		"false",
		"null",
		"\"str\"",
		"[]",
		"{}",
	};
	enum {
		SHET_INT = 0,
		SHET_FLOAT = 0,
		SHET_BOOL,
		SHET_NULL,
		SHET_STRING,
		SHET_ARRAY,
		SHET_OBJECT,
	} types[] = {
		SHET_INT,
		SHET_INT,
		SHET_INT,
		SHET_BOOL,
		SHET_BOOL,
		SHET_NULL,
		SHET_STRING,
		SHET_ARRAY,
		SHET_OBJECT,
	};
	size_t num_strings = sizeof(strings)/sizeof(char *);
	
	for (int i = 0; i < num_strings; i++) {
		jsmn_parser p;
		jsmn_init(&p);
		jsmntok_t tokens[4];
		
		jsmnerr_t e = jsmn_parse( &p
		                        , strings[i]
		                        , strlen(strings[i])
		                        , tokens
		                        , 4
		                        );
		// Make sure the string tokenzied successfully
		TASSERT(e >= 1);
		
		shet_json_t json;
		json.line = strings[i];
		json.token = tokens;
		
		// Check the types
		if (types[i] == SHET_INT) 
			TASSERT(SHET_JSON_IS_TYPE(json, SHET_INT));
		else
			TASSERT(!SHET_JSON_IS_TYPE(json, SHET_INT));
		
		if (types[i] == SHET_BOOL) 
			TASSERT(SHET_JSON_IS_TYPE(json, SHET_BOOL));
		else
			TASSERT(!SHET_JSON_IS_TYPE(json, SHET_BOOL));
		
		if (types[i] == SHET_NULL) 
			TASSERT(SHET_JSON_IS_TYPE(json, SHET_NULL));
		else
			TASSERT(!SHET_JSON_IS_TYPE(json, SHET_NULL));
		
		if (types[i] == SHET_STRING) 
			TASSERT(SHET_JSON_IS_TYPE(json, SHET_STRING));
		else
			TASSERT(!SHET_JSON_IS_TYPE(json, SHET_STRING));
		
		if (types[i] == SHET_ARRAY) 
			TASSERT(SHET_JSON_IS_TYPE(json, SHET_ARRAY));
		else
			TASSERT(!SHET_JSON_IS_TYPE(json, SHET_ARRAY));
		
		if (types[i] == SHET_OBJECT) 
			TASSERT(SHET_JSON_IS_TYPE(json, SHET_OBJECT));
		else
			TASSERT(!SHET_JSON_IS_TYPE(json, SHET_OBJECT));
	}
	
	return true;
}

////////////////////////////////////////////////////////////////////////////////
// Test token type conversion
////////////////////////////////////////////////////////////////////////////////

bool test_SHET_PARSE_JSON_VALUE_INT(void) {
	char *json_ints[] = {"[0]", "[123]", "[-1]", "[+1]"};
	int      c_ints[] = {  0,     123,     -1,     +1 };
	size_t num = sizeof(c_ints)/sizeof(int);
	
	for (int i = 0; i < num; i++) {
		jsmn_parser p;
		jsmn_init(&p);
		jsmntok_t tokens[4];
		
		jsmnerr_t e = jsmn_parse( &p
		                        , json_ints[i]
		                        , strlen(json_ints[i])
		                        , tokens
		                        , 4
		                        );
		TASSERT(e == 2);
		
		shet_json_t json;
		json.line = json_ints[i];
		json.token = tokens + 1;
		TASSERT_INT_EQUAL(SHET_PARSE_JSON_VALUE(json, SHET_INT), c_ints[i]);
	}
	
	return true;
}

bool test_SHET_PARSE_JSON_VALUE_FLOAT(void) {
	char *json_floats[] = {"[0]", "[1.5]", "[-1.5]", "[+1.5]", "[1e7]"};
	double   c_floats[] = { 0.0,    1.5,     -1.5,     +1.5  ,   1e7};
	size_t num = sizeof(c_floats)/sizeof(double);
	
	for (int i = 0; i < num; i++) {
		jsmn_parser p;
		jsmn_init(&p);
		jsmntok_t tokens[4];
		
		jsmnerr_t e = jsmn_parse( &p
		                        , json_floats[i]
		                        , strlen(json_floats[i])
		                        , tokens
		                        , 4
		                        );
		TASSERT(e == 2);
		
		shet_json_t json;
		json.line = json_floats[i];
		json.token = tokens + 1;
		TASSERT(SHET_PARSE_JSON_VALUE(json, SHET_FLOAT) == c_floats[i]);
	}
	
	return true;
}

bool test_SHET_PARSE_JSON_VALUE_BOOL(void) {
	char *json_bools[] = {"[true]", "[false]"};
	bool     c_bools[] = {  true,     false};
	size_t num = sizeof(c_bools)/sizeof(bool);
	
	for (int i = 0; i < num; i++) {
		jsmn_parser p;
		jsmn_init(&p);
		jsmntok_t tokens[4];
		
		jsmnerr_t e = jsmn_parse( &p
		                        , json_bools[i]
		                        , strlen(json_bools[i])
		                        , tokens
		                        , 4
		                        );
		TASSERT(e == 2);
		
		shet_json_t json;
		json.line = json_bools[i];
		json.token = tokens + 1;
		TASSERT(SHET_PARSE_JSON_VALUE(json, SHET_BOOL) == c_bools[i]);
	}
	
	return true;
}

bool test_SHET_PARSE_JSON_VALUE_STRING(void) {
	char s0[] = "[\"\"]";
	char s1[] = "[\"I am a magical string!\"]";
	char *json_strings[] = {s0,s1};
	char    *c_strings[] = {"", "I am a magical string!"};
	size_t num = sizeof(c_strings)/sizeof(char *);
	
	for (int i = 0; i < num; i++) {
		jsmn_parser p;
		jsmn_init(&p);
		jsmntok_t tokens[4];
		
		jsmnerr_t e = jsmn_parse( &p
		                        , json_strings[i]
		                        , strlen(json_strings[i])
		                        , tokens
		                        , 4
		                        );
		TASSERT(e == 2);
		
		shet_json_t json;
		json.line = json_strings[i];
		json.token = tokens + 1;
		TASSERT(strcmp(SHET_PARSE_JSON_VALUE(json, SHET_STRING), c_strings[i]) == 0);
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
	shet_deferred_t test_deferred;
	callback_result_t result;
	result.count = 0;
	send_command(&state, "test5", NULL, NULL,
	             &test_deferred, callback, NULL, &result);
	TASSERT(transmit_count == 6);
	TASSERT(result.count == 0);
	TASSERT_JSON_EQUAL_STR_STR(transmit_last_data, "[5, \"test5\"]");
	char response[] = "[5, \"return\", 0, [1,2,3,4]]";
	TASSERT(shet_process_line(&state, response, strlen(response)) == SHET_PROC_OK);
	TASSERT(result.count == 1);
	TASSERT_JSON_EQUAL_TOK_STR(result.json, "[1,2,3,4]");
	
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
	TASSERT(find_named_cb(&state, "/", SHET_EVENT_CCB) == NULL);
	TASSERT(find_named_cb(&state, "/", SHET_EVENT_DELETED_CCB) == NULL);
	TASSERT(find_named_cb(&state, "/", SHET_EVENT_CREATED_CCB) == NULL);
	TASSERT(find_named_cb(&state, "/", SHET_GET_PROP_CCB) == NULL);
	TASSERT(find_named_cb(&state, "/", SHET_SET_PROP_CCB) == NULL);
	TASSERT(find_named_cb(&state, "/", SHET_CALL_CCB) == NULL);
	
	// ...and that searching doesn't add stuff
	TASSERT(state.callbacks == NULL);
	
	// Add a return
	shet_deferred_t d1;
	d1.type = SHET_RETURN_CB;
	d1.data.return_cb.id = 0;
	add_deferred(&state, &d1);
	
	// Make sure it is there the hard way
	TASSERT(state.callbacks == &d1);
	TASSERT(state.callbacks->next == NULL);
	
	// Make sure it is found (but not found by anything else)
	TASSERT(find_return_cb(&state, 0) == &d1);
	TASSERT(find_return_cb(&state, 1) == NULL);
	TASSERT(find_named_cb(&state, "/", SHET_EVENT_CCB) == NULL);
	TASSERT(find_named_cb(&state, "/", SHET_EVENT_DELETED_CCB) == NULL);
	TASSERT(find_named_cb(&state, "/", SHET_EVENT_CREATED_CCB) == NULL);
	TASSERT(find_named_cb(&state, "/", SHET_GET_PROP_CCB) == NULL);
	TASSERT(find_named_cb(&state, "/", SHET_SET_PROP_CCB) == NULL);
	TASSERT(find_named_cb(&state, "/", SHET_CALL_CCB) == NULL);
	
	// Make sure it can be added a second time without duplicating
	add_deferred(&state, &d1);
	
	// Make sure it is there the hard way
	TASSERT(state.callbacks == &d1);
	TASSERT(state.callbacks->next == NULL);
	
	// Make sure it is found (but not found by anything else)
	TASSERT(find_return_cb(&state, 0) == &d1);
	TASSERT(find_return_cb(&state, 1) == NULL);
	TASSERT(find_named_cb(&state, "/", SHET_EVENT_CCB) == NULL);
	TASSERT(find_named_cb(&state, "/", SHET_EVENT_DELETED_CCB) == NULL);
	TASSERT(find_named_cb(&state, "/", SHET_EVENT_CREATED_CCB) == NULL);
	TASSERT(find_named_cb(&state, "/", SHET_GET_PROP_CCB) == NULL);
	TASSERT(find_named_cb(&state, "/", SHET_SET_PROP_CCB) == NULL);
	TASSERT(find_named_cb(&state, "/", SHET_CALL_CCB) == NULL);
	
	// Make sure we can remove it
	remove_deferred(&state, &d1);
	
	// Make sure it is gone
	TASSERT(state.callbacks == NULL);
	TASSERT(find_return_cb(&state, 0) == NULL);
	
	// Add more than one item
	shet_deferred_t d2;
	d2.type = SHET_RETURN_CB;
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
	d1.type = SHET_EVENT_CB;
	d1.data.event_cb.watch_deferred = NULL;
	d1.data.event_cb.event_name = "/event/d1";
	add_deferred(&state, &d1);
	d2.type = SHET_EVENT_CB;
	d2.data.event_cb.watch_deferred = NULL;
	d2.data.event_cb.event_name = "/event/d2";
	add_deferred(&state, &d2);
	
	TASSERT(find_named_cb(&state, "/event/d1", SHET_EVENT_CB) == &d1);
	TASSERT(find_named_cb(&state, "/event/d2", SHET_EVENT_CB) == &d2);
	
	// And that non-existant stuff doesn't get found
	TASSERT(find_named_cb(&state, "/non_existant", SHET_EVENT_CB) == NULL);
	TASSERT(find_named_cb(&state, "/event/d1", SHET_ACTION_CB) == NULL);
	TASSERT(find_named_cb(&state, "/event/d1", SHET_PROP_CB) == NULL);
	TASSERT(find_named_cb(&state, "/event/d1", SHET_RETURN_CB) == NULL);
	
	// Try actions...
	remove_deferred(&state, &d1);
	remove_deferred(&state, &d2);
	d1.type = SHET_ACTION_CB;
	d1.data.action_cb.mkaction_deferred = NULL;
	d1.data.action_cb.action_name = "/action/d1";
	d2.type = SHET_ACTION_CB;
	d2.data.action_cb.mkaction_deferred = NULL;
	d2.data.action_cb.action_name = "/action/d2";
	add_deferred(&state, &d1);
	add_deferred(&state, &d2);
	
	TASSERT(find_named_cb(&state, "/action/d1", SHET_ACTION_CB) == &d1);
	TASSERT(find_named_cb(&state, "/action/d2", SHET_ACTION_CB) == &d2);
	
	TASSERT(find_named_cb(&state, "/action/d1", SHET_EVENT_CB) == NULL);
	TASSERT(find_named_cb(&state, "/action/d1", SHET_PROP_CB) == NULL);
	TASSERT(find_named_cb(&state, "/action/d1", SHET_RETURN_CB) == NULL);
	
	// Try properties...
	remove_deferred(&state, &d1);
	remove_deferred(&state, &d2);
	d1.type = SHET_PROP_CB;
	d1.data.prop_cb.mkprop_deferred = NULL;
	d1.data.prop_cb.prop_name = "/prop/d1";
	d2.type = SHET_PROP_CB;
	d2.data.prop_cb.mkprop_deferred = NULL;
	d2.data.prop_cb.prop_name = "/prop/d2";
	add_deferred(&state, &d1);
	add_deferred(&state, &d2);
	
	TASSERT(find_named_cb(&state, "/prop/d1", SHET_PROP_CB) == &d1);
	TASSERT(find_named_cb(&state, "/prop/d2", SHET_PROP_CB) == &d2);
	
	TASSERT(find_named_cb(&state, "/prop/d1", SHET_EVENT_CB) == NULL);
	TASSERT(find_named_cb(&state, "/prop/d1", SHET_ACTION_CB) == NULL);
	TASSERT(find_named_cb(&state, "/prop/d1", SHET_RETURN_CB) == NULL);
	
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


bool test_shet_process_line_errors(void) {
	RESET_TRANSMIT_CB();
	shet_state_t state;
	shet_state_init(&state, "\"tester\"", transmit_cb, (void *)test_shet_state_init);
	
	// Send an empty string
	TASSERT(shet_process_line(&state, "", 0) == SHET_PROC_INVALID_JSON);
	
	// Send an invalid piece of JSON
	char line1[] = "[not valid : even 1 bit}";
	TASSERT(shet_process_line(&state, line1, strlen(line1)) == SHET_PROC_INVALID_JSON);
	
	// Send an command of the wrong type
	char line2[] = "{\"wrong\":\"type\"}";
	TASSERT(shet_process_line(&state, line2, strlen(line2)) == SHET_PROC_MALFORMED_COMMAND);
	
	// Send an command which is too short
	char line3[] = "[0]";
	TASSERT(shet_process_line(&state, line3, strlen(line3)) == SHET_PROC_MALFORMED_COMMAND);
	
	// Send an command with a non-string name
	char line4[] = "[0, 123]";
	TASSERT(shet_process_line(&state, line4, strlen(line4)) == SHET_PROC_MALFORMED_COMMAND);
	
	// Send an command which is unrecognised
	char line5[] = "[0, \"thiswillneverexist\"]";
	TASSERT(shet_process_line(&state, line5, strlen(line5)) == SHET_PROC_UNKNOWN_COMMAND);
	
	// Send a return with a non-integer ID
	char line6[] = "[false, \"return\", 0, none]";
	TASSERT(shet_process_line(&state, line6, strlen(line6)) == SHET_PROC_MALFORMED_RETURN);
	
	// Send a return with a non-integer success
	char line7[] = "[0, \"return\", false, none]";
	TASSERT(shet_process_line(&state, line7, strlen(line7)) == SHET_PROC_MALFORMED_RETURN);
	
	// Send a return with no argument
	char line8[] = "[0, \"return\", 0]";
	TASSERT(shet_process_line(&state, line8, strlen(line8)) == SHET_PROC_MALFORMED_RETURN);
	
	// Send a return with too-many arguments
	char line9[] = "[0, \"return\", 0, 1,2,3]";
	TASSERT(shet_process_line(&state, line9, strlen(line9)) == SHET_PROC_MALFORMED_RETURN);
	
	// Send a command without a path
	char line10[] = "[0, \"event\"]";
	TASSERT(shet_process_line(&state, line10, strlen(line10)) == SHET_PROC_MALFORMED_ARGUMENTS);
	
	// Send an event created with arguments (supposed to be none)
	char line11[] = "[0, \"eventcreated\", \"/test\", 1]";
	TASSERT(shet_process_line(&state, line11, strlen(line11)) == SHET_PROC_MALFORMED_ARGUMENTS);
	
	// Send an event deleted with arguments (supposed to be none)
	char line12[] = "[0, \"eventdeleted\", \"/test\", 1]";
	TASSERT(shet_process_line(&state, line12, strlen(line12)) == SHET_PROC_MALFORMED_ARGUMENTS);
	
	// Send a get property with arguments (supposed to be none)
	char line13[] = "[0, \"getprop\", \"/test\", 1]";
	TASSERT(shet_process_line(&state, line13, strlen(line13)) == SHET_PROC_MALFORMED_ARGUMENTS);
	
	// Send a set property with too-many arguments
	char line14[] = "[0, \"setprop\", \"/test\", 1,2,3]";
	TASSERT(shet_process_line(&state, line14, strlen(line14)) == SHET_PROC_MALFORMED_ARGUMENTS);
	
	return true;
}



bool test_shet_set_error_callback(void) {
	RESET_TRANSMIT_CB();
	shet_state_t state;
	shet_state_init(&state, "\"tester\"", transmit_cb, NULL);
	
	// Make sure nothing happens when an unknown (non-successful) return arrives
	// without an error callback setup
	char line1[] = "[0,\"return\",1,[1,2,3]]";
	TASSERT(shet_process_line(&state, line1, strlen(line1)) == SHET_PROC_OK);
	
	// Set up the error callback
	callback_result_t result;
	result.count = 0;
	shet_set_error_callback(&state, callback, &result);
	
	// Make sure unhandled unsuccessful returns get caught
	char line2[] = "[1,\"return\",1,[1,2,3]]";
	TASSERT(shet_process_line(&state, line2, strlen(line2)) == SHET_PROC_OK);
	TASSERT_INT_EQUAL(result.count, 1);
	TASSERT_JSON_EQUAL_TOK_STR(result.json, "[1,2,3]");
	
	char line3[] = "[1,\"return\",0,[3,2,1]]";
	TASSERT(shet_process_line(&state, line3, strlen(line3)) == SHET_PROC_OK);
	TASSERT_INT_EQUAL(result.count, 1);
	
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
	shet_deferred_t make_deferreds[num];
	// Deferreds for the live part of the element (e.g. event, call, set, get).
	shet_deferred_t deferreds[num];
	// Event objects for any events
	shet_event_t events[num];
	
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
	void make(shet_state_t *state, shet_json_t json, void *user_data) {
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
		TASSERT(shet_process_line(&state, msg, strlen(msg)) == SHET_PROC_OK);
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
		TASSERT(shet_process_line(&state, msg, strlen(msg)) == SHET_PROC_OK);
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
	shet_deferred_t deferred;
	callback_result_t result;
	result.count = 0;
	shet_ping(&state, "[1,2,3,{1:2,3:4}]",
	          &deferred, callback, NULL, &result);
	TASSERT_INT_EQUAL(transmit_count, 2);
	TASSERT_JSON_EQUAL_STR_STR(transmit_last_data, "[1,\"ping\",[1,2,3,{1:2,3:4}]]");
	
	// Send response
	char response1[] = "[1,\"return\",0,[1,2,3,{1:2,3:4}]]";
	TASSERT(shet_process_line(&state, response1, strlen(response1)) ==
	SHET_PROC_OK);
	TASSERT_INT_EQUAL(result.count, 1);
	TASSERT_JSON_EQUAL_TOK_STR(result.json, "[1,2,3,{1:2,3:4}]");
	
	// Send a second ping
	shet_ping(&state, "[3,2,1]",
	          &deferred, callback, NULL, &result);
	TASSERT_INT_EQUAL(transmit_count, 3);
	TASSERT_JSON_EQUAL_STR_STR(transmit_last_data, "[2,\"ping\",[3,2,1]]");
	
	// Cancel the response
	shet_cancel_deferred(&state, &deferred);
	
	// Check that the response now doesn't trigger a callback
	char response2[] = "[2,\"return\",0,[3,2,1]]";
	TASSERT(shet_process_line(&state, response2, strlen(response2)) ==
	SHET_PROC_OK);
	TASSERT_INT_EQUAL(result.count, 1);
	
	return true;
}


bool test_return(void) {
	RESET_TRANSMIT_CB();
	shet_state_t state;
	shet_state_init(&state, "\"tester\"", transmit_cb, NULL);
	
	// A function which immediately returns nothing
	void return_null(shet_state_t *state, shet_json_t json, void *user_data) {
		shet_return(state, 0, NULL);
	}
	
	// A function which immediately returns a value
	void return_value(shet_state_t *state, shet_json_t json, void *user_data) {
		shet_return(state, 0, "[1,2,3]");
	}
	
	// A function which doesn't respond but simply copies down the ID
	char return_id[100];
	void return_later(shet_state_t *state, shet_json_t json, void *user_data) {
		strcpy(return_id, shet_get_return_id(state));
	}
	
	shet_deferred_t deferred;
	
	// Test an action which returns nothing (also tests objects as IDs)
	shet_make_action(&state, "/test/action",
	                 &deferred, return_null, NULL,
	                 NULL, NULL, NULL, NULL);
	char line1[] = "[{\"whacky\":[\"id\",2,3]}, \"docall\", \"/test/action\", null]";
	TASSERT_INT_EQUAL(shet_process_line(&state, line1, strlen(line1)), SHET_PROC_OK);
	TASSERT_INT_EQUAL(transmit_count, 3);
	TASSERT_JSON_EQUAL_STR_STR(transmit_last_data, "[{\"whacky\":[\"id\",2,3]}, \"return\", 0, null]");
	shet_remove_action(&state, "/test/action", NULL, NULL, NULL, NULL);
	
	// Test an action which returns some value (also tests arrays as IDs)
	shet_make_action(&state, "/test/action",
	                 &deferred, return_value, NULL,
	                 NULL, NULL, NULL, NULL);
	char line2[] = "[[\"volume\",11], \"docall\", \"/test/action\", null]";
	TASSERT(shet_process_line(&state, line2, strlen(line2)) == SHET_PROC_OK);
	TASSERT_INT_EQUAL(transmit_count, 6);
	TASSERT_JSON_EQUAL_STR_STR(transmit_last_data, "[[\"volume\":11], \"return\", 0, [1,2,3]]");
	shet_remove_action(&state, "/test/action", NULL, NULL, NULL, NULL);
	
	// Test an action which doesn't return immediately (also tests strings as IDs)
	shet_make_action(&state, "/test/action",
	                 &deferred, return_later, NULL,
	                 NULL, NULL, NULL, NULL);
	char line3[] = "[\"eye-dee\", \"docall\", \"/test/action\", null]";
	TASSERT(shet_process_line(&state, line3, strlen(line3)) == SHET_PROC_OK);
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
	TASSERT(shet_process_line(&state, line4, strlen(line4)) == SHET_PROC_OK);
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
	
	shet_deferred_t deferred1;
	shet_deferred_t deferred2;
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
	TASSERT(shet_process_line(&state, line1, strlen(line1)) == SHET_PROC_OK);
	TASSERT_INT_EQUAL(result1.count, 1);
	TASSERT_JSON_EQUAL_TOK_STR(result1.json, "[]");
	TASSERT_INT_EQUAL(result2.count, 0);
	TASSERT_INT_EQUAL(transmit_count, 4);
	TASSERT_JSON_EQUAL_STR_STR(transmit_last_data, "[0,\"return\",0,[]]");
	
	char line2[] = "[1,\"docall\",\"/test/action2\"]";
	TASSERT(shet_process_line(&state, line2, strlen(line2)) == SHET_PROC_OK);
	TASSERT_INT_EQUAL(result1.count, 1);
	TASSERT_INT_EQUAL(result2.count, 1);
	TASSERT_JSON_EQUAL_TOK_STR(result2.json, "[]");
	TASSERT_INT_EQUAL(transmit_count, 5);
	TASSERT_JSON_EQUAL_STR_STR(transmit_last_data, "[1,\"return\",0,[]]");
	
	// Make sure actions can be removed
	shet_remove_action(&state, "/test/action2", NULL, NULL, NULL, NULL);
	TASSERT_INT_EQUAL(transmit_count, 6);
	TASSERT_JSON_EQUAL_STR_STR(transmit_last_data, "[3,\"rmaction\",\"/test/action2\"]");
	char line3[] = "[2,\"docall\",\"/test/action2\"]";
	TASSERT(shet_process_line(&state, line3, strlen(line3)) == SHET_PROC_OK);
	TASSERT_INT_EQUAL(result1.count, 1);
	TASSERT_INT_EQUAL(result2.count, 1);
	TASSERT_INT_EQUAL(transmit_count, 7);
	TASSERT_JSON_EQUAL_STR_STR(transmit_last_data, "[2,\"return\",1,\"No callback handler registered!\"]");
	
	// Call with a single string argument
	char line4[] = "[3,\"docall\",\"/test/action1\", \"just me\"]";
	TASSERT(shet_process_line(&state, line4, strlen(line4)) == SHET_PROC_OK);
	TASSERT_INT_EQUAL(result1.count, 2);
	TASSERT_JSON_EQUAL_TOK_STR(result1.json, "[\"just me\"]");
	TASSERT_INT_EQUAL(transmit_count, 8);
	TASSERT_JSON_EQUAL_STR_STR(transmit_last_data, "[3,\"return\",0,[\"just me\"]]");
	
	// Call with a single non-string primitive argument
	char line5[] = "[4,\"docall\",\"/test/action1\", true]";
	TASSERT(shet_process_line(&state, line5, strlen(line5)) == SHET_PROC_OK);
	TASSERT_INT_EQUAL(result1.count, 3);
	TASSERT_JSON_EQUAL_TOK_STR(result1.json, "[true]");
	TASSERT_INT_EQUAL(transmit_count, 9);
	TASSERT_JSON_EQUAL_STR_STR(transmit_last_data, "[4,\"return\",0,[true]]");
	
	// Call with a single compound argument
	char line6[] = "[5,\"docall\",\"/test/action1\", [1,2,3]]";
	TASSERT(shet_process_line(&state, line6, strlen(line6)) == SHET_PROC_OK);
	TASSERT_INT_EQUAL(result1.count, 4);
	TASSERT_JSON_EQUAL_TOK_STR(result1.json, "[[1,2,3]]");
	TASSERT_INT_EQUAL(transmit_count, 10);
	TASSERT_JSON_EQUAL_STR_STR(transmit_last_data, "[5,\"return\",0,[[1,2,3]]]");
	
	// Call with multiple arguments
	char line7[] = "[6,\"docall\",\"/test/action1\", 1,2,3]";
	TASSERT(shet_process_line(&state, line7, strlen(line7)) == SHET_PROC_OK);
	TASSERT_INT_EQUAL(result1.count, 5);
	TASSERT_JSON_EQUAL_TOK_STR(result1.json, "[1,2,3]");
	TASSERT_INT_EQUAL(transmit_count, 11);
	TASSERT_JSON_EQUAL_STR_STR(transmit_last_data, "[6,\"return\",0,[1,2,3]]");
	
	// Call an imaginary action
	char line8[] = "[7,\"docall\",\"/test/imaginary\", 1,2,3]";
	TASSERT(shet_process_line(&state, line8, strlen(line8)) == SHET_PROC_OK);
	TASSERT_INT_EQUAL(result1.count, 5);
	TASSERT_INT_EQUAL(transmit_count, 12);
	TASSERT_JSON_EQUAL_STR_STR(transmit_last_data, "[7,\"return\",1,\"No callback handler registered!\"]");
	
	return true;
}


bool test_shet_call_action(void) {
	RESET_TRANSMIT_CB();
	shet_state_t state;
	shet_state_init(&state, "\"tester\"", transmit_cb, NULL);
	
	// Test a call with no argument
	shet_deferred_t deferred;
	callback_result_t result;
	result.count = 0;
	shet_call_action(&state, "/test/action", NULL,
	                 &deferred, callback, NULL, &result);
	TASSERT_INT_EQUAL(transmit_count, 2);
	TASSERT_JSON_EQUAL_STR_STR(transmit_last_data, "[1,\"call\",\"/test/action\"]");
	
	// Test the return
	char line1[] = "[1,\"return\",0,null]";
	TASSERT(shet_process_line(&state, line1, strlen(line1)) == SHET_PROC_OK);
	TASSERT_INT_EQUAL(result.count, 1);
	
	// Test a call with an argument
	shet_call_action(&state, "/test/action", "\"magic\"",
	                 &deferred, callback, NULL, &result);
	TASSERT_INT_EQUAL(transmit_count, 3);
	TASSERT_JSON_EQUAL_STR_STR(transmit_last_data, "[2,\"call\",\"/test/action\", \"magic\"]");
	
	// Test the return
	char line2[] = "[2,\"return\",0,null]";
	TASSERT(shet_process_line(&state, line2, strlen(line2)) == SHET_PROC_OK);
	TASSERT_INT_EQUAL(result.count, 2);
	
	// Test a call with a failed response
	shet_call_action(&state, "/test/action", NULL,
	                 &deferred, NULL, callback, &result);
	TASSERT_INT_EQUAL(transmit_count, 4);
	TASSERT_JSON_EQUAL_STR_STR(transmit_last_data, "[3,\"call\",\"/test/action\"]");
	
	// Test the return with an error
	char line3[] = "[3,\"return\",1,\"fail\"]";
	TASSERT(shet_process_line(&state, line3, strlen(line3)) == SHET_PROC_OK);
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
	
	shet_deferred_t deferred1;
	shet_deferred_t deferred2;
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
	TASSERT(shet_process_line(&state, line1, strlen(line1)) == SHET_PROC_OK);
	TASSERT_INT_EQUAL(result1.count, 1);
	TASSERT_JSON_EQUAL_TOK_STR(result1.json, "[123]");
	TASSERT_INT_EQUAL(result2.count, 0);
	TASSERT_INT_EQUAL(transmit_count, 4);
	TASSERT_JSON_EQUAL_STR_STR(transmit_last_data, "[0,\"return\",0,null]");
	
	char line2[] = "[1,\"setprop\",\"/test/prop2\",321]";
	TASSERT(shet_process_line(&state, line2, strlen(line2)) == SHET_PROC_OK);
	TASSERT_INT_EQUAL(result1.count, 1);
	TASSERT_INT_EQUAL(result2.count, 1);
	TASSERT_JSON_EQUAL_TOK_STR(result2.json, "[321]");
	TASSERT_INT_EQUAL(transmit_count, 5);
	TASSERT_JSON_EQUAL_STR_STR(transmit_last_data, "[1,\"return\",0,null]");
	
	// Make sure they can be got independently
	char line3[] = "[0,\"getprop\",\"/test/prop1\"]";
	TASSERT(shet_process_line(&state, line3, strlen(line3)) == SHET_PROC_OK);
	TASSERT_INT_EQUAL(result1.count, 2);
	TASSERT_JSON_EQUAL_TOK_STR(result1.json, "[]");
	TASSERT_INT_EQUAL(result2.count, 1);
	TASSERT_INT_EQUAL(transmit_count, 6);
	TASSERT_JSON_EQUAL_STR_STR(transmit_last_data, "[0,\"return\",0,[1,2,3]]");
	
	char line4[] = "[1,\"getprop\",\"/test/prop2\"]";
	TASSERT(shet_process_line(&state, line4, strlen(line4)) == SHET_PROC_OK);
	TASSERT_INT_EQUAL(result1.count, 2);
	TASSERT_INT_EQUAL(result2.count, 2);
	TASSERT_JSON_EQUAL_TOK_STR(result2.json, "[]");
	TASSERT_INT_EQUAL(transmit_count, 7);
	TASSERT_JSON_EQUAL_STR_STR(transmit_last_data, "[1,\"return\",0,[3,2,1]]");
	
	// Make sure properties can be removed independently
	shet_remove_prop(&state, "/test/prop2", NULL, NULL, NULL, NULL);
	TASSERT_INT_EQUAL(transmit_count, 8);
	TASSERT_JSON_EQUAL_STR_STR(transmit_last_data, "[3,\"rmprop\",\"/test/prop2\"]");
	
	// Prop 1 should remain
	char line5[] = "[0,\"getprop\",\"/test/prop1\"]";
	TASSERT(shet_process_line(&state, line5, strlen(line5)) == SHET_PROC_OK);
	TASSERT_INT_EQUAL(result1.count, 3);
	TASSERT_JSON_EQUAL_TOK_STR(result1.json, "[]");
	TASSERT_INT_EQUAL(result2.count, 2);
	TASSERT_INT_EQUAL(transmit_count, 9);
	TASSERT_JSON_EQUAL_STR_STR(transmit_last_data, "[0,\"return\",0,[1,2,3]]");
	
	// Prop 2 should nolonger exist
	char line6[] = "[1,\"getprop\",\"/test/prop2\"]";
	TASSERT(shet_process_line(&state, line6, strlen(line6)) == SHET_PROC_OK);
	TASSERT_INT_EQUAL(result1.count, 3);
	TASSERT_INT_EQUAL(result2.count, 2);
	TASSERT_INT_EQUAL(transmit_count, 10);
	TASSERT_JSON_EQUAL_STR_STR(transmit_last_data, "[1,\"return\",1,\"No callback handler registered!\"]");
	
	return true;
}


bool test_shet_set_prop_and_shet_get_prop(void) {
	RESET_TRANSMIT_CB();
	shet_state_t state;
	shet_state_init(&state, "\"tester\"", transmit_cb, NULL);
	
	shet_deferred_t deferred;
	callback_result_t result;
	result.count = 0;
	
	// Test get
	shet_get_prop(&state, "/test/get",
	              &deferred, callback, NULL, &result);
	TASSERT_INT_EQUAL(transmit_count, 2);
	TASSERT_JSON_EQUAL_STR_STR(transmit_last_data, "[1,\"get\",\"/test/get\"]");
	
	// Test the return
	char line1[] = "[1,\"return\",0,[1,2,3]]";
	TASSERT(shet_process_line(&state, line1, strlen(line1)) == SHET_PROC_OK);
	TASSERT_INT_EQUAL(result.count, 1);
	TASSERT_JSON_EQUAL_TOK_STR(result.json, "[1,2,3]");
	
	// Test set
	shet_set_prop(&state, "/test/set", "[3,2,1]",
	              &deferred, callback, NULL, &result);
	TASSERT_INT_EQUAL(transmit_count, 3);
	TASSERT_JSON_EQUAL_STR_STR(transmit_last_data, "[2,\"set\",\"/test/set\", [3,2,1]]");
	
	// Test the return
	char line2[] = "[2,\"return\",0,null]";
	TASSERT(shet_process_line(&state, line2, strlen(line2)) == SHET_PROC_OK);
	TASSERT_INT_EQUAL(result.count, 2);
	TASSERT_JSON_EQUAL_TOK_STR(result.json, "null");
	
	// Test that error conditions work
	shet_set_prop(&state, "/test/set", "[9,9,9]",
	              &deferred, NULL, callback, &result);
	TASSERT_INT_EQUAL(transmit_count, 4);
	TASSERT_JSON_EQUAL_STR_STR(transmit_last_data, "[3,\"set\",\"/test/set\", [9,9,9]]");
	char line3[] = "[3,\"return\",1,\"fail\"]";
	TASSERT(shet_process_line(&state, line3, strlen(line3)) == SHET_PROC_OK);
	TASSERT_INT_EQUAL(result.count, 3);
	TASSERT_JSON_EQUAL_TOK_STR(result.json, "\"fail\"");
	
	return true;
}


////////////////////////////////////////////////////////////////////////////////
// Test events
////////////////////////////////////////////////////////////////////////////////

bool test_shet_make_event(void) {
	RESET_TRANSMIT_CB();
	shet_state_t state;
	shet_state_init(&state, "\"tester\"", transmit_cb, NULL);
	
	shet_deferred_t deferred;
	shet_event_t event;
	callback_result_t result;
	result.count = 0;
	
	// Make sure events can be created
	shet_make_event(&state, "/test/event", &event, NULL, NULL, NULL, NULL);
	TASSERT_INT_EQUAL(transmit_count, 2);
	TASSERT_JSON_EQUAL_STR_STR(transmit_last_data, "[1,\"mkevent\",\"/test/event\"]");
	
	// Make sure we can raise the event without an argument
	shet_raise_event(&state, "/test/event", NULL, NULL, NULL, NULL, NULL);
	TASSERT_INT_EQUAL(transmit_count, 3);
	TASSERT_JSON_EQUAL_STR_STR(transmit_last_data, "[2,\"raise\",\"/test/event\"]");
	
	// Make sure we can raise the event with a single argument
	shet_raise_event(&state, "/test/event", "123", NULL, NULL, NULL, NULL);
	TASSERT_INT_EQUAL(transmit_count, 4);
	TASSERT_JSON_EQUAL_STR_STR(transmit_last_data, "[3,\"raise\",\"/test/event\", 123]");
	
	// Make sure we can raise the event with a many argumenten
	shet_raise_event(&state, "/test/event", "1,2,3", NULL, NULL, NULL, NULL);
	TASSERT_INT_EQUAL(transmit_count, 5);
	TASSERT_JSON_EQUAL_STR_STR(transmit_last_data, "[4,\"raise\",\"/test/event\", 1,2,3]");
	
	// Make sure events can be deleted
	shet_remove_event(&state, "/test/event", NULL, NULL, NULL, NULL);
	TASSERT_INT_EQUAL(transmit_count, 6);
	TASSERT_JSON_EQUAL_STR_STR(transmit_last_data, "[5,\"rmevent\",\"/test/event\"]");
	
	return true;
}


bool test_shet_watch_event(void) {
	RESET_TRANSMIT_CB();
	shet_state_t state;
	shet_state_init(&state, "\"tester\"", transmit_cb, NULL);
	
	shet_deferred_t deferred1;
	shet_deferred_t deferred2;
	callback_result_t result1;
	callback_result_t result2;
	result1.count = 0;
	result2.count = 0;
	
	// Watch two independent events to ensure both receive notifications. Also
	// tests no-argument events.
	shet_watch_event(&state, "/test/event1",
	                 &deferred1, callback, NULL, NULL, &result1,
	                 NULL, NULL, NULL, NULL);
	TASSERT_INT_EQUAL(transmit_count, 2);
	TASSERT_JSON_EQUAL_STR_STR(transmit_last_data, "[1,\"watch\",\"/test/event1\"]");
	shet_watch_event(&state, "/test/event2",
	                 &deferred2, callback, NULL, NULL, &result2,
	                 NULL, NULL, NULL, NULL);
	TASSERT_INT_EQUAL(transmit_count, 3);
	TASSERT_JSON_EQUAL_STR_STR(transmit_last_data, "[2,\"watch\",\"/test/event2\"]");
	
	char line1[] = "[0,\"event\",\"/test/event1\"]";
	TASSERT(shet_process_line(&state, line1, strlen(line1)) == SHET_PROC_OK);
	TASSERT_INT_EQUAL(result1.count, 1);
	TASSERT_JSON_EQUAL_TOK_STR(result1.json, "[]");
	TASSERT_INT_EQUAL(result2.count, 0);
	
	char line2[] = "[1,\"event\",\"/test/event2\"]";
	TASSERT(shet_process_line(&state, line2, strlen(line2)) == SHET_PROC_OK);
	TASSERT_INT_EQUAL(result1.count, 1);
	TASSERT_INT_EQUAL(result2.count, 1);
	TASSERT_JSON_EQUAL_TOK_STR(result2.json, "[]");
	
	// Make sure that we can ignore just one
	shet_ignore_event(&state, "/test/event2", NULL, NULL, NULL, NULL);
	TASSERT_INT_EQUAL(transmit_count, 4);
	TASSERT_JSON_EQUAL_STR_STR(transmit_last_data, "[3,\"ignore\",\"/test/event2\"]");
	
	// Test that the remaining one still works (also tests events with arguments).
	char line3[] = "[2,\"event\",\"/test/event1\", 1,2,3]";
	TASSERT(shet_process_line(&state, line3, strlen(line3)) == SHET_PROC_OK);
	TASSERT_INT_EQUAL(result1.count, 2);
	TASSERT_JSON_EQUAL_TOK_STR(result1.json, "[1,2,3]");
	TASSERT_INT_EQUAL(result2.count, 1);
	
	// And that the ignored one doesn't...
	char line4[] = "[3,\"event\",\"/test/event2\"]";
	TASSERT(shet_process_line(&state, line4, strlen(line4)) == SHET_PROC_OK);
	TASSERT_INT_EQUAL(result1.count, 2);
	TASSERT_INT_EQUAL(result2.count, 1);
	
	// Test that we can receive eventcreated
	shet_watch_event(&state, "/test/event3",
	                 &deferred2, NULL, callback, NULL, &result2,
	                 NULL, NULL, NULL, NULL);
	
	char line5[] = "[5,\"eventcreated\",\"/test/event3\"]";
	TASSERT(shet_process_line(&state, line5, strlen(line5)) == SHET_PROC_OK);
	TASSERT_INT_EQUAL(result1.count, 2);
	TASSERT_INT_EQUAL(result2.count, 2);
	TASSERT_JSON_EQUAL_TOK_STR(result2.json, "[]");
	
	// And that we can receive eventdeleted
	shet_ignore_event(&state, "/test/event3", NULL, NULL, NULL, NULL);
	shet_watch_event(&state, "/test/event4",
	                 &deferred2, NULL, NULL, callback, &result2,
	                 NULL, NULL, NULL, NULL);
	
	char line6[] = "[6,\"eventdeleted\",\"/test/event4\"]";
	TASSERT(shet_process_line(&state, line6, strlen(line6)) == SHET_PROC_OK);
	TASSERT_INT_EQUAL(result1.count, 2);
	TASSERT_INT_EQUAL(result2.count, 3);
	TASSERT_JSON_EQUAL_TOK_STR(result2.json, "[]");
	
	return true;
}


////////////////////////////////////////////////////////////////////////////////
// Test JSON unpacking macros
////////////////////////////////////////////////////////////////////////////////


bool test_SHET_UNPACK_JSON(void) {
	jsmntok_t tokens[100];
	shet_json_t json;
	
	bool parse(char *str) {
		json.line = str;
		json.token = tokens;
		jsmn_parser p;
		jsmn_init(&p);
		jsmnerr_t e = jsmn_parse(&p, str, strlen(str),
	                           tokens, 100);
		return e >= 0;
	}
	
	// Variables which may be set during unpacking
	int i1 = 0;
	int i2 = 0;
	int i3 = 0;
	int i4 = 0;
	double f1 = 0.0;
	double f2 = 0.0;
	double f3 = 0.0;
	double f4 = 0.0;
	bool b1 = false;
	bool b2 = false;
	bool b3 = false;
	bool b4 = false;
	const char *s1 = NULL;
	const char *s2 = NULL;
	const char *s3 = NULL;
	const char *s4 = NULL;
	shet_json_t a1; a1.token = NULL;
	shet_json_t a2; a2.token = NULL;
	shet_json_t a3; a3.token = NULL;
	shet_json_t a4; a4.token = NULL;
	shet_json_t o1; a1.token = NULL;
	shet_json_t o2; a2.token = NULL;
	shet_json_t o3; a3.token = NULL;
	shet_json_t o4; a4.token = NULL;
	
	// Flag to clear on failure
	bool ok = true;
	
	// Test can unpack an int
	char line1[] = "123";
	TASSERT(parse(line1));
	SHET_UNPACK_JSON(json, ok=false;, i1, SHET_INT);
	TASSERT(ok);
	TASSERT_INT_EQUAL(i1, 123);
	
	// Test can unpack a float
	char line2[] = "1.5";
	TASSERT(parse(line2));
	SHET_UNPACK_JSON(json, ok=false;, f1, SHET_FLOAT);
	TASSERT(ok);
	TASSERT(f1 == 1.5);
	
	// Test can unpack a boolean
	char line3[] = "true";
	TASSERT(parse(line3));
	SHET_UNPACK_JSON(json, ok=false;, b1, SHET_BOOL);
	TASSERT(ok);
	TASSERT(b1 == true);
	
	// Test can unpack a null
	char line4[] = "null";
	TASSERT(parse(line4));
	SHET_UNPACK_JSON(json, ok=false;, doesnotexist, SHET_NULL);
	TASSERT(ok);
	
	// Test can unpack a string
	char line5[] = "\"hello\"";
	TASSERT(parse(line5));
	SHET_UNPACK_JSON(json, ok=false;, s1, SHET_STRING);
	TASSERT(ok);
	TASSERT(s1 != NULL);
	TASSERT(strcmp(s1, "hello") == 0);
	
	// Test can unpack a whole array
	char line6[] = "[1,2,3]";
	TASSERT(parse(line6));
	SHET_UNPACK_JSON(json, ok=false;, a1, SHET_ARRAY);
	TASSERT(ok);
	TASSERT_JSON_EQUAL_TOK_STR(a1, "[1,2,3]");
	
	// Test can unpack a whole array
	char line7[] = "{1:2, 3:4}";
	TASSERT(parse(line7));
	SHET_UNPACK_JSON(json, ok=false;, o1, SHET_OBJECT);
	TASSERT(ok);
	TASSERT_JSON_EQUAL_TOK_STR(o1, "{1:2, 3:4}");
	
	// Test unpacking a simple array
	char line8[] = "[1,2,3,4]";
	TASSERT(parse(line8));
	SHET_UNPACK_JSON(json, ok=false;,
		_, SHET_ARRAY_BEGIN,
			i1, SHET_INT,
			i2, SHET_INT,
			i3, SHET_INT,
			i4, SHET_INT,
		_, SHET_ARRAY_END,
	);
	TASSERT(ok);
	TASSERT_INT_EQUAL(i1, 1);
	TASSERT_INT_EQUAL(i2, 2);
	TASSERT_INT_EQUAL(i3, 3);
	TASSERT_INT_EQUAL(i4, 4);
	
	// Test unpacking a multi-type array
	char line9[] = "[1,2.5,true,null,\"abc\",[3,2,1],{true:false}]";
	TASSERT(parse(line9));
	SHET_UNPACK_JSON(json, ok=false;,
		_, SHET_ARRAY_BEGIN,
			i2, SHET_INT,
			f2, SHET_FLOAT,
			b2, SHET_BOOL,
			notexist, SHET_NULL,
			s2, SHET_STRING,
			a2, SHET_ARRAY,
			o2, SHET_OBJECT,
		_, SHET_ARRAY_END,
	);
	TASSERT(ok);
	TASSERT_INT_EQUAL(i2, 1);
	TASSERT(f2 == 2.5);
	TASSERT(b2 == true);
	TASSERT(s2 != NULL);
	TASSERT(strcmp(s2,"abc") == 0);
	TASSERT_JSON_EQUAL_TOK_STR(a2, "[3,2,1]");
	TASSERT_JSON_EQUAL_TOK_STR(o2, "{true:false}");
	
	// Test unpacking a nested array
	char line10[] = "[-1,[-2,-3],-4]";
	TASSERT(parse(line10));
	SHET_UNPACK_JSON(json, ok=false;,
		_, SHET_ARRAY_BEGIN,
			i1, SHET_INT,
			_, SHET_ARRAY_BEGIN,
				i2, SHET_INT,
				i3, SHET_INT,
			_, SHET_ARRAY_END,
			i4, SHET_INT,
		_, SHET_ARRAY_END,
	);
	TASSERT(ok);
	TASSERT_INT_EQUAL(i1, -1);
	TASSERT_INT_EQUAL(i2, -2);
	TASSERT_INT_EQUAL(i3, -3);
	TASSERT_INT_EQUAL(i4, -4);
	
	// Test wrong type 
	char line11[] = "false";
	TASSERT(parse(line11));
	SHET_UNPACK_JSON(json, ok=false;, i1, SHET_INT);
	TASSERT(!ok);
	ok = true;
	
	// Test too-many arguments for singleton
	char line12[] = "1";
	TASSERT(parse(line12));
	SHET_UNPACK_JSON(json, ok=false;, i1, SHET_INT, i2, SHET_INT);
	TASSERT(!ok);
	ok = true;
	
	// Test too-many arguments for array
	char line13[] = "[1]";
	TASSERT(parse(line13));
	SHET_UNPACK_JSON(json, ok=false;, i1, SHET_INT, i2, SHET_INT);
	TASSERT(!ok);
	ok = true;
	
	// Test too-few arguments for array
	char line14[] = "[1,2,3]";
	TASSERT(parse(line14));
	SHET_UNPACK_JSON(json, ok=false;, i1, SHET_INT, i2, SHET_INT);
	TASSERT(!ok);
	ok = true;
	
	// Test no arguments for array
	char line15[] = "1";
	TASSERT(parse(line15));
	SHET_UNPACK_JSON(json, ok=false;);
	TASSERT(!ok);
	ok = true;
	
	return true;
}


////////////////////////////////////////////////////////////////////////////////
// Test JSON packing macros
////////////////////////////////////////////////////////////////////////////////


bool test_SHET_PACK_JSON_LENGTH(void) {
	int i1 = INT_MAX;
	int i2 = INT_MIN;
	double f1 = 999999999.999999999;
	double f2 = -999999999.999999999;
	bool b1 = true;
	bool b2 = false;
	const char s1[] = "";
	const char s2[] = "hello, world!";
	const char a1[] = "[1,2,3]";
	const char a2[] = "[3,2,1]";
	const char o1[] = "{1:2,3:4}";
	const char o2[] = "{2:1,4:3}";
	
	// A large buffer to use for testing
	char buf[100];
	
	
	// An single char string should be large enough for a null.
	TASSERT_INT_EQUAL(SHET_PACK_JSON_LENGTH(), 1);
	
	// Enough for a big integer
	sprintf(buf, "%d", i2);
	TASSERT(SHET_PACK_JSON_LENGTH(i2, SHET_INT) >= strlen(buf) + 1);
	
	// Enough for a big float
	sprintf(buf, "%f", f2);
	TASSERT(SHET_PACK_JSON_LENGTH(f2, SHET_FLOAT) >= strlen(buf) + 1);
	
	// Enough for a bool
	TASSERT_INT_EQUAL(SHET_PACK_JSON_LENGTH(b1, SHET_BOOL), strlen(b1 ? "true" : "false") + 1);
	TASSERT_INT_EQUAL(SHET_PACK_JSON_LENGTH(b2, SHET_BOOL), strlen(b2 ? "true" : "false") + 1);
	
	// Enough for a null
	TASSERT_INT_EQUAL(SHET_PACK_JSON_LENGTH(_, SHET_NULL), strlen("null") + 1);
	
	// Enough for an empty string
	TASSERT_INT_EQUAL(SHET_PACK_JSON_LENGTH(s1, SHET_STRING), strlen(s1) + 2 + 1);
	
	// Enough for a non-empty string
	TASSERT_INT_EQUAL(SHET_PACK_JSON_LENGTH(s2, SHET_STRING), strlen(s2) + 2 + 1);
	
	// Enough for an array
	TASSERT_INT_EQUAL(SHET_PACK_JSON_LENGTH(a1, SHET_ARRAY), strlen(a1) + 1);
	
	// Enough for an object
	TASSERT_INT_EQUAL(SHET_PACK_JSON_LENGTH(o1, SHET_OBJECT), strlen(o1) + 1);
	
	// Enough for a series of objects (with commas between them)
	TASSERT_INT_EQUAL(SHET_PACK_JSON_LENGTH(s1, SHET_STRING, s2, SHET_STRING),
	                  1+ // "
	                  strlen(s1)+
	                  1+ // "
	                  1+ // ,
	                  1+ // "
	                  strlen(s2)+
	                  1+ // "
	                  1); // \0
	
	// Enough for an empty packed array
	TASSERT_INT_EQUAL(SHET_PACK_JSON_LENGTH(_, SHET_ARRAY_BEGIN, _, SHET_ARRAY_END), 3);
	
	// Enough for a series of nested objects (with commas between them)
	TASSERT_INT_EQUAL(SHET_PACK_JSON_LENGTH(
		_, SHET_ARRAY_BEGIN,
			_, SHET_ARRAY_BEGIN,
			_, SHET_ARRAY_BEGIN,
			_, SHET_ARRAY_END,
			_, SHET_ARRAY_END,
			s1, SHET_STRING,
			_, SHET_ARRAY_BEGIN,
				s2, SHET_STRING,
			_, SHET_ARRAY_END,
		_, SHET_ARRAY_END),
		1+ // [
		4+ // [[]]
		1+ // ,
		1+ // "
		strlen(s1)+
		1+ // "
		1+ // ,
		1+ // [
		1+ // "
		strlen(s2)+
		1+ // "
		1+ // ]
		1+ // ]
		1); // \0
	
	return true;
}

bool test_SHET_PACK_JSON(void) {
	char buf[100];
	
	// An empty value
	SHET_PACK_JSON(buf);
	TASSERT(strcmp(buf, "") == 0);
	
	// A single Integer
	SHET_PACK_JSON(buf, 123, SHET_INT);
	TASSERT_JSON_EQUAL_STR_STR(buf, "123");
	
	// A single float
	SHET_PACK_JSON(buf, 2.5, SHET_FLOAT);
	TASSERT_JSON_EQUAL_STR_STR(buf, "2.500000");
	
	// A single bool
	SHET_PACK_JSON(buf, true, SHET_BOOL);
	TASSERT_JSON_EQUAL_STR_STR(buf, "true");
	
	// A single null
	SHET_PACK_JSON(buf, _, SHET_NULL);
	TASSERT_JSON_EQUAL_STR_STR(buf, "null");
	
	// A single string
	SHET_PACK_JSON(buf, "my string", SHET_STRING);
	TASSERT_JSON_EQUAL_STR_STR(buf, "\"my string\"");
	
	// A single array
	SHET_PACK_JSON(buf, "[1,2,3]", SHET_ARRAY);
	TASSERT_JSON_EQUAL_STR_STR(buf, "[1,2,3]");
	
	// A single object
	SHET_PACK_JSON(buf, "{1:2,3:4}", SHET_OBJECT);
	TASSERT_JSON_EQUAL_STR_STR(buf, "{1:2,3:4}");
	
	// A packed empty array
	SHET_PACK_JSON(buf,
		_, SHET_ARRAY_BEGIN,
		_, SHET_ARRAY_END);
	TASSERT_JSON_EQUAL_STR_STR(buf, "[]");
	
	// A packed singleton array
	SHET_PACK_JSON(buf,
		_, SHET_ARRAY_BEGIN,
			1, SHET_INT,
		_, SHET_ARRAY_END);
	TASSERT_JSON_EQUAL_STR_STR(buf, "[1]");
	
	// A packed single-dimensional array
	SHET_PACK_JSON(buf,
		_, SHET_ARRAY_BEGIN,
			1, SHET_INT,
			2, SHET_INT,
			3, SHET_INT,
		_, SHET_ARRAY_END);
	TASSERT_JSON_EQUAL_STR_STR(buf, "[1,2,3]");
	
	// A nested array
	SHET_PACK_JSON(buf,
		_, SHET_ARRAY_BEGIN,
			_, SHET_ARRAY_BEGIN,
				_, SHET_ARRAY_BEGIN,
				_, SHET_ARRAY_END,
			_, SHET_ARRAY_END,
			1, SHET_INT,
			_, SHET_ARRAY_BEGIN,
				2, SHET_INT,
			_, SHET_ARRAY_END,
			3, SHET_INT,
		_, SHET_ARRAY_END);
	TASSERT_JSON_EQUAL_STR_STR(buf, "[[[]],1,[2],3]");
	
	return true;
}


////////////////////////////////////////////////////////////////////////////////
// Test EZSHET Watches
////////////////////////////////////////////////////////////////////////////////

// A watch callback which accepts no arguments
int ez_watch_count = 0;
void ez_watch(shet_state_t *shet) { ez_watch_count++; }
EZSHET_DECLARE_WATCH(ez_watch);
EZSHET_DECLARE_WATCH(ez_watch);
EZSHET_WATCH("/ez_watch", ez_watch)

// A watch callback which accepts an argument of every type
int ez_watch_args_count = 0;
int ez_watch_args_int = 0;
float ez_watch_args_float = 0.0;
bool ez_watch_args_bool = false;
const char *ez_watch_args_string = NULL;
int ez_watch_args_array_0 = 0;
int ez_watch_args_array_1 = 0;
shet_json_t ez_watch_args_array = {NULL, NULL};
shet_json_t ez_watch_args_object = {NULL, NULL};
void ez_watch_args(shet_state_t *shet, int i, double f, bool b, const char *s, int a0, int a1, shet_json_t a, shet_json_t o) {
	ez_watch_args_count++;
	ez_watch_args_int = i;
	ez_watch_args_float = f;
	ez_watch_args_bool = b;
	ez_watch_args_string = s;
	ez_watch_args_array_0 = a0;
	ez_watch_args_array_1 = a1;
	ez_watch_args_array = a;
	ez_watch_args_object = o;
}
// This time don't declare the watch and make sure it still works
EZSHET_WATCH("/ez_watch_args", ez_watch_args,
	SHET_INT,
	SHET_FLOAT,
	SHET_BOOL,
	SHET_NULL,
	SHET_STRING,
	SHET_ARRAY_BEGIN,
		SHET_INT,
		SHET_INT,
	SHET_ARRAY_END,
	SHET_ARRAY,
	SHET_OBJECT
);


bool test_EZSHET_WATCH(void) {
	shet_state_t state;
	RESET_TRANSMIT_CB();
	shet_state_init(&state, NULL, transmit_cb, NULL);
	
	// Test that registration works
	TASSERT(!EZSHET_IS_REGISTERED(ez_watch));
	EZSHET_ADD(&state, ez_watch);
	TASSERT_INT_EQUAL(transmit_count, 2);
	TASSERT_JSON_EQUAL_STR_STR(transmit_last_data, "[1,\"watch\",\"/ez_watch\"]");
	
	// Test that it does not appear if registration fails
	TASSERT(!EZSHET_IS_REGISTERED(ez_watch));
	char line1[] = "[1, \"return\", 1, \"fail\"]";
	TASSERT(shet_process_line(&state, line1, strlen(line1)) == SHET_PROC_OK);
	TASSERT(!EZSHET_IS_REGISTERED(ez_watch));
	
	// Try registering again
	EZSHET_ADD(&state, ez_watch);
	TASSERT_INT_EQUAL(transmit_count, 3);
	TASSERT_JSON_EQUAL_STR_STR(transmit_last_data, "[2,\"watch\",\"/ez_watch\"]");
	
	// Test that it does appear if registration succeeds
	TASSERT(!EZSHET_IS_REGISTERED(ez_watch));
	char line2[] = "[2, \"return\", 0, null]";
	TASSERT(shet_process_line(&state, line2, strlen(line2)) == SHET_PROC_OK);
	TASSERT(EZSHET_IS_REGISTERED(ez_watch));
	
	// Test that the event works when no arguments are passed
	TASSERT_INT_EQUAL(ez_watch_count, 0);
	char line3[] = "[\"id\", \"event\", \"/ez_watch\"]";
	TASSERT(shet_process_line(&state, line3, strlen(line3)) == SHET_PROC_OK);
	TASSERT_INT_EQUAL(ez_watch_count, 1);
	TASSERT_INT_EQUAL(transmit_count, 4);
	TASSERT_JSON_EQUAL_STR_STR(transmit_last_data, "[\"id\",\"return\",0,null]");
	
	// Test that the event fails when any arguments are passed
	TASSERT_INT_EQUAL(EZSHET_ERROR_COUNT(ez_watch), 0);
	char line4[] = "[\"eyedee\", \"event\", \"/ez_watch\", null]";
	TASSERT(shet_process_line(&state, line4, strlen(line4)) == SHET_PROC_OK);
	TASSERT_INT_EQUAL(EZSHET_ERROR_COUNT(ez_watch), 1);
	TASSERT_INT_EQUAL(ez_watch_count, 1);
	TASSERT_INT_EQUAL(transmit_count, 5);
	TASSERT_JSON_EQUAL_STR_STR(transmit_last_data, "[\"eyedee\",\"return\",1,\"Expected no value\"]");
	
	// Test that un-registration works
	EZSHET_REMOVE(&state, ez_watch);
	TASSERT_INT_EQUAL(transmit_count, 6);
	TASSERT_JSON_EQUAL_STR_STR(transmit_last_data, "[3,\"ignore\",\"/ez_watch\"]");
	TASSERT(!EZSHET_IS_REGISTERED(ez_watch));
	
	// Register the watch with arguments
	TASSERT(!EZSHET_IS_REGISTERED(ez_watch_args));
	EZSHET_ADD(&state, ez_watch_args);
	TASSERT_INT_EQUAL(transmit_count, 7);
	TASSERT_JSON_EQUAL_STR_STR(transmit_last_data, "[4,\"watch\",\"/ez_watch_args\"]");
	char line5[] = "[4, \"return\", 0, null]";
	TASSERT(shet_process_line(&state, line5, strlen(line5)) == SHET_PROC_OK);
	TASSERT(EZSHET_IS_REGISTERED(ez_watch_args));
	
	// Check that it can be called with appropriate arguments
	TASSERT_INT_EQUAL(ez_watch_args_count, 0);
	char line6[] = "[\"eyed\", \"event\", \"/ez_watch_args\", 1, 2.5, true, null, \"epic\", [13,37], [3,2,1], {1:2,3:4}]";
	TASSERT(shet_process_line(&state, line6, strlen(line6)) == SHET_PROC_OK);
	TASSERT_INT_EQUAL(ez_watch_args_count, 1);
	TASSERT_INT_EQUAL(transmit_count, 8);
	TASSERT_JSON_EQUAL_STR_STR(transmit_last_data, "[\"eyed\",\"return\",0,null]");
	TASSERT_INT_EQUAL(ez_watch_args_int, 1);
	TASSERT(ez_watch_args_float == 2.5);
	TASSERT(ez_watch_args_bool == true);
	TASSERT(strcmp(ez_watch_args_string, "epic") == 0);
	TASSERT_INT_EQUAL(ez_watch_args_array_0, 13);
	TASSERT_INT_EQUAL(ez_watch_args_array_1, 37);
	TASSERT_JSON_EQUAL_TOK_STR(ez_watch_args_array, "[3,2,1]");
	TASSERT_JSON_EQUAL_TOK_STR(ez_watch_args_object, "{1:2,3:4}");
	
	// Check that it can be called with inappropriate arguments
	TASSERT_INT_EQUAL(EZSHET_ERROR_COUNT(ez_watch_args), 0);
	char line7[] = "[\"idee\", \"event\", \"/ez_watch_args\", \"this ain't no good!\"]";
	TASSERT(shet_process_line(&state, line7, strlen(line7)) == SHET_PROC_OK);
	TASSERT_INT_EQUAL(ez_watch_args_count, 1);
	TASSERT_INT_EQUAL(EZSHET_ERROR_COUNT(ez_watch_args), 1);
	TASSERT_INT_EQUAL(transmit_count, 9);
	TASSERT_JSON_EQUAL_STR_STR(transmit_last_data, "[\"idee\",\"return\",1,\"Expected int, float, bool, null, string, [int, int], array, object\"]");
	
	return true;
}


////////////////////////////////////////////////////////////////////////////////
// Test EZSHET Events
////////////////////////////////////////////////////////////////////////////////

// Create an event with no arguments. Also test no-declaration is fine.
EZSHET_EVENT("/ez_event", ez_event);

// An event with arguments of every type. Ensure double-declaration is OK.
EZSHET_DECLARE_EVENT(ez_event_args,
	SHET_ARRAY_BEGIN,
		SHET_ARRAY_BEGIN,
			SHET_INT,
		SHET_ARRAY_END,
		SHET_FLOAT,
	SHET_ARRAY_END,
	SHET_NULL,
	SHET_BOOL,
	SHET_STRING,
	SHET_ARRAY,
	SHET_OBJECT);
EZSHET_DECLARE_EVENT(ez_event_args,
	SHET_ARRAY_BEGIN,
		SHET_ARRAY_BEGIN,
			SHET_INT,
		SHET_ARRAY_END,
		SHET_FLOAT,
	SHET_ARRAY_END,
	SHET_NULL,
	SHET_BOOL,
	SHET_STRING,
	SHET_ARRAY,
	SHET_OBJECT);
EZSHET_EVENT("/ez_event_args", ez_event_args,
	SHET_ARRAY_BEGIN,
		SHET_ARRAY_BEGIN,
			SHET_INT,
		SHET_ARRAY_END,
		SHET_FLOAT,
	SHET_ARRAY_END,
	SHET_NULL,
	SHET_BOOL,
	SHET_STRING,
	SHET_ARRAY,
	SHET_OBJECT);


bool test_EZSHET_EVENT(void) {
	shet_state_t state;
	RESET_TRANSMIT_CB();
	shet_state_init(&state, NULL, transmit_cb, NULL);
	
	// Test that events can't be sent yet
	TASSERT_INT_EQUAL(EZSHET_ERROR_COUNT(ez_event), 0);
	ez_event(&state);
	TASSERT_INT_EQUAL(EZSHET_ERROR_COUNT(ez_event), 1);
	TASSERT_INT_EQUAL(transmit_count, 1);
	
	// Test that registration sends a request
	TASSERT(!EZSHET_IS_REGISTERED(ez_event));
	EZSHET_ADD(&state, ez_event);
	TASSERT_INT_EQUAL(transmit_count, 2);
	TASSERT_JSON_EQUAL_STR_STR(transmit_last_data, "[1,\"mkevent\",\"/ez_event\"]");
	
	// Test that events still can't be sent
	TASSERT_INT_EQUAL(EZSHET_ERROR_COUNT(ez_event), 1);
	ez_event(&state);
	TASSERT_INT_EQUAL(EZSHET_ERROR_COUNT(ez_event), 2);
	TASSERT_INT_EQUAL(transmit_count, 2);
	
	// Test that registration can fail
	TASSERT(!EZSHET_IS_REGISTERED(ez_event));
	char line1[] = "[1, \"return\", 1, \"fail\"]";
	TASSERT(shet_process_line(&state, line1, strlen(line1)) == SHET_PROC_OK);
	TASSERT(!EZSHET_IS_REGISTERED(ez_event));
	
	// Test that events still can't be sent
	TASSERT_INT_EQUAL(EZSHET_ERROR_COUNT(ez_event), 2);
	ez_event(&state);
	TASSERT_INT_EQUAL(EZSHET_ERROR_COUNT(ez_event), 3);
	TASSERT_INT_EQUAL(transmit_count, 2);
	
	// Test that registration can work
	TASSERT(!EZSHET_IS_REGISTERED(ez_event));
	EZSHET_ADD(&state, ez_event);
	TASSERT_INT_EQUAL(transmit_count, 3);
	TASSERT_JSON_EQUAL_STR_STR(transmit_last_data, "[2,\"mkevent\",\"/ez_event\"]");
	TASSERT(!EZSHET_IS_REGISTERED(ez_event));
	char line2[] = "[2, \"return\", 0, null]";
	TASSERT(shet_process_line(&state, line2, strlen(line2)) == SHET_PROC_OK);
	TASSERT(EZSHET_IS_REGISTERED(ez_event));
	
	// Test that events can (finally) be sent
	TASSERT_INT_EQUAL(EZSHET_ERROR_COUNT(ez_event), 3);
	ez_event(&state);
	TASSERT_INT_EQUAL(EZSHET_ERROR_COUNT(ez_event), 3);
	TASSERT_INT_EQUAL(transmit_count, 4);
	TASSERT_JSON_EQUAL_STR_STR(transmit_last_data, "[3,\"raise\",\"/ez_event\"]");
	
	// ...and that it wasn't just a fluke!
	TASSERT_INT_EQUAL(EZSHET_ERROR_COUNT(ez_event), 3);
	ez_event(&state);
	TASSERT_INT_EQUAL(EZSHET_ERROR_COUNT(ez_event), 3);
	TASSERT_INT_EQUAL(transmit_count, 5);
	TASSERT_JSON_EQUAL_STR_STR(transmit_last_data, "[4,\"raise\",\"/ez_event\"]");
	
	// Make sure events can be removed
	TASSERT(EZSHET_IS_REGISTERED(ez_event));
	EZSHET_REMOVE(&state, ez_event);
	TASSERT_INT_EQUAL(transmit_count, 6);
	TASSERT_JSON_EQUAL_STR_STR(transmit_last_data, "[5,\"rmevent\",\"/ez_event\"]");
	TASSERT(!EZSHET_IS_REGISTERED(ez_event));
	
	// Test that the event doesn't work any more
	TASSERT_INT_EQUAL(EZSHET_ERROR_COUNT(ez_event), 3);
	ez_event(&state);
	TASSERT_INT_EQUAL(EZSHET_ERROR_COUNT(ez_event), 4);
	TASSERT_INT_EQUAL(transmit_count, 6);
	
	// Test that registration sends a request
	TASSERT(!EZSHET_IS_REGISTERED(ez_event_args));
	EZSHET_ADD(&state, ez_event_args);
	TASSERT_INT_EQUAL(transmit_count, 7);
	TASSERT_JSON_EQUAL_STR_STR(transmit_last_data, "[6,\"mkevent\",\"/ez_event_args\"]");
	TASSERT(!EZSHET_IS_REGISTERED(ez_event_args));
	char line3[] = "[6, \"return\", 0, null]";
	TASSERT(shet_process_line(&state, line3, strlen(line3)) == SHET_PROC_OK);
	TASSERT(EZSHET_IS_REGISTERED(ez_event_args));
	
	// Test that the event works
	TASSERT_INT_EQUAL(EZSHET_ERROR_COUNT(ez_event_args), 0);
	ez_event_args(&state, 1, 2.5, true, "hello, world", "[1,2,3]", "{1:2,3:4}");
	TASSERT_INT_EQUAL(EZSHET_ERROR_COUNT(ez_event_args), 0);
	TASSERT_INT_EQUAL(transmit_count, 8);
	TASSERT_JSON_EQUAL_STR_STR(transmit_last_data, "[7,\"raise\",\"/ez_event_args\","
		" [[1], 2.500000], null, true, \"hello, world\", [1,2,3], {1:2,3:4}]");
	
	
	return true;
}


////////////////////////////////////////////////////////////////////////////////
// Test EZSHET Actions
////////////////////////////////////////////////////////////////////////////////

// Create an action which takes nothing and returns nothing. Also test
// declaration works.
unsigned int ez_action_count = 0;
void ez_action(shet_state_t *shet) {
	ez_action_count ++;
}
EZSHET_DECLARE_ACTION(ez_action);
EZSHET_DECLARE_ACTION(ez_action);
EZSHET_ACTION("/ez_action", ez_action, SHET_NULL);

// Create an action which takes one of every type and returns a single value.
// Also tests when declarations aren't used.
unsigned int ez_action_args_count = 0;
int         ez_action_args_i = 0;
double      ez_action_args_f = 0.0;
bool        ez_action_args_b = false;
const char *ez_action_args_s = NULL;
shet_json_t ez_action_args_a;
shet_json_t ez_action_args_o;
int ez_action_args(shet_state_t *shet, int i, double f, bool b, const char *s, shet_json_t a, shet_json_t o) {
	ez_action_args_count ++;
	ez_action_args_i = i;
	ez_action_args_f = f;
	ez_action_args_b = b;
	ez_action_args_s = s;
	ez_action_args_a = a;
	ez_action_args_o = o;
	return 1337;
}
EZSHET_ACTION("/ez_action_args", ez_action_args,
	SHET_INT,
	SHET_ARRAY_BEGIN,
		SHET_INT,
		SHET_FLOAT,
	SHET_ARRAY_END,
	SHET_BOOL,
	SHET_NULL,
	SHET_STRING,
	SHET_ARRAY,
	SHET_OBJECT);

// Create an action which takes one of every type and returns those same values
// (possibly inverted, when possible).
unsigned int ez_action_ret_args_count = 0;
void ez_action_ret_args(shet_state_t *shet,
                       int *ri, double *rf, bool *rb, const char **rs, const char **ra, const char **ro,
                       int i, double f, bool b, const char *s, shet_json_t a, shet_json_t o) {
	ez_action_ret_args_count ++;
	*ri = -i;
	*rf = -f;
	*rb = !b;
	*rs = s;
	a.line[a.token->end] = '\0';
	*ra = a.line + a.token->start;
	o.line[o.token->end] = '\0';
	*ro = o.line + o.token->start;;
}
EZSHET_ACTION("/ez_action_ret_args", ez_action_ret_args,
	EZSHET_RETURN_ARGS_BEGIN,
		SHET_ARRAY_BEGIN,
			SHET_INT,
			SHET_FLOAT,
		SHET_ARRAY_END,
		SHET_BOOL,
		SHET_NULL,
		SHET_STRING,
		SHET_ARRAY,
		SHET_OBJECT,
	EZSHET_RETURN_ARGS_END,
	SHET_ARRAY_BEGIN,
		SHET_INT,
		SHET_FLOAT,
	SHET_ARRAY_END,
	SHET_BOOL,
	SHET_NULL,
	SHET_STRING,
	SHET_ARRAY,
	SHET_OBJECT);


bool test_EZSHET_ACTION(void) {
	shet_state_t state;
	RESET_TRANSMIT_CB();
	shet_state_init(&state, NULL, transmit_cb, NULL);
	
	// Test that registration works
	TASSERT(!EZSHET_IS_REGISTERED(ez_action));
	EZSHET_ADD(&state, ez_action);
	TASSERT_INT_EQUAL(transmit_count, 2);
	TASSERT_JSON_EQUAL_STR_STR(transmit_last_data, "[1,\"mkaction\",\"/ez_action\"]");
	
	// Test that it does not appear if registration fails
	TASSERT(!EZSHET_IS_REGISTERED(ez_action));
	char line1[] = "[1, \"return\", 1, \"fail\"]";
	TASSERT(shet_process_line(&state, line1, strlen(line1)) == SHET_PROC_OK);
	TASSERT(!EZSHET_IS_REGISTERED(ez_action));
	
	// Test that it does appear after successful registration
	EZSHET_ADD(&state, ez_action);
	TASSERT_INT_EQUAL(transmit_count, 3);
	TASSERT_JSON_EQUAL_STR_STR(transmit_last_data, "[2,\"mkaction\",\"/ez_action\"]");
	char line2[] = "[2, \"return\", 0, null]";
	TASSERT(shet_process_line(&state, line2, strlen(line2)) == SHET_PROC_OK);
	TASSERT(EZSHET_IS_REGISTERED(ez_action));
	
	// Test that the action now works
	char line3[] = "[0, \"docall\", \"/ez_action\"]";
	TASSERT_INT_EQUAL(ez_action_count, 0);
	TASSERT(shet_process_line(&state, line3, strlen(line3)) == SHET_PROC_OK);
	TASSERT_INT_EQUAL(transmit_count, 4);
	TASSERT_JSON_EQUAL_STR_STR(transmit_last_data, "[0,\"return\",0,null]");
	TASSERT_INT_EQUAL(ez_action_count, 1);
	TASSERT_INT_EQUAL(EZSHET_ERROR_COUNT(ez_action), 0);
	
	// Test that the action rejects anything other than a null argument
	char line4[] = "[0, \"docall\", \"/ez_action\", 123]";
	TASSERT(shet_process_line(&state, line4, strlen(line4)) == SHET_PROC_OK);
	TASSERT_INT_EQUAL(transmit_count, 5);
	TASSERT_JSON_EQUAL_STR_STR(transmit_last_data, "[0,\"return\",1,\"Expected no value\"]");
	TASSERT_INT_EQUAL(EZSHET_ERROR_COUNT(ez_action), 1);
	TASSERT_INT_EQUAL(ez_action_count, 1);
	
	// Unregister the action and make sure it stops working
	EZSHET_REMOVE(&state, ez_action);
	TASSERT_INT_EQUAL(transmit_count, 6);
	TASSERT_JSON_EQUAL_STR_STR(transmit_last_data, "[3,\"rmaction\",\"/ez_action\"]");
	
	// Register the action with many arguments and a return.
	EZSHET_ADD(&state, ez_action_args);
	TASSERT_INT_EQUAL(transmit_count, 7);
	TASSERT_JSON_EQUAL_STR_STR(transmit_last_data, "[4,\"mkaction\",\"/ez_action_args\"]");
	char line5[] = "[4, \"return\", 0, null]";
	TASSERT(shet_process_line(&state, line5, strlen(line5)) == SHET_PROC_OK);
	TASSERT(EZSHET_IS_REGISTERED(ez_action_args));
	
	// Test that the action works
	char line6[] = "[0, \"docall\", \"/ez_action_args\","
	               "[1,2.5],true,null,\"hello\",[1,2,3],{1:2,3:4}"
	               "]";
	TASSERT_INT_EQUAL(ez_action_args_count, 0);
	TASSERT(shet_process_line(&state, line6, strlen(line6)) == SHET_PROC_OK);
	TASSERT_INT_EQUAL(transmit_count, 8);
	TASSERT_JSON_EQUAL_STR_STR(transmit_last_data, "[0,\"return\",0,1337]");
	TASSERT_INT_EQUAL(ez_action_args_count, 1);
	TASSERT_INT_EQUAL(EZSHET_ERROR_COUNT(ez_action_args), 0);
	
	// Check the arguments made it
	TASSERT_INT_EQUAL(ez_action_args_i, 1);
	TASSERT(ez_action_args_f == 2.5);
	TASSERT(ez_action_args_b == true);
	TASSERT(strcmp(ez_action_args_s, "hello") == 0);
	TASSERT_JSON_EQUAL_TOK_STR(ez_action_args_a, "[1,2,3]");
	TASSERT_JSON_EQUAL_TOK_STR(ez_action_args_o, "{1:2,3:4}");
	
	// Test wrong arguments break it
	char line7[] = "[0, \"docall\", \"/ez_action_args\","
	               "1,2,3,\"magic unicorn fairies\""
	               "]";
	TASSERT(shet_process_line(&state, line7, strlen(line7)) == SHET_PROC_OK);
	TASSERT_INT_EQUAL(transmit_count, 9);
	TASSERT_JSON_EQUAL_STR_STR(transmit_last_data, "[0,\"return\",1,\""
	                           "Expected [int, float], bool, null, string, array, object"
	                           "\"]");
	TASSERT_INT_EQUAL(ez_action_args_count, 1);
	TASSERT_INT_EQUAL(EZSHET_ERROR_COUNT(ez_action_args), 1);
	
	// Get rid of it
	EZSHET_REMOVE(&state, ez_action_args);
	TASSERT_INT_EQUAL(transmit_count, 10);
	TASSERT_JSON_EQUAL_STR_STR(transmit_last_data, "[5,\"rmaction\",\"/ez_action_args\"]");
	
	// Register the action with many arguments which returns all its arguments.
	EZSHET_ADD(&state, ez_action_ret_args);
	TASSERT_INT_EQUAL(transmit_count, 11);
	TASSERT_JSON_EQUAL_STR_STR(transmit_last_data, "[6,\"mkaction\",\"/ez_action_ret_args\"]");
	char line8[] = "[6, \"return\", 0, null]";
	TASSERT(shet_process_line(&state, line8, strlen(line8)) == SHET_PROC_OK);
	TASSERT(EZSHET_IS_REGISTERED(ez_action_ret_args));
	
	// Test that the action works
	char line9[] = "[0, \"docall\", \"/ez_action_ret_args\","
	               "[1,2.5],true,null,\"hello\",[1,2,3],{1:2,3:4}"
	               "]";
	TASSERT_INT_EQUAL(ez_action_ret_args_count, 0);
	TASSERT(shet_process_line(&state, line9, strlen(line9)) == SHET_PROC_OK);
	TASSERT_INT_EQUAL(transmit_count, 12);
	TASSERT_JSON_EQUAL_STR_STR(transmit_last_data, "[0,\"return\",0,"
	                          "[-1,-2.500000],false,null,\"hello\",[1,2,3],{1:2,3:4}"
	                          "]");
	TASSERT_INT_EQUAL(ez_action_ret_args_count, 1);
	TASSERT_INT_EQUAL(EZSHET_ERROR_COUNT(ez_action_ret_args), 0);
	
	// Test the wrong arguments still break it
	char line10[] = "[0, \"docall\", \"/ez_action_ret_args\","
	               "1,2,3,\"magic gremlin fairies\""
	               "]";
	TASSERT(shet_process_line(&state, line10, strlen(line10)) == SHET_PROC_OK);
	TASSERT_INT_EQUAL(transmit_count, 13);
	TASSERT_JSON_EQUAL_STR_STR(transmit_last_data, "[0,\"return\",1,\""
	                           "Expected [int, float], bool, null, string, array, object"
	                           "\"]");
	TASSERT_INT_EQUAL(ez_action_ret_args_count, 1);
	TASSERT_INT_EQUAL(EZSHET_ERROR_COUNT(ez_action_ret_args), 1);
	
	return true;
}



////////////////////////////////////////////////////////////////////////////////
// World starts here
////////////////////////////////////////////////////////////////////////////////

int main(int argc, char *argv[]) {
	bool (*tests[])(void) = {
		test_SHET_PARSE_JSON_VALUE_INT,
		test_SHET_PARSE_JSON_VALUE_FLOAT,
		test_SHET_PARSE_JSON_VALUE_BOOL,
		test_SHET_PARSE_JSON_VALUE_STRING,
		test_SHET_JSON_IS_TYPE,
		test_deferred_utilities,
		test_shet_state_init,
		test_shet_process_line_errors,
		test_shet_set_error_callback,
		test_send_command,
		test_shet_register,
		test_shet_cancel_deferred_and_shet_ping,
		test_return,
		test_shet_make_action,
		test_shet_call_action,
		test_shet_make_prop,
		test_shet_set_prop_and_shet_get_prop,
		test_shet_make_event,
		test_shet_watch_event,
		test_SHET_UNPACK_JSON,
		test_SHET_PACK_JSON_LENGTH,
		test_SHET_PACK_JSON,
		test_EZSHET_WATCH,
		test_EZSHET_EVENT,
		test_EZSHET_ACTION,
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
