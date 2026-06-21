#ifndef LEXBORPP_DOCUMENT_ID_INDEX_HPP_
#define LEXBORPP_DOCUMENT_ID_INDEX_HPP_

#include <string>
#include <string_view>
#include <unordered_map>

#include "lexbor/dom/dom.h"

#include "lexborpp/core.hpp"

namespace lexborpp {

class document_id_index {
public:
  document_id_index() = default;

  explicit document_id_index(lxb_dom_node_t* root) {
    build(root);
  }

  [[nodiscard]] lxb_dom_node_t* find(std::string_view id) const noexcept {
    auto key = std::string(id);
    auto it = map_.find(key);
    return it != map_.end() ? it->second : nullptr;
  }

  [[nodiscard]] bool contains(std::string_view id) const noexcept {
    auto key = std::string(id);
    return map_.contains(key);
  }

  [[nodiscard]] bool empty() const noexcept { return map_.empty(); }
  [[nodiscard]] auto size() const noexcept -> std::size_t { return map_.size(); }

  void rebuild(lxb_dom_node_t* root) {
    map_.clear();
    build(root);
  }

private:
  void build(lxb_dom_node_t* root) {
    if (root == nullptr) return;
    for (auto* node : node_walker{root}) {
      if (is_non_element_node(node)) continue;
      auto const id = get_attr_value(node, "id");
      if (id.has_value() && !id->empty()) {
        auto key = std::string(*id);
        if (!map_.contains(key)) {
          map_.emplace(std::move(key), node);
        }
      }
    }
  }

  std::unordered_map<std::string, lxb_dom_node_t*> map_;
};

}  // namespace lexborpp

#endif  // LEXBORPP_DOCUMENT_ID_INDEX_HPP_
