#ifndef EZSHET_INTERNAL_H
#define EZSHET_INTERNAL_H

/**
 * Force the pre-processor to expand the macro a large number of times.
 *
 * This is ueseful when you have a macro which evaluates to a valid macro
 * expression, for example:
 *
 *   #define A(x) x+1
 *   #define EMPTY
 *   #define NOT_QUITE_RIGHT(x) A EMPTY (x)
 *   NOT_QUITE_RIGHT(999)
 *
 * Here's what happens inside the C preprocessor:
 *
 * 1. It sees a macro "NOT_QUITE_RIGHT" and performs a single macro expansion
 *    pass on its arguments. Since the argument is "999" and this isn't a macro,
 *    this is a boring step and it results in "NOT_QUITE_RIGHT(999)", i.e. no
 *    change.
 * 2. It re-writes macro for its definition, substituting in arguments. In this
 *    case we get "A EMPTY (999)"
 * 3. The preprocessor runs a final pass of macro expansion on this yeilding "A
 *    (999)" by expanding "EMPTY" into "".
 *
 * Unfortunately, this doesn't quite meet expectations since you may expect this
 * to in turn be expanded to "999+1". We can force the macro processor to make
 * another pass by abusing the first step of macro exapnsion: the preprocessor
 * handles arguments in their own pass. If we define a macro which does nothing
 * except produce its arguments e.g.:
 *
 *   #define DO_NOTHING(...) __VA_ARGS__
 *
 * We can now do "DO_NOTHING(NOT_QUITE_RIGHT(999))" causing "NOT_QUITE_RIGHT" to be
 * expanded to "A (999)" in its own pass thanks it it being an argument. Next,
 * when the second step occurrs, we just get "A (999)" again. In the final step,
 * the preprocessor does a pass over this finally expanding to "999+1" as we
 * always wanted.
 *
 * The EVAL defined below is essentially "DO_NOTHING(DO_NOTHING(DO_NOTHING(..."
 * which results in the preprocessor making a large number of passes. Given most
 * (useful) recursive macros we could write will terminate after only a few
 * levels of recursion, this should be sufficient for most programs. Still, true
 * recursion is still not possible!
 */
#define EVAL(...) EVAL1024(__VA_ARGS__)
#define EVAL1024(...) EVAL512(EVAL512(__VA_ARGS__))
#define EVAL512(...) EVAL256(EVAL256(__VA_ARGS__))
#define EVAL256(...) EVAL128(EVAL128(__VA_ARGS__))
#define EVAL128(...) EVAL64(EVAL64(__VA_ARGS__))
#define EVAL64(...) EVAL32(EVAL32(__VA_ARGS__))
#define EVAL32(...) EVAL16(EVAL16(__VA_ARGS__))
#define EVAL16(...) EVAL8(EVAL8(__VA_ARGS__))
#define EVAL8(...) EVAL4(EVAL4(__VA_ARGS__))
#define EVAL4(...) EVAL2(EVAL2(__VA_ARGS__))
#define EVAL2(...) EVAL1(EVAL1(__VA_ARGS__))
#define EVAL1(...) __VA_ARGS__

/**
 * Concatenate the first argument with the remaing arguments treated as a
 * string.
 */
#define CAT(a, ...) PRIMITIVE_CAT(a, __VA_ARGS__)
#define PRIMITIVE_CAT(a, ...) a ## __VA_ARGS__

// ???
#define CHECK_N(x, n, ...) n
#define CHECK(...) CHECK_N(__VA_ARGS__, 0,)
#define PROBE(x) x, 1,

/**
 * Negate the boolean (given as 0 for false, anything else otherwise).
 *
 * When 0, _NOT_0 is found and the second argument is taken. When 1 (or any other
 * value) is used, the macro won't be found and will become a single argument to
 * CHECK. Since CHECK returns 0 if it has only one argument, this results in 0
 * being returned by not.
 *
 * ~ is used arbitrarily as a "filler" value for the first argument produced by
 * the _NOT_0. This value happens causes errors if it makes it into the final
 * output and is handy for this reason.
 */
// XXX?
#define NOT(x) CHECK(PRIMITIVE_CAT(NOT_, x))
#define NOT_0 PROBE(~)

#define COMPL(b) PRIMITIVE_CAT(COMPL_, b)
#define COMPL_0 1
#define COMPL_1 0

/**
 * Macro version of the famous "cast to bool" which takes anything and casts it
 * to 0 if it is 0 and 1 otherwise.
 */
#define BOOL(x) COMPL(NOT(x))


/**
 * Macro if statement. Usage:
 *
 *   IF(c)( \
 *     expansion when true, \
 *     expansion when false \
 *   )
 *
 * Here's how:
 *
 * 1. The preprocessor expands the arguments to _IF casting the condition to '0'
 *    or '1'.
 * 2. The casted condition is concatencated onto _IF_ giving _IF_0 or _IF_1.
 * 3. The _IF_0 and _IF_1 macros either return their second or first arguments
 *    respectively (e.g. they implement the "choice selection" part of the
 *    macro).
 * 4. Note that the true/false clauses are in the extra set of brackets; thus
 *    these become the arguments to _IF_0 or _IF_1 and thus a selection is made!
 */
#define IF(c) _IF(BOOL(c))
#define _IF(c) PRIMITIVE_CAT(_IF_,c)
#define _IF_0(t,f) f
#define _IF_1(t,f) t

//#define IIF(c) PRIMITIVE_CAT(IIF_, c)
//#define IIF_0(t, f) f
//#define IIF_1(t, f) t
//#define IF(c) IIF(BOOL(c))

#define EAT(...)
#define EXPAND(...) __VA_ARGS__
#define WHEN(c,...) IF(c)(EXPAND, EAT)

/**
 * Macro which checks if it has any arguments. Returns '0' if there are no
 * arguments, '1' otherwise.
 *
 * This macro works by trying to concatenate the first argument with
 * _END_OF_ARGUMENTS_. If the first argument is empty (e.g. not present) then
 * only _END_OF_ARGUMENTS_ will remain, otherwise it will be
 * _END_OF_ARGUMENTS_something_here. In the former case, the macro expands to a
 * 0 when it is expanded prior to BOOL being expanded. Otherwise, it will
 * expand to something other than 0 and BOOL will force this to '1'.
 */
#define HAS_ARGS(x, ...) BOOL(PRIMITIVE_CAT(_END_OF_ARGUMENTS_,x)())
#define _END_OF_ARGUMENTS_() 0

/**
 * Macro map/list comprehension. Usage:
 *
 *   MAP(op, ...)
 *
 * Produces a comma-seperated list of the result of op(arg) for each arg.
 *
 * Limitations: no input value may be empty.
 */
#define OBSTRUCT(id) id DEFER(EMPTY)()

#define EMPTY()
#define DEFER(id) id EMPTY()
#define FOR(op, cur_val, ...) \
  WHEN(cur_val 0)(op(cur_val)) \
  WHEN(__VA_ARGS__ 0)(OBSTRUCT(DEFER(_FOR))()(op, __VA_ARGS__))
  

#define _FOR() FOR

#define MAKE_HAPPY(x) happy_##x

EVAL(FOR(MAKE_HAPPY, 1,2,3,4))




#define FOREVER(x) x DEFER(FOREVER_INDIRECT)()(x)
#define FOREVER_INDIRECT() FOREVER

//EVAL(FOREVER(zzz))


#endif
