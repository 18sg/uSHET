/**
 * Utility macros for working with JSON values tokenised by JSMN.
 *
 * These macros may be of use to end-users wishing to mechanise the dull parts
 * of extracting values from JSON tokenised by JSMN.
 *
 * Be warned that these macros and their implementations make use of fairly
 * advanced C pre-processor tricks so be careful using them and understand that
 * the error messages produced can be very obtuse.
 */


#ifndef SHET_JSON_H
#define SHET_JSON_H

#include "jsmn.h"
#include "cpp_magic.h"

#include "shet.h"

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
	CAT(_SHET_GET_JSON_PARSED_TYPE_,type)()

#define _SHET_GET_JSON_PARSED_TYPE_SHET_INT()    int
#define _SHET_GET_JSON_PARSED_TYPE_SHET_FLOAT()  double
#define _SHET_GET_JSON_PARSED_TYPE_SHET_BOOL()   bool
#define _SHET_GET_JSON_PARSED_TYPE_SHET_STRING() const char *
#define _SHET_GET_JSON_PARSED_TYPE_SHET_ARRAY()  shet_json_t
#define _SHET_GET_JSON_PARSED_TYPE_SHET_OBJECT() shet_json_t


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
	CAT(_SHET_GET_JSON_ENCODED_TYPE_,type)()

#define _SHET_GET_JSON_ENCODED_TYPE_SHET_INT()    int
#define _SHET_GET_JSON_ENCODED_TYPE_SHET_FLOAT()  double
#define _SHET_GET_JSON_ENCODED_TYPE_SHET_BOOL()   bool
#define _SHET_GET_JSON_ENCODED_TYPE_SHET_STRING() const char *
#define _SHET_GET_JSON_ENCODED_TYPE_SHET_ARRAY()  const char *
#define _SHET_GET_JSON_ENCODED_TYPE_SHET_OBJECT() const char *


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
	CAT(_SHET_JSON_IS_TYPE_,type)((json))

#define _SHET_JSON_IS_TYPE_SHET_NUMBER(json) \
	( (json).token->type == JSMN_PRIMITIVE && \
	  ( (json).line[(json).token->start] == '+' || \
	    (json).line[(json).token->start] == '-' || \
	    ( (json).line[(json).token->start] >= '0' && \
	      (json).line[(json).token->start] <= '9' \
	    ) \
	  ) \
	)

#define _SHET_JSON_IS_TYPE_SHET_INT(json) \
	_SHET_JSON_IS_TYPE_SHET_NUMBER((json))
#define _SHET_JSON_IS_TYPE_SHET_FLOAT(json) \
	_SHET_JSON_IS_TYPE_SHET_NUMBER((json))

#define _SHET_JSON_IS_TYPE_SHET_BOOL(json) \
	( (json).token->type == JSMN_PRIMITIVE && \
	  ( (json).line[(json).token->start] == 't' || \
	    (json).line[(json).token->start] == 'f' \
	  ) \
	)

#define _SHET_JSON_IS_TYPE_SHET_NULL(json) \
	( (json).token->type == JSMN_PRIMITIVE && \
	  (json).line[(json).token->start] == 'n' \
	)

#define _SHET_JSON_IS_TYPE_SHET_STRING(json) \
	((json).token->type == JSMN_STRING)

#define _SHET_JSON_IS_TYPE_SHET_ARRAY(json) \
	((json).token->type == JSMN_ARRAY)
#define _SHET_JSON_IS_TYPE_SHET_ARRAY_BEGIN(json) \
	_SHET_JSON_IS_TYPE_SHET_ARRAY((json))
#define _SHET_JSON_IS_TYPE_SHET_ARRAY_END(json) \
	(true)

#define _SHET_JSON_IS_TYPE_SHET_OBJECT(json) \
	((json).token->type == JSMN_OBJECT)


////////////////////////////////////////////////////////////////////////////////
// JSON value parsing.
////////////////////////////////////////////////////////////////////////////////

