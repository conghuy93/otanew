#include "otto_webserver.h"
#include "mcp_server.h"
#include "application.h"
#include <cJSON.h>
#include <stdio.h>

extern "C" {

static const char *TAG = "OttoWeb";

// Global variables
bool webserver_enabled = false;
static httpd_handle_t server = NULL;
static int s_retry_num = 0;

// WiFi event handler for monitoring system WiFi connection
void otto_system_wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "System WiFi connected, IP: " IPSTR, IP2STR(&event->ip_info.ip));
        ESP_LOGI(TAG, "🌐 Otto Web Controller available at: http://" IPSTR, IP2STR(&event->ip_info.ip));
        
        // Start Otto web server automatically
        if (server == NULL) {
            otto_start_webserver();
        }
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGI(TAG, "System WiFi disconnected, Otto Web Controller stopped");
    }
}

// Register to listen for system WiFi events
esp_err_t otto_register_wifi_listener(void) {
    esp_event_handler_instance_t instance_got_ip;
    esp_event_handler_instance_t instance_disconnected;
    
    esp_err_t ret = esp_event_handler_instance_register(IP_EVENT,
                                                       IP_EVENT_STA_GOT_IP,
                                                       &otto_system_wifi_event_handler,
                                                       NULL,
                                                       &instance_got_ip);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register IP event handler: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ret = esp_event_handler_instance_register(WIFI_EVENT,
                                             WIFI_EVENT_STA_DISCONNECTED,
                                             &otto_system_wifi_event_handler,
                                             NULL,
                                             &instance_disconnected);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register WiFi event handler: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "Otto WiFi event listener registered");
    return ESP_OK;
}

// WiFi event handler function (inside extern "C" block but separate definition)  
void otto_wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < WIFI_MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "Retry to connect to WiFi AP");
        } else {
            ESP_LOGI(TAG, "Failed to connect to WiFi AP");
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        
        // Start webserver when WiFi connected
        otto_start_webserver();
    }
}

// Start HTTP server automatically when WiFi is connected
esp_err_t otto_auto_start_webserver_if_wifi_connected(void) {
    // Check if WiFi is already connected (from main system)
    wifi_ap_record_t ap_info;
    esp_err_t wifi_status = esp_wifi_sta_get_ap_info(&ap_info);
    
    if (wifi_status == ESP_OK) {
        ESP_LOGI(TAG, "WiFi already connected to: %s", ap_info.ssid);
        
        // Get current IP
        esp_netif_t* netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
        if (netif) {
            esp_netif_ip_info_t ip_info;
            if (esp_netif_get_ip_info(netif, &ip_info) == ESP_OK) {
                ESP_LOGI(TAG, "Current IP: " IPSTR, IP2STR(&ip_info.ip));
                ESP_LOGI(TAG, "Otto Web Controller will be available at: http://" IPSTR, IP2STR(&ip_info.ip));
                
                // Start web server immediately
                return otto_start_webserver();
            }
        }
    } else {
        ESP_LOGI(TAG, "WiFi not connected yet, Otto Web Controller will start when WiFi connects");
    }
    
    return ESP_OK;
}

// Original WiFi initialization (for standalone mode if needed)
esp_err_t otto_wifi_init_sta(void) {
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &otto_wifi_event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &otto_wifi_event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {};
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    wifi_config.sta.pmf_cfg.capable = true;
    wifi_config.sta.pmf_cfg.required = false;
    
    strncpy((char*)wifi_config.sta.ssid, WIFI_SSID, sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char*)wifi_config.sta.password, WIFI_PASS, sizeof(wifi_config.sta.password) - 1);
    
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "WiFi initialization finished");

    return ESP_OK;
}

