#include "Network.h"
#include "Config.h"
#include "Revision.h"
#include <Windows.h>
#include <cstdio>
#include <winhttp.h>

static std::string HttpsGet(LPCWSTR host, LPCWSTR path)
{
    std::string result;

    HINTERNET session = WinHttpOpen(L"AC/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session) return result;

    HINTERNET connect = WinHttpConnect(session, host, INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!connect) { WinHttpCloseHandle(session); return result; }

    HINTERNET request = WinHttpOpenRequest(connect, L"GET", path, nullptr,
        WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
    if (!request) { WinHttpCloseHandle(connect); WinHttpCloseHandle(session); return result; }

    DWORD timeout = 5000;
    WinHttpSetOption(request, WINHTTP_OPTION_CONNECT_TIMEOUT, &timeout, sizeof(timeout));
    WinHttpSetOption(request, WINHTTP_OPTION_RECEIVE_TIMEOUT, &timeout, sizeof(timeout));

    if (WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
        WINHTTP_NO_REQUEST_DATA, 0, 0, 0) && WinHttpReceiveResponse(request, nullptr))
    {
        char buf[4096];
        DWORD bytesRead = 0;
        while (WinHttpReadData(request, buf, sizeof(buf), &bytesRead) && bytesRead > 0)
        {
            result.append(buf, bytesRead);
            bytesRead = 0;
        }
    }

    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connect);
    WinHttpCloseHandle(session);
    return result;
}

static void HttpsPost(LPCWSTR host, LPCWSTR path, const std::string& body)
{
    HINTERNET session = WinHttpOpen(L"AC/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session) return;

    HINTERNET connect = WinHttpConnect(session, host, INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!connect) { WinHttpCloseHandle(session); return; }

    HINTERNET request = WinHttpOpenRequest(connect, L"POST", path, nullptr,
        WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
    if (!request) { WinHttpCloseHandle(connect); WinHttpCloseHandle(session); return; }

    DWORD timeout = 5000;
    WinHttpSetOption(request, WINHTTP_OPTION_CONNECT_TIMEOUT, &timeout, sizeof(timeout));
    WinHttpSetOption(request, WINHTTP_OPTION_RECEIVE_TIMEOUT, &timeout, sizeof(timeout));

    WinHttpAddRequestHeaders(request, L"Content-Type: application/json",
        (DWORD)-1, WINHTTP_ADDREQ_FLAG_ADD);

    WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
        (LPVOID)body.c_str(), (DWORD)body.size(), (DWORD)body.size(), 0);
    WinHttpReceiveResponse(request, nullptr);

    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connect);
    WinHttpCloseHandle(session);
}

static std::string jsonEscape(const std::string& s)
{
    std::string out;
    out.reserve(s.size() + 4);
    for (unsigned char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else {
                    out += (char)c;
                }
        }
    }
    return out;
}

static std::string discordEscape(const std::string& s)
{
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s) {
        switch (c) {
            case '_': case '*': case '~': case '`':
            case '|': case '>': case '\\': case '#':
            case ':':
                out += '\\';
                [[fallthrough]];
            default:
                out += c;
        }
    }
    return out;
}

namespace Network {
    bool IsBanned(const std::string& uuid)
    {
        std::string response = HttpsGet(GITHUB_BANNED_HOST, GITHUB_BANNED_PATH);
        if (response.empty()) return false;
        std::string needle = "\"" + uuid + "\"";
        return response.find(needle) != std::string::npos;
    }

    void ReportUser(const std::string& username, const std::string& uuid)
    {

        const std::string safeName = jsonEscape(discordEscape(username));
        const std::string safeUuid = jsonEscape(uuid);

        std::string body =
            "{\"username\":\"manuclicker | " BUILD_REVISION "\","
            "\"content\":\"**" + safeName + "** (`" + safeUuid + "`) loaded AC\"}";
        HttpsPost(DISCORD_WEBHOOK_HOST, DISCORD_WEBHOOK_PATH, body);
    }
}
