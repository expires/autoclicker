#pragma once
#include <string>

namespace LLMClient
{
    // Synchronous POST to Ollama /api/chat at localhost:11434.
    // Returns the assistant content string, or empty on failure/timeout.
    std::string Chat(const std::string& model,
                     const std::string& systemPrompt,
                     const std::string& userMessage);
}
