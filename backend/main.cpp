// VeloraAI - program entry point.
//
// Notice how SHORT this file is. main() is the "composition root": its only job is to
// create the objects and connect them together. All real work lives inside the classes.
//
//   Config ──► createProvider() ──► LLMProvider (OpenAI / local model)
//   Config ──► ToolRegistry::createDefault() ──► tools (search, calculator, ...)
//                    both go into  ──► Agent ──► HttpServer ──► browser
#include "ai/Agent.h"
#include "ai/OpenAICompatibleProvider.h"
#include "core/Config.h"
#include "core/Exceptions.h"
#include "server/HttpServer.h"
#include "tools/ToolRegistry.h"

#include <iostream>

int main() {
    try {
        Config config = Config::fromEnvironment();

        Agent agent(createProvider(config), ToolRegistry::createDefault(config));
        HttpServer server(config, agent);

        return server.run() ? 0 : 1;
    } catch (const VeloraError& e) {  // our own, expected errors
        std::cerr << "VeloraAI could not start: " << e.what() << "\n";
    } catch (const std::exception& e) {  // anything else
        std::cerr << "Unexpected error: " << e.what() << "\n";
    }
    return 1;
}
