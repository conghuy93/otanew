/*
    Otto机器人控制器 - MCP协议版本
*/

#include <cJSON.h>
#include <esp_log.h>

#include <cstring>

#include "application.h"
#include "board.h"
#include "display.h"
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
    // Idle management
    int idle_no_action_ticks_ = 0;    // seconds without actions (ActionTask polls every 1s)
    bool idle_mode_ = false;          // true when idle behavior is active
    uint32_t idle_emoji_tick_ = 0;    // seconds since last emoji change in idle

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
        ACTION_DOG_SCRATCH = 14,  // New: Sit + BR leg scratch (gãi ngứa)
        
        // Legacy actions (adapted for 4 servos)
        ACTION_WALK = 15,
        ACTION_TURN = 16,
        ACTION_JUMP = 17,
        ACTION_BEND = 18,
        ACTION_HOME = 19,
        // Utility actions
        ACTION_DELAY = 20  // Delay in milliseconds, use 'speed' as delay duration
        , ACTION_DOG_JUMP_HAPPY = 21  // Special: Jump with happy emoji (for touch sensor)
    };

    static void ActionTask(void* arg) {
        OttoController* controller = static_cast<OttoController*>(arg);
        OttoActionParams params;
        
        ESP_LOGI(TAG, "🚀 ActionTask started! Attaching servos...");
        controller->otto_.AttachServos();
        ESP_LOGI(TAG, "✅ Servos attached successfully");

        while (true) {
            if (xQueueReceive(controller->action_queue_, &params, pdMS_TO_TICKS(1000)) == pdTRUE) {
                ESP_LOGI(TAG, "⚡ Executing action: type=%d, steps=%d, speed=%d", 
                         params.action_type, params.steps, params.speed);
                controller->is_action_in_progress_ = true;
                controller->idle_no_action_ticks_ = 0; // reset idle timer on new action
                controller->idle_mode_ = false;        // exit idle mode on any action
                controller->idle_emoji_tick_ = 0;

                switch (params.action_type) {
                    // Dog-style movement actions
                    case ACTION_DOG_WALK:
                        controller->otto_.DogWalk(params.steps, params.speed);
                        break;
                    case ACTION_DOG_WALK_BACK:
                        ESP_LOGI(TAG, "🐕 DogWalkBack: steps=%d, speed=%d", params.steps, params.speed);
                        controller->otto_.DogWalkBack(params.steps, params.speed);
                        break;
                    case ACTION_DOG_TURN_LEFT:
                        ESP_LOGI(TAG, "🐕 DogTurnLeft: steps=%d, speed=%d", params.steps, params.speed);
                        controller->otto_.DogTurnLeft(params.steps, params.speed);
                        break;
                    case ACTION_DOG_TURN_RIGHT:
                        ESP_LOGI(TAG, "🐕 DogTurnRight: steps=%d, speed=%d", params.steps, params.speed);
                        controller->otto_.DogTurnRight(params.steps, params.speed);
                        break;
                    case ACTION_DOG_SIT_DOWN:
                        ESP_LOGI(TAG, "🐕 DogSitDown: speed=%d", params.speed);
                        controller->otto_.DogSitDown(params.speed);
                        break;
                    case ACTION_DOG_LIE_DOWN:
                        ESP_LOGI(TAG, "🐕 DogLieDown: speed=%d", params.speed);
                        controller->otto_.DogLieDown(params.speed);
                        break;
                    case ACTION_DOG_JUMP:
                        {
                            // Show angry emoji on jump and keep until complete
                            auto display = Board::GetInstance().GetDisplay();
                            if (display) display->SetEmotion("angry");
                            controller->otto_.DogJump(params.speed);
                            // Reset to neutral after jump completes
                            if (display) display->SetEmotion("neutral");
                        }
                        break;
                    case ACTION_DOG_JUMP_HAPPY:
                        {
                            // Touch-triggered happy jump
                            auto display = Board::GetInstance().GetDisplay();
                            if (display) display->SetEmotion("happy");
                            controller->otto_.DogJump(params.speed);
                            // Reset to neutral after jump completes
                            if (display) display->SetEmotion("neutral");
                        }
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
                        {
                            // Always show sleepy emoji during stretch and keep until complete
                            auto display = Board::GetInstance().GetDisplay();
                            if (display) display->SetEmotion("sleepy");
                            controller->otto_.DogStretch(params.steps, params.speed);
                            // Reset to neutral after stretch completes
                            if (display) display->SetEmotion("neutral");
                        }
                        break;
                    case ACTION_DOG_SCRATCH:
                        ESP_LOGI(TAG, "🐕 DogScratch: scratches=%d, speed=%d", params.steps, params.speed);
                        controller->otto_.DogScratch(params.steps, params.speed);
                        break;
                        
                    // Legacy actions (adapted for 4 servos)
                    case ACTION_WALK:
                        controller->otto_.Walk(params.steps, params.speed, params.direction);
                        break;
                    case ACTION_TURN:
                        controller->otto_.Turn(params.steps, params.speed, params.direction);
                        break;
                    case ACTION_JUMP:
                        {
                            auto display = Board::GetInstance().GetDisplay();
                            if (display) display->SetEmotion("angry");
                            controller->otto_.Jump(params.steps, params.speed);
                            // Reset to neutral after jump completes
                            if (display) display->SetEmotion("neutral");
                        }
                        break;
                    case ACTION_BEND:
                        controller->otto_.Bend(params.steps, params.speed, params.direction);
                        break;
                    case ACTION_HOME:
                        ESP_LOGI(TAG, "🏠 Going Home");
                        controller->otto_.Home();
                        break;
                    case ACTION_DELAY:
                        ESP_LOGI(TAG, "⏱️ Delay: %d ms", params.speed);
                        vTaskDelay(pdMS_TO_TICKS(params.speed));
                        break;
                    default:
                        ESP_LOGW(TAG, "⚠️ Unknown action type: %d", params.action_type);
                        break;
                }
                
                // Note: Removed auto-return-to-home logic to allow action sequences
                // If you need to return home, queue ACTION_HOME explicitly
                
                controller->is_action_in_progress_ = false;
                ESP_LOGI(TAG, "✅ Action completed");
                vTaskDelay(pdMS_TO_TICKS(20));
            } else {
                // No action received within 1 second -> idle tick
                controller->idle_no_action_ticks_++;

                // If in idle mode, periodically randomize emoji from a set
                if (controller->idle_mode_) {
                    controller->idle_emoji_tick_++;
                    if (controller->idle_emoji_tick_ >= 10) { // change every ~10s
                        controller->idle_emoji_tick_ = 0;
                        const char* kIdleEmojis[] = {"happy", "winking", "cool", "sleepy", "surprised"};
                        uint32_t r = esp_random();
                        const char* chosen = kIdleEmojis[r % (sizeof(kIdleEmojis)/sizeof(kIdleEmojis[0]))];
                        auto display = Board::GetInstance().GetDisplay();
                        if (display) display->SetEmotion(chosen);
                        ESP_LOGI(TAG, "🛌 Idle mode emoji -> %s", chosen);
                    }
                }

                // Enter idle mode after 120s without actions
                if (!controller->idle_mode_ && controller->idle_no_action_ticks_ >= 120) {
                    ESP_LOGI(TAG, "🛌 Idle timeout reached (120s). Lying down and enabling idle emojis.");
                    controller->idle_mode_ = true;
                    controller->idle_emoji_tick_ = 0;

                    // Move to lie down posture at a gentle pace
                    controller->otto_.DogLieDown(1500);

                    // Set an initial random emoji from the set
                    const char* kIdleEmojis[] = {"happy", "winking", "cool", "sleepy", "surprised"};
                    uint32_t r = esp_random();
                    const char* chosen = kIdleEmojis[r % (sizeof(kIdleEmojis)/sizeof(kIdleEmojis[0]))];
                    auto display = Board::GetInstance().GetDisplay();
                    if (display) display->SetEmotion(chosen);
                }
            }
        }
    }

    void StartActionTaskIfNeeded() {
        if (action_task_handle_ == nullptr) {
            ESP_LOGI(TAG, "🚀 Creating ActionTask...");
            BaseType_t result = xTaskCreate(ActionTask, "otto_action", 1024 * 3, this, 
                                           configMAX_PRIORITIES - 1, &action_task_handle_);
            if (result == pdPASS) {
                ESP_LOGI(TAG, "✅ ActionTask created successfully with handle: %p", action_task_handle_);
            } else {
                ESP_LOGE(TAG, "❌ Failed to create ActionTask!");
                action_task_handle_ = nullptr;
            }
        } else {
            ESP_LOGD(TAG, "ActionTask already running");
        }
    }

    void QueueAction(int action_type, int steps, int speed, int direction, int amount) {
        ESP_LOGI(TAG, "🎯 QueueAction called: type=%d, steps=%d, speed=%d, direction=%d, amount=%d", 
                 action_type, steps, speed, direction, amount);

        if (action_queue_ == nullptr) {
            ESP_LOGE(TAG, "❌ Action queue is NULL! Cannot queue action.");
            return;
        }

        OttoActionParams params = {action_type, steps, speed, direction, amount};
        
        BaseType_t result = xQueueSend(action_queue_, &params, portMAX_DELAY);
        if (result == pdTRUE) {
            ESP_LOGI(TAG, "✅ Action queued successfully. Queue space remaining: %d", 
                     uxQueueSpacesAvailable(action_queue_));
            StartActionTaskIfNeeded();
        } else {
            ESP_LOGE(TAG, "❌ Failed to queue action! Queue full or error.");
        }
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
        ESP_LOGI(TAG, "🤖 Initializing OttoController...");
        ESP_LOGI(TAG, "Servo pins configuration:");
        ESP_LOGI(TAG, "  LEFT_LEG_PIN (Left Front): GPIO %d", LEFT_LEG_PIN);
        ESP_LOGI(TAG, "  RIGHT_LEG_PIN (Right Front): GPIO %d", RIGHT_LEG_PIN);
        ESP_LOGI(TAG, "  LEFT_FOOT_PIN (Left Back): GPIO %d", LEFT_FOOT_PIN);
        ESP_LOGI(TAG, "  RIGHT_FOOT_PIN (Right Back): GPIO %d", RIGHT_FOOT_PIN);
        
        // Initialize Otto with 4 servo pins for dog-style movement
        otto_.Init(LEFT_LEG_PIN, RIGHT_LEG_PIN, LEFT_FOOT_PIN, RIGHT_FOOT_PIN);

        ESP_LOGI(TAG, "✅ Kiki Dog Robot initialized with 4 servos");

        LoadTrimsFromNVS();

        ESP_LOGI(TAG, "📦 Creating action queue (size=10)...");
        action_queue_ = xQueueCreate(10, sizeof(OttoActionParams));
        
        if (action_queue_ == nullptr) {
            ESP_LOGE(TAG, "❌ FATAL: Failed to create action queue!");
        } else {
            ESP_LOGI(TAG, "✅ Action queue created successfully");
        }

        ESP_LOGI(TAG, "🏠 Queuing initial HOME action...");
        QueueAction(ACTION_HOME, 1, 1000, 0, 0);  // Initialize to home position

        RegisterMcpTools();
        ESP_LOGI(TAG, "🎉 KikiController initialization complete!");
    }

    void RegisterMcpTools() {
        auto& mcp_server = McpServer::GetInstance();

        ESP_LOGI(TAG, "🐕 Registering Kiki the Adorable Dog Robot MCP Tools...");

        // IMPORTANT: I am Kiki, a cute 4-legged dog robot! 🐶
        // I can walk, run, sit, lie down, jump, dance, wave, and do tricks like a real puppy!
        // Use these tools to control my movements and make me perform adorable actions.

        // Dog-style movement actions
        mcp_server.AddTool("self.dog.walk_forward",
                           "🐕 I walk forward like a cute puppy! Make me walk forward with my 4 legs.\n"
                           "Args:\n"
                           "  steps (1-10): How many steps I should walk forward\n"
                           "  speed (50-500ms): Movement speed - lower is faster, higher is slower\n"
                           "Example: 'Otto, walk forward 3 steps' or 'Move forward'",
                           PropertyList({Property("steps", kPropertyTypeInteger, 2, 1, 10),
                                         Property("speed", kPropertyTypeInteger, 150, 50, 500)}),
                           [this](const PropertyList& properties) -> ReturnValue {
                               int steps = properties["steps"].value<int>();
                               int speed = properties["speed"].value<int>();
                               ESP_LOGI(TAG, "🐾 Kiki is walking forward %d steps!", steps);
                               // FAST RESPONSE: Execute immediately like esp-hi, no queue delay
                               otto_.DogWalk(steps, speed);
                               return true;
                           });

        mcp_server.AddTool("self.dog.walk_backward",
                           "🐕 I walk backward like a cautious puppy! Make me step back carefully.\n"
                           "Args:\n"
                           "  steps (1-10): How many steps I should walk backward\n"
                           "  speed (50-500ms): Movement speed - lower is faster\n"
                           "Example: 'Otto, step back' or 'Walk backward 2 steps'",
                           PropertyList({Property("steps", kPropertyTypeInteger, 2, 1, 10),
                                         Property("speed", kPropertyTypeInteger, 150, 50, 500)}),
                           [this](const PropertyList& properties) -> ReturnValue {
                               int steps = properties["steps"].value<int>();
                               int speed = properties["speed"].value<int>();
                               ESP_LOGI(TAG, "🐾 Kiki is carefully walking backward %d steps!", steps);
                               // FAST RESPONSE: Execute immediately like esp-hi
                               otto_.DogWalkBack(steps, speed);
                               return true;
                           });

        mcp_server.AddTool("self.dog.turn_left",
                           "🐕 I turn left like a playful puppy! Make me spin to the left.\n"
                           "Args:\n"
                           "  steps (1-10): How many turning movements\n"
                           "  speed (50-500ms): Turn speed\n"
                           "Example: 'Otto, turn left' or 'Spin to the left'",
                           PropertyList({Property("steps", kPropertyTypeInteger, 3, 1, 10),
                                         Property("speed", kPropertyTypeInteger, 150, 50, 500)}),
                           [this](const PropertyList& properties) -> ReturnValue {
                               int steps = properties["steps"].value<int>();
                               int speed = properties["speed"].value<int>();
                               ESP_LOGI(TAG, "🐾 Kiki is turning left!");
                               // FAST RESPONSE: Execute immediately like esp-hi
                               otto_.DogTurnLeft(steps, speed);
                               return true;
                           });

        mcp_server.AddTool("self.dog.turn_right",
                           "🐕 I turn right like a curious puppy! Make me spin to the right.\n"
                           "Args:\n"
                           "  steps (1-10): How many turning movements\n"
                           "  speed (50-500ms): Turn speed\n"
                           "Example: 'Otto, turn right' or 'Look to the right'",
                           PropertyList({Property("steps", kPropertyTypeInteger, 3, 1, 10),
                                         Property("speed", kPropertyTypeInteger, 150, 50, 500)}),
                           [this](const PropertyList& properties) -> ReturnValue {
                               int steps = properties["steps"].value<int>();
                               int speed = properties["speed"].value<int>();
                               ESP_LOGI(TAG, "🐾 Kiki is turning right!");
                               // FAST RESPONSE: Execute immediately like esp-hi
                               otto_.DogTurnRight(steps, speed);
                               return true;
                           });

        mcp_server.AddTool("self.dog.sit_down",
                           "🐕 I sit down like an obedient puppy! Make me sit nicely.\n"
                           "Args:\n"
                           "  delay (100-2000ms): How long the sitting motion takes\n"
                           "Example: 'Otto, sit!' or 'Sit down like a good boy'",
                           PropertyList({Property("delay", kPropertyTypeInteger, 500, 100, 2000)}),
                           [this](const PropertyList& properties) -> ReturnValue {
                               int delay = properties["delay"].value<int>();
                               ESP_LOGI(TAG, "🐾 Kiki is sitting down like a good puppy!");
                               // FAST RESPONSE: Execute immediately like esp-hi
                               otto_.DogSitDown(delay);
                               return true;
                           });

        mcp_server.AddTool("self.dog.lie_down",
                           "🐕 I lie down like a tired puppy ready for a nap! Make me lie down and rest.\n"
                           "Args:\n"
                           "  delay (500-3000ms): How long the lying motion takes\n"
                           "Example: 'Otto, lie down' or 'Take a rest' or 'Nap time!'",
                           PropertyList({Property("delay", kPropertyTypeInteger, 1000, 500, 3000)}),
                           [this](const PropertyList& properties) -> ReturnValue {
                               int delay = properties["delay"].value<int>();
                               ESP_LOGI(TAG, "🐾 Kiki is lying down for a nap!");
                               // FAST RESPONSE: Execute immediately like esp-hi
                               otto_.DogLieDown(delay);
                               return true;
                           });

        mcp_server.AddTool("self.dog.jump",
                           "🐕 I jump and dance with excitement like a happy puppy! Make me dance and jump for joy!\n"
                           "Args:\n"
                           "  delay (100-1000ms): Jump and dance speed\n"
                           "Example: 'Otto, dance and jump!' or 'Jump up!' or 'Show me your moves!'",
                           PropertyList({Property("delay", kPropertyTypeInteger, 200, 100, 1000)}),
                           [this](const PropertyList& properties) -> ReturnValue {
                               int delay = properties["delay"].value<int>();
                               ESP_LOGI(TAG, "🐾 Kiki is dancing and jumping! 💃🦘");
                               // FAST RESPONSE: Execute immediately like esp-hi
                               otto_.DogJump(delay);
                               return true;
                           });

        mcp_server.AddTool("self.dog.bow",
                           "🐕 I bow like a polite puppy greeting you! Make me bow to show respect.\n"
                           "Args:\n"
                           "  delay (1000-5000ms): How long I hold the bow\n"
                           "Example: 'Otto, bow' or 'Greet me nicely' or 'Say hello with a bow'",
                           PropertyList({Property("delay", kPropertyTypeInteger, 2000, 1000, 5000)}),
                           [this](const PropertyList& properties) -> ReturnValue {
                               int delay = properties["delay"].value<int>();
                               ESP_LOGI(TAG, "🐾 Kiki is bowing politely! 🙇");
                               // FAST RESPONSE: Execute immediately like esp-hi
                               otto_.DogBow(delay);
                               return true;
                           });

        mcp_server.AddTool("self.dog.dance",
                           "🐕 I dance and perform like a joyful puppy celebrating! Make me dance with style and happiness!\n"
                           "Args:\n"
                           "  cycles (1-10): How many dance moves\n"
                           "  speed (100-500ms): Dance speed\n"
                           "Example: 'Otto, dance!' or 'Let's celebrate!' or 'Show me your dance moves!'",
                           PropertyList({Property("cycles", kPropertyTypeInteger, 3, 1, 10),
                                         Property("speed", kPropertyTypeInteger, 200, 100, 500)}),
                           [this](const PropertyList& properties) -> ReturnValue {
                               int cycles = properties["cycles"].value<int>();
                               int speed = properties["speed"].value<int>();
                               ESP_LOGI(TAG, "🐾 Kiki is dancing with style! 💃✨");
                               // Set happy emoji
                               if (auto display = Board::GetInstance().GetDisplay()) {
                                   display->SetEmotion("happy");
                               }
                               // FAST RESPONSE: Execute immediately like esp-hi
                               otto_.DogDance(cycles, speed);
                               return true;
                           });

        mcp_server.AddTool("self.dog.wave_right_foot",
                           "🐕 I wave my right paw like a friendly puppy saying hi! Make me wave hello!\n"
                           "Args:\n"
                           "  waves (1-10): How many times to wave\n"
                           "  speed (20-200ms): Wave speed\n"
                           "Example: 'Otto, wave!' or 'Say hi!' or 'Wave your paw!'",
                           PropertyList({Property("waves", kPropertyTypeInteger, 5, 1, 10),
                                         Property("speed", kPropertyTypeInteger, 50, 20, 200)}),
                           [this](const PropertyList& properties) -> ReturnValue {
                               int waves = properties["waves"].value<int>();
                               int speed = properties["speed"].value<int>();
                               ESP_LOGI(TAG, "🐾 Kiki is waving his paw! 👋");
                               // FAST RESPONSE: Execute immediately like esp-hi
                               otto_.DogWaveRightFoot(waves, speed);
                               return true;
                           });

        mcp_server.AddTool("self.dog.dance_4_feet",
                           "🐕 I dance with all 4 feet like an excited puppy! Make me dance with coordinated paw movements!\n"
                           "Args:\n"
                           "  cycles (1-10): How many dance cycles\n"
                           "  speed (200-800ms): Dance speed delay\n"
                           "Example: 'Otto, dance with all your feet!' or 'Do the 4-feet dance!'",
                           PropertyList({Property("cycles", kPropertyTypeInteger, 6, 1, 10),
                                         Property("speed", kPropertyTypeInteger, 300, 200, 800)}),
                           [this](const PropertyList& properties) -> ReturnValue {
                               int cycles = properties["cycles"].value<int>();
                               int speed = properties["speed"].value<int>();
                               ESP_LOGI(TAG, "🐾 Kiki is dancing with all 4 feet! 🎵");
                               // Set happy emoji
                               if (auto display = Board::GetInstance().GetDisplay()) {
                                   display->SetEmotion("happy");
                               }
                               // FAST RESPONSE: Execute immediately like esp-hi
                               otto_.DogDance4Feet(cycles, speed);
                               return true;
                           });

        mcp_server.AddTool("self.dog.swing",
                           "🐕 I swing left and right like a happy puppy wagging my whole body! Make me sway with joy!\n"
                           "Args:\n"
                           "  cycles (1-20): How many swing cycles\n"
                           "  speed (5-50ms): Swing speed delay\n"
                           "Example: 'Otto, swing left and right!' or 'Wag your body!'",
                           PropertyList({Property("cycles", kPropertyTypeInteger, 8, 1, 20),
                                         Property("speed", kPropertyTypeInteger, 6, 5, 50)}),
                           [this](const PropertyList& properties) -> ReturnValue {
                               int cycles = properties["cycles"].value<int>();
                               int speed = properties["speed"].value<int>();
                               ESP_LOGI(TAG, "🐾 Kiki is swinging left and right! 🎶");
                               // Set happy emoji
                               if (auto display = Board::GetInstance().GetDisplay()) {
                                   display->SetEmotion("happy");
                               }
                               // FAST RESPONSE: Execute immediately like esp-hi
                               otto_.DogSwing(cycles, speed);
                               return true;
                           });

        mcp_server.AddTool("self.dog.stretch",
                           "🐕 I relax like a puppy taking it easy! Make me feel relaxed and comfortable!\n"
                           "Args:\n"
                           "  cycles (1-5): How many relaxation cycles\n"
                           "  speed (10-50ms): Relaxation speed delay\n"
                           "Example: 'Otto, relax!' or 'Take it easy!' or 'Chill out!'",
                           PropertyList({Property("cycles", kPropertyTypeInteger, 2, 1, 5),
                                         Property("speed", kPropertyTypeInteger, 15, 10, 50)}),
                           [this](const PropertyList& properties) -> ReturnValue {
                               int cycles = properties["cycles"].value<int>();
                               int speed = properties["speed"].value<int>();
                               ESP_LOGI(TAG, "🐾 Kiki is relaxing! 😌");
                               // Set sleepy emoji
                               if (auto display = Board::GetInstance().GetDisplay()) {
                                   display->SetEmotion("sleepy");
                               }
                               // FAST RESPONSE: Execute immediately like esp-hi
                               otto_.DogStretch(cycles, speed);
                               return true;
                           });

        // Legacy movement functions (for compatibility - prefer self.dog.* tools for newer features!)
        mcp_server.AddTool("self.otto.walk",
                           "🐕 [Legacy] Classic walk mode for backward compatibility.\n"
                           "Args:\n"
                           "  steps (1-20): Number of steps\n"
                           "  period (500-2000ms): Movement period\n"
                           "  direction (1=forward, -1=backward)\n"
                           "Note: Prefer using self.dog.walk_forward or self.dog.walk_backward instead!",
                           PropertyList({Property("steps", kPropertyTypeInteger, 4, 1, 20),
                                         Property("period", kPropertyTypeInteger, 1000, 500, 2000),
                                         Property("direction", kPropertyTypeInteger, 1, -1, 1)}),
                           [this](const PropertyList& properties) -> ReturnValue {
                               int steps = properties["steps"].value<int>();
                               int period = properties["period"].value<int>();
                               int direction = properties["direction"].value<int>();
                               QueueAction(ACTION_WALK, steps, period, direction, 0);
                               ESP_LOGI(TAG, "🐾 Otto legacy walk: steps=%d, period=%d, dir=%d", steps, period, direction);
                               return true;
                           });

        mcp_server.AddTool("self.otto.turn",
                           "🐕 [Legacy] Classic turn mode for backward compatibility.\n"
                           "Args:\n"
                           "  steps (1-20): Number of turn steps\n"
                           "  period (1000-3000ms): Movement period\n"
                           "  direction (1=left, -1=right)\n"
                           "Note: Prefer using self.dog.turn_left or self.dog.turn_right instead!",
                           PropertyList({Property("steps", kPropertyTypeInteger, 4, 1, 20),
                                         Property("period", kPropertyTypeInteger, 2000, 1000, 3000),
                                         Property("direction", kPropertyTypeInteger, 1, -1, 1)}),
                           [this](const PropertyList& properties) -> ReturnValue {
                               int steps = properties["steps"].value<int>();
                               int period = properties["period"].value<int>();
                               int direction = properties["direction"].value<int>();
                               QueueAction(ACTION_TURN, steps, period, direction, 0);
                               ESP_LOGI(TAG, "🐾 Otto legacy turn: steps=%d, period=%d, dir=%d", steps, period, direction);
                               return true;
                           });

        mcp_server.AddTool("self.otto.jump",
                           "🐕 [Legacy] Classic jump mode for backward compatibility.\n"
                           "Args:\n"
                           "  steps (1-10): Number of jumps\n"
                           "  period (1000-3000ms): Movement period\n"
                           "Note: Prefer using self.dog.jump instead!",
                           PropertyList({Property("steps", kPropertyTypeInteger, 1, 1, 10),
                                         Property("period", kPropertyTypeInteger, 2000, 1000, 3000)}),
                           [this](const PropertyList& properties) -> ReturnValue {
                               int steps = properties["steps"].value<int>();
                               int period = properties["period"].value<int>();
                               QueueAction(ACTION_JUMP, steps, period, 0, 0);
                               ESP_LOGI(TAG, "🐾 Otto legacy jump: steps=%d, period=%d", steps, period);
                               return true;
                           });

        // System tools
        mcp_server.AddTool("self.dog.stop", 
                           "🐕 I stop all my actions immediately like an obedient puppy! Make me stop whatever I'm doing!\n"
                           "Example: 'Otto, stop!' or 'Freeze!' or 'Stay!'", 
                           PropertyList(),
                           [this](const PropertyList& properties) -> ReturnValue {
                               if (action_task_handle_ != nullptr) {
                                   vTaskDelete(action_task_handle_);
                                   action_task_handle_ = nullptr;
                               }
                               is_action_in_progress_ = false;
                               xQueueReset(action_queue_);

                               ESP_LOGI(TAG, "🐾 Kiki stopped! 🛑");
                               // FAST RESPONSE: Execute immediately like esp-hi
                               otto_.Home();
                               return true;
                           });

        mcp_server.AddTool("self.dog.home", 
                           "🐕 I return to my standard standing position like a well-trained puppy! Make me stand at attention!\n"
                           "Example: 'Otto, go home!' or 'Stand up straight!' or 'Reset position!'", 
                           PropertyList(),
                           [this](const PropertyList& properties) -> ReturnValue {
                               ESP_LOGI(TAG, "🐾 Kiki going home! 🏠");
                               // FAST RESPONSE: Execute immediately like esp-hi
                               otto_.Home();
                               return true;
                           });

        // Combat/Action Sequences
        mcp_server.AddTool("self.dog.defend",
                           "🐕 I defend myself like a protective puppy! I back away, sit down, and lie low to protect myself!\n"
                           "This is a defense sequence: walk back 1 step → sit (3s) → lie down → wait (3s) → stand back up.\n"
                           "Example: 'Otto, defend yourself!' or 'Protect yourself!' or 'Get into defense position!'",
                           PropertyList(),
                           [this](const PropertyList& properties) -> ReturnValue {
                               ESP_LOGI(TAG, "🐾 Kiki is defending! 🛡️ (back → sit → lie → home)");
                               // Set shocked emoji and keep it during entire defense sequence
                               auto display = Board::GetInstance().GetDisplay();
                               if (display) {
                                   display->SetEmotion("shocked");
                               }
                               // FAST RESPONSE: Execute sequence immediately
                               otto_.DogWalkBack(1, 100);
                               otto_.DogSitDown(3000);
                               otto_.DogLieDown(1500);
                               vTaskDelay(pdMS_TO_TICKS(3000));
                               otto_.Home();
                               // Reset to neutral after defense complete
                               if (display) {
                                   display->SetEmotion("neutral");
                               }
                               return ""; // Return empty string - no text display, emoji only
                           });

        mcp_server.AddTool("self.dog.attack",
                           "🐕 I attack like a fierce little puppy! I charge forward, jump, and bow down!\n"
                           "This is an attack sequence: walk forward 2 steps → jump → bow.\n"
                           "Example: 'Otto, attack!' or 'Charge forward!' or 'Go get them!'",
                           PropertyList(),
                           [this](const PropertyList& properties) -> ReturnValue {
                               ESP_LOGI(TAG, "🐾 Kiki is attacking! ⚔️ (forward → jump → bow)");
                               // Set angry emoji and keep it during entire attack sequence
                               auto display = Board::GetInstance().GetDisplay();
                               if (display) {
                                   display->SetEmotion("angry");
                               }
                               // FAST RESPONSE: Execute sequence immediately
                               otto_.DogWalk(2, 100);
                               otto_.DogJump(200);
                               otto_.DogBow(1000);
                               // Reset to neutral after attack complete
                               if (display) {
                                   display->SetEmotion("neutral");
                               }
                               return true;
                           });

        mcp_server.AddTool("self.dog.celebrate",
                           "🐕 I celebrate like a victorious puppy! I dance, wave, and swing with pure joy!\n"
                           "This is a celebration sequence: dance 2x → wave 3x → swing 4x.\n"
                           "Example: 'Otto, celebrate!' or 'You did it!' or 'Victory dance!'",
                           PropertyList(),
                           [this](const PropertyList& properties) -> ReturnValue {
                               ESP_LOGI(TAG, "🐾 Kiki is celebrating! 🎉 (dance → wave → swing)");
                               // Set happy emoji and keep during celebration
                               auto display = Board::GetInstance().GetDisplay();
                               if (display) {
                                   display->SetEmotion("happy");
                               }
                               // FAST RESPONSE: Execute sequence immediately
                               otto_.DogDance(2, 200);
                               otto_.DogWaveRightFoot(3, 50);
                               otto_.DogSwing(4, 10);
                               // Reset to neutral after celebration
                               if (display) {
                                   display->SetEmotion("neutral");
                               }
                               return true;
                           });

        mcp_server.AddTool("self.dog.scratch",
                           "🐕 I scratch like a puppy with an itch! I sit down and move my back right leg to scratch!\n"
                           "This scratches 5 times while sitting.\n"
                           "Example: 'Otto, scratch!' or 'Got an itch?' or 'Scratch yourself!'",
                           PropertyList(),
                           [this](const PropertyList& properties) -> ReturnValue {
                               ESP_LOGI(TAG, "🐾 Kiki is scratching! 🐶");
                               // FAST RESPONSE: Execute immediately like esp-hi
                               otto_.DogScratch(5, 50);
                               return true;
                           });

        mcp_server.AddTool("self.dog.greet",
                           "🐕 I greet people like a friendly puppy! I stand up, wave my paw, and bow politely!\n"
                           "This is a greeting sequence: stand → wave 5x → bow.\n"
                           "Example: 'Otto, say hello!' or 'Greet our guest!' or 'Say hi!'",
                           PropertyList(),
                           [this](const PropertyList& properties) -> ReturnValue {
                               ESP_LOGI(TAG, "🐾 Kiki is greeting! 👋 (stand → wave → bow)");
                               // Set happy emoji and keep during greeting
                               auto display = Board::GetInstance().GetDisplay();
                               if (display) {
                                   display->SetEmotion("happy");
                               }
                               // FAST RESPONSE: Execute sequence immediately
                               otto_.Home();
                               otto_.DogWaveRightFoot(5, 50);
                               otto_.DogBow(2000);
                               // Reset to neutral after greeting
                               if (display) {
                                   display->SetEmotion("neutral");
                               }
                               return true;
                           });

        mcp_server.AddTool("self.dog.retreat",
                           "🐕 I retreat like a cautious puppy escaping danger! I back up fast, turn around, and run away!\n"
                           "This is a retreat sequence: walk back 3 steps → turn right 4x → walk forward 2 steps.\n"
                           "Example: 'Otto, retreat!' or 'Get away!' or 'Run away!'",
                           PropertyList(),
                           [this](const PropertyList& properties) -> ReturnValue {
                               ESP_LOGI(TAG, "🐾 Kiki is retreating! 🏃 (back → turn → run)");
                               // Set scared emoji and keep during retreat
                               auto display = Board::GetInstance().GetDisplay();
                               if (display) {
                                   display->SetEmotion("scared");
                               }
                               // FAST RESPONSE: Execute sequence immediately
                               otto_.DogWalkBack(3, 100);
                               otto_.DogTurnRight(4, 150);
                               otto_.DogWalk(2, 100);
                               // Reset to neutral after retreat
                               if (display) {
                                   display->SetEmotion("neutral");
                               }
                               return true;
                           });

        mcp_server.AddTool("self.dog.search",
                           "🐕 I search around like a curious puppy exploring! I look left, right, and walk forward to investigate!\n"
                           "This is a search sequence: turn left 2x → turn right 4x → turn left 2x → walk forward 2 steps.\n"
                           "Example: 'Otto, search around!' or 'Explore the area!' or 'Look around!'",
                           PropertyList(),
                           [this](const PropertyList& properties) -> ReturnValue {
                               ESP_LOGI(TAG, "🐾 Kiki is searching! 🔍 (look around → walk forward)");
                               // Set curious/surprised emoji and keep during search
                               auto display = Board::GetInstance().GetDisplay();
                               if (display) {
                                   display->SetEmotion("surprised");
                               }
                               // FAST RESPONSE: Execute sequence immediately
                               otto_.DogTurnLeft(2, 150);
                               otto_.DogTurnRight(4, 150);
                               otto_.DogTurnLeft(2, 150);
                               otto_.DogWalk(2, 150);
                               // Reset to neutral after search
                               if (display) {
                                   display->SetEmotion("neutral");
                               }
                               return true;
                           });

        // Debug tool for testing individual servos
        mcp_server.AddTool("self.dog.test_servo",
                           "🐕 I test my servo motors like a robot puppy in maintenance mode! This moves one leg to a specific angle.\n"
                           "Args:\n"
                           "  servo_id (0-3): Which servo to test (0=LF, 1=RF, 2=LB, 3=RB)\n"
                           "  angle (0-180): Target angle in degrees\n"
                           "Example: 'Test servo 0 at 90 degrees' (for debugging only)",
                           PropertyList({Property("servo_id", kPropertyTypeInteger, 0, 0, 3),
                                         Property("angle", kPropertyTypeInteger, 90, 0, 180)}),
                           [this](const PropertyList& properties) -> ReturnValue {
                               int servo_id = properties["servo_id"].value<int>();
                               int angle = properties["angle"].value<int>();
                               ESP_LOGI(TAG, "🐾 Testing servo %d at angle %d", servo_id, angle);
                               otto_.ServoAngleSet(servo_id, angle, 500);
                               return true;
                           });

        ESP_LOGI(TAG, "🐾 Dog Robot MCP tools registered! Kiki is ready to be a cute puppy! 🐶");
    }

    // Public method for web server to queue actions
    void ExecuteAction(int action_type, int steps, int speed, int direction, int amount) {
        QueueAction(action_type, steps, speed, direction, amount);
    }
    
    // Public method to stop all actions and clear queue
    void StopAll() {
        ESP_LOGI(TAG, "🛑 StopAll() called - clearing queue");
        
        // Reset the queue to clear all pending actions
        if (action_queue_ != nullptr) {
            xQueueReset(action_queue_);
            ESP_LOGI(TAG, "✅ Queue cleared");
        }
        
        // Set flag to stop current action
        is_action_in_progress_ = false;
        
        // Go to home position immediately
        otto_.Home();
        
        ESP_LOGI(TAG, "✅ Robot stopped and at home position");
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
        ESP_LOGI(TAG, "🌐 Web/Voice request: action=%d, steps=%d, speed=%d, dir=%d, amt=%d", 
                 action_type, steps, speed, direction, amount);
        
        if (g_otto_controller == nullptr) {
            ESP_LOGE(TAG, "❌ FATAL: Kiki controller not initialized!");
            return ESP_ERR_INVALID_STATE;
        }
        
        g_otto_controller->ExecuteAction(action_type, steps, speed, direction, amount);
        return ESP_OK;
    }
    
    // Stop and clear all queued actions
    esp_err_t otto_controller_stop_all() {
        ESP_LOGI(TAG, "🛑 STOP ALL requested from web/external");
        
        if (g_otto_controller == nullptr) {
            ESP_LOGE(TAG, "❌ FATAL: Kiki controller not initialized!");
            return ESP_ERR_INVALID_STATE;
        }
        
        // Call the public StopAll method
        g_otto_controller->StopAll();
        
        return ESP_OK;
    }
}
