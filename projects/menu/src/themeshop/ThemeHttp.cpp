#include "ThemeHttp.hpp"

#include "core/DebugLog.hpp"

#include <curl/curl.h>
#include <curlpp/Easy.hpp>
#include <curlpp/Infos.hpp>
#include <curlpp/Options.hpp>
#include <curlpp/cURLpp.hpp>
#include <switch.h>

#include <cstdio>
#include <atomic>
#include <mutex>
#include <sstream>
#include <stdexcept>

namespace {

std::mutex g_themeHttpMutex;
bool g_nifmInitialized = false;
bool g_socketInitialized = false;
bool g_curlInitialized = false;
std::atomic<bool> g_cancelPendingRequests{false};

constexpr int kRequestAttemptCount = 2;

std::string resultToString(Result rc) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "0x%08X", (unsigned int)rc);
    return buf;
}

bool runtimeInitializedLocked() {
    return g_nifmInitialized && g_socketInitialized && g_curlInitialized;
}

void shutdownRuntimeLocked() {
    if (g_curlInitialized) {
        curl_global_cleanup();
        g_curlInitialized = false;
    }
    if (g_socketInitialized) {
        socketExit();
        g_socketInitialized = false;
    }
    if (g_nifmInitialized) {
        nifmExit();
        g_nifmInitialized = false;
    }
}

bool initializeRuntimeLocked() {
    if (runtimeInitializedLocked())
        return true;

    if (!g_nifmInitialized) {
        Result rc = nifmInitialize(NifmServiceType_User);
        if (R_FAILED(rc)) {
            DebugLog::log("[themeshop] nifmInitialize failed: %s", resultToString(rc).c_str());
            shutdownRuntimeLocked();
            return false;
        }
        g_nifmInitialized = true;
    }

    if (!g_socketInitialized) {
        Result rc = socketInitializeDefault();
        if (R_FAILED(rc)) {
            DebugLog::log("[themeshop] socketInitializeDefault failed: %s", resultToString(rc).c_str());
            shutdownRuntimeLocked();
            return false;
        }
        g_socketInitialized = true;
    }

    if (!g_curlInitialized) {
        CURLcode rc = curl_global_init(CURL_GLOBAL_DEFAULT);
        if (rc != CURLE_OK) {
            DebugLog::log("[themeshop] curl_global_init failed: %s", curl_easy_strerror(rc));
            shutdownRuntimeLocked();
            return false;
        }
        g_curlInitialized = true;
    }

    DebugLog::log("[themeshop] http runtime ready");
    return true;
}

void ensureInternetConnectionReady(const std::string& url) {
    NifmInternetConnectionStatus status = NifmInternetConnectionStatus_ConnectingUnknown1;
    u32 strength = 0;
    Result rc = nifmGetInternetConnectionStatus(nullptr, &strength, &status);
    if (R_FAILED(rc)) {
        throw std::runtime_error("nifmGetInternetConnectionStatus failed: " + resultToString(rc));
    }
    if (status != NifmInternetConnectionStatus_Connected) {
        throw std::runtime_error("Internet connection is not ready for " + url);
    }
}

void configureRequest(curlpp::Easy& request,
                      const std::string& url,
                      std::ostringstream& response,
                      const std::list<std::string>& headers) {
    request.setOpt<curlpp::options::Url>(url);
    request.setOpt<curlpp::options::FollowLocation>(true);
    request.setOpt<curlpp::options::NoSignal>(true);
    request.setOpt<curlpp::options::ConnectTimeout>(4L);
    request.setOpt<curlpp::options::Timeout>(30L);
    request.setOpt<curlpp::options::IpResolve>((long)CURL_IPRESOLVE_V4);
    request.setOpt<curlpp::options::UserAgent>(std::string("SwitchU/") + SWITCHU_VERSION);
    if (!headers.empty()) {
        request.setOpt<curlpp::options::HttpHeader>(headers);
    }
    request.setOpt<curlpp::options::WriteStream>(&response);
}

