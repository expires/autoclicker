#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")

#include "LLMClient.h"
#include <string>

// Escape a string for embedding in a JSON value
static std::string jsonEscape(const std::string& s)
{
    std::string out;
    out.reserve(s.size());
    for (unsigned char c : s)
    {
        switch (c)
        {
        case '"':  out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n";  break;
        case '\r': out += "\\r";  break;
        case '\t': out += "\\t";  break;
        default:
            if (c >= 0x20) out += static_cast<char>(c);
        }
    }
    return out;
}

// Extract first string value for key from flat/nested JSON (no full parser needed)
static std::string extractStr(const std::string& json, const std::string& key)
{
    std::string needle = "\"" + key + "\"";
    auto pos = json.find(needle);
    if (pos == std::string::npos) return {};
    pos = json.find(':', pos + needle.size());
    if (pos == std::string::npos) return {};
    pos = json.find('"', pos + 1);
    if (pos == std::string::npos) return {};
    ++pos;
    std::string result;
    while (pos < json.size() && json[pos] != '"')
    {
        if (json[pos] == '\\' && pos + 1 < json.size())
        {
            ++pos;
            switch (json[pos])
            {
            case '"':  result += '"';  break;
            case '\\': result += '\\'; break;
            case 'n':  result += '\n'; break;
            case 'r':  result += '\r'; break;
            case 't':  result += '\t'; break;
            default:   result += json[pos];
            }
        }
        else
        {
            result += json[pos];
        }
        ++pos;
    }
    return result;
}

namespace LLMClient
{

std::string Chat(const std::string& model,
                 const std::string& systemPrompt,
                 const std::string& userMessage)
{
    // Build Ollama /api/chat request body
    std::string body =
        "{\"model\":\"" + jsonEscape(model) + "\","
        "\"messages\":["
        "{\"role\":\"system\",\"content\":\"" + jsonEscape(systemPrompt) + "\"},"
        "{\"role\":\"user\",\"content\":\"" + jsonEscape(userMessage) + "\"}"
        "],\"stream\":false,\"format\":\"json\"}";

    HINTERNET hSession = WinHttpOpen(
        L"MCBot/1.0",
        WINHTTP_ACCESS_TYPE_NO_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0);
    if (!hSession) return {};

    DWORD timeout = 15000; // 15s
    WinHttpSetOption(hSession, WINHTTP_OPTION_CONNECT_TIMEOUT, &timeout, sizeof(timeout));
    WinHttpSetOption(hSession, WINHTTP_OPTION_SEND_TIMEOUT,    &timeout, sizeof(timeout));
    WinHttpSetOption(hSession, WINHTTP_OPTION_RECEIVE_TIMEOUT, &timeout, sizeof(timeout));

    HINTERNET hConn = WinHttpConnect(hSession, L"localhost", 11434, 0);
    if (!hConn)
    {
        WinHttpCloseHandle(hSession);
        return {};
    }

    HINTERNET hReq = WinHttpOpenRequest(
        hConn, L"POST", L"/api/chat",
        nullptr, WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
    if (!hReq)
    {
        WinHttpCloseHandle(hConn);
        WinHttpCloseHandle(hSession);
        return {};
    }

    BOOL ok = WinHttpSendRequest(
        hReq,
        L"Content-Type: application/json\r\n", (DWORD)-1L,
        const_cast<char*>(body.c_str()), (DWORD)body.size(),
        (DWORD)body.size(), 0);

    std::string response;
    if (ok && WinHttpReceiveResponse(hReq, nullptr))
    {
        DWORD avail = 0;
        while (WinHttpQueryDataAvailable(hReq, &avail) && avail > 0)
        {
            std::string chunk(avail, '\0');
            DWORD read = 0;
            WinHttpReadData(hReq, chunk.data(), avail, &read);
            chunk.resize(read);
            response += chunk;
            avail = 0;
        }
    }

    WinHttpCloseHandle(hReq);
    WinHttpCloseHandle(hConn);
    WinHttpCloseHandle(hSession);

    if (response.empty()) return {};

    // Ollama response: {"message":{"role":"assistant","content":"..."}, ...}
    // Extract the "content" field which holds the LLM's text output
    return extractStr(response, "content");
}

} // namespace LLMClient
