/**
 * Implementation of EZ-SHET functions.
 *
 * Internally, the main EZSHET macros (e.g. EZSHET_WATCH, EZSHET_ACTION etc.)
 * generate a number of functions and variables whose names are derived from the
 * name supplied as the callback/first variable name. Simple wrapper functions
 * for registration and removal are defined which EZSHET_ADD and EZSHET_REMOVE
 * expand to. A wrapper function for callbacks will be generated which
 * automatically type-checks the incoming JSON and returns sane error messages
 * (if applicable) if this fails. The JSON's values are then unpacked and passed
 * to the user-defined callback function as conventional C arguments. The return
 * value of the callback may then also be returned as appropriate. Variables
 * will also be defined such as appropriate shet_deferred_t along with a counter
 * of type errors encountered by the callback wrapper.
 */


#ifndef EZSHET_INTERNAL_H
#define EZSHET_INTERNAL_H

#include "jsmn.h"
#include "cpp_magic.h"

#include "shet.h"
#include "shet_json.h"

#ifdef __cplusplus
extern "C" {
#endif

////////////////////////////////////////////////////////////////////////////////
// Underlying variable/function name mappings
////////////////////////////////////////////////////////////////////////////////

// Variable name used for the underlying "is registered" variable
#define _EZSHET_IS_REGISTERED_VAR(name) \
	_ezshet_is_registered_ ## name

// Variable name used for the underlying error count variable
#define _EZSHET_ERROR_COUNT_VAR(name) \
	_ezshet_error_count_ ## name

// Variable name used for storing the path string
#define _EZSHET_PATH_VAR(name) \
	_ezshet_path_ ## name

// Variable name used for an item's deferred
#define _EZSHET_DEFERRED_VAR(name) \
	_ezshet_deferred_ ## name

// Variable name used for an item's creation deferred
#define _EZSHET_DEFERRED_MAKE_VAR(name) \
	_ezshet_deferred_make_ ## name

// Function name for the underlying EZSHET_ADD function
#define _EZSHET_ADD_FN(name) \
	_ezshet_add_ ## name

// Function name for the underlying EZSHET_REMOVE function
#define _EZSHET_REMOVE_FN(name) \
	_ezshet_remove_ ## name

// Function name for the callback wrapper function
#define _EZSHET_WRAPPER_FN(name) \
	_ezshet_wrapper_ ## name

// Function name for the getter callback wrapper function
#define _EZSHET_GET_WRAPPER_FN(name) \
	_ezshet_get_wrapper_ ## name

// Function name for the setter callback wrapper function
#define _EZSHET_SET_WRAPPER_FN(name) \
	_ezshet_set_wrapper_ ## name



////////////////////////////////////////////////////////////////////////////////
// General functions
////////////////////////////////////////////////////////////////////////////////

#define _EZSHET_ADD(shet, name) \
	_EZSHET_ADD_FN(name)(shet)

#define _EZSHET_REMOVE(shet, name) \
	_EZSHET_REMOVE_FN(name)(shet)

#define _EZSHET_IS_REGISTERED(name) \
	_EZSHET_IS_REGISTERED_VAR(name)

#define _EZSHET_ERROR_COUNT(name) \
	_EZSHET_ERROR_COUNT_VAR(name)


////////////////////////////////////////////////////////////////////////////////
// Event watching
////////////////////////////////////////////////////////////////////////////////

#define _EZSHET_DECLARE_WATCH(name) \
	/* Underlying function for EZSHET_ADD */ \
	void _EZSHET_ADD_FN(name)(shet_state_t *shet); \
	/* Underlying function for EZSHET_REMOVE */ \
	void _EZSHET_REMOVE_FN(name)(shet_state_t *shet); \
	/* Underlying wrapper callback for the watch */ \
	void _EZSHET_WRAPPER_FN(name)(shet_state_t *shet, shet_json_t json, void *data); \
	/* Underlying variable for EZSHET_IS_REGISTERED */ \
	extern bool _EZSHET_IS_REGISTERED_VAR(name); \
	/* Underlying variable for EZSHET_ERROR_COUNT */ \
	extern unsigned int _EZSHET_ERROR_COUNT_VAR(name); \
	/* Variable storing the path of the watch */ \
	extern const char _EZSHET_PATH_VAR(name)[]; \
	/* The deferreds for the event and its registration. */ \
	extern shet_deferred_t _EZSHET_DEFERRED_VAR(name); \
	extern shet_deferred_t _EZSHET_DEFERRED_MAKE_VAR(name);

#define _EZSHET_WATCH(path, name, ...) \
	/* Underlying function for EZSHET_ADD */ \
	void _EZSHET_ADD_FN(name)(shet_state_t *shet) { \
		_EZSHET_IS_REGISTERED_VAR(name) = false;\
		shet_watch_event(shet, \
		                 _EZSHET_PATH_VAR(name), \
		                 &_EZSHET_DEFERRED_VAR(name), \
		                 _EZSHET_WRAPPER_FN(name), \
		                 NULL, \
		                 NULL, \
		                 NULL, \
		                 &_EZSHET_DEFERRED_MAKE_VAR(name), \
		                 _ezshet_set_is_registered, \
		                 _ezshet_clear_is_registered, \
		                 &_EZSHET_IS_REGISTERED_VAR(name)); \
	} \
	/* Underlying function for EZSHET_REMOVE */ \
	void _EZSHET_REMOVE_FN(name)(shet_state_t *shet) { \
		_EZSHET_IS_REGISTERED_VAR(name) = false;\
		shet_ignore_event(shet, \
		                 _ezshet_path_ ## name, \
		                 NULL, \
		                 NULL, \
		                 NULL, \
		                 NULL); \
	} \
	/* Underlying wrapper callback for watch events */ \
	void _EZSHET_WRAPPER_FN(name)(shet_state_t *shet, shet_json_t json, void *data) {\
		/* Create variables to unpack the JSON into. */ \
		_EZSHET_DEFINE_VARS(_EZSHET_NAME_TYPES(_EZSHET_WRAP_IN_ARRAY(__VA_ARGS__))); \
		/* Unpack the JSON. */ \
		bool error = false; \
		SHET_UNPACK_JSON(json, error=true;, \
			_EZSHET_NAME_TYPES(_EZSHET_WRAP_IN_ARRAY(__VA_ARGS__)) \
		) \
		/* Execute the callback and send the return via SHET. */ \
		if (!error) { \
			name(shet _EZSHET_ARGS_VARS(_EZSHET_NAME_TYPES(_EZSHET_WRAP_IN_ARRAY(__VA_ARGS__)))); \
			shet_return(shet, 0, NULL); \
		} else { \
			static const char err_message[] = _EZSHET_ERROR_MSG(__VA_ARGS__); \
			_EZSHET_ERROR_COUNT_VAR(name)++; \
			shet_return(shet, 1, err_message); \
		} \
	} \
	/* Underlying variable for EZSHET_IS_REGISTERED */ \
	bool _EZSHET_IS_REGISTERED_VAR(name) = false; \
	/* Underlying variable for EZSHET_ERROR_COUNT */ \
	unsigned int _EZSHET_ERROR_COUNT_VAR(name) = 0; \
	/* Variable storing the path of the watch */ \
	const char _EZSHET_PATH_VAR(name)[] = path; \
	/* The deferreds for the event and its registration. */ \
	shet_deferred_t _EZSHET_DEFERRED_VAR(name); \
	shet_deferred_t _EZSHET_DEFERRED_MAKE_VAR(name);


////////////////////////////////////////////////////////////////////////////////
// Internally-used utility macros
////////////////////////////////////////////////////////////////////////////////

/**
 * This macro is useful when passing a comma as an argument to a macro is desired.
 */
#define _EZSHET_COMMA() ,

/**
 * Given a list of types (e.g. SHET_INT), returns a new list prefixed with
 * SHET_ARRAY_BEGIN and postfixed with SHET_ARRAY_END.
 */
#define _EZSHET_WRAP_IN_ARRAY(...) \
	SHET_ARRAY_BEGIN IF(HAS_ARGS(__VA_ARGS__))(_EZSHET_COMMA() __VA_ARGS__), SHET_ARRAY_END


/**
 * Given a list of types (e.g. SHET_INT), returns a new pair name, type  for
 * each of the original inputs with an automatically chosen name.
 */
#define _EZSHET_NAME_TYPES(...) \
	MAP_WITH_ID(_EZSHET_NAME_TYPES_OP, _EZSHET_COMMA, __VA_ARGS__)

#define _EZSHET_NAME_TYPES_OP(type, name) \
	name, type


/**
 * Given an alternating list of names and types (e.g. SHET_INT), produce C
 * variable declarations accordingly. Filters out types such as SHET_NULL.
 */
#define _EZSHET_DEFINE_VARS(...) \
	MAP_PAIRS(_EZSHET_DEFINE_VARS_OP, EMPTY, __VA_ARGS__)

#define _EZSHET_DEFINE_VARS_OP(name, type) \
	IF(SHET_HAS_JSON_PARSED_TYPE(type))( \
		SHET_GET_JSON_PARSED_TYPE(type) name; \
	)


/**
 * Given an alternating list of names and types (e.g. SHET_INT), produce a list
 * of function call arguments for the names whose types are not SHET_NULL etc.
 * Each argument produced will be preceded by a comma.
 */
#define _EZSHET_ARGS_VARS(...) \
	MAP_PAIRS(_EZSHET_ARGS_VARS_OP, EMPTY, __VA_ARGS__)

#define _EZSHET_ARGS_VARS_OP(name, type) \
	IF(SHET_HAS_JSON_PARSED_TYPE(type))(_EZSHET_COMMA() name)


/**
 * Given a list of types (e.g. SHET_INT), produce a string literal containing a
 * JSON string with an error message of the style:
 *
 *   "Expected: [ int int string [ bool null ] ]"
 *
 * Note that commas are omitted (unfortunately) and that opening and closing
 * quotes are included.
 */
#define _EZSHET_ERROR_MSG(...) \
	"\"Expected " \
	IF_ELSE(HAS_ARGS(__VA_ARGS__))(\
		MAP(_EZSHET_ERROR_MSG_OP, _EZSHET_ERROR_MSG_SPACE, __VA_ARGS__), \
		"no value" \
	) \
	"\""

#define _EZSHET_ERROR_MSG_OP(type) \
	CAT(_EZSHET_ERROR_MSG_, type)()

#define _EZSHET_ERROR_MSG_SPACE() " "

#define _EZSHET_ERROR_MSG_SHET_INT()          "int"
#define _EZSHET_ERROR_MSG_SHET_FLOAT()        "float"
#define _EZSHET_ERROR_MSG_SHET_BOOL()         "bool"
#define _EZSHET_ERROR_MSG_SHET_STRING()       "string"
#define _EZSHET_ERROR_MSG_SHET_NULL()         "null"
#define _EZSHET_ERROR_MSG_SHET_ARRAY()        "array"
#define _EZSHET_ERROR_MSG_SHET_ARRAY_BEGIN()  "["
#define _EZSHET_ERROR_MSG_SHET_ARRAY_END()    "]"
#define _EZSHET_ERROR_MSG_SHET_OBJECT()       "object"
#define _EZSHET_ERROR_MSG_SHET_OBJECT_BEGIN() "{"
#define _EZSHET_ERROR_MSG_SHET_OBJECT_END()   "}"


////////////////////////////////////////////////////////////////////////////////
// Internally-used utility functions
////////////////////////////////////////////////////////////////////////////////

/**
 * Callback function used on successful registration. Sets the boolean pointed
 * to by the user argument to 'true'.
 */
void _ezshet_set_is_registered(shet_state_t *shet, shet_json_t json, void *_is_registered);

/**
 * Callback function used on unsuccessful registration. Sets the boolean pointed
 * to by the user argument to 'false'.
 */
void _ezshet_clear_is_registered(shet_state_t *shet, shet_json_t json, void *_is_registered);


#ifdef __cplusplus
}
#endif

#endif

