#include "sdkconfig.h"
#include "WebServer.h"
#include "ConfigManager.h"
#include "TelnetServer.h"
#include "MqttHandler.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_ota_ops.h"
#include "esp_http_client.h"
#include "esp_app_desc.h"
#include "esp_https_ota.h"
#include "esp_crt_bundle.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "cJSON.h"
#include <sstream>
#include <map>
#include <sstream>
#include <cstring>
#include <cstdlib>

static const char *TAG = "WebServer";
extern ConfigManager configManager;
extern TelnetServer telnetServer;

std::string WebServer::pendingPullUrl = "";
std::string WebServer::pendingGithubToken = "";
std::string WebServer::s_latestFoundVersion = "";
std::string WebServer::s_latestDownloadUrl = "";
std::string WebServer::s_latestReleaseUrl = "";
std::string WebServer::s_latestReleaseTitle = "";

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

esp_err_t WebServer::httpClientInitCb(esp_http_client_handle_t http_client)
{
    if (!pendingGithubToken.empty())
    {
        std::string authHeader = "Bearer " + pendingGithubToken;
        esp_http_client_set_header(http_client, "Authorization", authHeader.c_str());
        esp_http_client_set_header(http_client, "Accept", "application/octet-stream");
        esp_http_client_set_header(http_client, "User-Agent", "ESP32-HeatMeterGateway");
    }
    return ESP_OK;
}

#if defined(CONFIG_IDF_TARGET_ESP32S3)
static const char *TARGET_BIN_NAME = "firmware-esp32-s3.bin";
#else
static const char *TARGET_BIN_NAME = "firmware-esp32-c3.bin";
#endif

static std::string resolveGitHubAssetUrl(const std::string &inputUrl, const std::string &token)
{
    if (token.empty() || inputUrl.find("github.com") == std::string::npos || inputUrl.find("api.github.com") != std::string::npos)
    {
        return inputUrl;
    }

    size_t ghPos = inputUrl.find("github.com/");
    if (ghPos == std::string::npos)
    {
        return inputUrl;
    }

    std::string path = inputUrl.substr(ghPos + 11);
    std::istringstream ss(path);
    std::string owner, repo, releasesKw, actionKw, tag;

    std::getline(ss, owner, '/');
    std::getline(ss, repo, '/');
    std::getline(ss, releasesKw, '/');
    std::getline(ss, actionKw, '/');
    std::getline(ss, tag, '/');

    if (owner.empty() || repo.empty())
    {
        return inputUrl;
    }

    std::string targetBinName = TARGET_BIN_NAME;
    if (inputUrl.find(".bin") != std::string::npos)
    {
        size_t lastSlash = inputUrl.rfind('/');
        if (lastSlash != std::string::npos)
        {
            std::string fn = inputUrl.substr(lastSlash + 1);
            if (!fn.empty()) targetBinName = fn;
        }
    }

    std::string apiUrl;
    if (actionKw == "latest" || inputUrl.find("/releases/latest") != std::string::npos)
    {
        apiUrl = "https://api.github.com/repos/" + owner + "/" + repo + "/releases/latest";
    }
    else
    {
        if (tag.empty()) tag = actionKw;
        apiUrl = "https://api.github.com/repos/" + owner + "/" + repo + "/releases/tags/" + tag;
    }

    ESP_LOGI(TAG, "Resolving GitHub release asset for '%s' via API: %s", targetBinName.c_str(), apiUrl.c_str());

    esp_http_client_config_t apiConfig = {};
    apiConfig.url = apiUrl.c_str();
    apiConfig.timeout_ms = 15000;
    apiConfig.crt_bundle_attach = esp_crt_bundle_attach;
    apiConfig.max_redirection_count = 5;

    esp_http_client_handle_t apiClient = esp_http_client_init(&apiConfig);
    if (apiClient == nullptr)
    {
        return inputUrl;
    }

    std::string authHeader = "Bearer " + token;
    esp_http_client_set_header(apiClient, "Authorization", authHeader.c_str());
    esp_http_client_set_header(apiClient, "User-Agent", "ESP32-HeatMeterGateway");
    esp_http_client_set_header(apiClient, "Accept", "application/vnd.github.v3+json");

    esp_err_t err = esp_http_client_open(apiClient, 0);
    if (err != ESP_OK)
    {
        esp_http_client_cleanup(apiClient);
        return inputUrl;
    }

    esp_http_client_fetch_headers(apiClient);

    std::string responseBody;
    char buf[512];
    int readBytes = 0;
    while ((readBytes = esp_http_client_read(apiClient, buf, sizeof(buf) - 1)) > 0)
    {
        buf[readBytes] = '\0';
        responseBody += buf;
        if (responseBody.length() > 65536) break;
    }

    esp_http_client_close(apiClient);
    esp_http_client_cleanup(apiClient);

    std::string resolvedAssetUrl = "";
    cJSON *root = cJSON_Parse(responseBody.c_str());
    if (root != nullptr)
    {
        cJSON *assets = cJSON_GetObjectItem(root, "assets");
        if (cJSON_IsArray(assets))
        {
            int assetCount = cJSON_GetArraySize(assets);
            for (int i = 0; i < assetCount; i++)
            {
                cJSON *asset = cJSON_GetArrayItem(assets, i);
                cJSON *nameItem = cJSON_GetObjectItem(asset, "name");
                cJSON *urlItem = cJSON_GetObjectItem(asset, "url");

                if (cJSON_IsString(nameItem) && cJSON_IsString(urlItem))
                {
                    if (std::string(nameItem->valuestring) == targetBinName)
                    {
                        resolvedAssetUrl = urlItem->valuestring;
                        ESP_LOGI(TAG, "Auto-resolved asset URL: %s", resolvedAssetUrl.c_str());
                        break;
                    }
                }
            }
        }
        cJSON_Delete(root);
    }

    return !resolvedAssetUrl.empty() ? resolvedAssetUrl : inputUrl;
}

