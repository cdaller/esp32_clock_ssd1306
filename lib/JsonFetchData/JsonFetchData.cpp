#include "JsonFetchData.hpp"

#include "esp_log.h"

#include <ArduinoJson.h> //https://github.com/bblanchon/ArduinoJson
#include <HTTPClient.h>

JsonFetchData::JsonFetchData(char* jsonUrl, char* jsonPath, long fetchInvervalMs) {
    this->jsonUrl = jsonUrl;
    this->jsonPath = jsonPath;
    this->fetchIntervalMs = fetchInvervalMs;
    lastFetchValueMillis = -fetchIntervalMs - 100;
    setFormatter(DEFAULT_FORMATTER);
    ESP_LOGI(TAG, "json fetch data from %s using path %s", jsonUrl, jsonPath);
}

void JsonFetchData::setFormatter(char* formatter) { 
    this->formatter = String(formatter);
};


const char* JsonFetchData::getStatusAsText() {
    switch (lastStatus) {
    case 0:
        return "OK";
    case 1:
        return "Json Parse Error";
    case 2:
        return "Http Error";
    case 3:
        return "No Data Yet";
    default:
        return "Unkonwn Error";
    }
}

float JsonFetchData::getValue() {
    fetchValueIfNeeded();
    return lastValue;
}

char* JsonFetchData::getValueFormatted() { 
    fetchValueIfNeeded();
    return lastValueFormatted; 
}

int JsonFetchData::getStatus() { 
    fetchValueIfNeeded(); 
    return lastStatus; 
}

void JsonFetchData::fetchValueIfNeeded() {
    if (millis() - lastFetchValueMillis > fetchIntervalMs) {
        fetchJsonValue();
        String addOn;
        switch (lastStatus)
        {
        case STATUS_DATA_OK:
            addOn = "";
            break;
        case STATUS_HTTP_ERROR:
            addOn = "#";
            break;
        case STATUS_JSON_PARSE_ERROR:
            addOn = "*";
            break;
        case STATUS_NO_DATA:
            addOn = "?";
            break;
        default:
            break;
        }
        sprintf(lastValueFormatted, (formatter + addOn).c_str(), lastValue);
        lastFetchValueMillis = millis();
    }
}

void JsonFetchData::fetchJsonValue() {
  if (WiFi.status() == WL_CONNECTED) {
    ESP_LOGD(TAG, "requesting data via http");
    // from: https://github.com/espressif/arduino-esp32/blob/master/libraries/HTTPClient/examples/ReuseConnection/ReuseConnection.ino
    // use WiFiClient / WiFiSecureClient https://github.com/espressif/arduino-esp32/issues/3347
    WiFiClient client;
    HTTPClient http;
    http.begin(client, jsonUrl);

    int httpCode = http.GET();
    if(httpCode > 0) {
      ESP_LOGD(TAG, "[HTTP] GET... code: %d", httpCode);

      // file found at server
      if(httpCode == HTTP_CODE_OK) {
        String payload = http.getString();
        ESP_LOGD(TAG, payload);    

        parseJson(&payload[0], this->jsonPath);
      }
    } else {
      ESP_LOGW(TAG, "[HTTP] GET... failed, error: %s", http.errorToString(httpCode).c_str());
      lastStatus = STATUS_HTTP_ERROR;
    }
    http.end();    
  }
}

// parse jsonPaths like $.foo[1].bar.baz[2][3].value equals to foo[1].bar.baz[2][3].value
void JsonFetchData::parseJson(char* jsonString, char *jsonPath) {
    float value;
    int status;
    DynamicJsonBuffer jsonBuffer;
    
    JsonVariant root = jsonBuffer.parse(jsonString);
    JsonVariant element = root;

    if (root.success()) {
        // parse jsonPath and navigate through json object:
        char pathElement[40];
        int pathIndex = 0;

        ESP_LOGD(TAG, "parsing '%s'", jsonPath);
        for (int i = 0; jsonPath[i] != '\0'; i++){
            if (jsonPath[i] == '$') {
                element = root;
            } else if (jsonPath[i] == '.') {
                if (pathIndex > 0) {
                    pathElement[pathIndex++] = '\0';
                    // printf("pathElement '%s'\n", pathElement);
                    pathIndex = 0;
                    element = element[pathElement];
                    if (!element.success()) {
                        ESP_LOGW(TAG, "failed to parse key %s", pathElement);
                    }
                }
            } else if ((jsonPath[i] >= 'a' && jsonPath[i] <= 'z') 
                    || (jsonPath[i] >= 'A' && jsonPath[i] <= 'Z') 
                    || (jsonPath[i] >= '0' && jsonPath[i] <= '9')
                    || jsonPath[i] == '-' || jsonPath[i] == '_'
                    ) {
                pathElement[pathIndex++] = jsonPath[i];
            } else if (jsonPath[i] == '[') {
                if (pathIndex > 0) {
                    pathElement[pathIndex++] = '\0';
                    // printf("pathElement '%s'\n", pathElement);
                    pathIndex = 0;
                    element = element[pathElement];
                    if (!element.success()) {
                        ESP_LOGW(TAG, "failed in parsing key %s", pathElement);
                    }
                }
            } else if (jsonPath[i] == ']') {
                pathElement[pathIndex++] = '\0';
                int arrayIndex = strtod(pathElement, NULL);
                // printf("index '%s' = %d\n", pathElement, arrayIndex);
                pathIndex = 0;
                element = element[arrayIndex];
                if (!element.success()) {
                    ESP_LOGW(TAG, "failed in parsing index %d", arrayIndex);
                }
            }
        }  
        // final token if any:
        if (pathIndex > 0) {
            pathElement[pathIndex++] = '\0';
            // printf("pathElement '%s'\n", pathElement);
            pathIndex = 0;
            element = element[pathElement];
            if (!element.success()) {
                ESP_LOGW(TAG, "failed in parsing key %s", pathElement);
            }
        }

        value = element.as<float>();
        status = STATUS_DATA_OK;
        //jsonValue = measurements[1]["sensordatavalues"][0]["value"];
        ESP_LOGI(TAG, "success reading value: %f", value);
    } else {
        status = STATUS_JSON_PARSE_ERROR;
        ESP_LOGI("could not parse json for value");
    }

    if (status == STATUS_DATA_OK) {
        lastValue = value;
    }
    lastStatus = status;
}

