#ifndef LEXBORPP_TEST_FAKE_LEXBOR_H
#define LEXBORPP_TEST_FAKE_LEXBOR_H

#ifdef _WIN32
#if defined(FAKE_LEXBOR_BUILDING_SHARED)
#define FAKE_LEXBOR_API __declspec(dllexport)
#elif defined(FAKE_LEXBOR_SHARED)
#define FAKE_LEXBOR_API __declspec(dllimport)
#else
#define FAKE_LEXBOR_API
#endif
#else
#define FAKE_LEXBOR_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

FAKE_LEXBOR_API int lexborpp_test_fake_symbol(void);

#ifdef __cplusplus
}
#endif

#endif