void WebServer::pullUpdateTask(void *pvParameters)
{
    ESP_LOGI(TAG, "Starting Pull-OTA process for: %s", pendingPullUrl.c_str());
    telnetServer.telnetPrint("[OTA] Resolving GitHub release asset...\r\n");

    std::string downloadUrl = resolveGitHubAssetUrl(pendingPullUrl, pendingGithubToken);
    ESP_LOGI(TAG, "Final download target URL: %s", downloadUrl.c_str());

    telnetServer.telnetPrint("[OTA] Starting remote download from URL...\r\n");

    esp_http_client_config_t httpConfig = {};
    httpConfig.url = downloadUrl.c_str();
    httpConfig.timeout_ms = 30000;
    httpConfig.buffer_size = 4096;
    httpConfig.buffer_size_tx = 1024;
    httpConfig.crt_bundle_attach = esp_crt_bundle_attach;
    httpConfig.max_redirection_count = 5;
    httpConfig.keep_alive_enable = true;

    esp_https_ota_config_t otaConfig = {};
    otaConfig.http_config = &httpConfig;
    otaConfig.http_client_init_cb = &WebServer::httpClientInitCb;

    esp_err_t ret = esp_https_ota(&otaConfig);
    if (ret == ESP_OK)
    {
        ESP_LOGI(TAG, "OTA update successful. Rebooting in 3 seconds...");
        telnetServer.telnetPrint("[OTA] Update successful. Rebooting in 3 seconds...\r\n");
        xTaskCreate(&WebServer::restartTask, "restartTask", 2048, nullptr, 5, nullptr);
    }
    else
    {
        ESP_LOGE(TAG, "HTTPS OTA failed: %s", esp_err_to_name(ret));
        telnetServer.telnetPrint("[OTA] HTTPS OTA failed\r\n");
    }

    vTaskDelete(nullptr);
}

