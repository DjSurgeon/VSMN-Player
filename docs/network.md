# 🌐 Network Architecture (C++20)

The Network layer (`HttpClient`) of VSMN-Player is designed to be the bedrock of our streaming pipeline. It implements industrial-standard patterns to ensure extreme performance and network resilience.

---

## 1. Pimpl Idiom (Pointer to Implementation)

We extensively use the Pimpl Idiom (`class HttpClient::Impl`) to hide the internal details of *libcurl*.

* **Problem Solved:** Prevents "Dependency Leak". Including `#include <curl/curl.h>` in our public headers would force any file using `HttpClient` to drag the heavy *libcurl* header, drastically inflating compilation times.
* **Primary Benefit:** **Application Binary Interface (ABI) Stability**. The `HttpClient` object only contains a *Smart Pointer* (`std::unique_ptr<Impl>`). This guarantees the class size in memory will never change, regardless of how many private variables we add to the `Impl`.
* **Cold Path Optimization:** Global initialization resides in `http_init.cpp` and is deliberately not `inline` in the header. Since it's called once per app lifecycle (Cold Path), we prioritize avoiding the dependency leak over microsecond optimizations.

## 2. SRP Refactoring & Anonymous Namespaces

The monolithic networking logic is strictly broken down adhering to the Single Responsibility Principle (SRP).

* **Stateless Logic:** Mathematical functions like the Jitter RNG or transient error evaluation are locked inside an anonymous `namespace { ... }` in the `.cpp`.
  * **Why:** The compiler aggressively *inlines* this code and grants it *Internal Linkage*, making symbol collisions impossible across the project.
* **Declarative Orchestrator:** The public `download()` function is a clean, 20-line orchestrator devoid of raw C pointers, reading semantically top-to-bottom and delegating heavy lifting to the encapsulated `Impl`.

## 3. Zero-Copy Architecture & Rule of 5

For massive video transport (HLS/MPEG-TS), RAM is the bottleneck.

* **Move Semantics (Rule of 5):** The `HttpResponse` class explicitly deletes its copy constructors (`= delete`). The compiler will throw a fatal error if a developer accidentally attempts to copy a 10MB video segment by value in RAM. We force `std::move`.
* **Hot Path Inlining:** Short *Getters* and *Setters* of `HttpResponse` (e.g., `getBody()`) are implemented directly in the `.hpp` header.
  * **Why:** They are implicitly `inline`. When called thousands of times per second (*Hot Path*), we save the microsecond penalty of jumping to the `.cpp` file (Zero overhead).

## 4. Zero-Allocation Parsing & SIMD (`headerCallback`)

To prevent heap fragmentation during downloads, we prevent the `std::vector` from blindly growing (`.push_back`).

* **In-Flight Header Extraction:** We intercept the `Content-Length` header on the fly using `<charconv>` (`std::from_chars`) over a `std::string_view`. It is the fastest numeric parser in C++20 as it doesn't instantiate a single `std::string`.
* **Exact Pre-reservation:** Knowing the exact size, we invoke `.reserve()` before receiving the payload, guaranteeing **zero dynamic reallocations**.
* **Block Copy:** Instead of manual loops, we inject network packets into the vector using a `resize()` and a `noexcept` `std::memcpy()`, allowing the compiler to use vectorized instructions (SIMD/AVX) to maximize throughput.

## 5. Resilience: Full Jitter & Storm Control

* **Fail-Fast:** We classify errors strictly. A `404 Not Found` aborts immediately. We only retry on transient failures (`5xx`, timeouts).
* **Exponential Backoff with *Full Jitter*:** To prevent involuntary DDoS attacks (*Thundering Herd Problem*) when a server recovers. We use `thread_local std::mt19937` as an ultra-fast, lock-free PRNG to add "white noise" to the wait times.