// Send main control page HTML
void send_otto_control_page(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html");
    
    // Modern responsive HTML with Otto Robot theme
    httpd_resp_sendstr_chunk(req, "<!DOCTYPE html><html><head><meta charset='UTF-8'>");
    httpd_resp_sendstr_chunk(req, "<meta name='viewport' content='width=device-width, initial-scale=1.0'>");
    httpd_resp_sendstr_chunk(req, "<title>Dogmaster Control</title>");
    
    // CSS Styling - Modern Dark Cyber Theme
    httpd_resp_sendstr_chunk(req, "<style>");
    httpd_resp_sendstr_chunk(req, "* { margin: 0; padding: 0; box-sizing: border-box; }");
    httpd_resp_sendstr_chunk(req, "body { font-family: 'Segoe UI', 'Inter', sans-serif; background: linear-gradient(135deg, #0f0f0f 0%, #1a1a2e 20%, #16213e 50%, #0f3460 100%); min-height: 100vh; display: flex; justify-content: center; align-items: center; color: #e0e0e0; }");
    httpd_resp_sendstr_chunk(req, ".container { max-width: 800px; width: 95%; background: linear-gradient(145deg, rgba(255,255,255,0.05), rgba(255,255,255,0.1)); backdrop-filter: blur(20px); border-radius: 25px; padding: 30px; box-shadow: 0 15px 50px rgba(0,0,0,0.4), inset 0 1px 0 rgba(255,255,255,0.1); border: 1px solid rgba(255,255,255,0.1); }");
    httpd_resp_sendstr_chunk(req, ".header { text-align: center; margin-bottom: 30px; }");
    httpd_resp_sendstr_chunk(req, ".header h1 { font-size: 2.8em; margin-bottom: 10px; background: linear-gradient(45deg, #00d4ff, #ff00ff, #00ff88); -webkit-background-clip: text; -webkit-text-fill-color: transparent; text-shadow: 0 0 30px rgba(0,212,255,0.5); animation: glow 2s ease-in-out infinite alternate; }");
    httpd_resp_sendstr_chunk(req, "@keyframes glow { from { filter: drop-shadow(0 0 10px #00d4ff); } to { filter: drop-shadow(0 0 20px #ff00ff); } }");
    httpd_resp_sendstr_chunk(req, ".status { background: linear-gradient(45deg, rgba(0,255,150,0.2), rgba(0,200,255,0.2)); padding: 12px; border-radius: 15px; margin-bottom: 25px; text-align: center; border: 2px solid #00ff96; box-shadow: 0 5px 20px rgba(0,255,150,0.3); }");
    
    // Button styling - Cyber Neon Theme
    httpd_resp_sendstr_chunk(req, ".control-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(150px, 1fr)); gap: 15px; margin-bottom: 30px; }");
    httpd_resp_sendstr_chunk(req, ".btn { background: linear-gradient(145deg, #1a1a2e, #16213e); border: 2px solid #00d4ff; color: #00d4ff; padding: 15px 20px; border-radius: 20px; cursor: pointer; font-size: 14px; font-weight: bold; transition: all 0.4s ease; box-shadow: 0 5px 25px rgba(0,212,255,0.2), inset 0 1px 0 rgba(255,255,255,0.1); position: relative; overflow: hidden; }");
    httpd_resp_sendstr_chunk(req, ".btn::before { content: ''; position: absolute; top: 0; left: -100%; width: 100%; height: 100%; background: linear-gradient(90deg, transparent, rgba(0,212,255,0.4), transparent); transition: left 0.6s; }");
    httpd_resp_sendstr_chunk(req, ".btn:hover { transform: translateY(-3px); box-shadow: 0 10px 30px rgba(0,212,255,0.4), 0 0 20px rgba(0,212,255,0.3); color: #ffffff; border-color: #ff00ff; text-shadow: 0 0 10px #00d4ff; }");
    httpd_resp_sendstr_chunk(req, ".btn:hover::before { left: 100%; }");
    httpd_resp_sendstr_chunk(req, ".btn:active { transform: translateY(-1px); }");
    
    // Movement controls - Enhanced styling
    httpd_resp_sendstr_chunk(req, ".movement-section { margin-bottom: 30px; }");
    httpd_resp_sendstr_chunk(req, ".section-title { font-size: 1.4em; margin-bottom: 20px; text-align: center; background: linear-gradient(45deg, #00ff96, #00d4ff); -webkit-background-clip: text; -webkit-text-fill-color: transparent; text-shadow: 0 0 20px rgba(0,255,150,0.5); }");
    httpd_resp_sendstr_chunk(req, ".direction-pad { display: grid; grid-template-columns: 1fr 1fr 1fr; grid-template-rows: 1fr 1fr 1fr; gap: 12px; max-width: 320px; margin: 0 auto; }");
    httpd_resp_sendstr_chunk(req, ".direction-pad .btn { padding: 22px; font-size: 16px; font-weight: 600; }");
    httpd_resp_sendstr_chunk(req, ".btn-forward { grid-column: 2; grid-row: 1; border-color: #00ff96; color: #00ff96; }");
    httpd_resp_sendstr_chunk(req, ".btn-left { grid-column: 1; grid-row: 2; border-color: #ffaa00; color: #ffaa00; }");
    httpd_resp_sendstr_chunk(req, ".btn-stop { grid-column: 2; grid-row: 2; background: linear-gradient(145deg, #ff1744, #d50000); border-color: #ff1744; color: #ffffff; box-shadow: 0 5px 25px rgba(255,23,68,0.4); }");
    httpd_resp_sendstr_chunk(req, ".btn-right { grid-column: 3; grid-row: 2; border-color: #ffaa00; color: #ffaa00; }");
    httpd_resp_sendstr_chunk(req, ".btn-backward { grid-column: 2; grid-row: 3; border-color: #ff6b00; color: #ff6b00; }");
    
    // Fun actions - Purple/Pink theme for fun buttons
    httpd_resp_sendstr_chunk(req, ".fun-actions { margin-top: 25px; }");
    httpd_resp_sendstr_chunk(req, ".action-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(130px, 1fr)); gap: 12px; }");
    httpd_resp_sendstr_chunk(req, ".fun-actions .btn { border-color: #ff00ff; color: #ff00ff; background: linear-gradient(145deg, #2a1a2e, #3d1a4d); }");
    httpd_resp_sendstr_chunk(req, ".fun-actions .btn:hover { border-color: #00ff96; color: #ffffff; box-shadow: 0 10px 30px rgba(255,0,255,0.4), 0 0 20px rgba(255,0,255,0.3); }");
    
    // Response styling - Dark glass effect
    httpd_resp_sendstr_chunk(req, ".response { margin-top: 25px; padding: 18px; background: linear-gradient(145deg, rgba(0,0,0,0.3), rgba(0,0,0,0.1)); border-radius: 15px; min-height: 60px; border: 1px solid rgba(0,212,255,0.3); box-shadow: inset 0 1px 0 rgba(255,255,255,0.1); color: #00d4ff; }");
    httpd_resp_sendstr_chunk(req, "</style>");
    
    httpd_resp_sendstr_chunk(req, "</head><body>");
    
    // HTML Content
    httpd_resp_sendstr_chunk(req, "<div class='container'>");
    httpd_resp_sendstr_chunk(req, "<div class='header'>");
    httpd_resp_sendstr_chunk(req, "<div style='display: flex; justify-content: space-between; align-items: center; margin-bottom: 20px;'>");
    httpd_resp_sendstr_chunk(req, "<h1 style='margin: 0;'>🤖 Dogmaster Control</h1>");
    httpd_resp_sendstr_chunk(req, "<div style='font-size: 1.2em; color: #00d4ff; font-weight: bold;'>miniZ</div>");
    httpd_resp_sendstr_chunk(req, "</div>");
    httpd_resp_sendstr_chunk(req, "<div class='status' id='status'>🟢 Otto Ready - No Password Required!</div>");
    httpd_resp_sendstr_chunk(req, "</div>");
    
    // Movement Controls
    httpd_resp_sendstr_chunk(req, "<div class='movement-section'>");
    httpd_resp_sendstr_chunk(req, "<div class='section-title'>🎮 Movement Controls</div>");
    httpd_resp_sendstr_chunk(req, "<div class='direction-pad'>");
    httpd_resp_sendstr_chunk(req, "<button class='btn btn-forward' onclick='sendAction(\"dog_walk\", 3, 150)'>⬆️ Forward</button>");
    httpd_resp_sendstr_chunk(req, "<button class='btn btn-left' onclick='sendAction(\"dog_turn_left\", 2, 150)'>⬅️ Left</button>");
    httpd_resp_sendstr_chunk(req, "<button class='btn btn-stop' onclick='sendAction(\"dog_stop\", 0, 0)'>🛑 STOP</button>");
    httpd_resp_sendstr_chunk(req, "<button class='btn btn-right' onclick='sendAction(\"dog_turn_right\", 2, 150)'>➡️ Right</button>");
    httpd_resp_sendstr_chunk(req, "<button class='btn btn-backward' onclick='sendAction(\"dog_walk_back\", 3, 150)'>⬇️ Backward</button>");
    httpd_resp_sendstr_chunk(req, "</div>");
    httpd_resp_sendstr_chunk(req, "</div>");
    
    // Fun Actions
    httpd_resp_sendstr_chunk(req, "<div class='fun-actions'>");
    httpd_resp_sendstr_chunk(req, "<div class='section-title'>🎪 Fun Actions</div>");
    httpd_resp_sendstr_chunk(req, "<div class='action-grid'>");
    httpd_resp_sendstr_chunk(req, "<button class='btn' onclick='sendAction(\"dog_dance\", 3, 200)'>💃 Dance</button>");
    httpd_resp_sendstr_chunk(req, "<button class='btn' onclick='sendAction(\"dog_jump\", 1, 200)'>🦘 Jump</button>");
    httpd_resp_sendstr_chunk(req, "<button class='btn' onclick='sendAction(\"dog_bow\", 1, 2000)'>🙇 Bow</button>");
    httpd_resp_sendstr_chunk(req, "<button class='btn' onclick='sendAction(\"dog_sit_down\", 1, 500)'>🪑 Sit</button>");
    httpd_resp_sendstr_chunk(req, "<button class='btn' onclick='sendAction(\"dog_lie_down\", 1, 1000)'>🛏️ Lie Down</button>");
    httpd_resp_sendstr_chunk(req, "<button class='btn' onclick='sendAction(\"dog_wave_right_foot\", 5, 50)'>👋 Wave</button>");
    httpd_resp_sendstr_chunk(req, "<button class='btn' onclick='sendAction(\"dog_swing\", 5, 10)'>🎯 Swing</button>");
    httpd_resp_sendstr_chunk(req, "<button class='btn' onclick='sendAction(\"dog_stretch\", 2, 15)'>🧘 Stretch</button>");
    httpd_resp_sendstr_chunk(req, "<button class='btn' onclick='sendAction(\"dog_home\", 1, 500)'>🏠 Home</button>");
    httpd_resp_sendstr_chunk(req, "</div>");
    httpd_resp_sendstr_chunk(req, "</div>");
    
    // Response area
    httpd_resp_sendstr_chunk(req, "<div class='response' id='response'>Ready for commands...</div>");
    httpd_resp_sendstr_chunk(req, "</div>");
    
    // JavaScript with enhanced status colors
    httpd_resp_sendstr_chunk(req, "<script>");
    httpd_resp_sendstr_chunk(req, "function sendAction(action, param1, param2) {");
    httpd_resp_sendstr_chunk(req, "  document.getElementById('status').innerHTML = '⚡ Executing: ' + action;");
    httpd_resp_sendstr_chunk(req, "  document.getElementById('status').style.borderColor = '#ffaa00';");
    httpd_resp_sendstr_chunk(req, "  document.getElementById('status').style.background = 'linear-gradient(45deg, rgba(255,170,0,0.2), rgba(255,100,0,0.2))';");
    httpd_resp_sendstr_chunk(req, "  fetch('/action?cmd=' + action + '&p1=' + param1 + '&p2=' + param2)");
    httpd_resp_sendstr_chunk(req, "    .then(response => response.text())");
    httpd_resp_sendstr_chunk(req, "    .then(data => {");
    httpd_resp_sendstr_chunk(req, "      document.getElementById('response').innerHTML = data;");
    httpd_resp_sendstr_chunk(req, "      document.getElementById('status').innerHTML = '✨ Otto Ready';");
    httpd_resp_sendstr_chunk(req, "      document.getElementById('status').style.borderColor = '#00ff96';");
    httpd_resp_sendstr_chunk(req, "      document.getElementById('status').style.background = 'linear-gradient(45deg, rgba(0,255,150,0.2), rgba(0,200,255,0.2))';");
    httpd_resp_sendstr_chunk(req, "    })");
    httpd_resp_sendstr_chunk(req, "    .catch(error => {");
    httpd_resp_sendstr_chunk(req, "      document.getElementById('response').innerHTML = 'Error: ' + error;");
    httpd_resp_sendstr_chunk(req, "      document.getElementById('status').innerHTML = '❌ Error';");
    httpd_resp_sendstr_chunk(req, "      document.getElementById('status').style.borderColor = '#ff1744';");
    httpd_resp_sendstr_chunk(req, "      document.getElementById('status').style.background = 'linear-gradient(45deg, rgba(255,23,68,0.2), rgba(213,0,0,0.2))';");
    httpd_resp_sendstr_chunk(req, "    });");
    httpd_resp_sendstr_chunk(req, "}");
    
    // Auto refresh status with dynamic colors
    httpd_resp_sendstr_chunk(req, "setInterval(function() {");
    httpd_resp_sendstr_chunk(req, "  fetch('/status')");
    httpd_resp_sendstr_chunk(req, "    .then(response => response.text())");
    httpd_resp_sendstr_chunk(req, "    .then(data => {");
    httpd_resp_sendstr_chunk(req, "      if (data.includes('busy')) {");
    httpd_resp_sendstr_chunk(req, "        document.getElementById('status').innerHTML = '⚡ Otto is busy';");
    httpd_resp_sendstr_chunk(req, "        document.getElementById('status').style.borderColor = '#ffaa00';");
    httpd_resp_sendstr_chunk(req, "        document.getElementById('status').style.background = 'linear-gradient(45deg, rgba(255,170,0,0.2), rgba(255,100,0,0.2))';");
    httpd_resp_sendstr_chunk(req, "      } else {");
    httpd_resp_sendstr_chunk(req, "        document.getElementById('status').innerHTML = '✨ Otto Ready';");
    httpd_resp_sendstr_chunk(req, "        document.getElementById('status').style.borderColor = '#00ff96';");
    httpd_resp_sendstr_chunk(req, "        document.getElementById('status').style.background = 'linear-gradient(45deg, rgba(0,255,150,0.2), rgba(0,200,255,0.2))';");
    httpd_resp_sendstr_chunk(req, "      }");
    httpd_resp_sendstr_chunk(req, "    });");
    httpd_resp_sendstr_chunk(req, "}, 2000);");
    
    httpd_resp_sendstr_chunk(req, "</script>");
    httpd_resp_sendstr_chunk(req, "</body></html>");
    
    httpd_resp_sendstr_chunk(req, NULL); // End of chunks
}