esp_err_t WebServer::rootGetHandler(httpd_req_t *req)
{
    const esp_partition_t *running = esp_ota_get_running_partition();
    std::string runningSlot = (running != nullptr) ? running->label : "unknown";

    const esp_app_desc_t *appDesc = esp_app_get_description();
    std::string versionStr = std::string(appDesc->version);
    std::string buildInfo = std::string(appDesc->date) + " " + std::string(appDesc->time);

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
  <div class='info'>Version: <b>)" + versionStr + R"(</b> | Built: <b>)" + buildInfo + R"(</b> | Active Partition: <b>)" + runningSlot + R"(</b></div>

  <h3>System Configuration</h3>
  <form action='/save' method='POST'>
    <label>WiFi SSID</label>
    <input type='text' name='wifi_ssid' value=')" + configManager.m_wifiSsid + R"('>
    <label>WiFi Password</label>
    <input type='password' name='wifi_pass' value=')" + configManager.m_wifiPassword + R"('>
    <label>MQTT Broker (IP / Host)</label>
    <input type='text' name='mqtt_server' value=')" + configManager.m_mqttServer + R"('>
    <label>MQTT Port</label>
    <input type='number' name='mqtt_port' value=')" + std::to_string(configManager.m_mqttPort) + R"('>
    <label>MQTT User</label>
    <input type='text' name='mqtt_user' value=')" + configManager.m_mqttUser + R"('>
    <label>MQTT Password</label>
    <input type='password' name='mqtt_pass' value=')" + configManager.m_mqttPassword + R"('>
    <label>MQTT State Topic</label>
    <input type='text' name='mqtt_topic' value=')" + configManager.m_mqttTopic + R"('>
    <label>Read Interval (seconds)</label>
    <input type='number' name='read_interval_s' value=')" + std::to_string(configManager.m_readIntervalSeconds) + R"('>
    <label class='checkbox-label'>
      <input type='checkbox' name='dummy_mode' value='1' )" + (configManager.m_dummyMode ? "checked" : "") + R"(>
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

  <h3>Firmware Update (GitHub & URL)</h3>
  <form id='repoForm'>
    <label>GitHub Repository (Owner/Repo)</label>
    <input type='text' id='ghRepo' name='gh_repo' value=')" + configManager.m_githubRepo + R"(' placeholder='owner/repo'>
    
    <label class='checkbox-label'>
      <input type='checkbox' id='ghUpdateCheck' )" + (configManager.m_githubAutoCheck ? "checked" : "") + R"(>
      Automatisch auf Updates prüfen
    </label>

    <label class='checkbox-label'>
      <input type='checkbox' id='ghUpdate' )" + (configManager.m_githubAutoUpdate ? "checked" : "") + R"(>
      Automatisch Updates installieren
    </label>

    <label class='checkbox-label'>
      <input type='checkbox' id='ghPre' )" + (configManager.m_githubIncludePrerelease ? "checked" : "") + R"(>
      Prerelease einschließen
    </label>
    
    <label class='checkbox-label'>
      <input type='checkbox' id='ghNightly' )" + (configManager.m_githubIncludeNightly ? "checked" : "") + R"(>
      Nightlys einschließen
    </label>

    <label>GitHub Token (optional für private Repos)</label>
    <input type='password' id='ghToken' placeholder='ghp_...'>

    <div style='display:flex; gap:10px; margin-top:15px;'>
      <button type='button' style='margin-top:0;' onclick='saveRepoSettings()'>Repo speichern</button>
      <button type='button' class='secondary' style='margin-top:0;' id='checkBtn' onclick='checkForUpdates()'>Auf Updates prüfen</button>
    </div>
  </form>

  <div id='updateStatus' style='margin-top:15px;'></div>

  <form id='pullForm' action='/pull_update' method='POST' style='margin-top:20px;'>
    <label>Ausgewählte Firmware-URL</label>
    <input type='text' id='firmwareUrl' name='url' placeholder='https://.../firmware.bin'>
    <input type='hidden' id='formGhToken' name='gh_token'>
    <button type='submit' class='secondary' id='installBtn'>Pull Update manuell starten</button>
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
function saveRepoSettings() {
  const repo = document.getElementById('ghRepo').value.trim();
  const pre = document.getElementById('ghPre').checked ? '1' : '0';
  const nightly = document.getElementById('ghNightly').checked ? '1' : '0';

  const body = 'gh_repo=' + encodeURIComponent(repo) + '&gh_pre=' + pre + '&gh_nightly=' + nightly;
  fetch('/save_repo', {
    method: 'POST',
    headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
    body: body
  }).then(r => {
    if (r.ok) alert('Repository-Einstellungen gespeichert!');
    else alert('Fehler beim Speichern');
  }).catch(e => alert('Fehler: ' + e));
}

function checkForUpdates() {
  const repo = document.getElementById('ghRepo').value.trim();
  const includePre = document.getElementById('ghPre').checked;
  const includeNightly = document.getElementById('ghNightly').checked;
  const token = document.getElementById('ghToken').value.trim();
  const statusDiv = document.getElementById('updateStatus');
  const targetBin = ')" + std::string(TARGET_BIN_NAME) + R"(';
  const currentVersion = ')" + versionStr + R"(';

  if (!repo) {
    alert('Bitte gib ein Repository an');
    return;
  }

  statusDiv.innerHTML = '<div class="info">Prüfe GitHub Releases für <b>' + repo + '</b>...</div>';

  const headers = { 'Accept': 'application/vnd.github.v3+json' };
  if (token) headers['Authorization'] = 'Bearer ' + token;

  fetch('https://api.github.com/repos/' + repo + '/releases?per_page=10', { headers: headers })
    .then(res => {
      if (!res.ok) throw new Error('HTTP ' + res.status);
      return res.json();
    })
    .then(releases => {
      let selectedRelease = null;
      let matchedAsset = null;

      for (const rel of releases) {
        if (rel.draft) continue;

        const isNightly = /nightly|snapshot/i.test(rel.tag_name) || /nightly|snapshot/i.test(rel.name || '');
        if (isNightly && !includeNightly) continue;
        if (rel.prerelease && !isNightly && !includePre) continue;

        if (rel.assets && Array.isArray(rel.assets)) {
          const asset = rel.assets.find(a => a.name === targetBin);
          if (asset) {
            selectedRelease = rel;
            matchedAsset = asset;
            break;
          }
        }
      }

      if (!selectedRelease || !matchedAsset) {
        statusDiv.innerHTML = '<div style="color:orange;">Kein passendes Release mit ' + targetBin + ' gefunden.</div>';
        return;
      }

      const releaseVer = selectedRelease.tag_name;
      const downloadUrl = matchedAsset.browser_download_url;

      document.getElementById('firmwareUrl').value = downloadUrl;
      document.getElementById('formGhToken').value = token;

      const isNewer = (releaseVer !== currentVersion);
      let html = '<div style="background:#e8f4fd; border:1px solid #b6d4fe; padding:12px; border-radius:6px;">';
      html += '<strong>Gefunden: ' + (selectedRelease.name || releaseVer) + '</strong> (' + releaseVer + ')<br>';
      if (isNewer) {
        html += '<span style="color:green; font-weight:bold;">Neues Update verfügbar!</span> (Aktuell: ' + currentVersion + ')<br>';
        html += '<button type="button" class="secondary" style="margin-top:10px;" onclick="document.getElementById(\'pullForm\').submit();">🚀 Jetzt auf ' + releaseVer + ' aktualisieren</button>';
      } else {
        html += '<span style="color:#555;">Firmware ist bereits aktuell (' + currentVersion + ').</span>';
      }
      html += '</div>';
      statusDiv.innerHTML = html;
    })
    .catch(err => {
      statusDiv.innerHTML = '<div style="color:red;">Fehler bei Update-Prüfung: ' + err.message + '</div>';
    });
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

    if (params.find("wifi_ssid") != params.end()) configManager.m_wifiSsid = params["wifi_ssid"];
    if (params.find("wifi_pass") != params.end()) configManager.m_wifiPassword = params["wifi_pass"];
    if (params.find("mqtt_server") != params.end()) configManager.m_mqttServer = params["mqtt_server"];
    if (params.find("mqtt_port") != params.end()) configManager.m_mqttPort = std::atoi(params["mqtt_port"].c_str());
    if (params.find("mqtt_user") != params.end()) configManager.m_mqttUser = params["mqtt_user"];
    if (params.find("mqtt_pass") != params.end()) configManager.m_mqttPassword = params["mqtt_pass"];
    if (params.find("mqtt_topic") != params.end()) configManager.m_mqttTopic = params["mqtt_topic"];
    if (params.find("read_interval_s") != params.end()) configManager.m_readIntervalSeconds = std::atoi(params["read_interval_s"].c_str());

    configManager.m_dummyMode = (params.find("dummy_mode") != params.end() && params["dummy_mode"] == "1");

    configManager.saveConfig();

    telnetServer.telnetPrint("[Web] Configuration saved via web interface\r\n");
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
    telnetServer.telnetPrint("[OTA] Push firmware upload started...\r\n");

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
    telnetServer.telnetPrint("[OTA] Push upload successful. Rebooting...\r\n");

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
        pendingGithubToken = (params.find("gh_token") != params.end()) ? params["gh_token"] : "";
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
    config.lru_purge_enable = true;

    if (httpd_start(&m_serverHandle, &config) == ESP_OK)
    {
        httpd_uri_t rootUri = {
            .uri = "/",
            .method = HTTP_GET,
            .handler = &WebServer::rootGetHandler,
            .user_ctx = nullptr};
        httpd_register_uri_handler(m_serverHandle, &rootUri);

        httpd_uri_t saveUri = {
            .uri = "/save",
            .method = HTTP_POST,
            .handler = &WebServer::savePostHandler,
            .user_ctx = nullptr};
        httpd_register_uri_handler(m_serverHandle, &saveUri);

        httpd_uri_t updateUri = {
            .uri = "/update",
            .method = HTTP_POST,
            .handler = &WebServer::updatePostHandler,
            .user_ctx = nullptr};
        httpd_register_uri_handler(m_serverHandle, &updateUri);

        httpd_uri_t pullUpdateUri = {
            .uri = "/pull_update",
            .method = HTTP_POST,
            .handler = &WebServer::pullUpdatePostHandler,
            .user_ctx = nullptr};
        httpd_register_uri_handler(m_serverHandle, &pullUpdateUri);

        httpd_uri_t saveRepoUri = {
            .uri = "/save_repo",
            .method = HTTP_POST,
            .handler = &WebServer::saveRepoPostHandler,
            .user_ctx = nullptr};
        httpd_register_uri_handler(m_serverHandle, &saveRepoUri);

        ESP_LOGI(TAG, "HTTP web server started on port 80 (with Push & Pull OTA)");
    }
    else
    {
        ESP_LOGE(TAG, "Failed to start HTTP web server");
    }
}

void WebServer::stop()
{
    if (m_serverHandle != nullptr)
    {
        httpd_stop(m_serverHandle);
        m_serverHandle = nullptr;
        ESP_LOGI(TAG, "HTTP web server stopped");
    }
}

void WebServer::triggerUpdateCheck()
{
    xTaskCreate(&WebServer::updateCheckTask, "updateCheckTask", 8192, nullptr, 5, nullptr);
}

void WebServer::installLatestUpdate()
{
    if (s_latestDownloadUrl.empty())
    {
        ESP_LOGW(TAG, "No update download URL available to install");
        telnetServer.telnetPrint("[OTA] No update available to install\r\n");
        return;
    }
    pendingPullUrl = s_latestDownloadUrl;
    pendingGithubToken = "";
    xTaskCreate(&WebServer::pullUpdateTask, "pullUpdateTask", 8192, nullptr, 5, nullptr);
}

void WebServer::updateCheckTask(void *pvParameters)
{
    ESP_LOGI(TAG, "Checking for updates on repository: %s", configManager.m_githubRepo.c_str());
    telnetServer.telnetPrint("[OTA] Checking GitHub for firmware updates...\r\n");

    if (configManager.m_githubRepo.empty())
    {
        ESP_LOGW(TAG, "No GitHub repository configured");
        vTaskDelete(nullptr);
        return;
    }

    std::string apiUrl = "https://api.github.com/repos/" + configManager.m_githubRepo + "/releases?per_page=3";

    esp_http_client_config_t apiConfig = {};
    apiConfig.url = apiUrl.c_str();
    apiConfig.timeout_ms = 15000;
    apiConfig.crt_bundle_attach = esp_crt_bundle_attach;
    apiConfig.max_redirection_count = 5;

    esp_http_client_handle_t client = esp_http_client_init(&apiConfig);
    if (client == nullptr)
    {
        vTaskDelete(nullptr);
        return;
    }

    esp_http_client_set_header(client, "User-Agent", "ESP32-HeatMeterGateway");
    esp_http_client_set_header(client, "Accept", "application/vnd.github.v3+json");

    if (esp_http_client_open(client, 0) == ESP_OK)
    {
        esp_http_client_fetch_headers(client);
        std::string responseBody;
        char buf[512];
        int readBytes = 0;
        while ((readBytes = esp_http_client_read(client, buf, sizeof(buf) - 1)) > 0)
        {
            buf[readBytes] = '\0';
            responseBody += buf;
            if (responseBody.length() > 32768) break; // Schutz vor übergroßem JSON
        }
        esp_http_client_close(client);

        cJSON *releases = cJSON_Parse(responseBody.c_str());
        if (releases != nullptr && cJSON_IsArray(releases))
        {
            int releaseCount = cJSON_GetArraySize(releases);
            for (int i = 0; i < releaseCount; i++)
            {
                cJSON *rel = cJSON_GetArrayItem(releases, i);
                cJSON *draftItem = cJSON_GetObjectItem(rel, "draft");
                if (draftItem && cJSON_IsTrue(draftItem)) continue;

                cJSON *tagItem = cJSON_GetObjectItem(rel, "tag_name");
                cJSON *nameItem = cJSON_GetObjectItem(rel, "name");
                cJSON *preItem = cJSON_GetObjectItem(rel, "prerelease");
                cJSON *htmlUrlItem = cJSON_GetObjectItem(rel, "html_url");

                std::string tagName = tagItem ? tagItem->valuestring : "";
                std::string releaseName = nameItem ? nameItem->valuestring : tagName;
                bool isPrerelease = preItem && cJSON_IsTrue(preItem);

                bool isNightly = (tagName.find("nightly") != std::string::npos || releaseName.find("nightly") != std::string::npos);
                if (isNightly && !configManager.m_githubIncludeNightly) continue;
                if (isPrerelease && !isNightly && !configManager.m_githubIncludePrerelease) continue;

                // Passendes Binary in Assets suchen
                cJSON *assets = cJSON_GetObjectItem(rel, "assets");
                if (assets && cJSON_IsArray(assets))
                {
                    int assetCount = cJSON_GetArraySize(assets);
                    for (int j = 0; j < assetCount; j++)
                    {
                        cJSON *asset = cJSON_GetArrayItem(assets, j);
                        cJSON *aName = cJSON_GetObjectItem(asset, "name");
                        cJSON *aUrl = cJSON_GetObjectItem(asset, "browser_download_url");

                        if (aName && aUrl && std::string(aName->valuestring) == TARGET_BIN_NAME)
                        {
                            s_latestFoundVersion = tagName;
                            s_latestDownloadUrl = aUrl->valuestring;
                            s_latestReleaseUrl = htmlUrlItem ? htmlUrlItem->valuestring : "";
                            s_latestReleaseTitle = releaseName;
                            break;
                        }
                    }
                }
                if (!s_latestFoundVersion.empty()) break;
            }
            cJSON_Delete(releases);
        }
    }
    esp_http_client_cleanup(client);

    // Status an Home Assistant melden
    const esp_app_desc_t *appDesc = esp_app_get_description();
    std::string reportVersion = s_latestFoundVersion.empty() ? appDesc->version : s_latestFoundVersion;
    
    extern MqttHandler mqttHandler;
    mqttHandler.sendUpdateState(reportVersion, s_latestReleaseUrl, s_latestReleaseTitle);

    ESP_LOGI(TAG, "Update check finished. Latest version: %s (Installed: %s)", reportVersion.c_str(), appDesc->version);
    telnetServer.telnetPrint("[OTA] Update check finished\r\n");

    vTaskDelete(nullptr);
}

esp_err_t WebServer::saveRepoPostHandler(httpd_req_t *req)
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
            if (bytes == HTTPD_SOCK_ERR_TIMEOUT) continue;
            return ESP_FAIL;
        }
        buf[bytes] = '\0';
        postBody += buf;
        received += bytes;
    }

    std::map<std::string, std::string> params;
    parseFormBody(postBody, params);

    if (params.find("gh_repo") != params.end()) configManager.m_githubRepo = params["gh_repo"];
    configManager.m_githubAutoCheck = (params.find("ghUpdateCheck") != params.end() && params["ghUpdateCheck"] == "1");
    configManager.m_githubAutoUpdate = (params.find("ghUpdate") != params.end() && params["ghUpdate"] == "1");
    configManager.m_githubIncludePrerelease = (params.find("ghpre") != params.end() && params["ghpre"] == "1");
    configManager.m_githubIncludeNightly = (params.find("ghnightly") != params.end() && params["ghnightly"] == "1");

    configManager.saveConfig();
    ESP_LOGI(TAG, "Saved GitHub Repo settings: %s (Update Check: %d, Auto Update: %d, Pre: %d, Nightly: %d)",
             configManager.m_githubRepo.c_str(),
             configManager.m_githubAutoCheck,
             configManager.m_githubAutoUpdate,
             configManager.m_githubIncludePrerelease,
             configManager.m_githubIncludeNightly);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"status\":\"ok\"}");

    // Direkt einen neuen Check anstoßen
    triggerUpdateCheck();
    return ESP_OK;
}