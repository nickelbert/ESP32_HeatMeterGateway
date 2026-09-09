#pragma once

#include <string>
#include "esp_http_server.h"
#include "esp_http_client.h"

class WebServer
{
private:
    httpd_handle_t m_serverHandle = nullptr;

    static std::string pendingPullUrl;
    static std::string pendingGithubToken;
    static std::string s_latestFoundVersion;
    static std::string s_latestDownloadUrl;
    static std::string s_latestReleaseUrl;
    static std::string s_latestReleaseTitle;
    static esp_err_t httpClientInitCb(esp_http_client_handle_t http_client);
    static esp_err_t rootGetHandler(httpd_req_t *req);
    static esp_err_t savePostHandler(httpd_req_t *req);
    static esp_err_t updatePostHandler(httpd_req_t *req);
    static esp_err_t pullUpdatePostHandler(httpd_req_t *req);
    static esp_err_t saveRepoPostHandler(httpd_req_t *req);
    static void restartTask(void *pvParameters);
    static void pullUpdateTask(void *pvParameters);
    static void updateCheckTask(void *pvParameters);

public:
    void setup();
    void stop();
    static void triggerUpdateCheck();
    static void installLatestUpdate();
};
