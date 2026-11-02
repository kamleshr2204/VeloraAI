# VeloraAI – OOP Guide (in simple words)

This guide explains **which C++ OOP concept is used where** in VeloraAI, and *why*.
Read it with the code open. Every concept below points to a real file.

---

## 1. The big picture

VeloraAI is a chatbot. The browser sends the conversation to our C++ program, our program
asks an AI model (like GPT) for an answer, lets the model use **tools** (web search,
calculator…), and streams the answer back.

The old (reference) project was written mostly with *functions*. VeloraAI is rebuilt with
*classes*, where each class has **one job**:

```
backend/
├── main.cpp                  Creates the objects and connects them. Nothing else.
├── core/
│   ├── Types.h               ChatMessage, Source, ToolResult (+ operator overloading)
│   ├── Exceptions.h/.cpp     VeloraError → ApiError, NetworkError, ToolError, ConfigError
│   └── Config.h/.cpp         All settings (API key, model, port)
├── util/
│   ├── StringUtil.h/.cpp     Small text helpers
│   ├── WebClient.h/.cpp      Downloads web pages safely
│   └── Cache.h               Generic cache  →  a class TEMPLATE
├── search/
│   ├── SearchEngine.h        ABSTRACT class  (interface)
│   ├── TavilySearch          ┐
│   ├── DuckDuckGoSearch      ├─ three CHILD classes
│   └── WikipediaSearch       ┘
├── tools/
│   ├── Tool.h/.cpp           ABSTRACT class: "something the AI can do"
│   ├── CalculatorTool        ┐
│   ├── DateTimeTool          │
│   ├── FetchUrlTool          ├─ four CHILD classes
│   ├── WebSearchTool         ┘   (uses a list of SearchEngine objects)
│   ├── ExpressionParser      The maths engine used by CalculatorTool
│   └── ToolRegistry          A "phone book" that stores all tools
├── ai/
│   ├── LLMProvider.h         ABSTRACT class: "something that can answer"
│   ├── OpenAICompatibleProvider → LocalServerProvider
│   ├── PromptBuilder         Writes the instructions given to the AI
│   └── Agent                 The conversation loop (ask → tool → ask → answer)
└── server/
    └── HttpServer            Web server: /health, /chat, and the web page
```

The three **abstract classes** (`Tool`, `SearchEngine`, `LLMProvider`) are the heart of the design.

---

## 2. The concepts

### 2.1 Class and object
A **class** is a blueprint; an **object** is one thing built from it.
`ExpressionParser` is a class; `ExpressionParser("2+3")` is an object that remembers
its text (`text_`) and its position (`pos_`). Data + the functions that use it live together.

`struct` is a class whose members are public by default. We use it for plain data bundles
(`ChatMessage`, `Source`) that have no rules to protect. → `core/Types.h`

### 2.2 Encapsulation (hiding data)
Data members are **private**; the outside world uses **getters**.
→ `core/Config.h`

```cpp
class Config {
public:
    const std::string& apiKey() const { return apiKey_; }   // read only
private:
    std::string apiKey_;                                    // nobody can change it from outside
};
```
Why? Nobody can accidentally overwrite the API key, and we can change how it is stored
later without touching the rest of the program.

### 2.3 Constructors, `explicit`, private constructor + static factory
* A **constructor** sets up an object when it is created.
* `WebClient(bool blockPrivate)` decides its behaviour in the constructor. It is marked
  `explicit` so `WebClient c = true;` is not allowed by accident. → `util/WebClient.h`
* `Config` has a **private** constructor. You must call `Config::fromEnvironment()`, which
  validates the values (a bad port number throws `ConfigError`). So an invalid `Config`
  can never exist. → `core/Config.cpp`
* **Initializer list** (`: apiKey_(...), model_(...)`) builds members directly instead of
  assigning later. → `ai/OpenAICompatibleProvider.cpp`

### 2.4 Inheritance (IS-A)
A child class gets everything from its parent.
* `ApiError`, `NetworkError`, `ToolError`, `ConfigError` all inherit `VeloraError`, which
  inherits `std::runtime_error`. So one `catch (const VeloraError&)` catches all of ours.
  → `core/Exceptions.h`
* `LocalServerProvider` inherits `OpenAICompatibleProvider` and changes **one** function
  (`isConfigured()`), because a local model needs no API key. → `ai/OpenAICompatibleProvider.h`

`using Base::Base;` ("inheriting constructors") saves us from retyping constructors.

### 2.5 Abstraction: abstract classes and pure virtual functions
An **abstract class** describes *what* something can do, not *how*. It has at least one
**pure virtual function** (`= 0`), so you cannot create an object of it. Every child
**must** implement those functions.

