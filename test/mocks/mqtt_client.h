#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include "esp_err.h"

typedef void *esp_event_base_t;

typedef enum
{
    MQTT_EVENT_ANY = -1,
    MQTT_EVENT_ERROR = 0,
    MQTT_EVENT_CONNECTED = 1,
    MQTT_EVENT_DISCONNECTED = 2,
    MQTT_EVENT_SUBSCRIBED = 3,
    MQTT_EVENT_UNSUBSCRIBED = 4,
    MQTT_EVENT_PUBLISHED = 5,
    MQTT_EVENT_DATA = 6,
    MQTT_EVENT_BEFORE_CONNECT = 7,
    MQTT_EVENT_DELETED = 8
} esp_mqtt_event_id_t;

typedef struct
{
    esp_mqtt_event_id_t event_id;
    char *topic;
    int topic_len;
    char *data;
    int data_len;
} esp_mqtt_event_t;

typedef esp_mqtt_event_t *esp_mqtt_event_handle_t;
typedef void *esp_mqtt_client_handle_t;
typedef void (*esp_event_handler_t)(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data);

typedef struct
{
    struct
    {
        struct
        {
            const char *uri;
        } address;
    } broker;
    struct
    {
        const char *username;
        const char *client_id;
        struct
        {
            const char *password;
        } authentication;
    } credentials;
    struct
    {
        size_t size;
    } buffer;
    struct
    {
        size_t stack_size;
        int prio;
    } task;
} esp_mqtt_client_config_t;

struct PublishedMqttMessage
{
    std::string topic;
    std::string data;
    int qos;
    int retain;
};

extern std::vector<PublishedMqttMessage> g_publishedMqttMessages;

inline esp_mqtt_client_handle_t esp_mqtt_client_init(const esp_mqtt_client_config_t *config)
{
    (void)config;
    return (esp_mqtt_client_handle_t)1;
}

inline esp_err_t esp_mqtt_client_register_event(esp_mqtt_client_handle_t client, esp_mqtt_event_id_t event, esp_event_handler_t event_handler, void *event_handler_arg)
{
    (void)client;
    (void)event;
    (void)event_handler;
    (void)event_handler_arg;
    return ESP_OK;
}

inline esp_err_t esp_mqtt_client_start(esp_mqtt_client_handle_t client)
{
    (void)client;
    return ESP_OK;
}

inline int esp_mqtt_client_publish(esp_mqtt_client_handle_t client, const char *topic, const char *data, int len, int qos, int retain)
{
    (void)client;
    std::string payload = (len == 0 && data != nullptr) ? std::string(data) : std::string(data, len);
    g_publishedMqttMessages.push_back({topic ? topic : "", payload, qos, retain});
    return 1;
}

inline int esp_mqtt_client_subscribe(esp_mqtt_client_handle_t client, const char *topic, int qos)
{
    (void)client;
    (void)topic;
    (void)qos;
    return 1;
}