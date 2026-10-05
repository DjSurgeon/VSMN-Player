#include "internal/transfer_callbacks.hpp"

#include <charconv>
#include <string_view>
#include <system_error>

namespace iptv::network::internal {

namespace {

/**
 * @brief Reports whether the caller already asked the transfer to stop.
 * @param ctx Transfer context, flagged as aborted when the token has fired.
 * @return true when the token fired.
 */
bool consumeAbortRequest(TransferContext& ctx) noexcept {
  if (ctx.stop_token != nullptr && ctx.stop_token->stop_requested()) {
    ctx.aborted_by_user = true;
    return true;
  }
  return false;
}

/**
 * @brief Appends the body bytes libcurl delivered to the response buffer.
 * @return The number of bytes consumed, 0 to abort the transfer.
 */
std::size_t writeBodyChunk(char* ptr, std::size_t size, std::size_t nmemb,
                           TransferContext& ctx) noexcept {
  if (consumeAbortRequest(ctx)) {
    return 0;  // abort
  }

  const std::size_t total_size = size * nmemb;
  ctx.response->appendToBody(static_cast<const uint8_t*>(static_cast<const void*>(ptr)),
                             total_size);
  return total_size;
}

/**
 * @brief Parses a Content-Length value and pre-reserves the response buffer.
 */
void reserveBodyFromContentLength(const std::string_view val, TransferContext& ctx) {
  std::size_t content_length = 0;
  const auto [ptr, ec] = std::from_chars(val.data(), val.data() + val.size(), content_length);
  if (ec == std::errc{}) {
    ctx.response->reserveBody(content_length);
  }
}

/**
 * @brief Handles a Content-Length header line, if the line is one.
 * @param line The full header line including its trailing newline.
 * @param ctx Transfer context owning the response buffer.
 */
void handleContentLengthLine(const std::string_view line, TransferContext& ctx) {
  if (!line.starts_with("Content-Length:") && !line.starts_with("content-length:")) {
    return;
  }

  const size_t pos = line.find(':');
  if (pos == std::string_view::npos) {
    return;
  }

  std::string_view val = line.substr(pos + 1);
  const size_t first = val.find_first_not_of(" \t\r\n");
  if (first == std::string_view::npos) {
    return;
  }

  reserveBodyFromContentLength(val.substr(first), ctx);
}

std::size_t bodyCallback(char* ptr, std::size_t size, std::size_t nmemb, void* userdata) {
  if (userdata == nullptr) {
    return 0;
  }
  return writeBodyChunk(ptr, size, nmemb, *static_cast<TransferContext*>(userdata));
}

std::size_t headerCallback(char* buffer, std::size_t size, std::size_t nitems, void* userdata) {
  const std::size_t total = size * nitems;
  if (userdata == nullptr) {
    return total;
  }

  handleContentLengthLine(std::string_view(buffer, total),
                          *static_cast<TransferContext*>(userdata));
  return total;
}

int progressCallback(void* clientp) {
  if (clientp == nullptr) {
    return 0;
  }

  auto* ctx = static_cast<TransferContext*>(clientp);
  if (!consumeAbortRequest(*ctx)) {
    return 0;
  }
  return 1;  // Return non-zero to trigger CURLE_ABORTED_BY_CALLBACK
}

}  // namespace

void registerTransferCallbacks(CURL* easy_handle) {
  curl_easy_setopt(easy_handle, CURLOPT_WRITEFUNCTION, bodyCallback);
  curl_easy_setopt(easy_handle, CURLOPT_HEADERFUNCTION, headerCallback);
  curl_easy_setopt(easy_handle, CURLOPT_XFERINFOFUNCTION, progressCallback);
  curl_easy_setopt(easy_handle, CURLOPT_NOPROGRESS, 0L);
}

void bindTransferContext(CURL* easy_handle, TransferContext& ctx) {
  curl_easy_setopt(easy_handle, CURLOPT_WRITEDATA, &ctx);
  curl_easy_setopt(easy_handle, CURLOPT_HEADERDATA, &ctx);
  curl_easy_setopt(easy_handle, CURLOPT_XFERINFODATA, &ctx);
}

bool isTransientError(CURLcode res, HttpStatusCode status) noexcept {
  const auto code = static_cast<long>(status);
  return (res == CURLE_OPERATION_TIMEDOUT || res == CURLE_COULDNT_CONNECT ||
          res == CURLE_COULDNT_RESOLVE_HOST) ||
         (code >= 500 && code < 600) || status == HttpStatusCode::TooManyRequests;
}

}  // namespace iptv::network::internal