```cpp
class Tool {                                   // tools/Tool.h
public:
    virtual ~Tool() = default;
    virtual std::string name() const = 0;      // pure virtual: "every tool must have a name"
    virtual std::string describe(const json&) const = 0;
    ...
protected:
    virtual ToolResult run(const json& args) const = 0;   // "every tool must be able to run"
};
```
The other two abstract classes: `SearchEngine` (`search/SearchEngine.h`) and
`LLMProvider` (`ai/LLMProvider.h`).

### 2.6 Polymorphism (one call, many behaviours)
The `Agent` has a `const Tool*` and calls `tool->execute(args)`. It does **not** know if
that tool is the calculator or the search. At run time C++ looks at the real object and
runs *its* `run()`. This lookup uses the **vtable** (a hidden table of function pointers
inside every object that has virtual functions).

```cpp
const Tool* tool = tools_.find(call.name);   // could be ANY child   (ai/Agent.cpp)
ToolResult result = tool->execute(args);     // correct child's run() is chosen at run time
```
Same idea in three more places:
* `provider_->complete(...)` – OpenAI today, a local model tomorrow, a **fake** in tests.
* `engine->search(...)` in `WebSearchTool` – Tavily, DuckDuckGo or Wikipedia.
* `tool->definition()` for every tool in `ToolRegistry::definitions()`.

> **Why it matters:** to add a new tool you write ONE new class. You change **no** existing
> code in `Agent`. (This is the *Open/Closed Principle*: open for extension, closed for modification.)

### 2.7 `virtual` destructor
In every base class we write `virtual ~Tool() = default;`.
Without it, `delete basePointer` would only run the *base* destructor and the child's
cleanup would be skipped (undefined behaviour). **Rule: a class with virtual functions
needs a virtual destructor.**

### 2.8 `override`, `final`, `protected`
* `override` – the compiler checks that you really replace a base-class function. A typo in
  the name becomes a compile error instead of a silent bug.
* `final` – `class CalculatorTool final : public Tool` means no one can inherit from it.
* `protected` – visible to child classes but hidden from the outside. `Tool::run()` is
  protected: only `execute()` (inside Tool) calls it.

### 2.9 Template Method pattern
`Tool::execute()` is a normal function that calls the virtual `run()` inside `try/catch`.
Children only write `run()` and automatically get the safety net. A tool can never crash
the whole server, whatever it throws. → `tools/Tool.cpp`

### 2.10 Composition (HAS-A) vs inheritance (IS-A)
Ask: "is it a kind of…?" → inheritance. "Does it own/use…?" → composition.

| Class | Relationship |
|---|---|
| `CalculatorTool` | **is a** `Tool` (inheritance) |
| `WebSearchTool` | **has** a list of `SearchEngine`s (composition) |
| `Agent` | **has** an `LLMProvider`, a `ToolRegistry`, a `PromptBuilder` |
| `TavilySearch` | **is a** `SearchEngine` and **has** a `WebClient` |

Prefer composition when you can; it keeps classes small and flexible.

### 2.11 Smart pointers, RAII and move semantics
**RAII** = a resource (memory, file, lock) is acquired in the constructor and released in
the destructor, so it is released **automatically**, even when an exception happens.
We never write `new`/`delete` by hand.

* `std::unique_ptr<T>` – exactly **one owner**. `ToolRegistry` owns its tools:
  `std::vector<std::unique_ptr<Tool>>`. We store **pointers to the base class**, because
  storing `Tool` objects directly would "slice" off the child part.
* `std::move(x)` – hand over ownership instead of copying (`registry.add(std::move(tool))`).
  A `unique_ptr` cannot be copied, only moved.
* `std::shared_ptr<T>` – several owners, deleted when the last one goes. Used in
  `HttpServer` so the conversation stays alive until streaming ends.
* `std::lock_guard<std::mutex>` – locks in the constructor, unlocks in the destructor. → `util/Cache.h`

### 2.12 Class templates (generic programming)
`Cache<K, V>` is written once and works for any key/value types:

```cpp
Cache<std::string, ToolResult> cache_;   // used by WebSearchTool
Cache<int, std::string> other;           // same code, different types (see tests)
```
The compiler generates a separate real class for each combination. → `util/Cache.h`

### 2.13 Operator overloading
We teach C++ what `<<` and `==` mean for our own types. → `core/Types.h`

```cpp
std::cout << ChatMessage{"user", "hi"};          // prints  [user] hi
Source{"A","u"} == Source{"B","u"}               // true: same URL
```
`Agent` uses `==` through `std::find` to avoid showing the same link twice.

### 2.14 Exceptions
A function that cannot do its job `throw`s; somebody who knows what to do `catch`es.
* Provider throws `ApiError` (wrong key) or `NetworkError` (no internet).
* `Agent::chat` catches `VeloraError` (the common parent!) and shows `e.what()` to the user.
* `ToolError` = an *expected* problem (blocked address); other `std::exception`s = bugs.
  `Tool::execute` formats them differently.

