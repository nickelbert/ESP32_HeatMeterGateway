#include "WebServer.h"
#include "ConfigManager.h"
#include "TelnetServer.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_ota_ops.h"
#include "esp_http_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <map>
#include <sstream>
#include <cstring>
#include <cstdlib>

static const char *TAG = "WebServer";
extern ConfigManager ConfigManager;
extern TelnetServer TelnetServer;

static std::string pendingPullUrl = "";

static std::string urlDecode(const std::string &src)
{
    std::string result;
    result.reserve(src.length());

    for (size_t i = 0; i < src.length(); ++i)
    {
        if (src[i] == '+')
        {
            result += ' ';
        }
        else if (src[i] == '%' && i + 2 < src.length())
        {
            int hexVal = 0;
            if (sscanf(src.substr(i + 1, 2).c_str(), "%x", &hexVal) == 1)
            {
                result += static_cast<char>(hexVal);
                i += 2;
            }
            else
            {
                result += '%';
            }
        }
        else
        {
            result += src[i];
        }
    }
    return result;
}

static void parseFormBody(const std::string &body, std::map<std::string, std::string> &params)
{
    std::istringstream stream(body);
    std::string pair;

    while (std::getline(stream, pair, '&'))
    {
        size_t equalPos = pair.find('=');
        if (equalPos != std::string::npos)
        {
            std::string key = urlDecode(pair.substr(0, equalPos));
            std::string value = urlDecode(pair.substr(equalPos + 1));
            params[key] = value;
        }
    }
}

void WebServer::restartTask(void *pvParameters)
{
    vTaskDelay(pdMS_TO_TICKS(3000));
    ESP_LOGI(TAG, "Rebooting ESP32-C3 now...");
    esp_restart();
}

void WebServer::pullUpdateTask(void *pvParameters)
{
    ESP_LOGI(TAG, "Starting Pull-OTA download from: %s", pendingPullUrl.c_str());
    TelnetServer.telnetPrint("[OTA] Starting remote download from URL...\r\n");

    esp_http_client_config_t httpConfig = {};
    httpConfig.url = pendingPullUrl.c_str();
    httpConfig.timeout_ms = 15000;
    httpConfig.buffer_size = 2048;

    esp_http_client_handle_t client = esp_http_client_init(&httpConfig);
    if (client == nullptr)
    {
        ESP_LOGE(TAG, "Failed to initialize HTTP client for OTA");
        TelnetServer.telnetPrint("[OTA] Failed to init HTTP client\r\n");
        vTaskDelete(nullptr);
        return;
    }

    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err));
        TelnetServer.telnetPrint("[OTA] Failed to connect to server\r\n");
        esp_http_client_cleanup(client);
        vTaskDelete(nullptr);
        return;
    }

    int contentLength = esp_http_client_fetch_headers(client);
    ESP_LOGI(TAG, "Firmware size from server: %d bytes", contentLength);

    const esp_partition_t *updatePartition = esp_ota_get_next_update_partition(nullptr);
    if (updatePartition == nullptr)
    {
        ESP_LOGE(TAG, "No OTA partition available");
        esp_http_client_cleanup(client);
        vTaskDelete(nullptr);
        return;
    }

    esp_ota_handle_t otaHandle = 0;
    err = esp_ota_begin(updatePartition, OTA_WITH_SEQUENTIAL_WRITES, &otaHandle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_ota_begin failed: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        vTaskDelete(nullptr);
        return;
    }

    char rxBuffer[1024];
    int totalRead = 0;

    while (1)
    {
        int readBytes = esp_http_client_read(client, rxBuffer, sizeof(rxBuffer));
        if (readBytes < 0)
        {
            ESP_LOGE(TAG, "HTTP read error during OTA");
            esp_ota_abort(otaHandle);
            esp_http_client_cleanup(client);
            vTaskDelete(nullptr);
            return;
        }
        else if (readBytes == 0)
        {
            break;
        }

        err = esp_ota_write(otaHandle, rxBuffer, readBytes);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "esp_ota_write failed: %s", esp_err_to_name(err));
            esp_ota_abort(otaHandle);
            esp_http_client_cleanup(client);
            vTaskDelete(nullptr);
            return;
        }

        totalRead += readBytes;
    }

    esp_http_client_close(client);
    esp_http_client_cleanup(client);

    err = esp_ota_end(otaHandle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_ota_end failed: %s", esp_err_to_name(err));
        TelnetServer.telnetPrint("[OTA] Image validation failed\r\n");
        vTaskDelete(nullptr);
        return;
    }

    err = esp_ota_set_boot_partition(updatePartition);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_ota_set_boot_partition failed: %s", esp_err_to_name(err));
        TelnetServer.telnetPrint("[OTA] Failed to set boot partition\r\n");
        vTaskDelete(nullptr);
        return;
    }

    ESP_LOGI(TAG, "OTA successful (%d bytes written). Rebooting in 3 seconds...", totalRead);
    TelnetServer.telnetPrint("[OTA] Update successful. Rebooting in 3 seconds...\r\n");

    xTaskCreate(&WebServer::restartTask, "restartTask", 2048, nullptr, 5, nullptr);
    vTaskDelete(nullptr);
}

