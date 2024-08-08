/* TinyBuf - simple dynamic array - public domain - Bernhard Schelling 2020
   https://github.com/schellingb/c-data-structures
         no warranty implied; use at your own risk

   This file implements stretchy buffers as invented (?) by Sean Barrett.
   Based on the implementation from the public domain Bitwise project
   by Per Vognsen - https://github.com/pervognsen/bitwise

   It's a super simple type safe dynamic array for C with no need
   to predeclare any type or anything.
   The first time an element is added, memory for 16 elements are allocated.
   Then every time length is about to exceed capacity, capacity is doubled.
   Can be used in C++ with POD types (without any constructor/destructor).

   Be careful not to supply modifying statements to the macro arguments.
   Something like BUF_REMOVE(buf, i--); would have unintended results.

   Sample usage:

   mytype_t* buf = NULL;
   BUF_PUSH(buf, some_element);
   BUF_PUSH(buf, other_element);
   -- now BUF_LEN(buf) == 2, buf[0] == some_element, buf[1] == other_element

   -- Free allocated memory:
   BUF_FREE(buf);
   -- now buf == NULL, BUF_LEN(buf) == 0, BUF_CAP(buf) == 0

   -- Explicitly increase allocated memory and set capacity:
   BUF_FIT(buf, 100);
   -- now BUF_LEN(buf) == 0, BUF_CAP(buf) == 100

   -- Resize buffer (does not initialize or zero memory!):
   BUF_RESIZE(buf, 200);
   -- now BUF_LEN(buf) == 200, BUF_CAP(buf) == 200

   -- Remove an element in the middle, keeping order:
   BUF_REMOVE(buf, 30);
   -- now BUF_LEN(buf) == 199, everything past 30 shifted 1 up

   -- Remove an element in the middle, swapping the last element into it:
   BUF_SWAPREMOVE(buf, 10);
   -- now BUF_LEN(buf) == 198, element 198 was copied into 10

   -- Insert an element into the middle of the array:
   BUF_INSERT(buf, 100, some_element);
   -- now BUF_LEN(buf) == 199, everything past 99 shifted 1 down

   -- Make a gap of a given size in the middle of the array:
   mytype_t* ptr_gap = BUF_MAKEGAP(buf, 20, 11);
   -- now BUF_LEN(buf) == 210, everything past 19 shifted 11 down

   -- Add multiple elements at the end (uninitialized):
   mytype_t* ptr1 = BUF_ADD(buf, 10);
   -- now BUF_LEN(buf) == 220, ptr1 points to buf[210]

   -- Add multiple elements at the end (zeroing memory):
   mytype_t* ptr2 = BUF_ADDZEROED(buf, 10);
   -- now BUF_LEN(buf) == 230, ptr2 points to buf[220]

   -- To handle running out of memory:
   bool ran_out_of_memory = !BUF_TRYFIT(buf, 1000);
   -- before RESIZE or PUSH. When out of memory, buf will stay unmodified.

   PUBLIC DOMAIN (UNLICENSE)

   This is free and unencumbered software released into the public domain.

   Anyone is free to copy, modify, publish, use, compile, sell, or
   distribute this software, either in source code form or as a compiled
   binary, for any purpose, commercial or non-commercial, and by any
   means.

   In jurisdictions that recognize copyright laws, the author or authors
   of this software dedicate any and all copyright interest in the
   software to the public domain. We make this dedication for the benefit
   of the public at large and to the detriment of our heirs and
   successors. We intend this dedication to be an overt act of
   relinquishment in perpetuity of all present and future rights to this
   software under copyright law.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
   EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
   MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
   IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
   OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
   ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
   OTHER DEALINGS IN THE SOFTWARE.

   For more information, please refer to <http://unlicense.org/>
*/

#ifndef TINYBUF_H
#define TINYBUF_H

#include <stdlib.h> /* for malloc, realloc */
#include <string.h> /* for memmove, memset */
#include <stddef.h> /* for size_t */

