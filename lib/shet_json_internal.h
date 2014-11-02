/**
 * Implementations of utility macros for working with JSON values tokenised by
 * JSMN.
 */


#ifndef SHET_JSON_INTERNAL_H
#define SHET_JSON_INTERNAL_H

////////////////////////////////////////////////////////////////////////////////
// JSON-type to C-type mapping.
////////////////////////////////////////////////////////////////////////////////

#define _SHET_HAS_JSON_PARSED_TYPE(type) \
	IS_PROBE(CAT(_SHET_HAS_JSON_PARSED_TYPE_,type)())

#define _SHET_HAS_JSON_PARSED_TYPE_SHET_INT()    PROBE()
#define _SHET_HAS_JSON_PARSED_TYPE_SHET_FLOAT()  PROBE()
#define _SHET_HAS_JSON_PARSED_TYPE_SHET_BOOL()   PROBE()
#define _SHET_HAS_JSON_PARSED_TYPE_SHET_STRING() PROBE()
#define _SHET_HAS_JSON_PARSED_TYPE_SHET_ARRAY()  PROBE()
#define _SHET_HAS_JSON_PARSED_TYPE_SHET_OBJECT() PROBE()



#define _SHET_GET_JSON_PARSED_TYPE(type) \
	CAT(_SHET_GET_JSON_PARSED_TYPE_,type)()

#define _SHET_GET_JSON_PARSED_TYPE_SHET_INT()    int
#define _SHET_GET_JSON_PARSED_TYPE_SHET_FLOAT()  double
#define _SHET_GET_JSON_PARSED_TYPE_SHET_BOOL()   bool
#define _SHET_GET_JSON_PARSED_TYPE_SHET_STRING() const char *
#define _SHET_GET_JSON_PARSED_TYPE_SHET_ARRAY()  shet_json_t
#define _SHET_GET_JSON_PARSED_TYPE_SHET_OBJECT() shet_json_t



#define _SHET_GET_JSON_ENCODED_TYPE(type) \
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

#define _SHET_JSON_IS_TYPE(json, type) \
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
// Tokenised JSON value parsing.
////////////////////////////////////////////////////////////////////////////////

#define _SHET_PARSE_JSON_VALUE(json, type) \
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
// JSON value encoding.
////////////////////////////////////////////////////////////////////////////////

#define _SHET_ENCODED_JSON_LENGTH(var, type) \
	CAT(_SHET_ENCODED_JSON_LENGTH_,type)((var))

// Large enough for INT_MIN on anything up-to a 64-bit machine.
#define _SHET_ENCODED_JSON_LENGTH_SHET_INT(var) \
	( (sizeof(int) <= 2) ?  6 : \
	  (sizeof(int) <= 4) ? 11 : \
	                       20 )

// Large enough for an s9.9 decimal representation. An arbitrary choice.
#define _SHET_ENCODED_JSON_LENGTH_SHET_FLOAT(var) 20

#define _SHET_ENCODED_JSON_LENGTH_SHET_BOOL(var)        ((var) ? 4 : 5)
#define _SHET_ENCODED_JSON_LENGTH_SHET_NULL(var)        4
#define _SHET_ENCODED_JSON_LENGTH_SHET_STRING(var)      (strlen((var)) + 2)
#define _SHET_ENCODED_JSON_LENGTH_SHET_ARRAY(var)       (strlen((var)))
#define _SHET_ENCODED_JSON_LENGTH_SHET_ARRAY_BEGIN(var) 1
#define _SHET_ENCODED_JSON_LENGTH_SHET_ARRAY_END(var)   1
#define _SHET_ENCODED_JSON_LENGTH_SHET_OBJECT(var)      (strlen((var)))



#define _SHET_ENCODE_JSON_FORMAT(type) \
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



#define _SHET_ENCODE_JSON_VALUE(var, type) \
	CAT(_SHET_ENCODE_,type)(var)

static inline double _shet_clamp_non_finite(double var) {
	return isfinite(var) ? var : 0.0;
}

