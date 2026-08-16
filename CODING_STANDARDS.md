# Coding Standards & Guidelines

This document outlines the coding standards, naming conventions, and architectural best practices for this ESP-IDF / FreeRTOS C++ project. All contributions and automated AI code reviews must comply with these guidelines.

---

## 1. Language & Documentation

* **Code Language:** All class names, function names, variable names, enum names, type aliases, and comments must be strictly in **English**.
* **Comments:**
  * Comments should be meaningful, technical, and concise.
  * Avoid trivial, self-evident, or AI-generated filler comments.
  * Document non-obvious design decisions, protocol quirks, hardware timings, or memory ownership.

---

## 2. Naming Conventions (C++)

Strict adherence to naming conventions ensures consistency, eliminates name shadowing, and aids static code analysis:

| Element | Convention | Example | Notes |
| :--- | :--- | :--- | :--- |
| **Classes & Structs** | `PascalCase` | `ConfigManager`, `MeterT550`, `MqttHandler` | Clear type identity |
| **Class Instances / Objects** | `camelCase` | `configManager`, `meterT550`, `mqttHandler` | Avoids type-shadowing |
| **Member Variables** | `m_` + `camelCase` | `m_wifiSsid`, `m_readIntervalSeconds`, `m_serverSocket` | Distinguishes class members |
| **Local Variables** | `camelCase` | `intervalMs`, `now`, `trimmedLine`, `bytesRead` | Stack-allocated scope |
| **Function Parameters** | `camelCase` | `baudRate`, `obisCode`, `payload`, `eventData` | Clear parameter intent |
| **Constants** | `k` + `PascalCase` or `UPPER_CASE` | `kMaxRetries`, `TAG`, `DEFAULT_PORT` | Compile-time constants |
| **Enums & Enum Classes** | `PascalCase` (Enum & Values) | `enum class MeterState { Idle, Wakeup };` | Strongly typed enums |

> ⚠️ **Strict Rule:** Do not use `snake_case` for variables, methods, or class names.

---

## 3. Formatting & Code Style

### 3.1 Brace Placement (Allman Style)
Opening `{` and closing `}` braces must **always** start and end on their own separate line:

```cpp
void ExampleClass::processData(const std::string &inputData)
{
    if (!inputData.empty())
    {
        for (char c : inputData)
        {
            handleCharacter(c);
        }
    }
    else
    {
        ESP_LOGW(TAG, "Input data is empty");
    }
}
```

### 3.2 Indentation & Spacing
* Use **4 spaces** for indentation (no tab characters).
* Keep lines within a reasonable length (preferably $\le$ 120 characters).
* Place operators with surrounding spaces (`a + b`, `x = y`, `ptr == nullptr`).

---

## 4. Security & Secrets Management

* **No Hardcoded Credentials:** Never hardcode passwords, Wi-Fi credentials, tokens, or private keys in source files.
* **Persistent Configuration:** All network and MQTT settings must be configurable at runtime via NVS (Non-Volatile Storage) or Web UI.
* **Local Overrides:** Any testing credentials must remain in local, uncommitted configuration files ignored by Git.

---

## 5. ESP-IDF & FreeRTOS Best Practices

* **Memory Management:** Always pair allocations with appropriate deallocations (e.g. `cJSON_Delete()`, `free()`, socket `close()`).
* **Task Safety:** Ensure FreeRTOS tasks have sufficient stack allocation and incorporate non-blocking delays (`vTaskDelay`) to yield to the FreeRTOS scheduler and watchdog.
* **Logging:** Use official ESP-IDF logging macros (`ESP_LOGI`, `ESP_LOGW`, `ESP_LOGE`) with a local static `TAG`.
* **State Encapsulation:** Encapsulate hardware peripherals (UART, WiFi, Sockets, HTTP Server) inside dedicated manager classes.

---

## 6. AI Code Review Prompt / Checklist

When reviewing pull requests or generating code for this repository, verify:
- [ ] No `snake_case` identifiers used for variables or methods.
- [ ] All member variables start with `m_`.
- [ ] All global/local object instances use `camelCase`.
- [ ] All opening and closing braces follow the Allman style on new lines.
- [ ] Code and comments are exclusively in English.
- [ ] No passwords, Wi-Fi keys, or sensitive data are hardcoded.
- [ ] FreeRTOS tasks and ESP-IDF drivers follow lifecycle and memory safety practices.
