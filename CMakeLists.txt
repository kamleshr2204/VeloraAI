cmake_minimum_required(VERSION 3.14)
project(VeloraAI CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)

# nlohmann/json: use the installed one, or download it automatically.
find_package(nlohmann_json 3.2.0 QUIET)
if(NOT nlohmann_json_FOUND)
  include(FetchContent)
  FetchContent_Declare(json GIT_REPOSITORY https://github.com/nlohmann/json.git GIT_TAG v3.11.3)
  FetchContent_MakeAvailable(json)
endif()

# Everything except main.cpp goes into a library, so the program AND the tests can use it.
add_library(velora_core STATIC
  backend/core/Config.cpp
  backend/core/Exceptions.cpp
  backend/util/StringUtil.cpp
  backend/util/WebClient.cpp
  backend/search/TavilySearch.cpp
  backend/search/DuckDuckGoSearch.cpp
  backend/search/WikipediaSearch.cpp
  backend/tools/Tool.cpp
  backend/tools/ExpressionParser.cpp
  backend/tools/CalculatorTool.cpp
  backend/tools/DateTimeTool.cpp
  backend/tools/FetchUrlTool.cpp
  backend/tools/WebSearchTool.cpp
  backend/tools/ToolRegistry.cpp
  backend/ai/OpenAICompatibleProvider.cpp
  backend/ai/PromptBuilder.cpp
  backend/ai/Agent.cpp
  backend/server/HttpServer.cpp)

target_include_directories(velora_core PUBLIC backend)
# Must be defined for EVERY file that includes httplib.h (PUBLIC passes it on to users of the library).
target_compile_definitions(velora_core PUBLIC CPPHTTPLIB_OPENSSL_SUPPORT)
target_link_libraries(velora_core PUBLIC OpenSSL::SSL OpenSSL::Crypto Threads::Threads nlohmann_json::nlohmann_json)
if(WIN32)
  target_link_libraries(velora_core PUBLIC ws2_32 crypt32)
endif()

# The program
add_executable(velora backend/main.cpp)
target_link_libraries(velora PRIVATE velora_core)
set_target_properties(velora PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/backend)

# The unit tests:  cmake --build build --target velora_tests && ./build/velora_tests
add_executable(velora_tests tests/test_main.cpp)
target_link_libraries(velora_tests PRIVATE velora_core)
