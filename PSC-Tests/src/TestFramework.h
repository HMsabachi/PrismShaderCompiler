#pragma once

#include <cstdio>
#include <string>
#include <string_view>
#include <vector>

namespace psc_test
{

struct TestCase
{
    const char* Name;
    void (*Fn)();
};

inline std::vector<TestCase>& Registry()
{
    static std::vector<TestCase> registry;
    return registry;
}

inline int& FailureCount()
{
    static int count = 0;
    return count;
}

inline std::string ToText(const std::string& value) { return value; }
inline std::string ToText(std::string_view value) { return std::string(value); }
inline std::string ToText(const char* value) { return value != nullptr ? value : "(null)"; }
inline std::string ToText(bool value) { return value ? "true" : "false"; }

template <typename T>
std::string ToText(const T& value) { return std::to_string(value); }

inline void ReportFailure(const char* file, int line, const std::string& message)
{
    std::printf("       %s:%d\n       %s\n", file, line, message.c_str());
    ++FailureCount();
}

inline int RunAll()
{
    int passed = 0;

    for (const TestCase& test : Registry())
    {
        const int before = FailureCount();
        test.Fn();

        if (FailureCount() == before)
        {
            ++passed;
            std::printf("[ OK ] %s\n", test.Name);
        }
        else
        {
            std::printf("[FAIL] %s\n", test.Name);
        }
    }

    std::printf("\n%d/%zu passed, %d failure(s)\n",
                passed, Registry().size(), FailureCount());

    return FailureCount() == 0 ? 0 : 1;
}

struct Registrar
{
    Registrar(const char* name, void (*fn)()) { Registry().push_back({name, fn}); }
};

} // namespace psc_test

#define PSC_TEST(name)                                                        \
    static void name();                                                       \
    [[maybe_unused]] static const psc_test::Registrar name##_registrar(#name, &name); \
    static void name()

#define PSC_CHECK(cond)                                                       \
    do {                                                                      \
        if (!(cond))                                                          \
            psc_test::ReportFailure(__FILE__, __LINE__, "CHECK failed: " #cond); \
    } while (0)

#define PSC_CHECK_EQ(actual, expected)                                        \
    do {                                                                      \
        const auto psc_actual = (actual);                                     \
        const auto psc_expected = (expected);                                 \
        if (!(psc_actual == psc_expected))                                    \
            psc_test::ReportFailure(__FILE__, __LINE__,                       \
                std::string("CHECK_EQ failed: " #actual " == " #expected      \
                            "\n         actual: ") + psc_test::ToText(psc_actual) \
                + "\n       expected: " + psc_test::ToText(psc_expected));    \
    } while (0)
