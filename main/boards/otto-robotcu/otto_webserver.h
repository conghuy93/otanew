#ifndef OTTO_WEBSERVER_H
#define OTTO_WEBSERVER_H

#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

// WiFi credentials - Update these for your network
#define WIFI_SSID      "Huywifi"
#define WIFI_PASS      "0389827643"
#define WIFI_MAXIMUM_RETRY  5

// No authentication required - direct control
extern bool webserver_enabled;

// Function declarations
esp_err_t otto_wifi_init_sta(void);
esp_err_t otto_start_webserver(void);
esp_err_t otto_auto_start_webserver_if_wifi_connected(void);
esp_err_t otto_register_wifi_listener(void);
void otto_wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
void otto_system_wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);

// HTTP handlers
esp_err_t otto_root_handler(httpd_req_t *req);
esp_err_t otto_action_handler(httpd_req_t *req);
esp_err_t otto_status_handler(httpd_req_t *req);

// Otto action constants
#define ACTION_DOG_WALK            1
#define ACTION_DOG_WALK_BACK       2
#define ACTION_DOG_TURN_LEFT       3
#define ACTION_DOG_TURN_RIGHT      4
#define ACTION_DOG_SIT_DOWN        5
#define ACTION_DOG_LIE_DOWN        6
#define ACTION_DOG_JUMP            7
#define ACTION_DOG_BOW             8
#define ACTION_DOG_DANCE           9
#define ACTION_DOG_WAVE_RIGHT_FOOT 10
#define ACTION_DOG_DANCE_4_FEET    11
#define ACTION_DOG_SWING           12
#define ACTION_DOG_STRETCH         13
#define ACTION_WALK                14
#define ACTION_TURN                15
#define ACTION_JUMP                16
#define ACTION_HOME                17

// Otto control interface
void otto_execute_web_action(const char* action, int param1, int param2);

// Otto controller access
esp_err_t otto_controller_queue_action(int action_type, int steps, int speed, int direction, int amount);

#ifdef __cplusplus
}
#endif

#endif // OTTO_WEBSERVER_H