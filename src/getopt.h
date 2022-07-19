/*
  Copyright 2005-2014 Rich Felker, et al.

  This code is derived from software contributed to musl project
  by Rich Felker.
  
  Permission is hereby granted, free of charge, to any person obtaining
  a copy of this software and associated documentation files (the
  "Software"), to deal in the Software without restriction, including
  without limitation the rights to use, copy, modify, merge, publish,
  distribute, sublicense, and/or sell copies of the Software, and to
  permit persons to whom the Software is furnished to do so, subject to
  the following conditions:
  
  The above copyright notice and this permission notice shall be
  included in all copies or substantial portions of the Software.
  
  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
  CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
  TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
  SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#ifndef _GETOPT_H
#define _GETOPT_H

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_WIN32)
#include <tchar.h>
#else
#define TCHAR char
#endif

int getopt(int, TCHAR * const [], const TCHAR *);
extern TCHAR *optarg;
extern int optind, opterr, optopt, optreset;

struct option {
	const TCHAR *name;
	int has_arg;
	int *flag;
	int val;
};

int getopt_long(int, TCHAR *const *, const TCHAR *, const struct option *, int *);
int getopt_long_only(int, TCHAR *const *, const TCHAR *, const struct option *, int *);

#define no_argument        0
#define required_argument  1
#define optional_argument  2

#undef TCHAR

#ifdef __cplusplus
}
#endif

#endif
