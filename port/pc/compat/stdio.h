/* Shadows the system <stdio.h> for the PC build.
 *
 * include/global.h includes <stdio.h>, but nothing in the game uses it -- the
 * GBA build has no standard I/O at all and only wanted NULL.  Homebrew GCC's
 * fixed copy of Apple's header pulls in <_bounds.h>, which the installed SDK
 * does not ship, so including the real one fails outright.
 *
 * Providing NULL and nothing else keeps the game sources untouched.  Anything
 * in the port that genuinely needs stdio includes it before this directory is
 * reached, or uses <cstdio>-free SDL equivalents.
 */
#ifndef WWPC_COMPAT_STDIO_H
#define WWPC_COMPAT_STDIO_H

#include <stddef.h>

#endif
