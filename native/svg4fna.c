/* SVG4FNA - SVG Container and Renderer for FNA
 *
 * Copyright (c) 2024 Ethan Lee
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * In no event will the authors be held liable for any damages arising from
 * the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 * claim that you wrote the original software. If you use this software in a
 * product, an acknowledgment in the product documentation would be
 * appreciated but is not required.
 *
 * 2. Altered source versions must be plainly marked as such, and must not be
 * misrepresented as being the original software.
 *
 * 3. This notice may not be removed or altered from any source distribution.
 *
 * Ethan "flibitijibibo" Lee <flibitijibibo@flibitijibibo.com>
 *
 */

#include <SDL.h>

#define assert SDL_assert
#define printf SDL_Log

#define free SDL_free
#define malloc SDL_malloc
#define realloc SDL_realloc

#define memcpy SDL_memcpy
#define memset SDL_memset
#define strchr SDL_strchr
#define strcmp SDL_strcmp
#define strlen SDL_strlen
#define strncmp SDL_strncmp
#define strncpy SDL_strlcpy
#define strstr SDL_strstr
#define strtol SDL_strtol
#define strtoll SDL_strtoll

#define acos SDL_acos
#define acosf SDL_acosf
#define atan2f SDL_atan2f
#define ceil SDL_ceil
#define ceilf SDL_ceilf
#define cos SDL_cos
#define cosf SDL_cosf
#define exp SDL_exp
#define expf SDL_expf
#define floor SDL_floor
#define floorf SDL_floorf
#define fmod SDL_fmod
#define fmodf SDL_fmodf
#define ldexp SDL_scalbn
#define pow SDL_pow
#define powf SDL_powf
#define round SDL_round
#define roundf SDL_roundf
#define sin SDL_sin
#define sinf SDL_sinf
#define sqrt SDL_sqrt
#define sqrtf SDL_sqrtf
#define tan SDL_tan
#define tanf SDL_tanf

/* FIXME: Macro out fopen and friends */

#include "nanovg.c"
#include "nanosvg.c"
#include "nanovg_svg.c"
#include "nanovg_gpu.c"