size_t appendResponse(char* data, size_t size, size_t count, void* userData) {
    const size_t bytes = size * count;
    auto* response = static_cast<std::string*>(userData);
    response->append(data, bytes);
    return bytes;
}

int cancelIfAppletHandoff(void*, curl_off_t, curl_off_t, curl_off_t, curl_off_t) {
    return g_cancelPendingRequests.load(std::memory_order_acquire) ? 1 : 0;
}

std::vector<std::uint8_t> performRequestBytes(const std::string& url,
                                              const std::list<std::string>& headers) {
    CURL* request = curl_easy_init();
    if (!request)
        throw std::runtime_error("Could not create HTTP request");

    std::string response;
    curl_easy_setopt(request, CURLOPT_URL, url.c_str());
    curl_easy_setopt(request, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(request, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(request, CURLOPT_CONNECTTIMEOUT, 4L);
    curl_easy_setopt(request, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(request, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4);
    const std::string agent = std::string("SwitchU/") + SWITCHU_VERSION;
    curl_easy_setopt(request, CURLOPT_USERAGENT, agent.c_str());
    curl_easy_setopt(request, CURLOPT_WRITEFUNCTION, appendResponse);
    curl_easy_setopt(request, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(request, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(request, CURLOPT_XFERINFOFUNCTION, cancelIfAppletHandoff);

    struct curl_slist* requestHeaders = nullptr;
    for (const auto& header : headers)
        requestHeaders = curl_slist_append(requestHeaders, header.c_str());
    if (requestHeaders)
        curl_easy_setopt(request, CURLOPT_HTTPHEADER, requestHeaders);

    const CURLcode result = curl_easy_perform(request);
    if (requestHeaders)
        curl_slist_free_all(requestHeaders);
    if (result != CURLE_OK) {
        curl_easy_cleanup(request);
        if (result == CURLE_ABORTED_BY_CALLBACK &&
            g_cancelPendingRequests.load(std::memory_order_acquire)) {
            throw std::runtime_error("HTTP request cancelled for system applet handoff");
        }
        throw std::runtime_error(std::string("HTTP request failed: ") + curl_easy_strerror(result));
    }

    long statusCode = 0;
    curl_easy_getinfo(request, CURLINFO_RESPONSE_CODE, &statusCode);
    curl_easy_cleanup(request);
    if (statusCode < 200 || statusCode >= 300) {
        throw std::runtime_error("HTTP error " + std::to_string(statusCode));
    }

    return std::vector<std::uint8_t>(response.begin(), response.end());
}

std::vector<std::uint8_t> performBytes(const std::string& url,
                                       const std::list<std::string>& headers) {
    std::lock_guard<std::mutex> lk(g_themeHttpMutex);

    std::string lastError = "Theme Shop HTTP request failed";
    for (int attempt = 1; attempt <= kRequestAttemptCount; ++attempt) {
        try {
            if (g_cancelPendingRequests.load(std::memory_order_acquire))
                throw std::runtime_error("HTTP request cancelled for system applet handoff");
            if (!initializeRuntimeLocked()) {
                throw std::runtime_error("Theme Shop HTTP runtime is unavailable");
            }

            ensureInternetConnectionReady(url);
            auto bytes = performRequestBytes(url, headers);
            if (attempt > 1) {
                DebugLog::log("[themeshop] request recovered on retry %d: %s", attempt, url.c_str());
            }
            return bytes;
        } catch (const std::exception& ex) {
            lastError = ex.what();
            DebugLog::log("[themeshop] request failed (%d/%d): %s -> %s",
                          attempt,
                          kRequestAttemptCount,
                          url.c_str(),
                          ex.what());
        } catch (...) {
            lastError = "Unknown HTTP error";
            DebugLog::log("[themeshop] request failed (%d/%d): %s -> unknown error",
                          attempt,
                          kRequestAttemptCount,
                          url.c_str());
        }

        if (g_cancelPendingRequests.load(std::memory_order_acquire))
            break;

        if (attempt < kRequestAttemptCount) {
            shutdownRuntimeLocked();
        }
    }

    throw std::runtime_error(lastError);
}

} // namespace

namespace themeshop::http {

bool initialize() {
    std::lock_guard<std::mutex> lk(g_themeHttpMutex);
    return initializeRuntimeLocked();
}

void shutdown() {
    std::lock_guard<std::mutex> lk(g_themeHttpMutex);
    shutdownRuntimeLocked();
}

bool isInitialized() {
    std::lock_guard<std::mutex> lk(g_themeHttpMutex);
    return runtimeInitializedLocked();
}

void cancelPendingRequests() {
    g_cancelPendingRequests.store(true, std::memory_order_release);
}

std::vector<std::uint8_t> getBytes(const std::string& url,
                                   const std::list<std::string>& headers) {
    return performBytes(url, headers);
}

std::uint64_t getToFile(const std::string& url,
                        const std::string& destinationPath,
                        const std::function<void(std::uint64_t, std::uint64_t)>& onProgress) {
    std::lock_guard<std::mutex> lk(g_themeHttpMutex);

    std::string lastError = "Theme package download failed";
    for (int attempt = 1; attempt <= kRequestAttemptCount; ++attempt) {
        if (!initializeRuntimeLocked())
            throw std::runtime_error("Theme Shop HTTP runtime is unavailable");
        ensureInternetConnectionReady(url);

        std::FILE* out = std::fopen(destinationPath.c_str(), "wb");
        if (!out)
            throw std::runtime_error("Could not open " + destinationPath + " for writing");

        std::uint64_t written = 0;
        bool writeFailed = false;
        try {
            curlpp::Easy request;
            std::ostringstream discard;              // configureRequest quer um fluxo
            configureRequest(request, url, discard, {});
            // Sem limite de tempo total: um pacote de dezenas de megabytes por
            // WiFi passa dos 30 segundos que serve para JSON. O tempo de conexao
            // continua valendo, entao um servidor fora do ar ainda falha rapido.
            request.setOpt<curlpp::options::Timeout>(0L);
            request.setOpt<curlpp::options::LowSpeedLimit>(1024L);
            request.setOpt<curlpp::options::LowSpeedTime>(30L);
            request.setOpt<curlpp::options::WriteFunction>(
                [&](char* buffer, std::size_t size, std::size_t count) -> std::size_t {
                    const std::size_t bytes = size * count;
                    if (std::fwrite(buffer, 1, bytes, out) != bytes) {
                        writeFailed = true;
                        return 0;                    // aborta a transferencia
                    }
                    written += bytes;
                    if (onProgress)
                        onProgress(written, 0);
                    return bytes;
                });
            request.perform();

            const long statusCode = curlpp::infos::ResponseCode::get(request);
            if (statusCode < 200 || statusCode >= 300)
                throw std::runtime_error("HTTP error " + std::to_string(statusCode));

            std::fclose(out);
            if (writeFailed)
                throw std::runtime_error("Could not write to " + destinationPath);
            return written;
        } catch (const std::exception& ex) {
            std::fclose(out);
            std::remove(destinationPath.c_str());    // nada pela metade fica no cartao
            lastError = ex.what();
            DebugLog::log("[themeshop] package download failed (%d/%d): %s -> %s",
                          attempt, kRequestAttemptCount, url.c_str(), lastError.c_str());
            if (attempt < kRequestAttemptCount)
                shutdownRuntimeLocked();
        }
    }

    throw std::runtime_error(lastError);
}

std::string getText(const std::string& url,
                    const std::list<std::string>& headers) {
    auto bytes = performBytes(url, headers);
    return std::string(bytes.begin(), bytes.end());
}

} // namespace themeshop::http
