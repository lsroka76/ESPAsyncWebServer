// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright 2016-2025 Hristo Gochkov, Mathieu Carbou, Emil Muratov

#pragma once

#include <ESPAsyncWebServer.h>
#include "ChunkPrint.h"

#if ASYNC_JSON_SUPPORT == 1

#if ARDUINOJSON_VERSION_MAJOR == 6
#ifndef DYNAMIC_JSON_DOCUMENT_SIZE
#define DYNAMIC_JSON_DOCUMENT_SIZE 1024
#endif
#endif

// Json content type response classes

class AsyncJsonResponse : public AsyncAbstractResponse {
protected:
#if ARDUINOJSON_VERSION_MAJOR == 5
  DynamicJsonBuffer _jsonBuffer;
#elif ARDUINOJSON_VERSION_MAJOR == 6
  DynamicJsonDocument _jsonBuffer;
#else
  JsonDocument _jsonBuffer;
#endif

  JsonVariant _root;
  bool _isValid;

public:
#if ARDUINOJSON_VERSION_MAJOR == 6
  AsyncJsonResponse(bool isArray = false, size_t maxJsonBufferSize = DYNAMIC_JSON_DOCUMENT_SIZE);
#else
  AsyncJsonResponse(bool isArray = false);
#endif
  JsonVariant &getRoot() {
    return _root;
  }
  bool _sourceValid() const {
    return _isValid;
  }
  virtual size_t setLength();
  size_t getSize() const {
    return _jsonBuffer.size();
  }
  virtual size_t _fillBuffer(uint8_t *data, size_t len);
#if ARDUINOJSON_VERSION_MAJOR >= 6
  bool overflowed() const {
    return _jsonBuffer.overflowed();
  }
#endif
};

class PrettyAsyncJsonResponse : public AsyncJsonResponse {
public:
#if ARDUINOJSON_VERSION_MAJOR == 6
  PrettyAsyncJsonResponse(bool isArray = false, size_t maxJsonBufferSize = DYNAMIC_JSON_DOCUMENT_SIZE);
#else
  PrettyAsyncJsonResponse(bool isArray = false);
#endif
  size_t setLength() override;
  size_t _fillBuffer(uint8_t *data, size_t len) override;
};

// MessagePack content type response
#if ASYNC_MSG_PACK_SUPPORT == 1

class AsyncMessagePackResponse : public AsyncJsonResponse {
public:
#if ARDUINOJSON_VERSION_MAJOR == 6
  AsyncMessagePackResponse(bool isArray = false, size_t maxJsonBufferSize = DYNAMIC_JSON_DOCUMENT_SIZE) : AsyncJsonResponse(isArray, maxJsonBufferSize) {
    _contentType = asyncsrv::T_application_msgpack;
  }
#else
  AsyncMessagePackResponse(bool isArray = false) : AsyncJsonResponse(isArray) {
    _contentType = asyncsrv::T_application_msgpack;
  }
#endif
  size_t setLength() override;
  size_t _fillBuffer(uint8_t *data, size_t len) override;
};

#endif

// Body handler supporting both content types: JSON and MessagePack

class AsyncCallbackJsonWebHandler : public AsyncWebHandler {
protected:
  String _uri;
  WebRequestMethodComposite _method;
  ArJsonRequestHandlerFunction _onRequest;
#if ARDUINOJSON_VERSION_MAJOR == 6
  size_t maxJsonBufferSize;
#endif
  size_t _maxContentLength;

public:
#if ARDUINOJSON_VERSION_MAJOR == 6
  AsyncCallbackJsonWebHandler(const String &uri, ArJsonRequestHandlerFunction onRequest = nullptr, size_t maxJsonBufferSize = DYNAMIC_JSON_DOCUMENT_SIZE);
#else
  AsyncCallbackJsonWebHandler(const String &uri, ArJsonRequestHandlerFunction onRequest = nullptr);
#endif

  void setMethod(WebRequestMethodComposite method) {
    _method = method;
  }
  void setMaxContentLength(int maxContentLength) {
    _maxContentLength = maxContentLength;
  }
  void onRequest(ArJsonRequestHandlerFunction fn) {
    _onRequest = fn;
  }

  bool canHandle(AsyncWebServerRequest *request) const override final;
  void handleRequest(AsyncWebServerRequest *request) override final;
  void handleUpload(
    __unused AsyncWebServerRequest *request, __unused const String &filename, __unused size_t index, __unused uint8_t *data, __unused size_t len,
    __unused bool final
  ) override final {}
  void handleBody(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) override final;
  bool isRequestHandlerTrivial() const override final {
    return !_onRequest;
  }
};

#endif  // ASYNC_JSON_SUPPORT == 1
