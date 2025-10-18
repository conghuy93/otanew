/*
    Otto机器人控制器 - MCP协议版本
*/

#include <cJSON.h>
#include <esp_log.h>

#include <cstring>

#include "application.h"
#include "board.h"
#include "config.h"
#include "mcp_server.h"
#include "otto_movements.h"
#include "sdkconfig.h"
#include "settings.h"

#define TAG "OttoController"

class OttoController {
private:
    Otto otto_;
    TaskHandle_t action_task_handle_ = nullptr;
    QueueHandle_t action_queue_;
    bool is_action_in_progress_ = false;

    struct OttoActionParams {
        int action_type;
        int steps;
        int speed;
        int direction;
        int amount;
    };

    enum ActionType {
        // Dog-style movement actions (new)
        ACTION_DOG_WALK = 1,
        ACTION_DOG_WALK_BACK = 2,
        ACTION_DOG_TURN_LEFT = 3,
        ACTION_DOG_TURN_RIGHT = 4,
        ACTION_DOG_SIT_DOWN = 5,
        ACTION_DOG_LIE_DOWN = 6,
        ACTION_DOG_JUMP = 7,
        ACTION_DOG_BOW = 8,
        ACTION_DOG_DANCE = 9,
        ACTION_DOG_WAVE_RIGHT_FOOT = 10,
        ACTION_DOG_DANCE_4_FEET = 11,
        ACTION_DOG_SWING = 12,
        ACTION_DOG_STRETCH = 13,
        
        // Legacy actions (adapted for 4 servos)
        ACTION_WALK = 14,
        ACTION_TURN = 15,
        ACTION_JUMP = 16,
        ACTION_BEND = 17,
        ACTION_HOME = 18
    };

    static void ActionTask(void* arg) {
        OttoController* controller = static_cast<OttoController*>(arg);
        OttoActionParams params;
        controller->otto_.AttachServos();

        while (true) {
            if (xQueueReceive(controller->action_queue_, &params, pdMS_TO_TICKS(1000)) == pdTRUE) {
                ESP_LOGI(TAG, "执行动作: %d", params.action_type);
                controller->is_action_in_progress_ = true;

                switch (params.action_type) {
                    // Dog-style movement actions
                    case ACTION_DOG_WALK:
                        controller->otto_.DogWalk(params.steps, params.speed);
                        break;
                    case ACTION_DOG_WALK_BACK:
                        controller->otto_.DogWalkBack(params.steps, params.speed);
                        break;
                    case ACTION_DOG_TURN_LEFT:
                        controller->otto_.DogTurnLeft(params.steps, params.speed);
                        break;
                    case ACTION_DOG_TURN_RIGHT:
                        controller->otto_.DogTurnRight(params.steps, params.speed);
                        break;
                    case ACTION_DOG_SIT_DOWN:
                        controller->otto_.DogSitDown(params.speed);
                        break;
                    case ACTION_DOG_LIE_DOWN:
                        controller->otto_.DogLieDown(params.speed);
                        break;
                    case ACTION_DOG_JUMP:
                        controller->otto_.DogJump(params.speed);
                        break;
                    case ACTION_DOG_BOW:
                        controller->otto_.DogBow(params.speed);
                        break;
                    case ACTION_DOG_DANCE:
                        controller->otto_.DogDance(params.steps, params.speed);
                        break;
                    case ACTION_DOG_WAVE_RIGHT_FOOT:
                        controller->otto_.DogWaveRightFoot(params.steps, params.speed);
                        break;
                    case ACTION_DOG_DANCE_4_FEET:
                        controller->otto_.DogDance4Feet(params.steps, params.speed);
                        break;
                    case ACTION_DOG_SWING:
                        controller->otto_.DogSwing(params.steps, params.speed);
                        break;
                    case ACTION_DOG_STRETCH:
                        controller->otto_.DogStretch(params.steps, params.speed);
                        break;
                        
                    // Legacy actions (adapted for 4 servos)
                    case ACTION_WALK:
                        controller->otto_.Walk(params.steps, params.speed, params.direction);
                        break;
                    case ACTION_TURN:
                        controller->otto_.Turn(params.steps, params.speed, params.direction);
                        break;
                    case ACTION_JUMP:
                        controller->otto_.Jump(params.steps, params.speed);
                        break;
                    case ACTION_BEND:
                        controller->otto_.Bend(params.steps, params.speed, params.direction);
                        break;
                    case ACTION_HOME:
                        controller->otto_.Home();
                        break;
                }
                if (params.action_type != ACTION_HOME) {
                    controller->otto_.Home();  // Always return to home position after action
                }
                controller->is_action_in_progress_ = false;
                vTaskDelay(pdMS_TO_TICKS(20));
            }
        }
    }

