#include "base_error.h"

using namespace std;


  std::string static Backtrace(int skip)
  {
    void *callstack[128];
    const int nMaxFrames = sizeof(callstack) / sizeof(callstack[0]);
    char buf[1024];
    int nFrames = backtrace(callstack, nMaxFrames);
    char **symbols = backtrace_symbols(callstack, nFrames);

    std::ostringstream trace_buf;
    for (int i = skip; i < nFrames; i++)
    {
      // printf("%s\n", symbols[i]);

      Dl_info info;
      if (dladdr(callstack[i], &info) && info.dli_sname)
      {
        char *demangled = NULL;
        int status = -1;
        if (info.dli_sname[0] == '_')
          demangled = abi::__cxa_demangle(info.dli_sname, NULL, 0, &status);
        snprintf(buf,
                 sizeof(buf),
                 "%-3d %*p %s + %zd\n",
                 i,
                 int(2 + sizeof(void *) * 2),
                 callstack[i],
                 status == 0 ? demangled : info.dli_sname == 0 ? symbols[i] : info.dli_sname,
                 (char *)callstack[i] - (char *)info.dli_saddr);
        free(demangled);
      }
      else
      {
        snprintf(buf,
                 sizeof(buf),
                 "%-3d %*p %s\n",
                 i,
                 int(2 + sizeof(void *) * 2),
                 callstack[i],
                 symbols[i]);
      }
      trace_buf << buf;
    }
    free(symbols);
    if (nFrames == nMaxFrames)
      trace_buf << "[truncated]\n";
    return trace_buf.str();
  }

  void print_exception(const base_error &e, int level)
  {
    std::cerr << std::string(level, ' ') << "exception: " << e.what()
              << " message: " << e.GetMessage() << " e.StackTrace: " << e.GetStackTrace() << '\n';
    try
    {
      std::rethrow_if_nested(e);
    }
    catch (const base_error &e)
    {
      print_exception(e, level + 1);
    }
    catch (...)
    {
    }
  }

	base_error::base_error(const string &message, bool isTransient)
	{
		message_ = message;
		isTransient_ = isTransient;
		stack_trace_ = Backtrace(1);
	}