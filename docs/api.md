# 📚 API Reference

*(This page is a placeholder for the automated Doxygen API reference)*

Our source code is meticulously documented using the **Doxygen** standard format (`/** ... */`), following the Google C++ Style Guide rules.

### Automation Plan

In the future, we will integrate `breathe` or `moxygen` into MkDocs to automatically extract the Doxygen comments from headers like `http_response.hpp` and `http_client.hpp` and render them seamlessly here.

### Example: `HttpResponse` Payload

Our API is designed to be highly self-descriptive. For example, `HttpResponse` disables copying at the compiler level to enforce Zero-Copy:

```cpp
/**
 * @brief Represents the outcome of an HTTP request with strict move-only semantics.
 *
 * Enforces Zero-Copy architecture by deleting copy operations. It uses an explicit
 * pre-allocation model for the binary payload to avoid reallocations during large
 * MPEG-TS segment downloads.
 */
class HttpResponse {
 public:
  // No copies allowed to enforce move-only semantics
  HttpResponse(const HttpResponse&) = delete;
  HttpResponse& operator=(const HttpResponse&) = delete;
  
  // ...
};
```
