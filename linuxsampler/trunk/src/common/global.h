/***************************************************************************
 *                                                                         *
 *   LinuxSampler - modular, streaming capable sampler                     *
 *                                                                         *
 *   Copyright (C) 2003, 2004 by Benno Senoner and Christian Schoenebeck   *
 *   Copyright (C) 2005 - 2007 Christian Schoenebeck                       *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the Free Software           *
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston,                 *
 *   MA  02111-1307  USA                                                   *
 ***************************************************************************/

// All application global declarations that HAVE to be exposed to the C++
// API are defined here.

#ifndef __LS_GLOBAL_H__
#define __LS_GLOBAL_H__

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

#include <string>

typedef std::string String;

#if defined(WIN32)
#include <windows.h>
#define usleep(a) Sleep(a/1000)
#define sleep(a) Sleep(a*1000)
typedef unsigned int uint;
// FIXME: define proper functions which do proper alignement under Win32
#define alignedMalloc(a,b) malloc(b)
#define alignedFree(a) free(a)
#else
// needed for usleep under POSIX
#include <stdio.h>
// for uint
#include <sys/types.h>
#endif


#endif // __LS_GLOBAL_H__
