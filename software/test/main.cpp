#define DOCTEST_CONFIG_IMPLEMENT  // REQUIRED: Enable custom main()
#include <doctest.h>

// TEST_CASE ...
// TEST_SUITE ...

int main(int argc, char **argv)
{
  doctest::Context context;

  // Keep successful assertions/tests in the output so PlatformIO's doctest
  // parser can collect them (with `success = false` it sees zero test cases).
  context.setOption("success", true);
  context.setOption("no-exitcode", true); // Do not return non-zero code on failed test case

  // YOUR CUSTOM DOCTEST OPTIONS

  context.applyCommandLine(argc, argv);
  return context.run();
}