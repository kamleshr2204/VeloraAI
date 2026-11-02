// PromptBuilder: writes the "system prompt" - the instructions that tell the AI
// who it is and how to behave.
//
// It is its own small class so that:
//   * the Agent (conversation logic) stays separate from the wording of the prompt
//     (this is the "Single Responsibility Principle": one class, one job), and
//   * in Step 2 we can extend it with study modes (Explain / Quiz / Flashcards ...).
#ifndef VELORA_AI_PROMPTBUILDER_H
#define VELORA_AI_PROMPTBUILDER_H

#include <string>

class PromptBuilder {
public:
    std::string build() const;
};

#endif  // VELORA_AI_PROMPTBUILDER_H
