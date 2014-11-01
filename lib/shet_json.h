/**
 * Utility macros for working with JSON values tokenised by JSMN.
 *
 * These macros may be of use to end-users wishing to mechanise the dull parts
 * of extracting values from JSON tokenised by JSMN. They are heavily used by
 * the EZSHET library.
 *
 * Be warned that these macros and their implementations make use of fairly
 * advanced C pre-processor tricks so be careful using them and understand that
 * the error messages produced can be very obtuse. To make this file more
 * readable, macro implementations live in shet_json_internal.h.
 */


#ifndef SHET_JSON_H
#define SHET_JSON_H

#include <math.h>

#include "jsmn.h"
#include "cpp_magic.h"

#include "shet.h"

#include "shet_json_internal.h"

#ifdef __cplusplus
extern "C" {
#endif

////////////////////////////////////////////////////////////////////////////////
// Type Definitions
////////////////////////////////////////////////////////////////////////////////

/**
 * Basic JSON type names.
 *
 * These macros do not expand to any valid C and are only intended for
 * consumption by macros.
 */
#define SHET_INT    SHET_INT     // A JSON integer (e.g. 123)
#define SHET_FLOAT  SHET_FLOAT   // A JSON double-precision float (e.g. 2.5)
#define SHET_BOOL   SHET_BOOL    // A JSON boolean (e.g. true)
#define SHET_NULL   SHET_NULL    // A JSON null value (i.e. null)
#define SHET_STRING SHET_STRING  // A JSON string (e.g. "hello, world")
#define SHET_ARRAY  SHET_ARRAY   // A JSON array (e.g. [...])
#define SHET_OBJECT SHET_OBJECT  // A JSON object (e.g. {...})

/**
 * Unpacked JSON array type macros.
 *
 * Used when explicitly defining the type of a JSON arraay and its contents. For
 * example:
 *
 *   SHET_ARRAY_BEGIN,
 *     SHET_BOOL,
 *     SHET_ARRAY_BEGIN,
 *       SHET_INT,
 *       SHET_INT,
 *       SHET_INT,
 *     SHET_ARRAY_END,
 *     SHET_ARRAY,
 *   SHET_ARRAY_END
 *
 * The above defines a JSON type [bool, [int, int, int], [...]]. Notice that
 * nesting is permitted and that the non-unpacked SHET_ARRAY can also be
 * included to mean an array whose contents is not checked.
 *
 * Pairs of SHET_ARRAY_BEGIN and SHET_ARRAY_END should always be propperly
 * matched and correctly nested otherwise very strange behaviour will ensue.
 */
#define SHET_ARRAY_BEGIN SHET_ARRAY_BEGIN
#define SHET_ARRAY_END   SHET_ARRAY_END


////////////////////////////////////////////////////////////////////////////////
// JSON-type to C-type mapping.
////////////////////////////////////////////////////////////////////////////////

/**
 * Determine if a type has an C equivalent type. For example, "null" does not.
 * Expands to 1 if the type has a C equivalent and 0 otherwise. If 0,
 * SHET_GET_JSON_PARSED_TYPE should not be used.
 *
 * @param type The type (e.g. SHET_INT).
 * @returns 1 if a corresponding C type exists, 0 otherwise.
 */
#define SHET_HAS_JSON_PARSED_TYPE(type) \
	_SHET_HAS_JSON_PARSED_TYPE(type)


/**
 * Get the C type produced when parsing a given JSON type using
 * SHET_PARSE_JSON_VALUE.
 *
 * Note that NULL and unpacked array types are not defined since they do not
 * correspond to any C value.
 *
 * This macro is mostly intended for use by other macros; end users are advised
 * to just write the types by hand on account of their obviousness!
 *
 * @param type A type macro (e.g. SHET_INT).
 * @returns The equivalent C type.
 */
#define SHET_GET_JSON_PARSED_TYPE(type) \
	_SHET_GET_JSON_PARSED_TYPE(type)


/**
 * Get the C types which are expected when encoding a given JSON type using
 * SHET_ENCODE_JSON_VALUE.
 *
 * Note: JSON compound types (i.e. arrays and objects) cannot be encoded since
 * they have no obvious analogue in C. Instead they are expected to be
 * pre-encoded and just passed as a const char *.
 *
 * This macro is mostly intended for use by other macros; end users are advised
 * to just write the types by hand on account of their obviousness!
 *
 * @param type A type macro (e.g. SHET_INT).
 * @returns The equivalent C type.
 */
#define SHET_GET_JSON_ENCODED_TYPE(type) \
	_SHET_GET_JSON_ENCODED_TYPE(type)


////////////////////////////////////////////////////////////////////////////////
// JSON Type-checking
////////////////////////////////////////////////////////////////////////////////

/**
 * Check that the type of a shet_json_t matches a given type. Note that this
 * accepts only a single type (e.g. it cannot be used to check the contents of
 * a SHET_ARRAY_BEGIN/SHET_ARRAY_END pair).
 *
 * Limitation: SHET_INT and SHET_FLOAT are not checked for the absence or
 * presence of a decimal point. It this matters, the user should check for
 * themselves!
 *
 * @param json A shet_json_t referring to the tokenised JSON value to test.
 * @param type The type to check for (e.g. SHET_INT).
 * @returns A boolean expression which evaluates to true when the type is
 *          correct, false otherwise.
 */
#define SHET_JSON_IS_TYPE(json, type) \
	_SHET_JSON_IS_TYPE(json, type)


////////////////////////////////////////////////////////////////////////////////
// Tokenised JSON value parsing.
////////////////////////////////////////////////////////////////////////////////

/**
 * Generate a C expression which parses the given shet_json_t into a C type.
 *
 * The specified type must have a C type according to SHET_HAS_JSON_PARSED_TYPE and
 * the shet_json_t should have been type-checked, e.g. with SHET_JSON_IS_TYPE.
 *
 * Limitations:
 *
 * * This function clobbers the characters immediately surrounding the specified
 *   token in the underlying JSON string.
 * * If the value supplied is not part of a compound object (e.g. an array) this
 *   macro will not generate safe code!
 * * The shet_json_t for SHET_ARRAY and SHET_OBJECT types are simply passed-through.
 * * Unpacked types (e.g. SHET_ARRAY_BEGIN) are not supported. See
 *   SHET_UNPACK_JSON instead.
 *
 * Example usage:
 *
 *   shet_json_t my_json;
 *
 *   // ...
 *   // Parse ["hello, world!", 1,2,3] using JSMN and set my_json to the string
 *   // token.
 *   // ...
 *
 *   if (SHET_JSON_IS_TYPE(my_json, SHET_STRING)) {
 *     const char *str = SHET_PARSE_JSON_VALUE(my_json, SHET_STRING);
 *     printf("my_json contained '%s'\n", str);
 *   } else {
 *     printf("Typecheck failed!\n");
 *   }
 *
 * @param json A shet_json_t referring to the tokenised JSON value to be parsed.
 * @param type The type of the value (e.g. SHET_INT).
 * @returns A C expression which evaluates to the value of the JSON.
 */
#define SHET_PARSE_JSON_VALUE(json, type) \
	_SHET_PARSE_JSON_VALUE(json, type)


////////////////////////////////////////////////////////////////////////////////
// JSON value encoding.
////////////////////////////////////////////////////////////////////////////////

/**
 * Get an upper bound of the encoded length of the JSON string representing a C
 * value to encode. Intended for use with SHET_ENCODE_JSON_FORMAT and
 * SHET_ENCODE_JSON_VALUE.
 *
 * This macro is useful for defining a sensibly sized character array to encode
 * a C value into JSON.
 *
 * Example usage:
 *
 *   char json_string[SHET_ENCODED_JSON_LENGTH(my_string, SHET_STRING) + 1];
 *
 * Limitations:
 * * The string length for a float is arbitrary and not guaranteed to be
 *   sufficient. Printing very large floats may use an unpredictable amount of
 *   space and thus this macro should be considered unsafe in the general case
 *   when working with floats!
 * * Strings are assumed to be already appropriately escaped and
 *   null-terminated.
 * * Arrays and objects should be given as simple null-terminated strings
 *   containing valid JSON arrays and objects.
 *
 * @param var A C variable containing the value which is to be encoded. Should
 *            be of the type specified by SHET_GET_JSON_ENCODED_TYPE.
 * @param type The corresponding JSON type (e.g. SHET_INT).
 * @returns A C expression giving the size of an appropriate character array
 *          for storing the encoded value of the given JSON value, without a
 *          space for a NULL terminator. May over-allocate space.
 */
#define SHET_ENCODED_JSON_LENGTH(var, type) \
	_SHET_ENCODED_JSON_LENGTH(var, type)


/**
 * Get the printf format string literal for a given type (e.g. SHET_INT).
 * Designed for use with SHET_ENCODE_JSON_VALUE and SHET_ENCODED_JSON_LENGTH.
 *
 * See SHET_ENCODE_JSON_FORMAT for example usage.
 *
 * Limitations:
 * * The float formatting scheme is unspecified.
 *
 * @param type The type of the element (e.g. SHET_INT).
 * @returns A printf format string literal which can be used in
 *          conjunction with SHET_ENCODE_JSON_VALUE.
 */
#define SHET_ENCODE_JSON_FORMAT(type) \
	_SHET_ENCODE_JSON_FORMAT(type)


/**
 * Produce a printf argument suitable for use with a format string generated by
 * SHET_ENCODE_JSON_FORMAT. All values are proceeded by a comma.
 *
 * See also SHET_PACK_JSON for a higher-level wrapper around this macro.
 *
 * Example usage:
 *
 *   // Two values to encode as JSON.
 *   int my_int = 123;
 *   const char my_string[] = "Hello, world!";
 *
 *   // The buffer to write the JSON to. Note the extra character for the null
 *   // terminator.
 *   char json[ SHET_ENCODED_JSON_LENGTH(my_int, SHET_INT) +
 *              SHET_ENCODED_JSON_LENGTH(my_string, SHET_STRING) +
 *              1 ];
 *
 *   // Encode the variables as JSON. Note the lack of commas between the
 *   // macros: SHET_ENCODE_JSON_FORMAT produces string literals to be
 *   // concatenated and the SHET_ENCODE_JSON_VALUE macros insert their own
 *   // proceeding commas. Also note that a seperating comma and the surrounding
 *   // brackets are specified by hand.
 *   sprintf(json, "["
 *                 SHET_ENCODE_JSON_FORMAT(SHET_INT)
 *                 ", "
 *                 SHET_ENCODE_JSON_FORMAT(SHET_STRING)
 *                 "]"
 *                 SHET_ENCODE_JSON_VALUE(my_int, SHET_INT)
 *                 SHET_ENCODE_JSON_VALUE(my_string, SHET_STRING));
 *
 * The above example will write the following null-terminated string into the
 * 'json' variable.
 *
 *   [123, "Hello, world!"]
 *
 * Limitations:
 *
 * * JSON does not support NaN or Inf float values and so these are replaced
 *   with 0.0.
 * * This will not escape strings for JSON, e.g. all quotes, newlines and other
 *   special characters must already be escaped by the user.
 *
 * @param var A C variable containing the value to encode.
 * @param type The type of the value (e.g. SHET_INT).
 * @returns A printf value argument which can be used in conjunction with the
 *          format strings produced by SHET_ENCODE_JSON_FORMAT.
 */
#define SHET_ENCODE_JSON_VALUE(var, type) \
	_SHET_ENCODE_JSON_VALUE(var, type)


////////////////////////////////////////////////////////////////////////////////
// JSON value unpacking.
////////////////////////////////////////////////////////////////////////////////

/**
 * Generate C code which unpacks the JSON provided into variables, executing
 * on_error if types fail to match.
 *
 * Example usage:
 *
 *   shet_json_t my_json;
 *
 *   // ... Parse '[1,false,null,"hello"]' into my_json ...
 *
 *   bool error = false;
 *   int an_int;
 *   bool a_bool;
 *   const char *a_string;
 *   SHET_UNPACK_JSON(my_json, error = true;,
 *     _, SHET_ARRAY_BEGIN,
 *       an_int, SHET_INT,
 *       a_bool, SHET_BOOL,
 *       _, SHET_NULL,
 *       a_string, SHET_STRING,
 *     _, SHET_ARRAY_END
 *   );
 *
 *   if (!error)
 *     // ... Do something with values[] ...
 *
 * In the above example, C code is generated which verifies that the JSON
 * matches the format [int, bool, null, string] and places the parsed values
 * into the specified variables.
 *
 * Limitations:
 * * Cannot currently unpack objects.
 * * See limitations of SHET_PARSE_JSON_VALUE.
 *
 * @param json A shet_json_t pointing at the JSON value to unpack.
 * @param on_error C code to execute if a JSON type-check fails.
 * @param ... Alternating var_name and type (e.g. SHET_INT) corresponding with
 *            variables to extract. If a SHET_ARRAY_BEGIN/SHET_ARRAY_END pair is
 *            included, the contents of the array can be unpacked. The named
 *            variables should already be defined and will be assigned to by the
 *            code generated by this macro. Values may be written even if
 *            on_error is called.
 */
#define SHET_UNPACK_JSON(json, on_error, ...) \
	_SHET_UNPACK_JSON(json, on_error, __VA_ARGS__)


////////////////////////////////////////////////////////////////////////////////
// JSON value packing.
////////////////////////////////////////////////////////////////////////////////

/**
 * Generate C code which packs a number of C variables' values into a valid JSON
 * string.
 *
 * A higher-level, all-in-one wrapper around SHET_ENCODE_JSON_FORMAT and
 * SHET_ENCODE_JSON_VALUE macros. Notably automatically inserts commas into the
 * format string.
 *
 * See also SHET_PACK_JSON_LENGTH to determine the buffer size required for a
 * given set of values/types.
 *
 * Example usage:
 *
 *   // Two values to pack into a JSON array.
 *   int my_int = 123;
 *   const char my_string[] = "Hello, world!";
 *
 *   // The buffer to write the JSON to.
 *   char json[
 *     SHET_PACK_JSON_LENGTH(
 *       _, SHET_ARRAY_BEGIN,
 *         my_int, SHET_INT,
 *         my_string, SHET_STRING,
 *       _, SHET_ARRAY_END
 *     )
 *   ];
 *
 *   // Pack the variables as JSON.
 *   SHET_PACK_JSON_LENGTH(
 *     json,
 *     _, SHET_ARRAY_BEGIN,
 *       my_int, SHET_INT,
 *       my_string, SHET_STRING,
 *     _, SHET_ARRAY_END
 *   )
 *
 * The above example will write the following null-terminated string into the
 * 'json' variable.
 *
 *   [123,"Hello, world!"]
 *
 * Note that if the SHET_ARRAY_BEGIN and SHET_ARRAY_END parts were not present
 * in the example, the produced JSON string would be the same but simply lacking
 * the braces. This can be useful for generating subsections of a JSON array
 * string.
 *
 * Limitations:
 * * See limitations of SHET_ENCODE_JSON_FORMAT and SHET_ENCODE_JSON_VALUE.
 *
 * @param out A char * in which to write the JSON. Should be 
 * @param ... Alternating variable_names and types (e.g. SHET_INT) corresponding
 *            with variables to extract.
 */
#define SHET_PACK_JSON(out, ...) \
	_SHET_PACK_JSON(out, __VA_ARGS__)


/**
 * Work out the total length required for a JSON string produced by
 * SHET_PACK_JSON, including a null terminator. See SHET_PACK_JSON for
 * example usage.
 *
 * @param ... Alternating variable_names and types (e.g. SHET_INT) corresponding
 *            with variables to pack into JSON.
 * @returns A C expression which evaluates to the length of string required to
 *          store the produced JSON.
 */
#define SHET_PACK_JSON_LENGTH(...) \
	_SHET_PACK_JSON_LENGTH(__VA_ARGS__) \


////////////////////////////////////////////////////////////////////////////////
// Unpacked JSON value copying.
////////////////////////////////////////////////////////////////////////////////

/**
 * Deep-copy a variable unpacked by SHET_UNPACK_JSON into another variable
 * suitable for passing to SHET_PACK_JSON. Since the values are deep copied, the
 * lifetime of the destination variables is not bounded by those given as input.
 *
 * @param dst The destination variable name. Should be of the type given by
 *            SHET_GET_JSON_ENCODED_TYPE(type).
 * @param src The source variable name. Should be of the type given by
 *             SHET_GET_JSON_PARSED_TYPE(type).
 * @param type The type of the value (e.g. SHET_INT).
 * @returns A C expression which copies from src to dst.
 */
#define SHET_UNPACKED_VALUE_COPY(dst, src, type) \
	_SHET_UNPACKED_VALUE_COPY(dst, src, type)


////////////////////////////////////////////////////////////////////////////////
// JSON type sequence to string literal
////////////////////////////////////////////////////////////////////////////////

/**
 * Given a list of types (e.g. SHET_INT), produce a C string literal which
 * describes those types. Useful for generating error messages.
 *
 * For example:
 *
 *   SHET_JSON_TYPES_AS_STRING(
 *     SHET_INT,
 *     SHET_ARRAY_BEGIN,
 *       SHET_BOOL,
 *       SHET_NULL,
 *     SHET_ARRAY_END,
 *     SHET_ARRAY,
 *     SHET_OBJECT
 *   );
 *
 * Expands to a string literal:
 *
 *   int, [bool, null], array, object
 */
#define SHET_JSON_TYPES_AS_STRING(...) \
	_SHET_JSON_TYPES_AS_STRING(__VA_ARGS__)


/**
 * Given a single type (e.g. SHET_INT), expand to a C string literal naming that
 * type.
 */
#define SHET_JSON_TYPE_AS_STRING(type) \
	_SHET_JSON_TYPE_AS_STRING(type)


////////////////////////////////////////////////////////////////////////////////
// JSON token iteration
////////////////////////////////////////////////////////////////////////////////

/**
 * Get the next JSON value in an object/array.
 *
 * This is useful for skipping over (possibly nested) compound objects.
 *
 * Note: this function does not do any bounds checking!
 */
shet_json_t shet_next_token(shet_json_t json);

/**
 * Get the total number of tokens in the passed JSON, including those within
 * compound objects.
 *
 * Note: this function does not do any bounds checking!
 */
unsigned int shet_count_tokens(shet_json_t json);

#ifdef __cplusplus
}
#endif

#endif
