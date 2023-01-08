#pragma once

#include <string.h>
#include <assert.h>

#define COPY(Dest, Start, End) memcpy(Dest, Start, End - Start)

#define APPLY_TO_ARRAY(X, Macro) ({ typeof(X[0]) __attribute__((__unused__)) (*should_be_an_array)[NAIVE_ELEMENTSOF(X)] = &X; Macro; })
#define NAIVE_ELEMENTSOF(Arr) ( sizeof(Arr) / (sizeof((Arr)[0]) ?: 1) )

#define elementsof(Arr) APPLY_TO_ARRAY(Arr, NAIVE_ELEMENTSOF(Arr))
#define endof(Arr) APPLY_TO_ARRAY(Arr, (Arr) + sizeof (Arr))
#define lastof(Arr) APPLY_TO_ARRAY(Arr, (Arr)[sizeof (Arr) - 1])

/** Prints textual description in addtion to regular message
 */
#define assertion(Msg, Assertion) assert( ((void)(Msg""), Assertion) )
