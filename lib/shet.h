/**
 * uSHET a microcontroller-friendly implementation of the SHET client protocol.
 */

#ifndef SHET_H
#define SHET_H

#include <stdlib.h>
#include <stdbool.h>

#include "jsmn.h"

#ifdef __cplusplus
extern "C" {
#endif

// Utility macro to "use" a variable to stop GCC complaining.
#ifndef USE
#define USE(x) (void)(x)
#endif


////////////////////////////////////////////////////////////////////////////////
// Resource allocation constants
////////////////////////////////////////////////////////////////////////////////

/**
 * The number of JSON tokens to allocate in a shet_state_t for parsing a single
 * message.
 */
#ifndef SHET_NUM_TOKENS
#define SHET_NUM_TOKENS 30
#endif

/**
 * Number of characters in the buffer used to hold outgoing SHET messages for
 * the transmit callback to read from.
 */
#ifndef SHET_BUF_SIZE
#define SHET_BUF_SIZE 100
#endif

/**
 * Enable debug messages using printf.
 */
// #define SHET_DEBUG


////////////////////////////////////////////////////////////////////////////////
// Types
////////////////////////////////////////////////////////////////////////////////

/**
 * The global state for SHET.
 */
struct shet_state;
typedef struct shet_state shet_state_t;


/**
 * Storage used by setting up a callback.
 */
struct shet_deferred;
typedef struct shet_deferred shet_deferred_t;


/**
 * Storage used by making an event.
 */
struct shet_event;
typedef struct shet_event shet_event_t;


/**
 * A tokenised JSON value.
 */
typedef struct {
	// The underlying JSON string. Users should not rely on the whole of this
	// string being well formed and should only access the parts of it indicated
	// by the supplied token.
	char      *line;
	
	// The JSMN token for the JSON value passed. This token is part of an array of
	// tokens such that if this token refers to an array or object, the first
	// element of the array (or first key in the object) will be at token[1] and
	// so on.
	jsmntok_t *token;
} shet_json_t;



/**
 * All callbacks should be of this type.
 *
 * @param state The SHET state object associated with the callback.
 * @param json The JSON provided to the callback. This may be undefined and, in
 *             this case, must not be accessed.
 * @param user_data A user-defined pointer chosen when the callback was
 *                  registered.
 */
typedef void (*shet_callback_t)(shet_state_t *state,
                                shet_json_t json,
                                void *user_data);


/**
 * Success status of shet_process_line.
 */
typedef enum {
	// The line was handled successfully
	SHET_PROC_OK = 0,
	
	// The line could not be parsed due to insufficient jsmn tokens being
	// available. Consider increasing SHET_NUM_TOKENS.
	SHET_PROC_ERR_OUT_OF_TOKENS,
	
	// The line could not be parsed due to a JSON syntax error
	SHET_PROC_INVALID_JSON,
	
	// The command received could not be interpreted, e.g. because it didn't
	// follow the correct format for [id, command_name, ...].
	SHET_PROC_MALFORMED_COMMAND,
	
	// The command received was not recognised (in this case a error return will
	// also be sent to the SHET server).
	SHET_PROC_UNKNOWN_COMMAND,
	
	// A return command featured fields with the incorrect types or lengths
	SHET_PROC_MALFORMED_RETURN,
	
	// A command's arguments were of unexpected types or sizes.
	SHET_PROC_MALFORMED_ARGUMENTS,
} shet_processing_error_t;


// Since the above types must be accessible to the compiler to allow users to
// allocate storage for them they are defined in the following header. End-users
// should, however, consider the types defined in this header otherwise opaque.
#include "shet_internal.h"


////////////////////////////////////////////////////////////////////////////////
// General Library Functions
////////////////////////////////////////////////////////////////////////////////

/**
 * Initialise the SHET global state. This must be called before any other
 * functions can take place.
 *
 * @param state A pointer to an (unused) shet_state_t which is to be
 *              initialised.
 * @param connection_name Must be a unique JSON structure (as a string) which
 *                        uniquely identifies this device and application within
 *                        the SHET network. This string must be live for the
 *                        full lifetime of the SHET library.
 * @param transmit A function which will be called to transmit strings generated
 *                 by the library to the SHET server. The data argument is a
 *                 null-terminated string to transmit which will be live until
 *                 the call returns. The user_data argument is a pointer to the
 *                 value defined by transmit_user_data below. Note that the user
 *                 is responsible for ensuring reliable delivery.
 * @param transmit_user_data A user-defined pointer which will be passed to the
 *                           transmit callback.
 */
void shet_state_init(shet_state_t *state,
                     const char *connection_name,
                     void (*transmit)(const char *data, void *user_data),
                     void *transmit_user_data);

/**
 * Set the unhandled error callback. This callback will be called whenever a
 * callback has been registered but whenever the callback function defined is
 * NULL.
 *
 * Note that this function is intended as a catch-all SHET-reported error
 * handler. This means that is not called due to actual errors e.g. in the event
 * of protocol violations, unexpected or unhandled commands.
 *
 * @param state The global SHET state.
 * @param callback The callback function to use for unhandled errors. Set to
 *                 NULL to disable. The JSON value is undefined.
 * @param callback_arg User defined data to be passed to the callback.
 */
void shet_set_error_callback(shet_state_t *state,
                             shet_callback_t callback,
                             void *callback_arg);


/**
 * Re-register the client with the server. This command should be called
 * whenever the client re-connects to the SHET server. The command forces the
 * server to forget all held state for the previous connection and then
 * re-reigsters all watches, properties, actions and events.
 *
 * @param state The global SHET state.
 */
void shet_reregister(shet_state_t *state);

/**
 * Process a single message from the SHET server. This function expects exactly
 * one (\n delimited) line of data from the server. Note that the caller is
 * responsible for ensuring that the data passed to this function has not
 * suffered any ommisions or corruption.
 *
 * @param state The global SHET state.
 * @param line A buffer containing the line to process. Note that the data in
 *             the buffer may be corrupted by SHET. This string need only remain
 *             live until the call to shet_process_line returns. The string
 *             need not be null-terminated. The string need not contain a
 *             trailing newline.
 * @param line_length The number of characters in the line (not including an
 *                    optional null-terminator).
 * @return Returns a shet_processing_error_t indicating what, if anything, went
 *         wrong when processing the provided input line.
 */
shet_processing_error_t shet_process_line(shet_state_t *state, char *line, size_t line_length);

/**
 * Ping the SHET server.
 *
 * @param state The global SHET state.
 * @param args A value to send to the server to echo. A series of
 *             comma-delimited JSON values represented seperated by a string.
 *             Must contain at least one value or be NULL.
 * @param deferred A pointer to a deferred_t struct responsible for this
 *                 callback. This struct must remain live until either a
 *                 callback function is called or shet_cancel_deferred is called
 *                 by the user.
 * @param callback Callback function on (successful) ping response. The passed
 *                 JSON should be an array containing the ping's args. May be
 *                 NULL to ignore the callback.
 * @param err_callback Callback function on unsuccessful ping response. May be
 *                     NULL to ignore the callback.
 * @param callback_arg User-defined pointer to be passed to the callback and
 *                     err_callback functions.
 */
void shet_ping(shet_state_t *state,
               const char *args,
               shet_deferred_t *deferred,
               shet_callback_t callback,
               shet_callback_t err_callback,
               void *callback_arg);

/**
 * For use within (certain) callback functions only. Return a value to SHET, for
 * example, returing a value from an action's "call" callback.
 *
 * Note that this function can only be called from within the callback function
 * expecting the return response. See shet_return_with_id and shet_get_return_id
 * if a return must be postponed until later.
 *
 * @param state The global SHET state.
 * @param success Success indicator. 0 for success, anything else for failure.
 * @param value A valid JSON string containing a single value. If NULL, the
 *              JSON primitive null will be sent. This string need only be live
 *              until shet_return returns.
 */
void shet_return(shet_state_t *state,
                 int success,
                 const char *value);

/**
 * Return a value to SHET. Unlike shet_return_with_id, this function may be used
 * at a later time, outside of the relevant callback function.
 *
 * See shet_get_return_id for detalis of how to get an ID to pass to this
 * function.
 *
 * @param state The global SHET state.
 * @param id The ID of the request to be returned. This must be a
 *           (null-terminated) string containing a valid JSON value. This string
 *           need only be live until shet_return_with_id returns.
 * @param success Success indicator. 0 for success, anything else for failure.
 * @param value A valid JSON string containing a single value. If NULL, the
 *              JSON primitive null will be sent. This string need only be live
 *              until shet_return_with_id returns.
 */
void shet_return_with_id(shet_state_t *state,
                         const char *id,
                         int success,
                         const char *value);

/**
 * Get the string containing the JSON for the return ID for the current callback
 * suitable for use with shet_return_with_id. This should be copied by the user
 * if this ID is required after the callback function ends.
 *
 * @param state The global SHET state.
 * @return Returns a pointer to a string containg a JSON value repreesnting the
 *         return ID for the current callback. This string is live until the end
 *         of the callback function and should be copied if required afterwards.
 */
const char *shet_get_return_id(shet_state_t *state);


/**
 * Cancel all future callbacks associated with a shet_deferred_t. This function
 * is primarily intended use in simple timeout mechanisms. After calling this
 * function the shet_deferred_t may be reused for another purpose.
 *
 * This function should not be used to ignore watched events or remove
 * registered actions, properties or events. Doing this will result in undefined
 * behaviour.
 *
 * This function can safely be called with deferreds which have never
 * been used or which no future callbacks are possible.
 *
 * @param state The global SHET state.
 * @param deferred The deferred to unregister.
 */
void shet_cancel_deferred(shet_state_t *state, shet_deferred_t *deferred);


////////////////////////////////////////////////////////////////////////////////
// Action Functions
////////////////////////////////////////////////////////////////////////////////

/**
 * Make a new action in the SHET tree.
 *
 * @param state The global SHET state.
 * @param path A valid, null-terminated SHET path name. This string must remain
 *             live as long as the action remains registered.
 * @param action_deferred A pointer to a deferred_t struct responsible for
 *                        action callbacks. This struct must remain live until
 *                        the action is removed.
 * @param callback Callback function which implements the action. This function
 *                 should receive a (possibly empty) JSON array containing
 *                 arguments for the call. This function must return success and
 *                 optionally a value to SHET e.g. using shet_return.
 * @param action_arg User-defined pointer to be passed to the callback.
 * @param mkaction_deferred A pointer to a deferred_t struct responsible for
 *                          action creation callbacks. This struct must remain
 *                          live until the action is removed. If NULL no
 *                          callbacks will be registered for the action's
 *                          creation.
 * @param mkaction_callback Callback function on successful action creation.
 *                          This may be called multiple times, for example,
 *                          after shet_reregister. NULL if unused.
 * @param mkaction_err_callback Callback function on unsuccessful action
 *                              creation.  This may be called multiple times,
 *                              for example, after shet_reregister. NULL if
 *                              unused.
 * @param mkaction_arg User-defined pointer to be passed to the action creation
 *                     callbacks.
 */
void shet_make_action(shet_state_t *state,
                      const char *path,
                      shet_deferred_t *action_deferred,
                      shet_callback_t callback,
                      void *action_arg,
                      shet_deferred_t *mkaction_deferred,
                      shet_callback_t mkaction_callback,
                      shet_callback_t mkaction_err_callback,
                      void *mkaction_callback_arg);

/**
 * Remove an action and cancel any associated deferreds.
 *
 * @param state The global SHET state.
 * @param path The null-terminated SHET path name of the action created with
 *             shet_make_action which will be removed.
 * @param deferred A pointer to a deferred_t struct responsible for action
 *                 removal callbacks. This struct must remain live until one of
 *                 its callbacks is called or shet_cancel_deferred is called by
 *                 the user. The deferreds associated with the creation of the
 *                 action may be reused if desired. If NULL no callbacks will be
 *                 registered for the action's removal.
 * @param callback Callback function on successful action removal. After this
 *                 occurrs, the deferred can be considered cancelled. NULL if
 *                 unused.
 * @param err_callback Callback function on unsuccessful action removal. After
 *                     this occurrs, the deferred can be considered cancelled.
 *                     NULL if unused.
 * @param callback_arg User-defined pointer to be passed to the action removal
 *                     callbacks.
 */
void shet_remove_action(shet_state_t *state,
                        const char *path,
                        shet_deferred_t *deferred,
                        shet_callback_t callback,
                        shet_callback_t err_callback,
                        void *callback_arg);


/**
 * Call an action via SHET.
 *
 * @param state The global SHET state.
 * @param path A valid, null-terminated SHET path name. This string must remain
 *             live until this function returms.
 * @param args A null-terminated, comma-seperated string of JSON values to use
 *             as arguments to the call. Use NULL if no argument is desired.
 *             Must remain live until this function returns.
 * @param deferred A pointer to a deferred_t struct responsible for the response
 *                 callbacks. This struct must remain live until the call
 *                 returns or shet_cancel_deferred is called by the user.
 * @param callback Callback function on successful execution of the call. The
 *                 argument will be a single JSON value representing the value
 *                 returned by the call. When this callback occurrs, the
 *                 deferred can be considered cancelled. NULL if unused.
 * @param err_callback Callback function on unsuccessful execution of the call.
 *                     When this callback occurrs, the deferred can be
 *                     considered cancelled. NULL if unused.
 * @param callback_arg User-defined pointer to be passed to the callbacks.
 */
void shet_call_action(shet_state_t *state,
                     const char *path,
                     const char *args,
                     shet_deferred_t *deferred,
                     shet_callback_t callback,
                     shet_callback_t err_callback,
                     void *callback_arg);

////////////////////////////////////////////////////////////////////////////////
// Property Functions
////////////////////////////////////////////////////////////////////////////////

/**
 * Make a new property in the SHET tree.
 *
 * @param state The global SHET state.
 * @param path A valid, null-terminated SHET path name. This string must remain
 *             live as long as the property remains registered.
 * @param prop_deferred A pointer to a deferred_t struct responsible for
 *                      property callbacks. This struct must remain live until
 *                      the property is removed.
 * @param get_callback Callback function which gets the value of the property.
 *                     This function must return the value to SHET e.g. using
 *                     shet_return. The JSON passed is undefined.
 * @param set_callback Callback function which sets the value of the property.
 *                     This should be given a JSON array which contains a single
 *                     JSON value to set as the value of the property. This
 *                     function must return success to SHET e.g. using
 *                     shet_return.
 * @param prop_arg User-defined pointer to be passed to the get/set callbacks.
 * @param mkprop_deferred A pointer to a deferred_t struct responsible for
 *                        property creation callbacks. This struct must remain
 *                        live until the property is removed. If NULL no
 *                        callbacks will be registered for the property's
 *                        creation.
 * @param mkprop_callback Callback function on successful property creation.
 *                        This may be called multiple times, for example, after
 *                        shet_reregister. NULL if unused.
 * @param mkprop_err_callback Callback function on unsuccessful property
 *                            creation. This may be called multiple times, for
 *                            example, after shet_reregister. NULL if unused.
 * @param mkprop_arg User-defined pointer to be passed to the property creation
 *                   callbacks.
 */
void shet_make_prop(shet_state_t *state,
                    const char *path,
                    shet_deferred_t *prop_deferred,
                    shet_callback_t get_callback,
                    shet_callback_t set_callback,
                    void *prop_arg,
                    shet_deferred_t *mkprop_deferred,
                    shet_callback_t mkprop_callback,
                    shet_callback_t mkprop_err_callback,
                    void *mkprop_callback_arg);

/**
 * Remove a property and cancel any associated deferreds.
 *
 * @param state The global SHET state.
 * @param path The null-terminated SHET path name of the property created with
 *             shet_make_prop which will be removed.
 * @param deferred A pointer to a deferred_t struct responsible for property
 *                 removal callbacks. This struct must remain live until one of
 *                 its callbacks is called or shet_cancel_deferred is called by
 *                 the user. The deferreds associated with the creation of the
 *                 property may be reused if desired. If NULL no callbacks will
 *                 be registered for the property's removal.
 * @param callback Callback function on successful property removal. After this
 *                 occurrs, the deferred can be considered cancelled. NULL if
 *                 unused.
 * @param err_callback Callback function on unsuccessful property removal. After
 *                     this occurrs, the deferred can be considered cancelled.
 *                     NULL if unused.
 * @param callback_arg User-defined pointer to be passed to the property removal
 *                     callbacks.
 */
void shet_remove_prop(shet_state_t *state,
                      const char *path,
                      shet_deferred_t *deferred,
                      shet_callback_t callback,
                      shet_callback_t err_callback,
                      void *callback_arg);

/**
 * Get a property's value via SHET.
 *
 * @param state The global SHET state.
 * @param path A valid, null-terminated SHET path name. This string must remain
 *             live until this function returms.
 * @param deferred A pointer to a deferred_t struct responsible for the response
 *                 callbacks. This struct must remain live until the get
 *                 returns or shet_cancel_deferred is called by the user.
 * @param callback Callback function on successful getting of the property. The
 *                 callback should be given a JSON array containing a single
 *                 JSON value representing the value of the property. When this
 *                 callback occurrs, the deferred can be considered cancelled.
 * @param err_callback Callback function on unsuccessful getting of the
 *                     property. When this callback occurrs, the deferred can be
 *                     considered cancelled. NULL if unused.
 * @param callback_arg User-defined pointer to be passed to the callbacks.
 */
void shet_get_prop(shet_state_t *state,
                   const char *path,
                   shet_deferred_t *deferred,
                   shet_callback_t callback,
                   shet_callback_t err_callback,
                   void *callback_arg);

/**
 * Set a property's value via SHET.
 *
 * @param state The global SHET state.
 * @param path A valid, null-terminated SHET path name. This string must remain
 *             live until this function returms.
 * @param value A single JSON value as a null-terminated string to set the value
 *              to. Must remain live until this function returns.
 * @param deferred A pointer to a deferred_t struct responsible for the response
 *                 callbacks. This struct must remain live until the set
 *                 returns or shet_cancel_deferred is called by the user.
 * @param callback Callback function on successful setting of the property. When
 *                 this callback occurrs, the deferred can be considered
 *                 cancelled. NULL if unused.
 * @param err_callback Callback function on unsuccessful setting of the
 *                     property. When this callback occurrs, the deferred can be
 *                     considered cancelled. NULL if unused.
 * @param callback_arg User-defined pointer to be passed to the callbacks.
 */
void shet_set_prop(shet_state_t *state,
                   const char *path,
                   const char *value,
                   shet_deferred_t *deferred,
                   shet_callback_t callback,
                   shet_callback_t err_callback,
                   void *callback_arg);


////////////////////////////////////////////////////////////////////////////////
// Event Functions
////////////////////////////////////////////////////////////////////////////////

/**
 * Make a new event in the SHET tree.
 *
 * @param state The global SHET state.
 * @param path A valid, null-terminated SHET path name. This string must remain
 *             live as long as the event remains registered.
 * @param event An unused shet_event_t which represents the registration of the
 *              event. This must remain live as long as th event remains
 *              registered.
 * @param mkevent_deferred A pointer to a deferred_t struct responsible for
 *                         event creation callbacks. This struct must remain
 *                         live until the event is removed. If NULL no callbacks
 *                         will be registered for the event's creation.
 * @param mkevent_callback Callback function on successful event creation.  This
 *                         may be called multiple times, for example, after
 *                         shet_reregister. NULL if unused.
 * @param mkevent_err_callback Callback function on unsuccessful property
 *                            creation. This may be called multiple times, for
 *                            example, after shet_reregister. NULL if unused.
 * @param mkevent_arg User-defined pointer to be passed to the property creation
 *                   callbacks.
 */
void shet_make_event(shet_state_t *state,
                     const char *path,
                     shet_event_t *event,
                     shet_deferred_t *mkevent_deferred,
                     shet_callback_t mkevent_callback,
                     shet_callback_t mkevent_error_callback,
                     void *mkevent_callback_arg);

/**
 * Remove an event and cancel any associated deferreds.
 *
 * @param state The global SHET state.
 * @param path The null-terminated SHET path name of the event created with
 *             shet_make_event which will be removed.
 * @param deferred A pointer to a deferred_t struct responsible for event
 *                 removal callbacks. This struct must remain live until one of
 *                 its callbacks is called or shet_cancel_deferred is called by
 *                 the user. The deferreds associated with the creation of the
 *                 event may be reused if desired. If NULL no callbacks will
 *                 be registered for the event's removal.
 * @param callback Callback function on successful event removal. After this
 *                 occurrs, the deferred can be considered cancelled. NULL if
 *                 unused.
 * @param err_callback Callback function on unsuccessful event removal. After
 *                     this occurrs, the deferred can be considered cancelled.
 *                     NULL if unused.
 * @param callback_arg User-defined pointer to be passed to the event removal
 *                     callbacks.
 */
void shet_remove_event(shet_state_t *state,
                       const char *path,
                       shet_deferred_t *deferred,
                       shet_callback_t callback,
                       shet_callback_t error_callback,
                       void *callback_arg);

/**
 * Raise an event.
 *
 * @param state The global SHET state.
 * @param path The null-terminated SHET path name to raise an event for. This
 *             must remain live until the call returns.
 * @param value A null-terminated, comman-seperated string of JSON values
 *              representing the event value(s) or NULL if there is no value.
 *              This must remain live until the call returns.
 * @param deferred A pointer to a deferred_t struct responsible for event
 *                 raise callbacks. This struct must remain live until one of
 *                 its callbacks is called or shet_cancel_deferred is called by
 *                 the user. The deferreds associated with the creation of the
 *                 event may be reused if desired. If NULL no callbacks will
 *                 be registered for the event's removal.
 * @param callback Callback function on successful event raise. After this
 *                 occurrs, the deferred can be considered cancelled. NULL if
 *                 unused.
 * @param err_callback Callback function on unsuccessful event raise. After
 *                     this occurrs, the deferred can be considered cancelled.
 *                     NULL if unused.
 * @param callback_arg User-defined pointer to be passed to the event raise
 *                     callbacks.
 */
void shet_raise_event(shet_state_t *state,
                      const char *path,
                      const char *value,
                      shet_deferred_t *deferred,
                      shet_callback_t callback,
                      shet_callback_t error_callback,
                      void *callback_arg);

/**
 * Watch an event in the SHET tree.
 *
 * @param state The global SHET state.
 * @param path A valid, null-terminated SHET path name. This string must remain
 *             live as long as the event remains watched.
 * @param event_deferred A pointer to a deferred_t struct responsible for
 *                       event callbacks. This struct must remain live until
 *                       the event is removed.
 * @param event_callback Callback function called whenever the event fires.
 *                       This is given a (possibly empty) JSON array of values
 *                       indicating the value of the event. This function must
 *                       return success to SHET e.g. using shet_return. NULL if
 *                       unused.
 * @param created_callback Callback function called whenever the event is
 *                         created in the SHET tree. This function must return
 *                         success to SHET e.g. using shet_return. NULL if
 *                         unused. The passed JSON is undefined.
 * @param deleted_callback Callback function called whenever the event is
 *                         deleted in the SHET tree. This function must return
 *                         success to SHET e.g. using shet_return. NULL if
 *                         unused. The passed JSON is undefined.
 * @param event_arg User-defined pointer to be passed to the event callbacks.
 * @param watch_deferred A pointer to a deferred_t struct responsible for
 *                       event watching callbacks. This struct must remain live
 *                       until the event is ignored. If NULL no callbacks will
 *                       be registered for the events's watching.
 * @param watch_callback Callback function on successful event watching.
 *                       This may be called multiple times, for example, after
 *                       shet_reregister. NULL if unused.
 * @param watch_err_callback Callback function on unsuccessful event watching.
 *                           This may be called multiple times, for example,
 *                           after shet_reregister. NULL if unused.
 * @param watch_arg User-defined pointer to be passed to the event watching
 *                  callbacks.
 */
void shet_watch_event(shet_state_t *state,
                      const char *path,
                      shet_deferred_t *event_deferred,
                      shet_callback_t event_callback,
                      shet_callback_t created_callback,
                      shet_callback_t deleted_callback,
                      void *event_arg,
                      shet_deferred_t *watch_deferred,
                      shet_callback_t watch_callback,
                      shet_callback_t watch_error_callback,
                      void *watch_callback_arg);

/**
 * Ignore a watched event and cancel any associated deferreds.
 *
 * @param state The global SHET state.
 * @param path The null-terminated SHET path name of the event watched with
 *             shet_watch_event which will be ignored.
 * @param deferred A pointer to a deferred_t struct responsible for event ignore
 *                 callbacks. This struct must remain live until one of its
 *                 callbacks is called or shet_cancel_deferred is called by the
 *                 user. The deferreds associated with the creation of the
 *                 watch may be reused if desired. If NULL no callbacks will
 *                 be registered for the ignore.
 * @param callback Callback function on successful event ignore. After this
 *                 occurrs, the deferred can be considered cancelled. NULL if
 *                 unused.
 * @param err_callback Callback function on unsuccessful event ignore. After
 *                     this occurrs, the deferred can be considered cancelled.
 *                     NULL if unused.
 * @param callback_arg User-defined pointer to be passed to the event ignoring
 *                     callbacks.
 */
void shet_ignore_event(shet_state_t *state,
                       const char *path,
                       shet_deferred_t *deferred,
                       shet_callback_t callback,
                       shet_callback_t error_callback,
                       void *callback_arg);

#ifdef __cplusplus
}
#endif

#endif
