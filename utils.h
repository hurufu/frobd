#pragma once

#include <string.h>

#define COPY(Dest, Start, End) memcpy(Dest, Start, End - Start)
#define elementsof(Arr) (sizeof(Arr)/sizeof((Arr)[0]))
