#ifndef LEXBORPP_RANGES_SPECIALIZATIONS_HPP_
#define LEXBORPP_RANGES_SPECIALIZATIONS_HPP_

#include <ranges>

#include "lexbor/dom/dom.h"

#include "lexborpp/core.hpp"
namespace std::ranges {

template <>
inline constexpr bool enable_borrowed_range<lexborpp::node_walker> = true;

template <>
inline constexpr bool enable_borrowed_range<lexborpp::node_sibling_walker> = true;

template <>
inline constexpr bool enable_borrowed_range<lexborpp::node_prev_sibling_walker> = true;

template <>
inline constexpr bool enable_borrowed_range<lexborpp::attr_walker> = true;

}  // namespace std::ranges

#endif  // LEXBORPP_RANGES_SPECIALIZATIONS_HPP_
