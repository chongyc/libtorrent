/*

Copyright (c) 2007, Arvid Norberg
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in
      the documentation and/or other materials provided with the distribution.
    * Neither the name of the author nor the names of its
      contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.

*/

#ifdef __GNUC__

#include <cxxabi.h>
#include <string>
#include <cstring>
#include <stdlib.h>

std::string demangle(char const* name)
{
// in case this string comes
	char const* start = strchr(name, '(');
	if (start != 0) ++start;
	else start = name;
	char const* end = strchr(start, '+');

	std::string in;
	if (end == 0) in.assign(start);
	else in.assign(start, end);

	size_t len;
	int status;
	char* unmangled = ::abi::__cxa_demangle(in.c_str(), 0, &len, &status);
	if (unmangled == 0) return in;
	std::string ret(unmangled);
	free(unmangled);
	return ret;
}

#endif

#ifndef NDEBUG

#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#if defined __linux__ && defined __GNUC__
#include <execinfo.h>
#endif

void assert_fail(char const* expr, int line, char const* file, char const* function)
{

	fprintf(stderr, "assertion failed. Please file a bugreport at "
		"http://code.rasterbar.com/libtorrent/newticket\n"
		"Please include the following information:\n\n"
		"file: '%s'\n"
		"line: %d\n"
		"function: %s\n"
		"expression: %s\n"
		"stack:\n", file, line, function, expr);

#if defined __linux__ && defined __GNUC__
	void* stack[50];
	int size = backtrace(stack, 50);
	char** symbols = backtrace_symbols(stack, size);

	for (int i = 0; i < size; ++i)
	{
		fprintf(stderr, "%d: %s\n", i, demangle(symbols[i]).c_str());
	}

	free(symbols);
#endif
 	// send SIGINT to the current process
 	// to break into the debugger
 	raise(SIGINT);
 	abort();
}

#else

void assert_fail(char const* expr, int line, char const* file, char const* function) {}

#endif