// Root page handler
esp_err_t otto_root_handler(httpd_req_t *req) {
    ESP_LOGI(TAG, "Root page requested");
    send_otto_control_page(req);
    return ESP_OK;
}

} // extern "C"

// C++ function to execute Otto actions (with real controller integration)
void otto_execute_web_action(const char* action, int param1, int param2) {
    ESP_LOGI(TAG, "🎮 Web Control: %s (param1:%d, param2:%d)", action, param1, param2);
    
    // Map web actions to controller actions (order matters - check specific first)
    esp_err_t ret = ESP_OK;
    
    if (strstr(action, "walk_back")) {
        ret = otto_controller_queue_action(ACTION_DOG_WALK_BACK, param1, param2, 0, 0);
        ESP_LOGI(TAG, "🐕 Walking backward: %d steps, speed %d", param1, param2);
    } else if (strstr(action, "walk_forward") || strstr(action, "walk")) {
        ret = otto_controller_queue_action(ACTION_DOG_WALK, param1, param2, 0, 0);
        ESP_LOGI(TAG, "🐕 Walking forward: %d steps, speed %d", param1, param2);
    } else if (strstr(action, "turn_left") || (strstr(action, "turn") && param1 < 0)) {
        ret = otto_controller_queue_action(ACTION_DOG_TURN_LEFT, abs(param1), param2, 0, 0);
        ESP_LOGI(TAG, "🐕 Turning left: %d steps, speed %d", abs(param1), param2);
    } else if (strstr(action, "turn_right") || (strstr(action, "turn") && param1 > 0)) {
        ret = otto_controller_queue_action(ACTION_DOG_TURN_RIGHT, param1, param2, 0, 0);
        ESP_LOGI(TAG, "🐕 Turning right: %d steps, speed %d", param1, param2);
    } else if (strstr(action, "turn")) {
        // Default turn right if no direction specified
        ret = otto_controller_queue_action(ACTION_DOG_TURN_RIGHT, param1, param2, 0, 0);
        ESP_LOGI(TAG, "🐕 Turning right (default): %d steps, speed %d", param1, param2);
    } else if (strstr(action, "sit")) {
        ret = otto_controller_queue_action(ACTION_DOG_SIT_DOWN, 1, param2, 0, 0);
        ESP_LOGI(TAG, "🐕 Sitting down with delay %d", param2);
    } else if (strstr(action, "lie")) {
        ret = otto_controller_queue_action(ACTION_DOG_LIE_DOWN, 1, param2, 0, 0);
        ESP_LOGI(TAG, "🐕 Lying down with delay %d", param2);
    } else if (strstr(action, "bow")) {
        ret = otto_controller_queue_action(ACTION_DOG_BOW, 1, param2, 0, 0);
        ESP_LOGI(TAG, "🐕 Bowing with delay %d", param2);
    } else if (strstr(action, "jump")) {
        ret = otto_controller_queue_action(ACTION_DOG_JUMP, 1, param2, 0, 0);
        ESP_LOGI(TAG, "🐕 Jumping with delay %d", param2);
    } else if (strstr(action, "dance")) {
        ret = otto_controller_queue_action(ACTION_DOG_DANCE, param1, param2, 0, 0);
        ESP_LOGI(TAG, "🐕 Dancing: %d cycles, speed %d", param1, param2);
    } else if (strstr(action, "wave")) {
        ret = otto_controller_queue_action(ACTION_DOG_WAVE_RIGHT_FOOT, param1, param2, 0, 0);
        ESP_LOGI(TAG, "🐕 Waving: %d times, speed %d", param1, param2);
    } else if (strstr(action, "swing")) {
        ret = otto_controller_queue_action(ACTION_DOG_SWING, param1, param2, 0, 0);
        ESP_LOGI(TAG, "🐕 Swinging: %d cycles, speed %d", param1, param2);
    } else if (strstr(action, "stretch")) {
        ret = otto_controller_queue_action(ACTION_DOG_STRETCH, param1, param2, 0, 0);
        ESP_LOGI(TAG, "🐕 Stretching: %d cycles, speed %d", param1, param2);
    } else if (strstr(action, "home")) {
        ret = otto_controller_queue_action(ACTION_HOME, 1, 500, 0, 0);
        ESP_LOGI(TAG, "🏠 Going to home position");
    } else {
        ESP_LOGW(TAG, "❌ Unknown action: %s", action);
        return;
    }
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "✅ Action queued successfully");
    } else {
        ESP_LOGE(TAG, "❌ Failed to queue action: %s", esp_err_to_name(ret));
    }
}