    void StartActionTaskIfNeeded() {
        if (action_task_handle_ == nullptr) {
            xTaskCreate(ActionTask, "otto_action", 1024 * 3, this, configMAX_PRIORITIES - 1,
                        &action_task_handle_);
        }
    }

    void QueueAction(int action_type, int steps, int speed, int direction, int amount) {
        ESP_LOGI(TAG, "Dog Action Control: type=%d, steps=%d, speed=%d, direction=%d, amount=%d", 
                 action_type, steps, speed, direction, amount);

        OttoActionParams params = {action_type, steps, speed, direction, amount};
        xQueueSend(action_queue_, &params, portMAX_DELAY);
        StartActionTaskIfNeeded();
    }

    void LoadTrimsFromNVS() {
        Settings settings("otto_trims", false);

        int left_front = settings.GetInt("left_front", 0);
        int right_front = settings.GetInt("right_front", 0);
        int left_back = settings.GetInt("left_back", 0);
        int right_back = settings.GetInt("right_back", 0);

        ESP_LOGI(TAG, "从NVS加载微调设置: 左前=%d, 右前=%d, 左后=%d, 右后=%d",
                 left_front, right_front, left_back, right_back);

        otto_.SetTrims(left_front, right_front, left_back, right_back);
    }

public:
    OttoController() {
        // Debug servo pins before initialization
        ESP_LOGI(TAG, "Servo pins configuration:");
        ESP_LOGI(TAG, "  LEFT_LEG_PIN (Left Front): GPIO %d", LEFT_LEG_PIN);
        ESP_LOGI(TAG, "  RIGHT_LEG_PIN (Right Front): GPIO %d", RIGHT_LEG_PIN);
        ESP_LOGI(TAG, "  LEFT_FOOT_PIN (Left Back): GPIO %d", LEFT_FOOT_PIN);
        ESP_LOGI(TAG, "  RIGHT_FOOT_PIN (Right Back): GPIO %d", RIGHT_FOOT_PIN);
        
        // Initialize Otto with 4 servo pins for dog-style movement
        otto_.Init(LEFT_LEG_PIN, RIGHT_LEG_PIN, LEFT_FOOT_PIN, RIGHT_FOOT_PIN);

        ESP_LOGI(TAG, "Otto Dog Robot initialized with 4 servos");

        LoadTrimsFromNVS();

        action_queue_ = xQueueCreate(10, sizeof(OttoActionParams));

        QueueAction(ACTION_HOME, 1, 1000, 0, 0);  // Initialize to home position

        RegisterMcpTools();
    }

