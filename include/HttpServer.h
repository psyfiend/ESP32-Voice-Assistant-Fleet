#pragma once
//
// HttpServer - the device's one HTTP server, on ESP-IDF's esp_http_server.
//
// Issue #58 brought it in for screenshots; Phase 4's web config page (4.5) and
// the captive portal are meant to hang their routes off this same instance
// rather than start a second server.
//
// WHY esp_http_server AND NOT ARDUINO'S WebServer. ROADMAP section 5.5 once
// chose the synchronous Arduino WebServer pumped from loop(). The owner
// overruled that on 2026-09-24: the project avoids Arduino-only facilities so a
// later move to ESP-IDF is not a port. esp_http_server IS the IDF server, it is
// already linked into both prebuilt framework variants, and this file uses
// nothing from Arduino except Serial for its two log lines.
//
// The cost of that choice is threading: handlers run on the server's OWN TASK,
// not on loop(). A handler must therefore never touch LVGL - the same rule as
// every provider (ROADMAP 4.2). A handler that needs the screen asks the LVGL
// thread for it and waits; see UI/Screenshot.cpp for the pattern.
//
// Like SystemCore, this file includes no LVGL header. Routes are registered by
// whoever owns them - "UI code registers with lower layers" - and the server
// only starts if somebody registered one, so a build with no routes pays for
// no task and no socket.
//
#include <stdint.h>
#include <esp_http_server.h>

class HttpServer {
public:
    using Handler = esp_err_t (*)(httpd_req_t *req);

    static constexpr uint16_t PORT       = 80;
    static constexpr uint8_t  MAX_ROUTES = 8;

    // Register a route. Allowed before or after the server starts; before, it
    // is queued and registered at start. `uri` must outlive the server (a
    // string literal). Returns false when the table is full or IDF refuses it.
    bool addRoute(const char *uri, httpd_method_t method, Handler handler);

    // Call every loop(). Starts the server the first time the link is up and
    // at least one route exists; after that it does nothing. The server is not
    // stopped when the link drops - its listening socket is bound to every
    // interface and serves again as soon as the link returns.
    void loop(bool online);

    bool     running() const { return _server != nullptr; }
    uint8_t  routeCount() const { return _count; }
    const char *routeUri(uint8_t i) const { return i < _count ? _routes[i].uri : nullptr; }

private:
    bool start();
    bool registerRoute(uint8_t i);

    struct Route {
        const char     *uri;
        httpd_method_t  method;
        Handler         handler;
    };

    httpd_handle_t _server = nullptr;
    Route          _routes[MAX_ROUTES] = {};
    uint8_t        _count = 0;
    bool           _failed = false;   // start failed once; do not retry every loop
};
