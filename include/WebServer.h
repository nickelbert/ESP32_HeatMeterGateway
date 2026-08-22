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
    static esp_err_t httpClientInitCb(esp_http_client_handle_t http_client);
    static esp_err_t rootGetHandler(httpd_req_t *req);
    static esp_err_t savePostHandler(httpd_req_t *req);
    static esp_err_t updatePostHandler(httpd_req_t *req);
    static esp_err_t pullUpdatePostHandler(httpd_req_t *req);
    static void restartTask(void *pvParameters);
    static void pullUpdateTask(void *pvParameters);

public:
    void setup();
    void stop();
};
