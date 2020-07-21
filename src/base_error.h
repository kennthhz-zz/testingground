#include <cxxabi.h>   // for __cxa_demangle
#include <dlfcn.h>    // for dladdr
#include <execinfo.h> // for backtrace
#include <stdio.h>
#include <stdlib.h>
#include <atomic>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <type_traits>

using namespace std;

std::string static Backtrace(int skip = 1);

class base_error : public std::exception
{
  public:
    base_error(const string &message, bool isTransient = false);

    string GetMessage() const noexcept { return message_; }

    string GetStackTrace() const noexcept { return stack_trace_; }

    bool IsTransient() const noexcept { return isTransient_; }

  protected:
    string message_;
    string stack_trace_;
    bool isTransient_;
};

void print_exception(const base_error &e, int level = 0);