/*
 Copyright (c) 2014-present PlatformIO <contact@platformio.org>

 Licensed under the Apache License, Version 2.0 (the "License");
 you may not use this file except in compliance with the License.
 You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License.
**/

#include <Arduino.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

#include "BintrayClient.h"
#include "BintrayCertificates.h"

BintrayClient::BintrayClient(const String &user, const String &repository, const String &package)
    : m_user(user), m_repo(repository), m_package(package),
      m_storage_host("dl.bintray.com"),
      m_api_host("api.bintray.com")
{
    m_certificates.emplace_back("cloudfront.net", CLOUDFRONT_API_ROOT_CA);
    m_certificates.emplace_back("akamai.bintray.com", BINTRAY_AKAMAI_ROOT_CA);
    m_certificates.emplace_back("bintray.com", BINTRAY_API_ROOT_CA);
}

String BintrayClient::getUser() const
{
    return m_user;
}

String BintrayClient::getRepository() const
{
    return m_repo;
}

String BintrayClient::getPackage() const
{
    return m_package;
}

String BintrayClient::getStorageHost() const
{
    return m_storage_host;
}

String BintrayClient::getApiHost() const
{
    return m_api_host;
}

String BintrayClient::getLatestVersionRequestUrl() const
{
    return String("https://") + getApiHost() + "/packages/" + getUser() + "/" + getRepository() + "/" + getPackage() + "/versions/_latest";
}

String BintrayClient::getBinaryRequestUrl(const String &version) const
{
    return String("https://") + getApiHost() + "/packages/" + getUser() + "/" + getRepository() + "/" + getPackage() + "/versions/" + version + "/files";
}

const char *BintrayClient::getCertificate(const String &url) const
{
    for(auto& cert: m_certificates) {
        if(url.indexOf(cert.first) >= 0) {
            return cert.second;
        }
    }

    // Return the certificate for *.bintray.com by default
    return m_certificates.rbegin()->second;
}

String BintrayClient::requestHTTPContent(const String &url) const
{
    String payload;
    HTTPClient http;
    http.begin(url, getCertificate(url));
    int httpCode = http.GET();

    if (httpCode > 0)
    {
        if (httpCode == HTTP_CODE_OK)
        {
            payload = http.getString();
        }
    }
    else
    {
        Serial.printf("GET request failed, error: %s\n", http.errorToString(httpCode).c_str());
    }

    http.end();
    return payload;
}

String BintrayClient::getLatestVersion() const
{
    String version;
    const String url = getLatestVersionRequestUrl();
    String jsonResult = requestHTTPContent(url);
    const size_t bufferSize = 1024;
    if (jsonResult.length() > bufferSize)
    {
        Serial.println("Error: Could parse JSON. Input data is too big!");
        return version;
    }
    StaticJsonDocument<bufferSize> jsonBuffer;
    auto err = deserializeJson(jsonBuffer, jsonResult.c_str());
    // Check for errors in parsing
    if (err) {
        Serial.print(F("deserializeJson() failed with code "));
        Serial.println(err.c_str());
        return version;
    }
    return jsonBuffer["name"];
}

String BintrayClient::getBinaryPath(const String &version) const
{
    String path;
    const String url = getBinaryRequestUrl(version);
    String jsonResult = requestHTTPContent(url);

    const size_t bufferSize = 1024;
    if (jsonResult.length() > bufferSize)
    {
        Serial.println("Error: Could parse JSON. Input data is too big!");
        return path;
    }
    StaticJsonDocument<bufferSize> jsonBuffer;
    auto err = deserializeJson(jsonBuffer, jsonResult.c_str());
    if (err) {
        Serial.print(F("deserializeJson() failed with code "));
        Serial.println(err.c_str());
        return path;
    }
    JsonObject firstItem = jsonBuffer[0];
    return "/" + getUser() + "/" + getRepository() + "/" + firstItem["path"].as<String>();
}
 