    void RegisterMcpTools() {
        auto& mcp_server = McpServer::GetInstance();

        ESP_LOGI(TAG, "开始注册Dog Robot MCP工具...");

        // Dog-style movement actions
        mcp_server.AddTool("self.dog.walk_forward",
                           "狗式前进。steps: 前进步数(1-10); speed: 速度延迟(50-500ms，数值越小越快)",
                           PropertyList({Property("steps", kPropertyTypeInteger, 2, 1, 10),
                                         Property("speed", kPropertyTypeInteger, 150, 50, 500)}),
                           [this](const PropertyList& properties) -> ReturnValue {
                               int steps = properties["steps"].value<int>();
                               int speed = properties["speed"].value<int>();
                               QueueAction(ACTION_DOG_WALK, steps, speed, 0, 0);
                               return true;
                           });

        mcp_server.AddTool("self.dog.walk_backward",
                           "狗式后退。steps: 后退步数(1-10); speed: 速度延迟(50-500ms，数值越小越快)",
                           PropertyList({Property("steps", kPropertyTypeInteger, 2, 1, 10),
                                         Property("speed", kPropertyTypeInteger, 150, 50, 500)}),
                           [this](const PropertyList& properties) -> ReturnValue {
                               int steps = properties["steps"].value<int>();
                               int speed = properties["speed"].value<int>();
                               QueueAction(ACTION_DOG_WALK_BACK, steps, speed, 0, 0);
                               return true;
                           });

        mcp_server.AddTool("self.dog.turn_left",
                           "狗式左转。steps: 转动次数(1-10); speed: 速度延迟(50-500ms，数值越小越快)",
                           PropertyList({Property("steps", kPropertyTypeInteger, 3, 1, 10),
                                         Property("speed", kPropertyTypeInteger, 150, 50, 500)}),
                           [this](const PropertyList& properties) -> ReturnValue {
                               int steps = properties["steps"].value<int>();
                               int speed = properties["speed"].value<int>();
                               QueueAction(ACTION_DOG_TURN_LEFT, steps, speed, 0, 0);
                               return true;
                           });

        mcp_server.AddTool("self.dog.turn_right",
                           "狗式右转。steps: 转动次数(1-10); speed: 速度延迟(50-500ms，数值越小越快)",
                           PropertyList({Property("steps", kPropertyTypeInteger, 3, 1, 10),
                                         Property("speed", kPropertyTypeInteger, 150, 50, 500)}),
                           [this](const PropertyList& properties) -> ReturnValue {
                               int steps = properties["steps"].value<int>();
                               int speed = properties["speed"].value<int>();
                               QueueAction(ACTION_DOG_TURN_RIGHT, steps, speed, 0, 0);
                               return true;
                           });

        mcp_server.AddTool("self.dog.sit_down",
                           "狗式坐下。delay: 动作延迟时间(100-2000ms)",
                           PropertyList({Property("delay", kPropertyTypeInteger, 500, 100, 2000)}),
                           [this](const PropertyList& properties) -> ReturnValue {
                               int delay = properties["delay"].value<int>();
                               QueueAction(ACTION_DOG_SIT_DOWN, 1, delay, 0, 0);
                               return true;
                           });

        mcp_server.AddTool("self.dog.lie_down",
                           "狗式躺下。delay: 动作延迟时间(500-3000ms)",
                           PropertyList({Property("delay", kPropertyTypeInteger, 1000, 500, 3000)}),
                           [this](const PropertyList& properties) -> ReturnValue {
                               int delay = properties["delay"].value<int>();
                               QueueAction(ACTION_DOG_LIE_DOWN, 1, delay, 0, 0);
                               return true;
                           });

        mcp_server.AddTool("self.dog.jump",
                           "狗式跳跃。delay: 动作延迟时间(100-1000ms)",
                           PropertyList({Property("delay", kPropertyTypeInteger, 200, 100, 1000)}),
                           [this](const PropertyList& properties) -> ReturnValue {
                               int delay = properties["delay"].value<int>();
                               QueueAction(ACTION_DOG_JUMP, 1, delay, 0, 0);
                               return true;
                           });

        mcp_server.AddTool("self.dog.bow",
                           "狗式鞠躬。delay: 保持鞠躬时间(1000-5000ms)",
                           PropertyList({Property("delay", kPropertyTypeInteger, 2000, 1000, 5000)}),
                           [this](const PropertyList& properties) -> ReturnValue {
                               int delay = properties["delay"].value<int>();
                               QueueAction(ACTION_DOG_BOW, 1, delay, 0, 0);
                               return true;
                           });

        mcp_server.AddTool("self.dog.dance",
                           "狗式跳舞。cycles: 跳舞循环次数(1-10); speed: 速度延迟(100-500ms)",
                           PropertyList({Property("cycles", kPropertyTypeInteger, 3, 1, 10),
                                         Property("speed", kPropertyTypeInteger, 200, 100, 500)}),
                           [this](const PropertyList& properties) -> ReturnValue {
                               int cycles = properties["cycles"].value<int>();
                               int speed = properties["speed"].value<int>();
                               QueueAction(ACTION_DOG_DANCE, cycles, speed, 0, 0);
                               return true;
                           });

        mcp_server.AddTool("self.dog.wave_right_foot",
                           "狗式右前脚挥手。waves: 挥手次数(1-10); speed: 速度延迟(20-200ms)",
                           PropertyList({Property("waves", kPropertyTypeInteger, 5, 1, 10),
                                         Property("speed", kPropertyTypeInteger, 50, 20, 200)}),
                           [this](const PropertyList& properties) -> ReturnValue {
                               int waves = properties["waves"].value<int>();
                               int speed = properties["speed"].value<int>();
                               QueueAction(ACTION_DOG_WAVE_RIGHT_FOOT, waves, speed, 0, 0);
                               return true;
                           });

        mcp_server.AddTool("self.dog.dance_4_feet",
                           "狗式四脚同步舞蹈。cycles: 舞蹈循环次数(1-10); speed: 速度延迟(200-800ms)",
                           PropertyList({Property("cycles", kPropertyTypeInteger, 6, 1, 10),
                                         Property("speed", kPropertyTypeInteger, 300, 200, 800)}),
                           [this](const PropertyList& properties) -> ReturnValue {
                               int cycles = properties["cycles"].value<int>();
                               int speed = properties["speed"].value<int>();
                               QueueAction(ACTION_DOG_DANCE_4_FEET, cycles, speed, 0, 0);
                               return true;
                           });

        mcp_server.AddTool("self.dog.swing",
                           "狗式左右摇摆。cycles: 摇摆循环次数(1-20); speed: 速度延迟(5-50ms)",
                           PropertyList({Property("cycles", kPropertyTypeInteger, 8, 1, 20),
                                         Property("speed", kPropertyTypeInteger, 6, 5, 50)}),
                           [this](const PropertyList& properties) -> ReturnValue {
                               int cycles = properties["cycles"].value<int>();
                               int speed = properties["speed"].value<int>();
                               QueueAction(ACTION_DOG_SWING, cycles, speed, 0, 0);
                               return true;
                           });

        mcp_server.AddTool("self.dog.stretch",
                           "狗式伸展运动。cycles: 伸展循环次数(1-5); speed: 速度延迟(10-50ms)",
                           PropertyList({Property("cycles", kPropertyTypeInteger, 2, 1, 5),
                                         Property("speed", kPropertyTypeInteger, 15, 10, 50)}),
                           [this](const PropertyList& properties) -> ReturnValue {
                               int cycles = properties["cycles"].value<int>();
                               int speed = properties["speed"].value<int>();
                               QueueAction(ACTION_DOG_STRETCH, cycles, speed, 0, 0);
                               return true;
                           });

        // Legacy movement functions (for compatibility)
        mcp_server.AddTool("self.otto.walk",
                           "经典步行模式。steps: 步数(1-20); period: 周期(500-2000ms); direction: 方向(1=前进,-1=后退)",
                           PropertyList({Property("steps", kPropertyTypeInteger, 4, 1, 20),
                                         Property("period", kPropertyTypeInteger, 1000, 500, 2000),
                                         Property("direction", kPropertyTypeInteger, 1, -1, 1)}),
                           [this](const PropertyList& properties) -> ReturnValue {
                               int steps = properties["steps"].value<int>();
                               int period = properties["period"].value<int>();
                               int direction = properties["direction"].value<int>();
                               QueueAction(ACTION_WALK, steps, period, direction, 0);
                               return true;
                           });

        mcp_server.AddTool("self.otto.turn",
                           "经典转向模式。steps: 步数(1-20); period: 周期(1000-3000ms); direction: 方向(1=左转,-1=右转)",
                           PropertyList({Property("steps", kPropertyTypeInteger, 4, 1, 20),
                                         Property("period", kPropertyTypeInteger, 2000, 1000, 3000),
                                         Property("direction", kPropertyTypeInteger, 1, -1, 1)}),
                           [this](const PropertyList& properties) -> ReturnValue {
                               int steps = properties["steps"].value<int>();
                               int period = properties["period"].value<int>();
                               int direction = properties["direction"].value<int>();
                               QueueAction(ACTION_TURN, steps, period, direction, 0);
                               return true;
                           });

        mcp_server.AddTool("self.otto.jump",
                           "经典跳跃模式。steps: 跳跃次数(1-10); period: 周期(1000-3000ms)",
                           PropertyList({Property("steps", kPropertyTypeInteger, 1, 1, 10),
                                         Property("period", kPropertyTypeInteger, 2000, 1000, 3000)}),
                           [this](const PropertyList& properties) -> ReturnValue {
                               int steps = properties["steps"].value<int>();
                               int period = properties["period"].value<int>();
                               QueueAction(ACTION_JUMP, steps, period, 0, 0);
                               return true;
                           });

        // System tools
        mcp_server.AddTool("self.dog.stop", "立即停止所有动作", PropertyList(),
                           [this](const PropertyList& properties) -> ReturnValue {
                               if (action_task_handle_ != nullptr) {
                                   vTaskDelete(action_task_handle_);
                                   action_task_handle_ = nullptr;
                               }
                               is_action_in_progress_ = false;
                               xQueueReset(action_queue_);

                               QueueAction(ACTION_HOME, 1, 500, 0, 0);
                               return true;
                           });

        mcp_server.AddTool("self.dog.home", "回到标准站立姿势", PropertyList(),
                           [this](const PropertyList& properties) -> ReturnValue {
                               QueueAction(ACTION_HOME, 1, 500, 0, 0);
                               return true;
                           });

        // Debug tool for testing individual servos
        mcp_server.AddTool("self.dog.test_servo",
                           "测试单个舵机。servo_id: 舵机编号(0-3); angle: 角度(0-180)",
                           PropertyList({Property("servo_id", kPropertyTypeInteger, 0, 0, 3),
                                         Property("angle", kPropertyTypeInteger, 90, 0, 180)}),
                           [this](const PropertyList& properties) -> ReturnValue {
                               int servo_id = properties["servo_id"].value<int>();
                               int angle = properties["angle"].value<int>();
                               ESP_LOGI(TAG, "Testing servo %d at angle %d", servo_id, angle);
                               otto_.ServoAngleSet(servo_id, angle, 500);
                               return true;
                           });

        ESP_LOGI(TAG, "Dog Robot MCP工具注册完成");
    }

    // Public method for web server to queue actions
    void ExecuteAction(int action_type, int steps, int speed, int direction, int amount) {
        QueueAction(action_type, steps, speed, direction, amount);
    }

    ~OttoController() {
        if (action_task_handle_ != nullptr) {
            vTaskDelete(action_task_handle_);
            action_task_handle_ = nullptr;
        }
        vQueueDelete(action_queue_);
    }
};

static OttoController* g_otto_controller = nullptr;

void InitializeOttoController() {
    if (g_otto_controller == nullptr) {
        g_otto_controller = new OttoController();
        ESP_LOGI(TAG, "Otto控制器已初始化并注册MCP工具");
    }
}

// C interface for webserver to access controller
extern "C" {
    esp_err_t otto_controller_queue_action(int action_type, int steps, int speed, int direction, int amount) {
        if (g_otto_controller == nullptr) {
            ESP_LOGE(TAG, "Otto controller not initialized");
            return ESP_ERR_INVALID_STATE;
        }
        
        g_otto_controller->ExecuteAction(action_type, steps, speed, direction, amount);
        return ESP_OK;
    }
}
