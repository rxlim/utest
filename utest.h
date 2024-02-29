#pragma once

#include <chrono>
#include <cmath>
#include <condition_variable>
#include <functional>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include <iostream>

struct ProofFailure
{
    std::string suite_name;
    std::string proof_name;
    std::string filename;
    uint32_t line_no;
    std::string test;
    std::string actual;
    std::string expected;
    std::string actual_str;
};

class BaseFixture;

#if !defined(API_utest)
#error(API_utest is not defined)
#endif

std::unordered_map<std::string, std::vector<std::function<void()>>>& utest_suites();
std::unordered_map<std::string, std::vector<std::unique_ptr<BaseFixture>>>& utest_suite_proofs();
std::string& utest_active_suite_name();
std::vector<ProofFailure>& proof_failures();
inline std::mutex failures_mutex;

bool register_suite_function(const char* name, std::function<void()> suite_function);

// Helper barrier with timeout support
class Barrier
{
public:
    Barrier(int32_t count) :
        count_(count)
    { }

    void arrive()
    {
        {
            std::lock_guard<std::mutex> lock(count_mutex_);
            count_ -= 1;
        }
        condition_.notify_all();
    }

    void wait(const std::chrono::milliseconds& timeout = std::chrono::milliseconds(1000000))
    {
        try {
            std::unique_lock<std::mutex> lock(count_mutex_);
            if (!condition_.wait_for(lock, timeout, [this]{ return count_ == 0; })) {
                throw std::runtime_error("Barrier timeout");
            }
        }
        catch (...) {
            throw std::runtime_error("Waiting on condition variable failed");
        }
    }

    template <typename... A>
    void arrive_and_wait(A&&... args)
    {
        arrive();
        wait(std::forward<A>(args)...);
    }

private:
    std::mutex count_mutex_;
    int32_t count_ = 0;
    std::condition_variable condition_;
};

class BaseFixture
{
public:
    virtual ~BaseFixture() {}

    std::function<void(BaseFixture*)> utest_wrapper;
    std::string utest_suite_name;
    std::string utest_proof_name;

    // TODO: Moving actual_str below test might make it easier to
    // read att the call site. 'test' expected 'actual_str' to be 'expected'
    // but was found to be 'actual'.
    void add_failure(const std::string& filename,
                     uint32_t line_no,
                     const std::string& test,
                     const std::string& actual,
                     const std::string& expected,
                     const std::string& actual_str)
    {
        std::lock_guard<std::mutex> lock(failures_mutex);
        proof_failures().emplace_back(ProofFailure{
            utest_suite_name,
            utest_proof_name,
            filename,
            line_no,
            test,
            actual,
            expected,
            actual_str
        });
    }

    bool utest_assert(bool pred,
                      const std::string& filename,
                      uint32_t line_no,
                      const std::string& test,
                      bool report_failure = true)
    {
        if (!pred && report_failure) {
            add_failure(filename, line_no, test, "false", "true", test);
        }
        return pred;
    }

    template <typename A, typename E>
    bool utest_assert_eq(const A& actual,
                         const E& expected,
                         const std::string& filename,
                         uint32_t line_no,
                         const std::string& actual_str,
                         const std::string& expected_str,
                         bool report_failure = true)
    {
        if (!(utest_cmp_eq(actual, expected))) {
            if (report_failure) {
                add_failure(filename,
                            line_no,
                            actual_str + " == " + expected_str,
                            to_string<A>(actual),
                            to_string<E>(expected),
                            actual_str);
            }
            return false;
        }
        return true;
    }

    template <typename A, typename B>
    bool utest_cmp_eq(const A& a, const B& b)
    {
        return a == b;
    }

    bool utest_cmp_eq(float a, float b)
    {
        return std::fabs(b - a) < 0.0001;
    }

    bool utest_cmp_eq(double a, double b)
    {
        return std::fabs(b - a) < 0.0001;
    }