extern "C" {

// Action handler
esp_err_t otto_action_handler(httpd_req_t *req) {
    char query[200] = {0};
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
        char cmd[50] = {0};
        char p1_str[20] = {0};
        char p2_str[20] = {0};
        
        httpd_query_key_value(query, "cmd", cmd, sizeof(cmd));
        httpd_query_key_value(query, "p1", p1_str, sizeof(p1_str));
        httpd_query_key_value(query, "p2", p2_str, sizeof(p2_str));
        
        int param1 = atoi(p1_str);
        int param2 = atoi(p2_str);
        
        ESP_LOGI(TAG, "Action: %s, P1: %d, P2: %d", cmd, param1, param2);
        
        // Execute action
        otto_execute_web_action(cmd, param1, param2);
        
        // Send response
        httpd_resp_set_type(req, "text/plain");
        char response[200];
        snprintf(response, sizeof(response), "✅ Otto executed: %s (steps: %d, speed: %d)", cmd, param1, param2);
        httpd_resp_sendstr(req, response);
    } else {
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_sendstr(req, "❌ Missing action parameters");
    }
    
    return ESP_OK;
}

// Status handler
esp_err_t otto_status_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/plain");
    
    // Simple status - can be expanded with actual Otto status
    httpd_resp_sendstr(req, "ready");
    
    return ESP_OK;
}

// Start HTTP server
esp_err_t otto_start_webserver(void) {
    if (server != NULL) {
        ESP_LOGW(TAG, "Server already running");
        return ESP_OK;
    }
    
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.max_uri_handlers = 10;
    config.max_resp_headers = 8;
    config.stack_size = 8192;
    
    ESP_LOGI(TAG, "Starting HTTP server on port %d", config.server_port);
    
    if (httpd_start(&server, &config) == ESP_OK) {
        // Register URI handlers
        httpd_uri_t root_uri = {
            .uri = "/",
            .method = HTTP_GET,
            .handler = otto_root_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &root_uri);
        
        httpd_uri_t action_uri = {
            .uri = "/action",
            .method = HTTP_GET,
            .handler = otto_action_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &action_uri);
        
        httpd_uri_t status_uri = {
            .uri = "/status",
            .method = HTTP_GET,
            .handler = otto_status_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &status_uri);
        
        ESP_LOGI(TAG, "HTTP server started successfully");
        webserver_enabled = true;
        return ESP_OK;
    }
    
    ESP_LOGE(TAG, "Failed to start HTTP server");
    return ESP_FAIL;
}



} // extern "C"