/* Query functions */
#define BUF_LEN(b) ((b) ? BUF__HDR(b)->len : 0)
#define BUF_CAP(b) ((b) ? BUF__HDR(b)->cap : 0)
#define BUF_END(b) ((b) + BUF_LEN(b))
#define BUF_SIZEOF(b) ((b) ? BUF_LEN(b) * sizeof(*b) : 0)

/* Modifying functions */
#define BUF_FREE(b)       ((b) ? (free(BUF__HDR(b)), (*(void**)(&(b)) = (void*)0)) : 0)
#define BUF_FIT(b, n)     ((size_t)(n) <= BUF_CAP(b) ? 0 : (*(void**)(&(b)) = buf__grow((b), (size_t)(n), sizeof(*(b)))))
#define BUF_PUSH(b, val)  (BUF_FIT((b), BUF_LEN(b) + 1), (b)[BUF__HDR(b)->len++] = (val))
#define BUF_POP(b)        (b)[--BUF__HDR(b)->len]
#define BUF_RESIZE(b, sz) (BUF_FIT((b), (sz)), ((b) ? BUF__HDR(b)->len = (sz) : 0))
#define BUF_CLEAR(b)      ((b) ? BUF__HDR(b)->len = 0 : 0)
#define BUF_TRYFIT(b, n)  (BUF_FIT((b), (n)), (((b) && BUF_CAP(b) >= (size_t)(n)) || !(n)))

/* Utility functions */
#define BUF_SHIFT(b, to, fr, n) ((b) ? memmove((b) + (to), (b) + (fr), (n) * sizeof(*(b))) : 0)
#define BUF_REMOVE(b, idx)      (BUF_SHIFT(b, idx, (idx) + 1, --BUF__HDR(b)->len - (idx)))
#define BUF_SWAPREMOVE(b, idx)  (BUF_SHIFT(b, idx, --BUF__HDR(b)->len, 1))
#define BUF_MAKEGAP(b, idx, sz) (BUF_RESIZE((b), BUF_LEN(b)+(sz)), BUF_SHIFT(b, (idx)+(sz), idx, BUF_LEN(b)-(idx)-(sz)), (b)+(idx))
#define BUF_INSERT(b, idx, val) (*BUF_MAKEGAP(b, idx, 1) = (val))
#define BUF_ZERO(b, idx, n)     (memset((b)+(idx), 0, (n)*sizeof(*(b))))
#define BUF_ADD(b, n)           (BUF_FIT(b, BUF_LEN(b) + (n)), (b) + (BUF__HDR(b)->len += (n)) - (n))
#define BUF_ADDZEROED(b, n)     (BUF_FIT(b, BUF_LEN(b) + (n)), BUF_ZERO(b, BUF_LEN(b), n), (b) + (BUF__HDR(b)->len += (n)) - (n))

#define BUF__HDR(b) (((struct buf__hdr *)(b))-1)
struct buf__hdr { size_t len, cap; };

#ifdef __GNUC__
__attribute__((__unused__))
#elif defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable:4505) //unreferenced local function has been removed
#endif
static void *buf__grow(void *buf, size_t new_len, size_t elem_size)
{
	struct buf__hdr *new_hdr;
	size_t new_cap = 2 * BUF_CAP(buf), new_size;
	if (new_cap < new_len) new_cap = new_len;
	if (new_cap <      16) new_cap = 16;
	new_size = sizeof(struct buf__hdr) + new_cap*elem_size;
	if (buf)
	{
		new_hdr = (struct buf__hdr *)realloc(BUF__HDR(buf), new_size);
		if (!new_hdr)
			return buf; /* out of memory, return unchanged */
	}
	else
	{
		new_hdr = (struct buf__hdr *)malloc(new_size);
		if (!new_hdr)
			return (void*)0; /* out of memory */
		new_hdr->len = 0;
	}
	new_hdr->cap = new_cap;
	return new_hdr + 1;
}
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

#endif