/**
 * Determine if a type has a C equivalent type. For example, "null" does not.
 * Returns 1 if the type has a C equivalent and 0 otherwise. If 0 is returned,
 * SHET_GET_JSON_PARSED_TYPE should not be used.
 *
 * @param json A shet_json_t referring to the tokenised JSON value to be parsed.
 * @param type One of the type macros above specifying the type of the value.
 * @returns An expression which evaluates to the value.
 */
#define SHET_HAS_JSON_PARSED_TYPE(type) \
	IS_PROBE(CAT(_SHET_HAS_JSON_PARSED_TYPE_,type)())

#define _SHET_HAS_JSON_PARSED_TYPE_SHET_INT()    PROBE()
#define _SHET_HAS_JSON_PARSED_TYPE_SHET_FLOAT()  PROBE()
#define _SHET_HAS_JSON_PARSED_TYPE_SHET_BOOL()   PROBE()
#define _SHET_HAS_JSON_PARSED_TYPE_SHET_STRING() PROBE()
#define _SHET_HAS_JSON_PARSED_TYPE_SHET_ARRAY()  PROBE()
#define _SHET_HAS_JSON_PARSED_TYPE_SHET_OBJECT() PROBE()

/**
 * Generate a C expression which yields a C expression which parses the given
 * JSON value. The token should have previously been verified as having of the
 * correct type, e.g.  using SHET_JSON_IS_TYPE.
 *
 * Limitations:
 * * This function clobbers the underlying JSON string though only in such a way
 *   that the strings representing compound objects is corrupted. The atomic
 *   JSON values and the tokens will not be harmed.
 * * The SHET_INT and SHET_FLOAT are not safely parsed when the JSON
 *   string only contains that primitive. If the primitive is a member of an
 *   array or object, however, the numeric types are safe.
 *
 * @param json A shet_json_t referring to the tokenised JSON value to be parsed.
 * @param type One of the type macros above specifying the type of the value.
 * @returns An expression which evaluates to the value.
 */
#define SHET_PARSE_JSON_VALUE(json, type) \
	CAT(_SHET_PARSE_,type)((json))

// This is safe when the integer is part of an array or object because it will
// definitely be followed by one of ',', ']' or '}' which will terminate atoi.
#define _SHET_PARSE_SHET_INT(json) \
	(atoi((json).line + (json).token->start))

// This is safe when the integer is part of an array or object because it will
// definitely be followed by one of ',', ']' or '}' which will terminate atof.
#define _SHET_PARSE_SHET_FLOAT(json) \
	(atof((json).line + (json).token->start))

#define _SHET_PARSE_SHET_BOOL(json) \
	((bool)((json).line[(json).token->start] == 't'))

// Null terminate the string in the raw JSON (this is safe since the string will
// always be followed by a quote character we can safely NULL out).
static inline const char *_shet_parse_shet_string(shet_json_t json) {
	json.line[json.token->end] = '\0';
	return (const char *)(json.line + json.token->start);
}
#define _SHET_PARSE_SHET_STRING(json) \
	(_shet_parse_shet_string((json)))

#define _SHET_PARSE_SHET_ARRAY(json) \
	(json)

