#include <iostream>

#if !defined(UNICODE) || !defined(_UNICODE)
#error Windows error-message conversion requires Unicode in every configuration.
#endif

#if defined(MERIDIAN_EXPECT_RELEASE) && !defined(NDEBUG)
#error Release must retain NDEBUG; do not replace CMake release flag defaults.
#endif

#if defined(MERIDIAN_EXPECT_DEBUG) && !defined(_DEBUG)
#error Debug must use the debug runtime.
#endif

#if defined(_DLL)
#error Meridian and its static dependencies must use the same static runtime.
#endif

int main()
{
    std::cout << "Build configuration contract passed\n";
    return 0;
}
