#pragma once

#include <string.h>
#include <assert.h>

#define COPY(Dest, Start, End) memcpy(Dest, Start, End - Start)

#define APPLY_TO_ARRAY(X, Macro) ({ typeof(X[0]) __attribute__((__unused__)) (*should_be_an_array)[NAIVE_ELEMENTSOF(X)] = &X; Macro; })
#define NAIVE_ELEMENTSOF(Arr) ( sizeof(Arr) / (sizeof((Arr)[0]) ?: 1) )

#define elementsof(Arr) APPLY_TO_ARRAY(Arr, NAIVE_ELEMENTSOF(Arr))
#define endof(Arr) APPLY_TO_ARRAY(Arr, (Arr) + sizeof (Arr))
#define lastof(Arr) APPLY_TO_ARRAY(Arr, (Arr)[sizeof (Arr) - 1])

/** Prints textual description in addtion to regular message.
 *  @note Implementation was copied from assert.h with slight formatting modification
 *  @todo Move to a separate assertion.h header
 *  @todo Use NDEBUG macro to disable assertions
 */
#define assertion(Msg, Expr)\
    ((void) sizeof ((Expr) ? 1 : 0), __extension__ ({\
      if (Expr)\
        ; /* empty */\
      else\
        __assert_fail(#Expr " (" Msg ")", __FILE__, __LINE__, __ASSERT_FUNCTION);\
}))