esp_err_t WebServer::rootGetHandler(httpd_req_t *req)
{
    const esp_partition_t *running = esp_ota_get_running_partition();
    std::string runningSlot = (running != nullptr) ? running->label : "unknown";

    std::string html = R"(<!DOCTYPE html><html><head><meta charset='utf-8'>
<meta name='viewport' content='width=device-width, initial-scale=1'>
<title>Heat Meter Configuration</title>
<style>
  body { font-family: Arial, sans-serif; background: #f4f6f8; margin: 0; padding: 20px; color: #333; }
  .card { max-width: 500px; margin: 0 auto 20px auto; background: #fff; padding: 25px; border-radius: 8px; box-shadow: 0 4px 12px rgba(0,0,0,0.1); }
  h2 { margin-top: 0; color: #222; }
  h3 { margin-top: 25px; border-bottom: 2px solid #eee; padding-bottom: 8px; color: #444; }
  label { font-weight: bold; display: block; margin-top: 15px; margin-bottom: 5px; color: #555; font-size: 14px; }
  input[type=text], input[type=password], input[type=number], input[type=file] { width: 100%; padding: 10px; box-sizing: border-box; border: 1px solid #ccc; border-radius: 4px; }
  .checkbox-label { margin-top: 15px; display: flex; align-items: center; gap: 10px; cursor: pointer; }
  button { margin-top: 20px; width: 100%; padding: 12px; background: #0066cc; color: white; border: none; border-radius: 4px; font-size: 16px; cursor: pointer; }
  button:hover { background: #0052a3; }
  button.secondary { background: #28a745; }
  button.secondary:hover { background: #218838; }
  .info { font-size: 13px; color: #666; margin-top: 5px; }
</style>
</head><body>
<div class='card'>
  <h2>Landis+Gyr T550 Gateway</h2>
  <div class='info'>Target: ESP32-C3 | Active Partition: <b>)" + runningSlot + R"(</b></div>

  <h3>System Configuration</h3>
  <form action='/save' method='POST'>
    <label>WiFi SSID</label>
    <input type='text' name='wifi_ssid' value=')" + ConfigManager.wifiSsid + R"('>
    <label>WiFi Password</label>
    <input type='password' name='wifi_pass' value=')" + ConfigManager.wifiPassword + R"('>
    <label>MQTT Broker (IP / Host)</label>
    <input type='text' name='mqtt_server' value=')" + ConfigManager.mqttServer + R"('>
    <label>MQTT Port</label>
    <input type='number' name='mqtt_port' value=')" + std::to_string(ConfigManager.mqttPort) + R"('>
    <label>MQTT User</label>
    <input type='text' name='mqtt_user' value=')" + ConfigManager.mqttUser + R"('>
    <label>MQTT Password</label>
    <input type='password' name='mqtt_pass' value=')" + ConfigManager.mqttPassword + R"('>
    <label>MQTT State Topic</label>
    <input type='text' name='mqtt_topic' value=')" + ConfigManager.mqttTopic + R"('>
    <label>Read Interval (seconds)</label>
    <input type='number' name='read_interval_s' value=')" + std::to_string(ConfigManager.readIntervalSeconds) + R"('>
    <label class='checkbox-label'>
      <input type='checkbox' name='dummy_mode' value='1' )" + (ConfigManager.dummyMode ? "checked" : "") + R"(>
      Dummy Mode (Emulate data via Telnet)
    </label>
    <button type='submit'>Save Configuration & Restart</button>
  </form>

  <h3>Firmware Update (Push)</h3>
  <form action='/update' method='POST' enctype='application/octet-stream'>
    <label>Upload firmware.bin</label>
    <input type='file' id='fileInput' onchange='uploadFile()'>
    <div class='info'>Uploads and flashes binary into the secondary partition.</div>
  </form>

  <h3>Remote Update (Pull)</h3>
  <form action='/pull_update' method='POST'>
    <label>Firmware URL (HTTP)</label>
    <input type='text' name='url' placeholder='http://192.168.1.50/firmware.bin'>
    <button type='submit' class='secondary'>Check & Pull Update</button>
  </form>
</div>

<script>
function uploadFile() {
  const file = document.getElementById('fileInput').files[0];
  if (!file) return;
  if (!confirm('Flash firmware ' + file.name + ' now?')) return;
  
  const xhr = new XMLHttpRequest();
  xhr.open('POST', '/update', true);
  xhr.setRequestHeader('Content-Type', 'application/octet-stream');
  xhr.onload = function() {
    if (xhr.status === 200) {
      document.body.innerHTML = '<h2>Flash Successful!</h2><p>Rebooting in 5 seconds...</p>';
      setTimeout(function() { window.location.href = '/'; }, 5000);
    } else {
      alert('Flash failed: ' + xhr.responseText);
    }
  };
  xhr.send(file);
}
</script>
</body></html>)";

    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, html.c_str(), html.length());
    return ESP_OK;
}

esp_err_t WebServer::savePostHandler(httpd_req_t *req)
{
    int totalLen = req->content_len;
    int received = 0;
    std::string postBody = "";
    char buf[128];

    while (received < totalLen)
    {
        int bytes = httpd_req_recv(req, buf, std::min((int)sizeof(buf) - 1, totalLen - received));
        if (bytes <= 0)
        {
            if (bytes == HTTPD_SOCK_ERR_TIMEOUT)
            {
                continue;
            }
            return ESP_FAIL;
        }
        buf[bytes] = '\0';
        postBody += buf;
        received += bytes;
    }

    std::map<std::string, std::string> params;
    parseFormBody(postBody, params);

    if (params.find("wifi_ssid") != params.end()) ConfigManager.wifiSsid = params["wifi_ssid"];
    if (params.find("wifi_pass") != params.end()) ConfigManager.wifiPassword = params["wifi_pass"];
    if (params.find("mqtt_server") != params.end()) ConfigManager.mqttServer = params["mqtt_server"];
    if (params.find("mqtt_port") != params.end()) ConfigManager.mqttPort = std::atoi(params["mqtt_port"].c_str());
    if (params.find("mqtt_user") != params.end()) ConfigManager.mqttUser = params["mqtt_user"];
    if (params.find("mqtt_pass") != params.end()) ConfigManager.mqttPassword = params["mqtt_pass"];
    if (params.find("mqtt_topic") != params.end()) ConfigManager.mqttTopic = params["mqtt_topic"];
    if (params.find("read_interval_s") != params.end()) ConfigManager.readIntervalSeconds = std::atoi(params["read_interval_s"].c_str());

    ConfigManager.dummyMode = (params.find("dummy_mode") != params.end() && params["dummy_mode"] == "1");

    ConfigManager.saveConfig();

    TelnetServer.telnetPrint("[Web] Configuration saved via web interface\r\n");
    ESP_LOGI(TAG, "Configuration updated via web interface. Scheduling reboot...");

    std::string response = "<!DOCTYPE html><html><head><meta charset='utf-8'><title>Saved</title></head><body>"
                           "<h2>Configuration Saved!</h2><p>Restarting ESP in 3 seconds...</p></body></html>";

    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, response.c_str(), response.length());

    xTaskCreate(&WebServer::restartTask, "restartTask", 2048, nullptr, 5, nullptr);

    return ESP_OK;
}

esp_err_t WebServer::updatePostHandler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "Push-OTA firmware upload started (%d bytes)", req->content_len);
    TelnetServer.telnetPrint("[OTA] Push firmware upload started...\r\n");

    const esp_partition_t *updatePartition = esp_ota_get_next_update_partition(nullptr);
    if (updatePartition == nullptr)
    {
        ESP_LOGE(TAG, "No OTA partition available");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    esp_ota_handle_t otaHandle = 0;
    esp_err_t err = esp_ota_begin(updatePartition, OTA_WITH_SEQUENTIAL_WRITES, &otaHandle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_ota_begin failed: %s", esp_err_to_name(err));
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    char buf[1024];
    int remaining = req->content_len;

    while (remaining > 0)
    {
        int bytes = httpd_req_recv(req, buf, std::min((int)sizeof(buf), remaining));
        if (bytes <= 0)
        {
            if (bytes == HTTPD_SOCK_ERR_TIMEOUT)
            {
                continue;
            }
            ESP_LOGE(TAG, "Connection lost during OTA upload");
            esp_ota_abort(otaHandle);
            httpd_resp_send_500(req);
            return ESP_FAIL;
        }

        err = esp_ota_write(otaHandle, buf, bytes);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "esp_ota_write failed: %s", esp_err_to_name(err));
            esp_ota_abort(otaHandle);
            httpd_resp_send_500(req);
            return ESP_FAIL;
        }

        remaining -= bytes;
    }

    err = esp_ota_end(otaHandle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_ota_end failed: %s", esp_err_to_name(err));
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    err = esp_ota_set_boot_partition(updatePartition);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_ota_set_boot_partition failed: %s", esp_err_to_name(err));
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "OTA flash completed successfully. Rebooting...");
    TelnetServer.telnetPrint("[OTA] Push upload successful. Rebooting...\r\n");

    httpd_resp_sendstr(req, "OK");
    xTaskCreate(&WebServer::restartTask, "restartTask", 2048, nullptr, 5, nullptr);

    return ESP_OK;
}

esp_err_t WebServer::pullUpdatePostHandler(httpd_req_t *req)
{
    int totalLen = req->content_len;
    int received = 0;
    std::string postBody = "";
    char buf[128];

    while (received < totalLen)
    {
        int bytes = httpd_req_recv(req, buf, std::min((int)sizeof(buf) - 1, totalLen - received));
        if (bytes <= 0)
        {
            if (bytes == HTTPD_SOCK_ERR_TIMEOUT)
            {
                continue;
            }
            return ESP_FAIL;
        }
        buf[bytes] = '\0';
        postBody += buf;
        received += bytes;
    }

    std::map<std::string, std::string> params;
    parseFormBody(postBody, params);

    if (params.find("url") != params.end() && !params["url"].empty())
    {
        pendingPullUrl = params["url"];
        ESP_LOGI(TAG, "Pull-OTA requested for URL: %s", pendingPullUrl.c_str());

        std::string response = "<!DOCTYPE html><html><head><meta charset='utf-8'><title>Updating</title></head><body>"
                               "<h2>Pull Update Started!</h2><p>Downloading and flashing firmware in the background...</p>"
                               "<p>Check Telnet log for real-time progress. ESP will reboot on success.</p></body></html>";

        httpd_resp_set_type(req, "text/html");
        httpd_resp_send(req, response.c_str(), response.length());

        xTaskCreate(&WebServer::pullUpdateTask, "pullUpdateTask", 8192, nullptr, 5, nullptr);
        return ESP_OK;
    }

    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing URL parameter");
    return ESP_FAIL;
}

void WebServer::setup()
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 8;
    config.stack_size = 8192;

    if (httpd_start(&serverHandle, &config) == ESP_OK)
    {
        httpd_uri_t rootUri = {
            .uri = "/",
            .method = HTTP_GET,
            .handler = &WebServer::rootGetHandler,
            .user_ctx = nullptr};
        httpd_register_uri_handler(serverHandle, &rootUri);

        httpd_uri_t saveUri = {
            .uri = "/save",
            .method = HTTP_POST,
            .handler = &WebServer::savePostHandler,
            .user_ctx = nullptr};
        httpd_register_uri_handler(serverHandle, &saveUri);

        httpd_uri_t updateUri = {
            .uri = "/update",
            .method = HTTP_POST,
            .handler = &WebServer::updatePostHandler,
            .user_ctx = nullptr};
        httpd_register_uri_handler(serverHandle, &updateUri);

        httpd_uri_t pullUpdateUri = {
            .uri = "/pull_update",
            .method = HTTP_POST,
            .handler = &WebServer::pullUpdatePostHandler,
            .user_ctx = nullptr};
        httpd_register_uri_handler(serverHandle, &pullUpdateUri);

        ESP_LOGI(TAG, "HTTP web server started on port 80 (with Push & Pull OTA)");
    }
    else
    {
        ESP_LOGE(TAG, "Failed to start HTTP web server");
    }
}

void WebServer::stop()
{
    if (serverHandle != nullptr)
    {
        httpd_stop(serverHandle);
        serverHandle = nullptr;
        ESP_LOGI(TAG, "HTTP web server stopped");
    }
}
