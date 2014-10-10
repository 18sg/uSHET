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
// Test suite assertions
////////////////////////////////////////////////////////////////////////////////

// Standard "assert true" assertion
#define TASSERT(x) do { if (!(x)) { \
	fprintf(stderr, "TASSERT Failed: %s:%s:%d: "#x"\n",\
	        __FILE__,__func__,__LINE__);\
	return false; \
} } while (0)


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
// World starts here
////////////////////////////////////////////////////////////////////////////////

int main(int argc, char *argv[]) {
	bool (*tests[])(void) = {
		test_assert_int,
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
