#include <lexborpp.hpp>

#ifdef LEXBORPP_LINK_PROBE
#include <fake.h>
#endif

#if defined(LEXBORPP_EXPECT_STATIC) && !defined(LEXBOR_STATIC)
#error "LEXBOR_STATIC was not propagated"
#endif
#if defined(LEXBORPP_EXPECT_SHARED) && defined(LEXBOR_STATIC)
#error "LEXBOR_STATIC leaked into shared consumer"
#endif

auto main() -> int {
#ifdef LEXBORPP_RUN_QUERY
  auto document = lexborpp::parse_html("<p id='ok'>ok</p>");
  if (!document) {
    return 1;
  }
  if (lexborpp::query_selector<"#ok">(lexborpp::get_root(*document)) ==
      nullptr) {
    return 2;
  }
#endif
#ifdef LEXBORPP_LINK_PROBE
  if (lexborpp_test_fake_symbol() != 0x5a) {
    return 3;
  }
#endif
  return 0;
}