#define _SHET_PARSE_SHET_OBJECT(json) \
	(json)


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
 *     _, SHET_ARRAY_END,
 *   );
 *
 *   if (!error)
 *     // ... Do something with values[] ...
 *
 * In the above example, C code is generated which verifies that the JSON
 * matches the format [int, bool, null, string] and places the values into the
 * specified variables.
 *
 * Limitations:
 * * Cannot currently unpack objects.
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
	{ \
		/* Make a local copy of the token to allow it to be used as an iterator. */ \
		shet_json_t _json = (json);\
		/* The root does not have a parent */ \
		shet_json_t _parent; \
		_parent.token = NULL; \
		/* Number of tokens unpacked so far */ \
		size_t _num_unpacked = 0; \
		/* Error flag */ \
		bool _error = false; \
		/* Unpack everything */ \
		do { \
			MAP_PAIRS(_SHET_UNPACK_JSON_MAP, EMPTY, ##__VA_ARGS__); \
		} while (false); \
		/* Make sure everything was handled */ \
		if (_error || _json.token != shet_next_token((json)).token || _num_unpacked != 1) { \
			on_error; \
		} \
	}

// This function is mapped over pairs of type and name arguments and generates
// the appropriate code.
#define _SHET_UNPACK_JSON_MAP(name, type) \
	CAT(_SHET_UNPACK_JSON_, type)(name);

// Check that the current _json has not gone beyond the range of the current
// _parent or that it is of the wrong type.
#define _SHET_UNPACK_JSON_CHECK(type) \
	if ((!SHET_JSON_IS_TYPE(_json, type)) || \
	    (_parent.token != NULL && _num_unpacked >= _parent.token->size)) { \
		_error = true; \
		break; \
	}

#define _SHET_UNPACK_JSON_SHET_INT(name) \
	_SHET_UNPACK_JSON_CHECK(SHET_INT); \
	(name) = SHET_PARSE_JSON_VALUE(_json, SHET_INT); \
	_num_unpacked++; \
	_json.token++;


#define _SHET_UNPACK_JSON_SHET_FLOAT(name) \
	_SHET_UNPACK_JSON_CHECK(SHET_FLOAT); \
	(name) = SHET_PARSE_JSON_VALUE(_json, SHET_FLOAT); \
	_num_unpacked++; \
	_json.token++;


#define _SHET_UNPACK_JSON_SHET_BOOL(name) \
	_SHET_UNPACK_JSON_CHECK(SHET_BOOL); \
	(name) = SHET_PARSE_JSON_VALUE(_json, SHET_BOOL); \
	_num_unpacked++; \
	_json.token++;


#define _SHET_UNPACK_JSON_SHET_NULL(name) \
	_SHET_UNPACK_JSON_CHECK(SHET_NULL); \
	_num_unpacked++; \
	_json.token++;

#define _SHET_UNPACK_JSON_SHET_STRING(name) \
	_SHET_UNPACK_JSON_CHECK(SHET_STRING); \
	(name) = SHET_PARSE_JSON_VALUE(_json, SHET_STRING); \
	_num_unpacked++; \
	_json.token++;


#define _SHET_UNPACK_JSON_SHET_ARRAY(name) \
	_SHET_UNPACK_JSON_CHECK(SHET_ARRAY); \
	(name) = SHET_PARSE_JSON_VALUE(_json, SHET_ARRAY); \
	_num_unpacked++; \
	_json = shet_next_token(_json);


#define _SHET_UNPACK_JSON_SHET_OBJECT(name) \
	_SHET_UNPACK_JSON_CHECK(SHET_OBJECT); \
	(name) = SHET_PARSE_JSON_VALUE(_json, SHET_OBJECT); \
	_num_unpacked++; \
	_json = shet_next_token(_json);


#define _SHET_UNPACK_JSON_SHET_ARRAY_BEGIN(name) \
	_SHET_UNPACK_JSON_CHECK(SHET_ARRAY); \
	{ \
		size_t _num_unpacked = 0; \
		shet_json_t _parent = _json; \
		_json.token++;

#define _SHET_UNPACK_JSON_SHET_ARRAY_END(name) \
		if (_num_unpacked != _parent.token->size) { \
			_error = true; \
			break; \
		} \
	} \
	_num_unpacked++; \


////////////////////////////////////////////////////////////////////////////////
// JSON value encoding.
////////////////////////////////////////////////////////////////////////////////

/**
 * Get the format string for a given type (e.g. SHET_INT).
 *
 * Limitations:
 * * The float formatting scheme is unspecified. Printing very large floats may
 *   use an unpredictable amount of space and thus should be considered unsafe
 *   in the general case! Further special cases such as NaN which are not
 *   supported by JSON are not yet handled.
 * * This will not escape strings for JSON, e.g. all quotes, newlines and other
 *   special characters must be escaped by the user.
 *
 * @param type The type of the element (e.g. SHET_INT).
 * @returns A printf format string as a string literal which can be used in
 *          conjunction with SHET_ENCODE_JSON_VALUE.
 */
#define SHET_ENCODE_JSON_FORMAT(type) \
	CAT(_SHET_ENCODE_FORMAT_,type)()

#define _SHET_ENCODE_FORMAT_SHET_INT()         "%d"
#define _SHET_ENCODE_FORMAT_SHET_FLOAT()       "%f"
#define _SHET_ENCODE_FORMAT_SHET_BOOL()        "%s"
#define _SHET_ENCODE_FORMAT_SHET_NULL()        "null"
#define _SHET_ENCODE_FORMAT_SHET_STRING()      "\"%s\""
#define _SHET_ENCODE_FORMAT_SHET_ARRAY()       "%s"
#define _SHET_ENCODE_FORMAT_SHET_ARRAY_BEGIN() "["
#define _SHET_ENCODE_FORMAT_SHET_ARRAY_END()   "]"
#define _SHET_ENCODE_FORMAT_SHET_OBJECT()      "%s"

/**
 * Produce a value suitable for use with a format string generated by
 * SHET_ENCODE_JSON_FORMAT. All values are proceeded by a comma.
 *
 * * The float formatting scheme is unspecified. Printing very large floats may
 *   use an unpredictable amount of space and thus should be considered unsafe
 *   in the general case! Further special cases such as NaN which are not
 *   supported by JSON are not yet handled.
 * * This will not escape strings for JSON, e.g. all quotes, newlines and other
 *   special characters must be escaped by the user.
 *
 * @param var A C variable containing the value to encode.
 * @param type The type of the element (e.g. SHET_INT).
 * @returns A printf format string as a string literal which can be used in
 *          conjunction with SHET_ENCODE_JSON_VALUE.
 */
#define SHET_ENCODE_JSON_VALUE(var, type) \
	CAT(_SHET_ENCODE_,type)(var)

#define _SHET_ENCODE_SHET_INT(var)         , var
#define _SHET_ENCODE_SHET_FLOAT(var)       , var
#define _SHET_ENCODE_SHET_BOOL(var)        , ((var) ? "true" : "false")
#define _SHET_ENCODE_SHET_NULL(var)        /* Nothing */
#define _SHET_ENCODE_SHET_STRING(var)      , var
#define _SHET_ENCODE_SHET_ARRAY(var)       , var
#define _SHET_ENCODE_SHET_ARRAY_BEGIN(var) /* Nothing */
#define _SHET_ENCODE_SHET_ARRAY_END(var)   /* Nothing */
#define _SHET_ENCODE_SHET_OBJECT(var)      , var

/**
 * Get an upper bound of the encoded length of the JSON string representing a C
 * value to encode.
 *
 * Limitations:
 * * This macro does not provide facilities for generating JSON compound values
 *   (i.e. arrays and objects) - these should be supplied as ready-encoded,
 *   null-terminated JSON strings.
 * * The string length for a float is arbitrary and not guaranteed to be
 *   sufficient.
 * * This will not escape strings for JSON, e.g. all quotes, newlines and other
 *   special characters must be escaped by the user.
 *
 * @param var A C variable containing the value which is to be encoded.
 * @param type One of the type macros above specifying the type of the value.
 * @returns A C expression giving the size of an appropriate character array
 *          for storing the encoded value of the given JSON value, without a
 *          space for a NULL terminator. May over-allocate space.
 */
#define SHET_ENCODED_JSON_LENGTH(var, type) \
	CAT(_SHET_ENCODED_JSON_LENGTH_,type)((var))

// Large enough for INT_MIN on a 64-bit machine.
#define _SHET_ENCODED_JSON_LENGTH_SHET_INT(var) 20

// Large enough for an s9.9 decimal representation. An arbitrary choice.
#define _SHET_ENCODED_JSON_LENGTH_SHET_FLOAT(var) 20

#define _SHET_ENCODED_JSON_LENGTH_SHET_BOOL(var)        ((var) ? 4 : 5)
#define _SHET_ENCODED_JSON_LENGTH_SHET_NULL(var)        4
#define _SHET_ENCODED_JSON_LENGTH_SHET_STRING(var)      (strlen((var)) + 2)
#define _SHET_ENCODED_JSON_LENGTH_SHET_ARRAY(var)       (strlen((var)))
#define _SHET_ENCODED_JSON_LENGTH_SHET_ARRAY_BEGIN(var) 1
#define _SHET_ENCODED_JSON_LENGTH_SHET_ARRAY_END(var)   1
#define _SHET_ENCODED_JSON_LENGTH_SHET_OBJECT(var)      (strlen((var)))

////////////////////////////////////////////////////////////////////////////////
// JSON value packing.
////////////////////////////////////////////////////////////////////////////////

/**
 * Generate C code which packs the values provided into a valid JSON string.
 *
 * Limitations:
 * * Cannot currently unpack objects.
 *
 * @param out A char * in which to write the JSON. Should be 
 * @param ... Alternating var_name and type (e.g. SHET_INT) corresponding with
 *            variables to extract.
 */
#define SHET_PACK_JSON(out, ...) \
	sprintf( \
		out, \
		"" MAP_SLIDE(_SHET_PACK_JSON_FORMAT_OP, SHET_ENCODE_JSON_FORMAT, EMPTY,\
		             MAP_PAIRS(SECOND, COMMA, __VA_ARGS__)) \
		MAP_PAIRS(SHET_ENCODE_JSON_VALUE, EMPTY, __VA_ARGS__));

// Produces the cur_type's format string followed by a string "," if the next
// type is appropriate.
#define _SHET_PACK_JSON_FORMAT_OP(cur_type, next_type) \
	SHET_ENCODE_JSON_FORMAT(cur_type) \
	IF(SHET_JSON_IS_COMMA_BETWEEN(cur_type, next_type))(",")

/**
 * Work out the total length required for a JSON string produced by
 * SHET_PACK_JSON, including a null terminator.
 *
 * @param ... Alternating var_name and type (e.g. SHET_INT) corresponding with
 *            variables to pack.
 * @returns A C expression which evaluates to the length of string required to
 *          store the produced JSON.
 */
#define SHET_PACK_JSON_LENGTH(...) \
	( \
		/* Space for the values themselves. */ \
		MAP_PAIRS(SHET_ENCODED_JSON_LENGTH, PLUS, __VA_ARGS__) + \
		/* Space for intervening commas. */ \
		MAP_SLIDE(_SHET_PACK_JSON_LENGTH_OP, \
		          _SHET_PACK_JSON_LENGTH_LAST_OP, \
		          PLUS, MAP_PAIRS(SECOND, COMMA, __VA_ARGS__)) + \
		/* Null terminator. */ \
		1 \
	)

// Produce 1 if there will be a comma following cur_type, otherwise produces
// zero.
#define _SHET_PACK_JSON_LENGTH_OP(cur_type, next_type) \
	SHET_JSON_IS_COMMA_BETWEEN(cur_type, next_type)

// Always produces zero since the last value is never followed by a comma.
#define _SHET_PACK_JSON_LENGTH_LAST_OP(cur_type) \
	0


// Expands to 1 if a comma should be placed between the given types, 0 otherwise.
#define SHET_JSON_IS_COMMA_BETWEEN(first_type, second_type) \
	AND(NOT(IS_PROBE(CAT(_SHET_JSON_NO_COMMA_AFTER_, first_type)())), \
	    NOT(IS_PROBE(CAT(_SHET_JSON_NO_COMMA_BEFORE_, second_type)())))

#define _SHET_JSON_NO_COMMA_AFTER_SHET_ARRAY_BEGIN() PROBE()
#define _SHET_JSON_NO_COMMA_BEFORE_SHET_ARRAY_END() PROBE()


////////////////////////////////////////////////////////////////////////////////
// Unpacked JSON value copying.
////////////////////////////////////////////////////////////////////////////////

/**
 * Deep-copy a variable unpacked by SHET_UNPACK_JSON into another variable
 * suitable for passing to SHET_PACK_JSON. Since the values are deep copied, the
 * lifetime of the destination variables lasts beyond the scope of the SHET
 * callback function.
 *
 * @param dst The destination variable name. Should be of the type given by
 *            SHET_GET_JSON_ENCODED_TYPE(type).
 * @param src The source variable name. Should be of the type given by
 *             SHET_GET_JSON_PARSED_TYPE(type).
 * @param type The type of the value (e.g. SHET_INT).
 * @returns A C expression which copies from src to dst.
 */
#define SHET_UNPACKED_VALUE_COPY(dst, src, type) \
	IF(SHET_HAS_JSON_PARSED_TYPE(type))( \
		CAT(_SHET_UNPACKED_VALUE_COPY_,type)(dst, src))

#define _SHET_UNPACKED_VALUE_COPY_SHET_INT(dst, src) dst = src
#define _SHET_UNPACKED_VALUE_COPY_SHET_FLOAT(dst, src) dst = src
#define _SHET_UNPACKED_VALUE_COPY_SHET_BOOL(dst, src) dst = src
#define _SHET_UNPACKED_VALUE_COPY_SHET_STRING(dst, src) strcpy(dst, src)

#define _SHET_UNPACKED_VALUE_COPY_JSON(dst, src) \
	do { \
		memcpy(dst, src.line + src.token->start, \
		       (src.token->end - src.token->start) * sizeof(char)); \
		dst[src.token->end - src.token->start] = '\0'; \
	} while (0)

#define _SHET_UNPACKED_VALUE_COPY_SHET_ARRAY(dst, src) \
	_SHET_UNPACKED_VALUE_COPY_JSON(dst, src)

#define _SHET_UNPACKED_VALUE_COPY_SHET_OBJECT(dst, src) \
	_SHET_UNPACKED_VALUE_COPY_JSON(dst, src)

/**
 * Produce a value suitable for use with a format string generated by
 * SHET_ENCODE_JSON_FORMAT. All values are proceeded by a comma.
 *
 * * The float formatting scheme is unspecified. Printing very large floats may
 *   use an unpredictable amount of space and thus should be considered unsafe
 *   in the general case! Further special cases such as NaN which are not
 *   supported by JSON are not yet handled.
 * * This will not escape strings for JSON, e.g. all quotes, newlines and other
 *   special characters must be escaped by the user.
 *
 * @param var A C variable containing the value to encode.
 * @param type The type of the element (e.g. SHET_INT).
 * @returns A printf format string as a string literal which can be used in
 *          conjunction with SHET_ENCODE_JSON_VALUE.
 */
#define SHET_ENCODE_JSON_VALUE(var, type) \
	CAT(_SHET_ENCODE_,type)(var)

#define _SHET_ENCODE_SHET_INT(var)         , var
#define _SHET_ENCODE_SHET_FLOAT(var)       , var
#define _SHET_ENCODE_SHET_BOOL(var)        , ((var) ? "true" : "false")
#define _SHET_ENCODE_SHET_NULL(var)        /* Nothing */
#define _SHET_ENCODE_SHET_STRING(var)      , var
#define _SHET_ENCODE_SHET_ARRAY(var)       , var
#define _SHET_ENCODE_SHET_ARRAY_BEGIN(var) /* Nothing */
#define _SHET_ENCODE_SHET_ARRAY_END(var)   /* Nothing */
#define _SHET_ENCODE_SHET_OBJECT(var)      , var

/**
 * Get an upper bound of the encoded length of the JSON string representing a C
 * value to encode.
 *
 * Limitations:
 * * This macro does not provide facilities for generating JSON compound values
 *   (i.e. arrays and objects) - these should be supplied as ready-encoded,
 *   null-terminated JSON strings.
 * * The string length for a float is arbitrary and not guaranteed to be
 *   sufficient.
 * * This will not escape strings for JSON, e.g. all quotes, newlines and other
 *   special characters must be escaped by the user.
 *
 * @param var A C variable containing the value which is to be encoded.
 * @param type One of the type macros above specifying the type of the value.
 * @returns A C expression giving the size of an appropriate character array
 *          for storing the encoded value of the given JSON value, without a
 *          space for a NULL terminator. May over-allocate space.
 */
#define SHET_ENCODED_JSON_LENGTH(var, type) \
	CAT(_SHET_ENCODED_JSON_LENGTH_,type)((var))

// Large enough for INT_MIN on a 64-bit machine.
#define _SHET_ENCODED_JSON_LENGTH_SHET_INT(var) 20

// Large enough for an s9.9 decimal representation. An arbitrary choice.
#define _SHET_ENCODED_JSON_LENGTH_SHET_FLOAT(var) 20

#define _SHET_ENCODED_JSON_LENGTH_SHET_BOOL(var)        ((var) ? 4 : 5)
#define _SHET_ENCODED_JSON_LENGTH_SHET_NULL(var)        4
#define _SHET_ENCODED_JSON_LENGTH_SHET_STRING(var)      (strlen((var)) + 2)
#define _SHET_ENCODED_JSON_LENGTH_SHET_ARRAY(var)       (strlen((var)))
#define _SHET_ENCODED_JSON_LENGTH_SHET_ARRAY_BEGIN(var) 1
#define _SHET_ENCODED_JSON_LENGTH_SHET_ARRAY_END(var)   1
#define _SHET_ENCODED_JSON_LENGTH_SHET_OBJECT(var)      (strlen((var)))



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
	MAP_SLIDE(_SHET_JSON_TYPES_AS_STRING_OP, \
	          SHET_JSON_TYPE_AS_STRING, \
	          EMPTY, \
	          __VA_ARGS__)

#define _SHET_JSON_TYPES_AS_STRING_OP(type, next_type) \
	SHET_JSON_TYPE_AS_STRING(type) \
	IF(SHET_JSON_IS_COMMA_BETWEEN(type, next_type))(", ")

/**
 * Given a single type (e.g. SHET_INT), expand to a C string literal naming that
 * type.
 */
#define SHET_JSON_TYPE_AS_STRING(type) \
	CAT(_SHET_JSON_TYPE_AS_STRING_, type)()

#define _SHET_JSON_TYPE_AS_STRING_SHET_INT()          "int"
#define _SHET_JSON_TYPE_AS_STRING_SHET_FLOAT()        "float"
#define _SHET_JSON_TYPE_AS_STRING_SHET_BOOL()         "bool"
#define _SHET_JSON_TYPE_AS_STRING_SHET_STRING()       "string"
#define _SHET_JSON_TYPE_AS_STRING_SHET_NULL()         "null"
#define _SHET_JSON_TYPE_AS_STRING_SHET_ARRAY()        "array"
#define _SHET_JSON_TYPE_AS_STRING_SHET_ARRAY_BEGIN()  "["
#define _SHET_JSON_TYPE_AS_STRING_SHET_ARRAY_END()    "]"
#define _SHET_JSON_TYPE_AS_STRING_SHET_OBJECT()       "object"


////////////////////////////////////////////////////////////////////////////////
// JSON token iteration
////////////////////////////////////////////////////////////////////////////////

/**
 * Get the next JSON value in an object/array. This is useful for skipping over
 * (possibly nested) compound objects. Note: this function does not do any
 * bounds checking!
 */
shet_json_t shet_next_token(shet_json_t json);

/**
 * Get the total number of tokens in the passed JSON, including those within
 * compound objects. Note: this function does not do any bounds checking!
 */
unsigned int shet_count_tokens(shet_json_t json);

#ifdef __cplusplus
}
#endif

#endif
