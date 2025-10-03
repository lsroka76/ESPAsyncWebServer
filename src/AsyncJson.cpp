// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright 2016-2025 Hristo Gochkov, Mathieu Carbou, Emil Muratov

#include "AsyncJson.h"
#include "AsyncWebServerLogging.h"

#if ASYNC_JSON_SUPPORT == 1

// Json content type response classes

#if ARDUINOJSON_VERSION_MAJOR == 5
AsyncJsonResponse::AsyncJsonResponse(bool isArray) : _isValid{false} {
  _code = 200;
  _contentType = asyncsrv::T_application_json;
  if (isArray) {
    _root = _jsonBuffer.createArray();
  } else {
    _root = _jsonBuffer.createObject();
  }
}
#elif ARDUINOJSON_VERSION_MAJOR == 6
AsyncJsonResponse::AsyncJsonResponse(bool isArray, size_t maxJsonBufferSize) : _jsonBuffer(maxJsonBufferSize), _isValid{false} {
  _code = 200;
  _contentType = asyncsrv::T_application_json;
  if (isArray) {
    _root = _jsonBuffer.createNestedArray();
  } else {
    _root = _jsonBuffer.createNestedObject();
  }
}
#else
AsyncJsonResponse::AsyncJsonResponse(bool isArray) : _isValid{false} {
  _code = 200;
  _contentType = asyncsrv::T_application_json;
  if (isArray) {
    _root = _jsonBuffer.add<JsonArray>();
  } else {
    _root = _jsonBuffer.add<JsonObject>();
  }
}
#endif

size_t AsyncJsonResponse::setLength() {
#if ARDUINOJSON_VERSION_MAJOR == 5
  _contentLength = _root.measureLength();
#else
  _contentLength = measureJson(_root);
#endif
  if (_contentLength) {
    _isValid = true;
  }
  return _contentLength;
}

size_t AsyncJsonResponse::_fillBuffer(uint8_t *data, size_t len) {
  ChunkPrint dest(data, _sentLength, len);
#if ARDUINOJSON_VERSION_MAJOR == 5
  _root.printTo(dest);
#else
  serializeJson(_root, dest);
#endif
  return len;
}

#if ARDUINOJSON_VERSION_MAJOR == 6
PrettyAsyncJsonResponse::PrettyAsyncJsonResponse(bool isArray, size_t maxJsonBufferSize) : AsyncJsonResponse{isArray, maxJsonBufferSize} {}
#else
PrettyAsyncJsonResponse::PrettyAsyncJsonResponse(bool isArray) : AsyncJsonResponse{isArray} {}
#endif

size_t PrettyAsyncJsonResponse::setLength() {
#if ARDUINOJSON_VERSION_MAJOR == 5
  _contentLength = _root.measurePrettyLength();
#else
  _contentLength = measureJsonPretty(_root);
#endif
  if (_contentLength) {
    _isValid = true;
  }
  return _contentLength;
}

size_t PrettyAsyncJsonResponse::_fillBuffer(uint8_t *data, size_t len) {
  ChunkPrint dest(data, _sentLength, len);
#if ARDUINOJSON_VERSION_MAJOR == 5
  _root.prettyPrintTo(dest);
#else
  serializeJsonPretty(_root, dest);
#endif
  return len;
}

// MessagePack content type response
#if ASYNC_MSG_PACK_SUPPORT == 1

size_t AsyncMessagePackResponse::setLength() {
  _contentLength = measureMsgPack(_root);
  if (_contentLength) {
    _isValid = true;
  }
  return _contentLength;
}

size_t AsyncMessagePackResponse::_fillBuffer(uint8_t *data, size_t len) {
  ChunkPrint dest(data, _sentLength, len);
  serializeMsgPack(_root, dest);
  return len;
}

#endif

// Body handler supporting both content types: JSON and MessagePack

