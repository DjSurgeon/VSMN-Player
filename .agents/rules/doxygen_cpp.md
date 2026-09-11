# Doxygen C++ Coding Standard (Google Style)

This rule strictly defines how AI agents must document C++ code within the VSMN-Player repository. It follows the Google C++ Style Guide and industry standards.

## 1. Documentation Language
- **Mandatory:** All code comments, Doxygen blocks, commit messages, and internal documentation must be written in **English**, regardless of the language used in the chat or user prompts.

## 2. Public API (.hpp Files)
- Every public facing element (classes, structs, interfaces, public methods, enums) inside a header file (`.hpp`) MUST be documented using Doxygen blocks `/** ... */`.
- Use the `@brief` tag for the main description.
- Use `@param` for function arguments.
- Use `@return` to describe the return value if it is not `void`.
- Do not add Doxygen comments for overridden methods (`override`) if the base class interface already documents them, unless the implementation drastically changes the contract.

Example:
```cpp
/**
 * @brief Downloads data from the specified URL.
 *
 * @param url The endpoint to download from.
 * @param timeout The maximum time allowed for the request.
 * @return HttpResponse The result of the HTTP request.
 */
virtual HttpResponse download(const std::string& url, std::chrono::milliseconds timeout) = 0;
```

## 3. Implementation Details (.cpp Files)
- Private methods, internal logic, and variables inside `.cpp` files should NOT use Doxygen syntax (`/** */`).
- Use standard C++ single-line comments (`//`) to explain *why* complex implementation logic exists. 
- Avoid commenting on the *what* if the code is self-explanatory.

## 4. Execution Habit
- Whenever you (the agent) create or modify code, you must automatically verify the build and run tests (`ctest`) in the workspace to ensure you haven't broken the pipeline.
