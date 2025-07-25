/*
 * %CopyrightBegin%
 *
 * SPDX-License-Identifier: Apache-2.0
 * 
 * Copyright Ericsson AB 2002-2025. All Rights Reserved.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 * 
 * %CopyrightEnd%
 *

 */

#ifndef _EIDEF_H
#define _EIDEF_H

/* Common definitions used in ei user interface */

#include "config.h"		/* Central include of config.h */

#include <stddef.h>		/* We want to get definition of NULL */

#include "ei.h"			/* Want the API function declarations */

#define EISMALLBUF 2048

#ifndef HAVE_SOCKLEN_T
typedef int socklen_t;
#endif

#ifdef USE_ISINF_ISNAN		/* simulate finite() */
#  define isfinite(f) (!isinf(f) && !isnan(f))
#  define HAVE_ISFINITE
#elif defined(__GNUC__) && defined(HAVE_FINITE)
/* We use finite in gcc as it emits assembler instead of
   the function call that isfinite emits. The assembler is
   significantly faster. */
#  ifdef isfinite
#     undef isfinite
#  endif
#  define isfinite finite
#  ifndef HAVE_ISFINITE
#    define HAVE_ISFINITE
#  endif
#elif defined(isfinite) && !defined(HAVE_ISFINITE)
#  define HAVE_ISFINITE
#elif !defined(HAVE_ISFINITE) && defined(HAVE_FINITE)
#  define isfinite finite
#  define HAVE_ISFINITE
#endif

/* Taken from https://best.openssf.org/Compiler-Hardening-Guides/Compiler-Options-Hardening-Guide-for-C-and-C++.html#warn-about-implicit-fallthrough-in-switch-statements */
#ifdef __has_attribute
#  if __has_attribute(__fallthrough__)
#    define EI_FALLTHROUGH()                    __attribute__((__fallthrough__))
#  endif
#endif
#ifndef EI_FALLTHROUGH
# define EI_FALLTHROUGH()                    do {} while (0)  /* fallthrough */
#endif

typedef unsigned char  uint8;	/* FIXME use configure */
typedef unsigned short uint16;
typedef unsigned int   uint32;
typedef signed   char  int8;
typedef signed   short int16;
typedef signed   int   int32;

#ifdef DEBUG
#  define ASSERT(Cnd) ((void) ((Cnd) ? 1 : abort()))
#else
#  define ASSERT(Cnd)
#endif

/* EI_UNDEF can be used to silence false warnings about
 * "variable may be used uninitialized" while keeping the variable
 * marked as undefined by valgrind.
 */
#ifdef VALGRIND
#  define EI_UNDEF(V,I) do {} while(0)
#else
#  define EI_UNDEF(V,I) V = I
#endif

#endif /* _EIDEF_H */