    bool utest_assert_no_throw(std::function<void()> statement,
                               const std::string& filename,
                               uint32_t line_no,
                               bool report_failure = true)
    {
        try {
            statement();
            return true;
        }
        catch (...) {
            if (report_failure) {
                add_failure(filename,
                            line_no,
                            "try {...}",
                            "exception thrown",
                            "thrown",
                            "no exception");
            }
            return false;
        }
    }

    template <typename A>
    bool utest_assert_throw(std::function<void()> statement,
                            const std::string& filename,
                            uint32_t line_no,
                            const std::string& exception_str,
                            bool report_failure = true)
    {
        try {
            try {
                statement();
                if (report_failure) {
                    add_failure(filename,
                                line_no,
                                "try {...}",
                                "<none>",
                                exception_str,
                                "exception");
                }
                return false;
            }
            catch (const A& ex) {
                if (!(typeid(ex) == typeid(A))) {
                    // Caught base class, we were looking for some subclass
                    throw;
                }
                return true;
            }
        }
        catch (...) {
            // Wrong exception was thrown
            try {
                throw;
            }
            catch (const std::exception& ex) {
                // Let's catch std::exception explicitly
                // (because it's a common base class for exceptions)
                // to produce better errors using typeid on 'ex'
                if (report_failure) {
                    add_failure(filename,
                                line_no,
                                "try {...}",
                                typeid(ex).name(),
                                exception_str,
                                "exception");
                }
                return false;
            }
            catch (...) {
                if (report_failure) {
                    add_failure(filename,
                                line_no,
                                "try {...}",
                                "<unknown>",
                                exception_str,
                                "exception");
                }
                return false;
            }
        }
    }

    void sync_point(const std::string& name, int32_t count = 2)
    {
        auto key = name + std::to_string(count);
        {
            std::lock_guard<std::mutex> lock(barriers_mutex_);
            barriers_.try_emplace(key, count);
        }
        barriers_.at(key).arrive_and_wait();
    }

    using time_point = std::chrono::time_point<std::chrono::system_clock>;

    void mark_time(const std::string& name)
    {
        std::lock_guard<std::mutex> lock(time_points_mutex_);
        time_points_[name] = time_point::clock::now();
    }

    int64_t time_since_mark(const std::string& name)
    {
        std::lock_guard<std::mutex> lock(time_points_mutex_);
        if (time_points_.contains(name)) {
            return std::chrono::duration_cast<std::chrono::milliseconds>(
                time_point::clock::now() - time_points_[name]
            ).count();
        }
        else {
            return std::chrono::milliseconds::max().count();
        }
    }

    int64_t time_between_marks(const std::string& mark1,
                               const std::string& mark2)
    {
        if (!time_points_.contains(mark1)
            || !time_points_.contains(mark2)) {
            return std::chrono::milliseconds::max().count();
        }
        else {
            return std::chrono::duration_cast<std::chrono::milliseconds>(
                time_points_[mark2] - time_points_[mark1]
            ).count();
        }
    }


private:
    template <typename A>
    std::string to_string(const A& a)
    {
        std::stringstream ss;
        ss << a;
        return ss.str();
    }

    std::unordered_map<std::string, Barrier> barriers_;
    std::mutex barriers_mutex_;

    std::unordered_map<std::string, time_point> time_points_;
    std::mutex time_points_mutex_;
};

class Fixture : public BaseFixture
{
public:
    virtual void set_up() {}
    virtual void tear_down() {}
    std::function<void()> utest_proof;
};

class EmptyFixture : public Fixture { };

