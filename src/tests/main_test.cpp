// doctest's own main() -- everything else under tests/ just #includes "doctest/doctest.h" and
// defines TEST_CASEs; keeping DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN isolated to this one file avoids
// pulling doctest's implementation into more than one translation unit.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest/doctest.h"
