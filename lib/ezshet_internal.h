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
 * This macro will expand to nothing.
 */
#define EMPTY()

/**
 * Causes a function-style macro to require an additional pass to be expanded.
 *
 * This is useful, for example, when trying to implement recursion since the
 * recursive step must not be expanded in a single pass as the pre-processor
 * will catch it and prevent it.
 *
 * Usage:
 *
 *   DEFER1(IN_NEXT_PASS)(args, to, the, macro)
 *
 * How it works:
 *
 * 1. When DEFER1 is expanded, first its arguments are expanded which are
 *    simply IN_NEXT_PASS. Since this is a function-style macro and it has no
 *    arguments, nothing will happen.
 * 2. The body of DEFER1 will now be expanded resulting in EMPTY() being
 *    deleted. This results in "IN_NEXT_PASS (args, to, the macro)". Note that
 *    since the macro expander has already passed IN_NEXT_PASS by the time it
 *    expands EMPTY() and so it won't spot that the brackets which remain can be
 *    applied to IN_NEXT_PASS.
 * 3. At this point the macro expansion completes. If one more pass is made,
 *    IN_NEXT_PASS(args, to, the, macro) will be expanded as desired.
 */
#define DEFER1(id) id EMPTY()

/**
 * As with DEFER1 except here n additional passes are required for DEFERn.
 *
 * The mechanism is analogous.
 */
#define DEFER2(id) id EMPTY EMPTY()()
#define DEFER3(id) id EMPTY EMPTY EMPTY()()()
#define DEFER4(id) id EMPTY EMPTY EMPTY EMPTY()()()()
#define DEFER5(id) id EMPTY EMPTY EMPTY EMPTY EMPTY()()()()()
#define DEFER6(id) id EMPTY EMPTY EMPTY EMPTY EMPTY EMPTY()()()()()()
#define DEFER7(id) id EMPTY EMPTY EMPTY EMPTY EMPTY EMPTY EMPTY()()()()()()()
#define DEFER8(id) id EMPTY EMPTY EMPTY EMPTY EMPTY EMPTY EMPTY EMPTY()()()()()()()()


/**
 * Indirection around the standard ## concatenation operator. This simply
 * ensures that the arguments are expanded (once) before concatenation.
 */
#define CAT(a, ...) a ## __VA_ARGS__


/**
 * Get the second argument and ignore the rest.
 */
#define SECOND(x, n, ...) n

/**
 * Expects a single input (not containing commas). Returns 1 if the input is
 * PROBE() and otherwise returns 0.
 *
 * This can be useful as the basis of a NOT function.
 *
 * This macro abuses the fact that PROBE() contains a comma while other valid
 * inputs must not.
 */
#define IS_PROBE(...) SECOND(__VA_ARGS__, 0)
#define PROBE() ~, 1


/**
 * Logical negation. 0 is defined as false and everything else as true.
 *
 * When 0, _NOT_0 will be found which evaluates to the PROBE. When 1 (or any other
 * value) is given, an appropriately named macro won't be found and the
 * concatenated string will be produced. IS_PROBE then simply checks to see if
 * the PROBE was returned, cleanly converting the argument into a 1 or 0.
 */
#define NOT(x) IS_PROBE(CAT(_NOT_, x))
#define _NOT_0 PROBE()

/**
 * Macro version of C's famous "cast to bool" operator (i.e. !!) which takes
 * anything and casts it to 0 if it is 0 and 1 otherwise.
 */
#define BOOL(x) NOT(NOT(x))


/**
 * Macro if statement. Usage:
 *
 *   IF(c)(expansion when true)
 *
 * Here's how:
 *
 * 1. The preprocessor expands the arguments to _IF casting the condition to '0'
 *    or '1'.
 * 2. The casted condition is concatencated with _IF_ giving _IF_0 or _IF_1.
 * 3. The _IF_0 and _IF_1 macros either returns the argument or doesn't (e.g.
 *    they implement the "choice selection" part of the macro).
 * 4. Note that the "true" clause is in the extra set of brackets; thus these
 *    become the arguments to _IF_0 or _IF_1 and thus a selection is made!
 */
#define IF(c) _IF(BOOL(c))
#define _IF(c) CAT(_IF_,c)
#define _IF_0(t)
#define _IF_1(t) t

/**
 * Macro if/else statement. Usage:
 *
 *   IF_ELSE(c)( \
 *     expansion when true, \
 *     expansion when false \
 *   )
 *
 * The mechanism is analogous to IF.
 */