#define MODEL(suite_name) SUITE_GEN_UNIQUE(suite_name, __LINE__)
#define SUITE(suite_name) SUITE_GEN_UNIQUE(suite_name, __LINE__)
#define SUITE_GEN_UNIQUE(x, y) SUITE_INTERNAL(x, y)
#define SUITE_INTERNAL(suite_name, unique_line) \
    static void utest_suite ## unique_line(); \
    namespace {bool reg ## unique_line = register_suite_function(suite_name, utest_suite ## unique_line);} \
    static void utest_suite ## unique_line()

#define ENSURE(what) ENSURE_GIVEN(what, EmptyFixture)
#define ENSURE_GIVEN(what, given) \
    utest_suite_proofs()[utest_active_suite_name()].push_back(std::unique_ptr<BaseFixture>{static_cast<BaseFixture*>(new given{})}); \
    utest_suite_proofs()[utest_active_suite_name()].back()->utest_suite_name = utest_active_suite_name(); \
    utest_suite_proofs()[utest_active_suite_name()].back()->utest_proof_name = what; \
    utest_suite_proofs()[utest_active_suite_name()].back()->utest_wrapper = [&](BaseFixture* a) { \
        given* utest_fixture_ = static_cast<given*>(a); \
        utest_fixture_->set_up(); \
        utest_fixture_->utest_proof(); \
        utest_fixture_->tear_down(); \
    }; \
    static_cast<Fixture*>(utest_suite_proofs()[utest_active_suite_name()].back().get())->utest_proof = (std::function<void()>)[=, &fixture = *static_cast<given*>(utest_suite_proofs()[utest_active_suite_name()].back().get())]() \

#define ASSERT(pred) fixture.utest_assert((pred) ? true : false, __FILE__, __LINE__, #pred)
// Note: actual and expected might be expressions that need to be evaluated
// and can thus not be passed to utest_assert_eq directly.
#define ASSERT_EQ(actual, expected) ([&]{auto&& _utest_a=actual;auto&& _utest_e=expected; return fixture.utest_assert_eq(_utest_a, _utest_e, __FILE__,  __LINE__, #actual, #expected);}())
#define ASSERT_THROW(statement, exception) fixture.utest_assert_throw<exception>([&](){statement}, __FILE__, __LINE__, #exception)
#define ASSERT_NO_THROW(statement) fixture.utest_assert_no_throw([&](){statement}, __FILE__, __LINE__)

#define TRY_ASSERT(pred, timeoutms) ([&]{ \
    for (int32_t ta__t = 0; ta__t < (timeoutms/25); ++ta__t) { \
        if (fixture.utest_assert(pred ? true : false, __FILE__, __LINE__, #pred, false)) { \
            return true; \
        } \
        std::this_thread::sleep_for(std::chrono::milliseconds(25)); \
    } \
    return fixture.utest_assert(pred ? true : false, __FILE__, __LINE__, #pred);}())

#define TRY_ASSERT_EQ(actual, expected, timeoutms) ([&]{ \
    for (int32_t ta__t = 0; ta__t < (timeoutms/25); ++ta__t) { \
        auto&& _utest_a=actual; \
        auto&& _utest_e=expected; \
        if (fixture.utest_assert_eq(_utest_a, _utest_e, __FILE__,  __LINE__, #actual, #expected, false)) { \
            return true; \
        } \
        std::this_thread::sleep_for(std::chrono::milliseconds(25)); \
    } \
    auto&& _utest_a=actual; \
    auto&& _utest_e=expected; \
    return fixture.utest_assert_eq(_utest_a, _utest_e, __FILE__,  __LINE__, #actual, #expected);}())

#define TRY_ASSERT_THROW(statement, exception, timeoutms) ([&]{ \
    for (int32_t ta__t = 0; ta__t < (timeoutms/25); ++ta__t) { \
        if (fixture.utest_assert_throw<exception>([&](){statement}, __FILE__, __LINE__, #exception, false)) { \
            return true; \
        } \
        std::this_thread::sleep_for(std::chrono::milliseconds(25)); \
    } \
    return fixture.utest_assert_throw<exception>([&](){statement}, __FILE__, __LINE__, #exception);}())

#define TRY_ASSERT_NO_THROW(statement, timeoutms) ([&]{ \
    for (int32_t ta__t = 0; ta__t < (timeoutms/25); ++ta__t) { \
        if (fixture.utest_assert_no_throw([&](){statement}, __FILE__, __LINE__, false)) { \
            return true; \
        } \
        std::this_thread::sleep_for(std::chrono::milliseconds(25)); \
    } \
    return fixture.utest_assert_no_throw([&](){statement}, __FILE__, __LINE__);}())


