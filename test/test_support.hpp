#pragma once

#include <stdexcept>
#include <string_view>

#include "lexbor/html/html.h"
#include "lexborpp.hpp"

class html_document_fixture {
public:
  explicit html_document_fixture(std::string_view html) : document(lxb_html_document_create()) {
    if (document == nullptr) {
      throw std::runtime_error("Failed to create HTML document");
    }

    auto const status = lxb_html_document_parse(document, reinterpret_cast<lxb_char_t const*>(html.data()), html.size());
    if (status != LXB_STATUS_OK) {
      lxb_html_document_destroy(document);
      document = nullptr;
      throw std::runtime_error("Failed to parse HTML document");
    }
  }

  html_document_fixture(html_document_fixture const&) = delete;
  auto operator=(html_document_fixture const&) -> html_document_fixture& = delete;
  html_document_fixture(html_document_fixture&&) = delete;
  auto operator=(html_document_fixture&&) -> html_document_fixture& = delete;

  ~html_document_fixture() {
    if (document != nullptr) {
      lxb_html_document_destroy(document);
    }
  }

  [[nodiscard]] auto document_node() const noexcept -> lxb_dom_node_t* {
    return lxb_dom_interface_node(document);
  }

  [[nodiscard]] auto body_node() const noexcept -> lxb_dom_node_t* {
    return document == nullptr ? nullptr : lxb_dom_interface_node(lxb_html_document_body_element(document));
  }

  [[nodiscard]] auto by_id(std::string_view id) const noexcept -> lxb_dom_node_t* {
    return lexborpp::get_element_by_id(document_node(), id);
  }

private:
  lxb_html_document_t* document;
};
