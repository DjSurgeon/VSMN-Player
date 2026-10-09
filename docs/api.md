# 📚 API Reference

*(This page is a placeholder for the automated Doxygen API reference)*

Our source code is meticulously documented using the **Doxygen** standard format (`/** ... */`), following the Google C++ Style Guide rules.

### Automation Plan

In the future, we will integrate `breathe` or `moxygen` into MkDocs to automatically extract the Doxygen comments from headers like `http_response.hpp`, `http_client.hpp`, and `iptv/decoder/*.hpp` and render them seamlessly here.

### Example: Decoder Interfaces and Error Handling

Our API is designed to be highly self-descriptive and explicit. For example, `DecoderException` and the `DecoderError` enum clearly type all failure states across the decoding plane:

```cpp
/**
 * @brief Represents specific errors that can occur during decoding.
 */
enum class DecoderError {
    CodecNotFound,        ///< The requested codec could not be found or loaded.
    AllocationFailed,     ///< Memory allocation failed during decoder initialization or operation.
    // ...
};
```

### Example: Zero-Copy `HttpResponse` Payload

`HttpResponse` disables copying at the compiler level to enforce Zero-Copy:

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
