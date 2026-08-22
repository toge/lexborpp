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

  explicit document_id_index(lxb_dom_node_t* root) {
    build(root);
  }

  [[nodiscard]] lxb_dom_node_t* find(std::string_view id) const noexcept {
    auto it = map_.find(id);
    return it != map_.end() ? it->second : nullptr;
  }

  [[nodiscard]] bool contains(std::string_view id) const noexcept {
    return map_.contains(id);
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
        // emplace は既存キーでは失敗するため、重複 id は「最初の
        // 1 件勝ち残り」になる。
        map_.emplace(std::string(*id), node);
      }
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
  std::unordered_map<std::string, lxb_dom_node_t*,
                     transparent_string_hash, std::equal_to<>> map_;
};

}  // namespace lexborpp

#endif  // LEXBORPP_DOCUMENT_ID_INDEX_HPP_