#define _SHET_ENCODE_SHET_INT(var)         , (var)
#define _SHET_ENCODE_SHET_FLOAT(var)       , _shet_clamp_non_finite((var))
#define _SHET_ENCODE_SHET_BOOL(var)        , ((var) ? "true" : "false")
#define _SHET_ENCODE_SHET_NULL(var)        /* Nothing */
#define _SHET_ENCODE_SHET_STRING(var)      , (var)
#define _SHET_ENCODE_SHET_ARRAY(var)       , (var)
#define _SHET_ENCODE_SHET_ARRAY_BEGIN(var) /* Nothing */
#define _SHET_ENCODE_SHET_ARRAY_END(var)   /* Nothing */
#define _SHET_ENCODE_SHET_OBJECT(var)      , (var)


////////////////////////////////////////////////////////////////////////////////
// JSON value unpacking.
////////////////////////////////////////////////////////////////////////////////

#define _SHET_UNPACK_JSON(json, on_error, ...) \
	{ \
		/* Make a local copy of the token to allow it to be used as an iterator. */ \
		shet_json_t _json = (json);\
		/* The root does not have a parent */ \
		shet_json_t _parent; \
		_parent.token = NULL; \
		USE(_parent); \
		/* Number of tokens unpacked so far */ \
		int _num_unpacked = 0; \
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
	if ((_parent.token != NULL && _num_unpacked >= _parent.token->size) || \
	    (!SHET_JSON_IS_TYPE(_json, type))) { \
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
		int _num_unpacked = 0; \
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
// JSON value packing.
////////////////////////////////////////////////////////////////////////////////

#define _SHET_PACK_JSON(out, ...) \
	IF(HAS_ARGS(__VA_ARGS__))( \
		sprintf( \
			out, \
			/* The format strings (with commas between as required). */ \
			"" MAP_SLIDE(_SHET_PACK_JSON_FORMAT_OP, SHET_ENCODE_JSON_FORMAT, EMPTY,\
			             MAP_PAIRS(SECOND, COMMA, __VA_ARGS__)) \
			/* The values themselves. */ \
			MAP_PAIRS(SHET_ENCODE_JSON_VALUE, EMPTY, __VA_ARGS__)) \
	) \
	/* Special-case to silence warnings about empty format strings. */ \
	IF(NOT(HAS_ARGS(__VA_ARGS__)))(out[0] = '\0')

// Produces the cur_type's format string followed by a string "," if the next
// type is appropriate.
#define _SHET_PACK_JSON_FORMAT_OP(cur_type, next_type) \
	SHET_ENCODE_JSON_FORMAT(cur_type) \
	IF(_SHET_JSON_IS_COMMA_BETWEEN(cur_type, next_type))(",")




#define _SHET_PACK_JSON_LENGTH(...) \
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
	_SHET_JSON_IS_COMMA_BETWEEN(cur_type, next_type)

// Always produces zero since the last value is never followed by a comma.
#define _SHET_PACK_JSON_LENGTH_LAST_OP(cur_type) \
	0


// Expands to 1 if a comma should be placed between the given types, 0 otherwise.
#define _SHET_JSON_IS_COMMA_BETWEEN(first_type, second_type) \
	AND(NOT(IS_PROBE(CAT(_SHET_JSON_NO_COMMA_AFTER_, first_type)())), \
	    NOT(IS_PROBE(CAT(_SHET_JSON_NO_COMMA_BEFORE_, second_type)())))

#define _SHET_JSON_NO_COMMA_AFTER_SHET_ARRAY_BEGIN() PROBE()
#define _SHET_JSON_NO_COMMA_BEFORE_SHET_ARRAY_END() PROBE()


////////////////////////////////////////////////////////////////////////////////
// Unpacked JSON value copying.
////////////////////////////////////////////////////////////////////////////////

#define _SHET_UNPACKED_VALUE_COPY(dst, src, type) \
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



////////////////////////////////////////////////////////////////////////////////
// JSON type sequence to string literal
////////////////////////////////////////////////////////////////////////////////

#define _SHET_JSON_TYPES_AS_STRING(...) \
	MAP_SLIDE(_SHET_JSON_TYPES_AS_STRING_OP, \
	          SHET_JSON_TYPE_AS_STRING, \
	          EMPTY, \
	          __VA_ARGS__)

#define _SHET_JSON_TYPES_AS_STRING_OP(type, next_type) \
	SHET_JSON_TYPE_AS_STRING(type) \
	IF(_SHET_JSON_IS_COMMA_BETWEEN(type, next_type))(", ")




#define _SHET_JSON_TYPE_AS_STRING(type) \
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


#endif