#define IF_ELSE(c) _IF_ELSE(BOOL(c))
#define _IF_ELSE(c) CAT(_IF_ELSE_,c)
#define _IF_ELSE_0(...)
#define _IF_ELSE_1(...) __VA_ARGS__


/**
 * Macro which checks if it has any arguments. Returns '0' if there are no
 * arguments, '1' otherwise.
 *
 * This macro works as follows:
 *
 * 1. The first argument is concatenated with _END_OF_ARGUMENTS_.
 * 2. If the first argument is not present then only _END_OF_ARGUMENTS_ will
 *    remain, otherwise _END_OF_ARGUMENTS_something_here will remain.
 * 3. In the former case, the _END_OF_ARGUMENTS_() macro expands to a
 *    0 when it is expanded. In the latter, a non-zero result remains.
 * 4. BOOL is used to force non-zero results into 1 giving the clean 0 or 1
 *    output required.
 */
#define HAS_ARGS(x, ...) BOOL(CAT(_END_OF_ARGUMENTS_, x)())
#define _END_OF_ARGUMENTS_() 0


/**
 * Macro map/list comprehension. Usage:
 *
 *   MAP(op, sep, ...)
 *
 * Produces a 'sep()'-separated list of the result of op(arg) for each arg. This
 * macro requires that it be expanded at least as many times as there are
 * elements in the input. This can be achieved trivially for large lists using
 * the EVAL macro.
 *
 * Example Usage:
 *
 *   #define MAKE_HAPPY(x) happy_##x
 *   #define COMMA() ,
 *   EVAL(MAP(MAKE_HAPPY, COMMA, 1,2,3))
 *
 * Which expands to:
 *
 *    happy_1 , happy_2 , happy_3
 *
 * How it works:
 *
 * 1. The MAP macro is substituted for its body.
 * 2. In the body, op(cur_val) is substituted giving the value for this
 *    iteration.
 * 3. The IF macro expands according to whether further iterations are required.
 *    This expansion either produces _IF_0 or _IF_1.
 * 4. Since the IF is followed by a set of brackets containing the "if true"
 *    clause, these become the argument to _IF_0 or _IF_1. At this point, the
 *    macro in the brackets will be expanded giving the separator followed by
 *    _MAP EMPTY()()(op, sep, __VA_ARGS__).
 * 4. If the IF was not taken, the above will simply be discarded and everything
 *    stops. If the IF is taken, The expression is then processed a second time
 *    yielding "_MAP()(op, sep, __VA_ARGS__)". Note that this call looks very
 *    similar to the  essentially the same as the original call except the first
 *    argument has been dropped.
 * 5. At this point expansion will terminate. However, since we can force
 *    more rounds of expansion using EVAL1. In the argument-expansion pass of
 *    the EVAL, _MAP() is expanded to MAP which is then expanded using the
 *    arguments which follow it as in step 1-4. This is followed by a second
 *    expansion pass as the substitution of EVAL1() is expanded executing 1-4 a
 *    second time. This results in up to two iterations occurring. Using many
 *    nested EVAL1 macros, or the very-deeply-nested EVAL macro, will in this
 *    manner produce further iterations.
 *
 * Important tricks used:
 *
 * * If we directly produce "MAP" in an expansion of MAP, a special case in the
 *   preprocessor will prevent it being expanded in the future, even if we EVAL.
 *   As a result, the MAP macro carefully only expands to something containing
 *   "_MAP()" which requires a further expansion step to invoke MAP and thus
 *   implementing the recursion.
 * * To prevent _MAP being expanded within the macro we must first defer its
 *   expansion during its initial pass as an argument to _IF_0 or _IF_1. We must
 *   then defer its expansion a second time as part of the body of the _IF_0. As
 *   a result hence the DEFER2.
 * * _MAP seemingly gets away with producing itself because it actually only
 *   produces MAP. It just happens that when _MAP() is expanded in this case it
 *   is followed by some arguments which get consumed by MAP and produce a _MAP.
 *   As such, the macro expander never marks _MAP as expanding to itself and
 *   thus it will still be expanded in future productions of itself.
 */
#define MAP(op,sep,cur_val, ...) \
  op(cur_val) \
  IF(HAS_ARGS(__VA_ARGS__))( \
    sep() DEFER2(_MAP)()(op, sep, __VA_ARGS__) \
  )
#define _MAP() MAP


#endif
