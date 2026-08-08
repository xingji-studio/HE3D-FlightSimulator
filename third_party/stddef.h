#pragma once

#ifndef __SIZE_T
#define __SIZE_T
#endif

typedef __SIZE_TYPE__ size_t;
typedef __PTRDIFF_TYPE__ ptrdiff_t;

#ifndef SIZE_MAX
#define SIZE_MAX ((size_t)-1)
#endif

#ifndef NULL
#define NULL 0
#endif
