/*
 *	PROGRAM:		Firebird RDBMS definitions
 *	MODULE:			fb_types.h
 *	DESCRIPTION:	Firebird's platform independent data types header
 *
 *  The contents of this file are subject to the Initial
 *  Developer's Public License Version 1.0 (the "License");
 *  you may not use this file except in compliance with the
 *  License. You may obtain a copy of the License at
 *  https://www.ibphoenix.com/about/firebird/idpl.
 *
 *  Software distributed under the License is distributed AS IS,
 *  WITHOUT WARRANTY OF ANY KIND, either express or implied.
 *  See the License for the specific language governing rights
 *  and limitations under the License.
 *
 *  The Original Code was created by Mike Nordell and Mark O'Donohue
 *  for the Firebird Open Source RDBMS project.
 *
 *  Copyright (c) 2001
 *       Mike Nordel <tamlin@algonet.se>
 *       Mark O'Donohue <mark.odonohue@ludwig.edu.au>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 *
 * 2002.02.15 Sean Leyne - Code Cleanup, removed obsolete "OS/2" port
 *
 */
#pragma once
#ifndef INCLUDE_FB_TYPES_H
#define INCLUDE_FB_TYPES_H

#include <climits>
#include <type_traits>

inline constexpr size_t SIZEOF_LONG = sizeof(long);

static_assert(SIZEOF_LONG == 4 || SIZEOF_LONG == 8,
    "compile_time_failure: sizeof_long must be either 4 or 8");

using SLONG = std::conditional_t<SIZEOF_LONG == 8, int, long>;
using ULONG = std::conditional_t<SIZEOF_LONG == 8, unsigned int, unsigned long>;

inline constexpr SLONG SLONG_MIN = []() {
    if constexpr (SIZEOF_LONG == 8) {
        return INT_MIN;
    }
    else {
        return LONG_MIN;
    }
    }();

inline constexpr SLONG SLONG_MAX = []() {
    if constexpr (SIZEOF_LONG == 8) {
        return INT_MAX;
    }
    else {
        return LONG_MAX;
    }
}();

/* Basic data types */

using SCHAR = char;
using UCHAR = unsigned char;

using SSHORT = short;
using USHORT = unsigned short;

#ifdef _WINDOWS
using SINT64 = __int64;
using FB_UINT64 = unsigned __int64;
#else
using SINT64 = long long int;
using FB_UINT64 = unsigned long long int;
#endif


/* Substitution of API data types */

using ISC_SCHAR = SCHAR;
using ISC_UCHAR = UCHAR;
using ISC_SHORT = SSHORT;
using ISC_USHORT = USHORT;
using ISC_LONG = SLONG;
using ISC_ULONG = ULONG;
using ISC_INT64 = SINT64;
using ISC_UINT64 = FB_UINT64;

#include "firebird/impl/types_pub.h"

using SQUAD = ISC_QUAD;

/*
 * TMN: some misc data types from all over the place
 */
struct vary
{
    USHORT vary_length;
    char   vary_string[1]; /* CVC: The original declaration used UCHAR. */
};

struct lstring
{
    ULONG	lstr_length;
    ULONG	lstr_allocated;
    UCHAR*	lstr_address;
};

using BOOLEAN = unsigned char;
using TEXT = char; /* To be expunged over time */
using BYTE = unsigned char; /* Unsigned byte - common */
using IPTR = intptr_t;
using U_IPTR = uintptr_t;

using FPTR_VOID = void(*)();
using FPTR_VOID_PTR = void(*)(void*);
using FPTR_INT = int(*)();
using FPTR_INT_VOID_PTR = int(*)(void*);
using FPTR_PRINT_CALLBACK = void(*)(void*, SSHORT, const char*);
/* Used for isc_version */
using FPTR_VERSION_CALLBACK = void(*)(void*, const char*);
/* Used for isc_que_events and internal functions */
using FPTR_EVENT_CALLBACK = void(*)(void*, USHORT, const UCHAR*);

/* The type of JRD's ERR_post, DSQL's ERRD_post & post_error,
 * REMOTE's move_error & GPRE's post_error.
 */
namespace Firebird {
    namespace Arg {
        class StatusVector;
    }
}
using ErrorFunction = void (*)(const Firebird::Arg::StatusVector& v);
// kept for backward compatibility with old private API (CVT_move())
using FPTR_ERROR = void (*)(ISC_STATUS, ...);

using RCRD_OFFSET = ULONG;
using RCRD_LENGTH = ULONG;
using FLD_LENGTH = USHORT;
/* CVC: internal usage. I suspect the only reason to return int is that
vmslock.cpp:LOCK_convert() calls VMS' sys$enq that may require this signature,
but our code never uses the return value. */
using lock_ast_t = int (*)(void*);

/* Number of elements in an array */
template <typename T, std::size_t N>
constexpr FB_SIZE_T FB_NELEM(const T(&)[N]) noexcept
{
    return static_cast<FB_SIZE_T>(N);
}

// Intl types
using CHARSET_ID = SSHORT;
using COLLATE_ID = SSHORT;
using TTYPE_ID = USHORT;

// Stream type, had to move it from dsql/Nodes.h due to circular dependencies.
using StreamType = ULONG;

// Alignment rule
template <typename T>
constexpr T FB_ALIGN(T n, uintptr_t b)
{
    return (T)((((uintptr_t)n) + b - 1) & ~(b - 1));
}

// Various object IDs (longer-than-32-bit)

using AttNumber = FB_UINT64;
using TraNumber = FB_UINT64;
using StmtNumber = FB_UINT64;
using CommitNumber = FB_UINT64;
using SnapshotHandle = ULONG;
using SavNumber = SINT64;

#endif /* INCLUDE_FB_TYPES_H */