#if ARDUINOJSON_VERSION_MAJOR == 6
AsyncCallbackJsonWebHandler::AsyncCallbackJsonWebHandler(const String &uri, ArJsonRequestHandlerFunction onRequest, size_t maxJsonBufferSize)
  : _uri(uri), _method(HTTP_GET | HTTP_POST | HTTP_PUT | HTTP_PATCH), _onRequest(onRequest), maxJsonBufferSize(maxJsonBufferSize), _maxContentLength(16384) {}
#else
AsyncCallbackJsonWebHandler::AsyncCallbackJsonWebHandler(const String &uri, ArJsonRequestHandlerFunction onRequest)
  : _uri(uri), _method(HTTP_GET | HTTP_POST | HTTP_PUT | HTTP_PATCH), _onRequest(onRequest), _maxContentLength(16384) {}
#endif

bool AsyncCallbackJsonWebHandler::canHandle(AsyncWebServerRequest *request) const {
  if (!_onRequest || !request->isHTTP() || !(_method & request->method())) {
    return false;
  }

  if (_uri.length() && (_uri != request->url() && !request->url().startsWith(_uri + "/"))) {
    return false;
  }

#if ASYNC_MSG_PACK_SUPPORT == 1
  return request->method() == HTTP_GET || request->contentType().equalsIgnoreCase(asyncsrv::T_application_json)
         || request->contentType().equalsIgnoreCase(asyncsrv::T_application_msgpack);
#else
  return request->method() == HTTP_GET || request->contentType().equalsIgnoreCase(asyncsrv::T_application_json);
#endif
}

void AsyncCallbackJsonWebHandler::handleRequest(AsyncWebServerRequest *request) {
  if (_onRequest) {
    // GET request:
    if (request->method() == HTTP_GET) {
      JsonVariant json;
      _onRequest(request, json);
      return;
    }

    // POST / PUT / ... requests:
    // check if JSON body is too large, if it is, don't deserialize
    if (request->contentLength() > _maxContentLength) {
      async_ws_log_e("Content length exceeds maximum allowed");
      request->send(413);
      return;
    }

    if (request->_tempObject == NULL) {
      // there is no body
      request->send(400);
      return;
    }

#if ARDUINOJSON_VERSION_MAJOR == 5
    DynamicJsonBuffer doc;
#elif ARDUINOJSON_VERSION_MAJOR == 6
    DynamicJsonDocument doc(this->maxJsonBufferSize);
#else
    JsonDocument doc;
#endif

#if ARDUINOJSON_VERSION_MAJOR == 5
    JsonVariant json = doc.parse((const char *)request->_tempObject);
    if (json.success()) {
      _onRequest(request, json);
      return;
    }
#else
    DeserializationError error = request->contentType().equalsIgnoreCase(asyncsrv::T_application_msgpack)
                                   ? deserializeMsgPack(doc, (uint8_t *)(request->_tempObject))
                                   : deserializeJson(doc, (const char *)request->_tempObject);
    if (!error) {
      JsonVariant json = doc.as<JsonVariant>();
      _onRequest(request, json);
      return;
    }
#endif

    // error parsing the body
    request->send(400);
  }
}

void AsyncCallbackJsonWebHandler::handleBody(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
  if (_onRequest) {
    // ignore callback if size is larger than maxContentLength
    if (total > _maxContentLength) {
      return;
    }

    if (index == 0) {
      // this check allows request->_tempObject to be initialized from a middleware
      if (request->_tempObject == NULL) {
        request->_tempObject = calloc(total + 1, sizeof(uint8_t));  // null-terminated string
        if (request->_tempObject == NULL) {
          async_ws_log_e("Failed to allocate");
          request->abort();
          return;
        }
      }
    }

    if (request->_tempObject != NULL) {
      uint8_t *buffer = (uint8_t *)request->_tempObject;
      memcpy(buffer + index, data, len);
    }
  }
}

#endif  // ASYNC_JSON_SUPPORT
