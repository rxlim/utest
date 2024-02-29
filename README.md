# UTest

A C++ unittest framework.

## Usage example

```
#include "utest.h"

MODEL("Entity")
{
    ENSURE("That some condition is met")
    {
        ASSERT(true);
    };
}

int main(int argc, char* argv)
{
    return utest_main(argc, argv);
}

```


