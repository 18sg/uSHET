/**
 * Macros for working with JSON values tokenised by JSMN.
 */


#ifndef SHET_JSON_H
#define SHET_JSON_H

#include "jsmn.h"
#include "cpp_magic.h"

#include "shet.h"

#ifdef __cplusplus
extern "C" {
#endif


/**
 * These type definitions correspond with the basic JSON types and should be
 * supplied to the macros in this file when referring to a specific type.
 */
#define SHET_INT    SHET_INT
#define SHET_FLOAT  SHET_FLOAT
#define SHET_BOOL   SHET_BOOL
#define SHET_NULL   SHET_NULL
#define SHET_STRING SHET_STRING
#define SHET_ARRAY  SHET_ARRAY
#define SHET_OBJECT SHET_OBJECT

/**
 * These additional types may be used only for parsing JSON and not generating
 * it. They enable the user to extract values from a complex JSON structure.
 */
#define SHET_ARRAY_BEGIN   SHET_ARRAY_BEGIN
#define SHET_ARRAY_END     SHET_ARRAY_END
#define SHET_OBJECT_BEGIN  SHET_OBJECT_BEGIN
#define SHET_OBJECT_END    SHET_OBJECT_END


////////////////////////////////////////////////////////////////////////////////
// JSON type to C type mapping.
////////////////////////////////////////////////////////////////////////////////

/**
 * Get the C type produced when parsing a given JSON type.
 *
 * This macro is mostly intended for use by other macros; end users are advised
 * to just write the types by hand on account of their obviousness!
 *
 * Note that NULL and the array/object unpacking types are not defined.
 *
 * @param type One of the type macros above specifying the type of the value.
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
 * Get the C types which are expected when encoding a given JSON type.
 *
 * Note: JSON compound types (i.e. arrays and objects) cannot be encoded since
 * they have no obvious analogue in C. Instead they are expected to be
 * pre-encoded and just passed as a const char *.
 *
 * This macro is mostly intended for use by other macros; end users are advised
 * to just write the types by hand on account of their obviousness!
 *
 * @param type One of the type macros above specifying the type of the value.
 * @returns The equivalent C type.
 */
#define SHET_GET_JSON_ENCODED_TYPE(type) \
	CAT(_SHET_GET_JSON_ENCODED_TYPE_,type)()

#define _SHET_GET_JSON_ENCODED_TYPE_SHET_INT()    int
#define _SHET_GET_JSON_ENCODED_TYPE_SHET_FLOAT()  double
#define _SHET_GET_JSON_ENCODED_TYPE_SHET_BOOL()   bool
#define _SHET_GET_JSON_ENCODED_TYPE_SHET_NULL()   void *
#define _SHET_GET_JSON_ENCODED_TYPE_SHET_STRING() const char *
#define _SHET_GET_JSON_ENCODED_TYPE_SHET_ARRAY()  const char *
#define _SHET_GET_JSON_ENCODED_TYPE_SHET_OBJECT() const char *

////////////////////////////////////////////////////////////////////////////////
// JSON Type-checking
////////////////////////////////////////////////////////////////////////////////

/**
 * Check that the type of a shet_json_t matches a given type..
 *
 * Limitation: SHET_INT and SHET_FLOAT are not checked for the absence or
 * presence of a decimal point. It this matters, the user should check for
 * themselves!
 *
 * @param json A shet_json_t referring to the tokenised JSON value to test.
 * @param type One of the type macros above specifying the type to check for.
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
#define _SHET_JSON_IS_TYPE_SHET_OBJECT_BEGIN(json) \
	_SHET_JSON_IS_TYPE_SHET_OBJECT((json))
#define _SHET_JSON_IS_TYPE_SHET_OBJECT_END(json) \
	(true)


////////////////////////////////////////////////////////////////////////////////
// JSON value parsing.
////////////////////////////////////////////////////////////////////////////////

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

#define _SHET_PARSE_SHET_NULL(json) \
	((void *)NULL)

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
 * Limitations:
 * * Cannot currently unpack objects.
 *
 * @param json A shet_json_t pointing at the JSON value to unpack.
 * @param on_error C code to execute if a JSON type-check fails.
 * @param ... Alternating var_name and type (e.g. SHET_INT) corresponding with
 *            variables to extract. The named variables should already be
 *            defined and will be assigned to by the code generated by this
 *            macro.
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
			MAP_PAIRS(_SHET_UNPACK_JSON_MAP, EMPTY, __VA_ARGS__); \
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
 * Encode a C value as a JSON string.
 *
 * Limitations:
 * * This macro does not provide facilities for generating JSON compound values
 *   (i.e. arrays and objects) - these should be supplied as ready-encoded,
 *   null-terminated JSON strings.
 * * The float formatting scheme is unspecified. Printing very large floats may
 *   use an unpredictable amount of space and thus should be considered unsafe
 *   in the general case! Further special cases such as NaN which are not
 *   supported by JSON are not yet handled.
 * * This will not escape strings for JSON, e.g. all quotes, newlines and other
 *   special characters must be escaped by the user.
 *
 * @param var A C variable containing the value to encode.
 * @param type One of the type macros above specifying the type of the value.
 * @param dest A char * pointing at a character array to store the encoded value
 *             in followed by a NULL terminator. This must be at least as large
 *             as SHET_ENCODED_JSON_VALUE_SIZE reports.
 * @returns A C statement which writes a JSON string encoding the given variable
 *          into dest.
 */
#define SHET_ENCODE_JSON_VALUE(var, type, dest) \
	CAT(_SHET_ENCODE_,type)((var), (dest))

#define _SHET_ENCODE_JSON_INT(var, dest) \
	sprintf((dest), "%d", (var))

#define _SHET_ENCODE_JSON_FLOAT(var, dest) \
	sprintf((dest), "%f", (var))

#define _SHET_ENCODE_JSON_BOOL(var, dest) \
	strcpy((dst), (var) ? "true" : "false")

#define _SHET_ENCODE_JSON_NULL(var, dest) \
	strcpy((dst), "null")

#define _SHET_ENCODE_JSON_STRING(var, dest) \
	sprintf((dst), "\"%s\"", (var))

#define _SHET_ENCODE_JSON_ARRAY(var, dest) \
	strcpy((dest), (var));

#define _SHET_ENCODE_JSON_OBJECT(var, dest) \
	strcpy((dest), (var));

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
 *          for storing the encoded value of the given JSON value, including
 *          space for a NULL terminator. May over-allocate space.
 */
#define SHET_ENCODED_JSON_VALUE_SIZE(var, type) \
	CAT(_SHET_ENCODED_JSON_VALUE_SIZE_,type)((var))

// Large enough for INT_MIN on a 64-bit machine.
#define _SHET_ENCODED_JSON_VALUE_SIZE_INT(var) \
	21

// Large enough for an s9.9 decimal representation. An arbitrary choice.
#define _SHET_ENCODED_JSON_VALUE_SIZE_INT(var) \
	21

#define _SHET_ENCODED_JSON_VALUE_SIZE_BOOL(var) \
	6

#define _SHET_ENCODED_JSON_VALUE_SIZE_NULL(var) \
	5

#define _SHET_ENCODED_JSON_VALUE_SIZE_STRING(var) \
	(strlen((var)) + 3)

#define _SHET_ENCODED_JSON_VALUE_SIZE_ARRAY(var) \
	(strlen((var)) + 1)

#define _SHET_ENCODED_JSON_VALUE_SIZE_OBJECT(var) \
	(strlen((var)) + 1)


////////////////////////////////////////////////////////////////////////////////
// JSON token iteration
////////////////////////////////////////////////////////////////////////////////

/**
 * Get the next JSON value in an object/array. This is useful for skipping over
 * (possibly nested) compound objects. Note: this function does not do any
 * bounds checking!
 */
shet_json_t shet_next_token(shet_json_t json);


#ifdef __cplusplus
}
#endif

#endif
