#pragma once

#include <curl/curl.h>

#include <cstddef>
#include <stop_token>

#include "iptv/network/http_response.hpp"
#include "iptv/network/http_status_code.hpp"

namespace iptv::network::internal {

/**
 * @brief Unified context of the HTTP transfer currently in flight.
 *
 * Bundles what the libcurl callbacks need to know, without leaking libcurl types
 * or widening the public API.
 */
struct TransferContext {
  HttpResponse* response{nullptr};
  const std::stop_token* stop_token{nullptr};
  bool aborted_by_user{false};
};

/**
 * @brief Registers the body, header and progress callbacks on a curl easy handle.
 */
void registerTransferCallbacks(CURL* easy_handle);

/**
 * @brief Points the transfer callbacks of a curl easy handle at the context.
 */
void bindTransferContext(CURL* easy_handle, TransferContext& ctx);

/**
 * @brief Reports whether a failed attempt is worth retrying.
 * @param res The curl result code of the attempt.
 * @param status The HTTP status the attempt produced.
 * @return true for timeouts, connection failures, 5xx and 429; false otherwise.
 */
[[nodiscard]] bool isTransientError(CURLcode res, HttpStatusCode status) noexcept;

}  // namespace iptv::network::internal
