#include "utest.h"

using namespace std::literals;


MODEL("Helpers")
{
    ENSURE("get_environment_variable returns proper values") {
        auto path = get_environment_variable("PATH");
        ASSERT(!path.empty());
    };
}


MODEL("Barrier")
{
    ENSURE("0-count barrier wait() with no arrive causes no timeout")
    {
        Barrier b(0);
        ASSERT_NO_THROW(
            b.wait();
        );
    };

    ENSURE("1-count barrier wait() with no arrive causes timeout")
    {
        Barrier b(1);
        ASSERT_THROW(
            b.wait(1s);, std::runtime_error
        );
    };

    ENSURE("1-count barrier arrive_and_wait causes no timeout")
    {
        Barrier b(1);
        ASSERT_NO_THROW(
            b.arrive_and_wait();
        );
    };
}

MODEL("BaseFixture")
{
    ENSURE("Suite name is properly set in proof's fixture")
    {
        ASSERT_EQ(fixture.utest_suite_name, "BaseFixture");
    };

    ENSURE("Proof name is properly set in proof's fixture")
    {
        ASSERT_EQ(fixture.utest_proof_name, "Proof name is properly set in proof's fixture");
    };

    ENSURE("utest_cmp_eq returns true for equal floats")
    {
        ASSERT(fixture.utest_cmp_eq(0.0f, 0.0f));
        ASSERT(fixture.utest_cmp_eq(1.0f, 1.0f));
        ASSERT(fixture.utest_cmp_eq(1000.0f, 1000.0f));
        ASSERT(fixture.utest_cmp_eq(10000.0f, 10000.0f));
        ASSERT(fixture.utest_cmp_eq(10000000.0f, 10000000.0f));
    };

    ENSURE("utest_cmp_eq returns true for equal double")
    {
        ASSERT(fixture.utest_cmp_eq(0.0, 0.0));
        ASSERT(fixture.utest_cmp_eq(1.0, 1.0));
        ASSERT(fixture.utest_cmp_eq(1000.0, 1000.0));
        ASSERT(fixture.utest_cmp_eq(10000.0, 10000.0));
        ASSERT(fixture.utest_cmp_eq(10000000.0, 10000000.0));
    };

    ENSURE("Time since mark is positive")
    {
        fixture.mark_time("T1");
        ASSERT(fixture.time_since_mark("T1") >= 0);
    };

    ENSURE("ASSERT_EQ can compare string literals")
    {
        ASSERT_EQ("test", "test");
    };

    ENSURE("ASSERT_EQ can compare u8string literals")
    {
        ASSERT_EQ(u8"test", u8"test");
    };

    ENSURE("ASSERT_EQ can compare floats")
    {
        ASSERT_EQ(1.0f, 1.0f);
    };

    ENSURE("ASSERT_EQ can compare ints")
    {
        ASSERT_EQ(3, 3);
    };

    ENSURE("ASSERT_EQ can compare doubles")
    {
        ASSERT_EQ(3.4, 3.4);
    };
}