### 2.15 `static` members and `const`
* **static member function** belongs to the class, not an object: `DateTimeTool::now()`,
  `ApiError::fromResponse()`, `HttpServer::originAllowed()`. No object needed → easy to test.
* **const member function** (`std::string name() const`) promises not to change the object.
  Because `Agent::chat() const` changes nothing, many web requests can use **one** Agent at
  the same time safely.
* **mutable** – one controlled exception: `WebSearchTool::cache_` is `mutable` because
  `run()` is `const` but must still update the cache. (The cache has its own mutex.)

### 2.16 Dependency injection and the factory
* **Dependency injection**: `Agent` does not create its provider; `main()` passes it in.
  That is why tests can pass a `MockProvider` (fake AI without internet). → `tests/test_main.cpp`
* **Factory**: `createProvider(config)` and `ToolRegistry::createDefault(config)` decide
  which concrete classes to build and return base-class pointers.
* **Registry**: `ToolRegistry` = a lookup table name → tool.

---

## 3. Life of one message

You type: *"What is 1200 × 1.07⁵?"*

1. `script.js` sends `POST /chat` with the conversation.
2. `HttpServer` checks the origin, parses JSON (`parseConversation`), and calls `agent_.chat(...)`.
3. `Agent` builds the messages (system prompt from `PromptBuilder` + your history) and calls
   `provider_->complete(...)` — **polymorphic call**.
4. The model replies *"call `calculator` with `1200*(1.07)^5`"* instead of text.
5. `Agent` asks `ToolRegistry::find("calculator")` → gets a `const Tool*` →
   `tool->execute(args)` → **polymorphic call** → `CalculatorTool::run()` → `ExpressionParser`.
6. The result goes back to the model (as a `tool` message); `Agent` asks again.
7. The model writes the final answer; each piece is streamed to the browser as `delta` events.

---

## 4. Try it yourself: add a new tool in 5 steps

Goal: a `UnitConverterTool`. You will see that **no existing file needs changing** except one line in the factory.

1. Create `backend/tools/UnitConverterTool.h` – `class UnitConverterTool final : public Tool`.
2. Override `name()`, `description()`, `parameters()`, `describe()`.
3. Write `run()` – use `stringArg(args, "from")`; throw `ToolError` for bad units.
4. Add it to `ToolRegistry::createDefault`:
   `registry.add(std::make_unique<UnitConverterTool>());`
5. Add the `.cpp` to `CMakeLists.txt` and write a test in `tests/test_main.cpp`.

The AI will automatically see the new tool and can use it.

---

## 5. Likely viva questions

| Question | Short answer |
|---|---|
| Difference between abstraction and encapsulation? | Abstraction = show *what* (interface: `Tool`). Encapsulation = hide *data* (private members: `Config`). |
| What is a pure virtual function? | A virtual function with `= 0`; makes the class abstract; children must implement it. |
| Why a virtual destructor? | So deleting a child through a base pointer calls the child destructor too. |
| Compile-time vs run-time polymorphism? | Compile-time: templates, overloading (`Cache<K,V>`, `operator<<`). Run-time: virtual functions (`tool->execute`). |
| What is object slicing? | Copying a child into a base *object* cuts off the child part. We store pointers (`unique_ptr<Tool>`) to avoid it. |
| Why `unique_ptr` and not raw pointers? | Automatic delete (RAII), clear single ownership, no leaks even with exceptions. |
| Inheritance vs composition? | IS-A vs HAS-A. `CalculatorTool` is a `Tool`; `Agent` has a provider. |
| Why `override`? | Compiler catches mismatched signatures. |
| What does `const` after a function mean? | The function does not modify the object. |
| Where did you use exceptions? | `Exceptions.h`; provider throws, `Agent` catches `VeloraError`. |
| Where did you use templates? | `Cache<K,V>`. |
| How do you test without the internet? | Dependency injection: `MockProvider`, fake `SearchEngine`s. |

---

## 6. Reference project vs VeloraAI

| Reference (Nova) | VeloraAI |
|---|---|
| `Tools` = namespace of free functions, `if (name == "...")` chains | Abstract `Tool` + 4 child classes + `ToolRegistry` |
| `AIProvider` does config, prompt, HTTP streaming, tool loop | `Config`, `LLMProvider`, `PromptBuilder`, `Agent`, each with one job |
| Search fallback = nested `if (hits.empty())` | `SearchEngine` children tried in order |
| Errors = strings and `status` ints | Exception hierarchy |
| `main.cpp` has routes, CORS and parsing | `HttpServer` class; `main()` is ~10 lines |
| Cannot test without internet | 123 automated checks, fake AI and fake search |
