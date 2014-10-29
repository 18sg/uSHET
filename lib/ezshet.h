/**
 * A library of easy-to-use wrapper macros for uSHET which handle the "common
 * case" uses of uSHET with less boilerplate code.  Details such as defining
 * shet_deferred_t objects, recording success of registration and, most
 * significantly, unpacking JSON into native C types are automated.
 *
 * Note: the implementation of this library should be regarded as a little bit
 * magic due to its heavy use of advanced pre-processor macros and the
 * GCC-specific __attribute__((weak)). As a result, the implementation of these
 * macros is largely contained within lib/ezshet.h to minimise noise when
 * reading this header.
 */


#ifndef EZSHET_H
#define EZSHET_H

#include "jsmn.h"
#include "cpp_magic.h"

#include "shet.h"
#include "shet_json.h"
#include "ezshet_internal.h"

#ifdef __cplusplus
extern "C" {
#endif

////////////////////////////////////////////////////////////////////////////////
// Definitions
////////////////////////////////////////////////////////////////////////////////

/**
 * A special return type which allows multiple values to be returned via
 * return-pointers rather than a conventional return value.
 */
#define EZSHET_RETURN_ARGS_BEGIN EZSHET_RETURN_ARGS_BEGIN
#define EZSHET_RETURN_ARGS_END   EZSHET_RETURN_ARGS_END

////////////////////////////////////////////////////////////////////////////////
// General functions
////////////////////////////////////////////////////////////////////////////////

/**
 * Register a watch/event/property/action which has been defined using the
 * macros in this library with SHET.
 *
 * @param shet A shet_state_t to add to.
 * @param name The name given.
 */
#define EZSHET_ADD(shet, name) \
	_EZSHET_ADD(shet, name)

/**
 * Un-register a watch/event/property/action which has been defined using the
 * macros in this library with SHET.
 *
 * @param shet A shet_state_t to add to.
 * @param name The name given.
 */
#define EZSHET_REMOVE(shet, name) \
	_EZSHET_REMOVE(shet, name)

/**
 * Returns a boolean indicating whether the specified
 * watch/event/property/action has been successfully registered.
 *
 * @param name The name given.
 * @return A bool which is true if the given watch/event/property/action is
 *         working.
 */
#define EZSHET_IS_REGISTERED(name) \
	_EZSHET_IS_REGISTERED(name)

/**
 * Gives the number of times arguments of the wrong type were provided to the
 * wrapper function via SHET (and thus the callback wasn't called).
 *
 * @param name The name given.
 * @return An unsigned int giving the number of times a callback was given with
 *         arguments of the wrong type.
 */
#define EZSHET_ERROR_COUNT(name) \
	_EZSHET_ERROR_COUNT(name)


////////////////////////////////////////////////////////////////////////////////
// Event Watching
////////////////////////////////////////////////////////////////////////////////

/**
 * Watch a SHET event, calling the given function whenever the event occurs.
 *
 * @param path A string literal containing the path of the event to watch.
 * @param name The name of the callback function to call whenever the event
 *             occurs. The callback function will be passed a shet_state_t *
 *             followed by arguments as defined below corresponding with the
 *             event values. The callback should have a void return type.
 * @param ... The type(s) of the value(s) passed with the event (e.g. SHET_INT).
 *            Can nothing if no value is expected. May also be an unpacked type
 *            (e.g. SHET_ARRAY_BEGIN).
 */
#define EZSHET_WATCH(path, name, ...) \
	_EZSHET_WATCH(path, name, __VA_ARGS__)


/**
 * Generate a C declaration for the watch.
 *
 * If multiple source files are used, this will produce the appropriate
 * declarations for the watch for a header file. This will NOT declare the
 * callback function.
 *
 * @param name The name of the callback
 */
#define EZSHET_DECLARE_WATCH(name) \
	_EZSHET_DECLARE_WATCH(name)


////////////////////////////////////////////////////////////////////////////////
// Event Creation
////////////////////////////////////////////////////////////////////////////////

/**
 * Create a SHET event which can be raised by calling the defined function.
 *
 * @param path A string literal containing the path of the event to create.
 * @param name The name of the function to be created which raises the event.
 *             This function takes a pointer to a shet_state_t followed by
 *             arguments of the  types as specified below and no others.
 * @param ... The type(s) of the value(s) passed with the event (e.g. SHET_INT).
 *            Can be nothing if no value is expected.
 */
#define EZSHET_EVENT(path, name, ...) \
	_EZSHET_EVENT(path, name, __VA_ARGS__)

/**
 * Generate a C declaration for an event.
 *
 * If multiple source files are used, this will produce the appropriate
 * declarations for the event for a header file.
 *
 * @param name The name of the callback.
 * @param ... The type(s) of the value(s) passed with the event (e.g. SHET_INT).
 *            Can be nothing if no value is expected.
 */
#define EZSHET_DECLARE_EVENT(name, ...) \
	_EZSHET_DECLARE_EVENT(name, __VA_ARGS__)


////////////////////////////////////////////////////////////////////////////////
// Property Creation
////////////////////////////////////////////////////////////////////////////////

/**
 * Create a SHET property with callbacks for getters and setters.
 *
 * @param path A string literal containing the path of the property.
 * @param name The base name of the callback functions. Two functions should be
 *             defined with this name prefixed with get_ and set_ (e.g. if name
 *             is my_prop, you should define get_my_prop and set_my_prop). The
 *             get callback will simply be passed a shet_state_t * and should
 *             return a value of the appropriate type (either via arguments or
 *             return value, see type). The set callback will be passed a
 *             shet_state_t * followed by an appropriate value. The set callback
 *             should have a void return type.
 * @param type The type of the property (e.g. SHET_INT). This may not include
 *             unpacked types, e.g. SHET_ARRAY_BEGIN/SHET_ARRAY_END.
 *             Alternatively may be EZSHET_RETURN_ARGS_BEGIN followed by a
 *             number of types including the unpacked types and terminated by
 *             EZSHET_RETURN_ARGS_END. In the both cases, the setter receives
 *             the values as arguments and returns void. In the former case the
 *             getter returns its value. In the latter case the getter returns
 *             values via return-pointer arguments.
 */
#define EZSHET_PROP(path, name, type, ...) \
	_EZSHET_PROP(path, name, type, __VA_ARGS__)

/**
 * Generate a C declaration for the property.
 *
 * If multiple source files are used, this will produce the appropriate
 * declarations for the property for a header file. This will NOT declare all
 * callback functions for you automatically.
 *
 * @param name The name of the callback
 */
#define EZSHET_DECLARE_PROP(name) \
	_EZSHET_DECLARE_PROP(name)


////////////////////////////////////////////////////////////////////////////////
// Variables-as-property Creation
////////////////////////////////////////////////////////////////////////////////

/**
 * Create a SHET property which is implemented by a C variable (or group of
 * variables).
 *
 * @param path A string literal containing the path of the property.
 * @param name A C variable name. This must be declared, defined and initialised
 *             by the user.
 * @param type A type (e.g. SHET_INT). If an unpacked type (e.g.
 *             SHET_ARRAY_BEGIN) or SHET_NULL is given, the corresponding
 *             name will never be assigned to and need not exist.
 * @param ... A sequence of name, type arguments as above when type is an
 *            unpacked type, e.g. SHET_ARRAY_BEGIN. Note that the name given
 *            above will be used to identify the property for the purposes of
 *            the EZSHET library.
 */
#define EZSHET_VAR_PROP(path, name, type, ...) \
	_EZSHET_VAR_PROP(path, name, type, __VA_ARGS__)

/**
 * Generate a C declaration for the property.
 *
 * If multiple source files are used, this will produce the appropriate
 * declarations for the property for a header file. This will NOT declare the
 * underlying variables which hold the property's value. These variables need
 * only be declared in the same file as the corresponding EZSHET_VAR_PROP.
 *
 * @param name The name of the callback
 */
#define EZSHET_DECLARE_VAR_PROP(name) \
	_EZSHET_DECLARE_VAR_PROP(name)


////////////////////////////////////////////////////////////////////////////////
// Action Creation
////////////////////////////////////////////////////////////////////////////////

/**
 * Create a SHET action.
 *
 * @param path A string literal containing the path of the action.
 * @param name The name of the callback function which implements the action.
 *             The callback function will be passed a shet_state_t *
 *             followed by arguments as defined below corresponding with the
 *             call arguments. The callback may return a value as dictated by
 *             ret_type.
 * @param ret_type The return type of the action (e.g. SHET_INT). If this is a
 *                 normal type, the callback function's return value will be
 *                 used. If this value is EZSHET_RETURN_ARGS_BEGIN followed by a
 *                 list of types (which may include packed types e.g.
 *                 SHET_ARRAY_BEGIN), the callback is expected to return values
 *                 via corresponding return-pointer arguments which will appear
 *                 before the normal arguments to the callback and the function
 *                 should return void. The list of return values must be
 *                 terminated with EZSHET_RETURN_ARGS_END.
 * @param ... The type(s) of the value(s) passed with the event (e.g. SHET_INT).
 *            Can be nothing if no value is expected or an unpacked type (e.g.
 *            SHET_ARRAY_BEGIN).
 */
#define EZSHET_ACTION(path, name, ret_type, ...) \
	_EZSHET_ACTION(path, name, ret_type, __VA_ARGS__)

/**
 * Generate a C declaration for the action.
 *
 * If multiple source files are used, this will produce the appropriate
 * declarations for the action for a header file. This will NOT declare the
 * callback function for you automatically.
 *
 * @param name The name of the callback
 */
#define EZSHET_DECLARE_ACTION(name) \
	_EZSHET_DECLARE_ACTION(name)


#ifdef __cplusplus
}
#endif

#endif
