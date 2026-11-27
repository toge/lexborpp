#ifndef LEXBORPP_DOCUMENT_ID_INDEX_HPP_
#define LEXBORPP_DOCUMENT_ID_INDEX_HPP_

#include <string>
#include <string_view>
#include <unordered_map>

#include "lexbor/dom/dom.h"

#include "lexborpp/core.hpp"

namespace lexborpp {

/**
 * @brief 指定ルート配下の `id` 属性値からノードへの逆引きインデックスです。
 *
 * @warning このインデックスは構築時点のスナップショットです。構築後に
 *          DOM を編集（id の変更・ノードの削除など）しても追従しません。
 *          編集後は rebuild() で作り直してください。
 *          また、文書内で id が重複している場合、最初に見つかった
 *          ノードのみが登録されます。
 *          保持するノードポインタは元ドキュメントに紐づくため、
 *          ドキュメントを破棄した後の find() は無効なポインタを
 *          返します。インデックスの寿命は必ずドキュメントより
 *          短くしてください。
 */
class document_id_index {
public:
  document_id_index() = default;

  explicit document_id_index(lxb_dom_node_t const* root) {
    build(root);
  }

  [[nodiscard]] auto find(std::string_view id) const noexcept -> lxb_dom_node_t* {
    auto const it = map_.find(id);
    return it != map_.end() ? it->second.first : nullptr;
  }

  [[nodiscard]] auto is_ambiguous(std::string_view id) const noexcept -> bool {
    auto const it = map_.find(id);
    return it != map_.end() && it->second.ambiguous;
  }

  [[nodiscard]] auto covers_scope(lxb_dom_node_t const* scope) const noexcept -> bool {
    if (root_ == nullptr || scope == nullptr) return false;
    for (auto* node = const_cast<lxb_dom_node_t*>(scope);
         node != nullptr;
         node = node->parent) {
      if (node == root_) return true;
    }
    return false;
  }

  [[nodiscard]] bool contains(std::string_view id) const noexcept {
    return map_.contains(id);
  }

  [[nodiscard]] bool empty() const noexcept { return map_.empty(); }
  [[nodiscard]] auto size() const noexcept -> std::size_t { return map_.size(); }

  void rebuild(lxb_dom_node_t const* root) {
    map_.clear();
    build(root);
  }

private:
  struct index_entry {
    lxb_dom_node_t* first{};
    bool ambiguous{};
  };

  void build(lxb_dom_node_t const* root) {
    root_ = const_cast<lxb_dom_node_t*>(root);
    if (root_ == nullptr) return;
    map_.reserve(128); // ponytail: 1k idsで-26%、flat_hash_mapは依存増で却下
    for (auto* node : node_walker{root_}) {
      if (is_non_element_node(node)) continue;
      auto const id = get_attr_value(node, "id");
      if (!id.has_value() || id->empty()) continue;
      auto [entry, inserted] =
        map_.try_emplace(std::string{*id}, index_entry{node, false});
      if (!inserted) entry->second.ambiguous = true;
    }
  }

  // Transparent hash/equality enables lookup by std::string_view without
  // constructing a temporary std::string (avoids a per-lookup heap allocation).
  struct transparent_string_hash {
    using is_transparent = void;
    auto operator()(std::string_view sv) const noexcept -> std::size_t {
      return std::hash<std::string_view>{}(sv);
    }
  };
  std::unordered_map<std::string, index_entry,
                     transparent_string_hash, std::equal_to<>> map_;
  lxb_dom_node_t* root_{};
};

}  // namespace lexborpp

#endif  // LEXBORPP_DOCUMENT_ID_INDEX_HPP_
