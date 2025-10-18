#include "otto_movements.h"

#include <algorithm>

#include "oscillator.h"

static const char* TAG = "OttoMovements";

Otto::Otto() {
    is_otto_resting_ = false;
    speed_delay_ = 100;  // Reduced to 100ms for faster movement
    
    // Initialize all servo pins to -1 (not connected)
    for (int i = 0; i < SERVO_COUNT; i++) {
        servo_pins_[i] = -1;
        servo_trim_[i] = 0;
        servo_compensate_[i] = 0;  // Compensation angles
    }
}

Otto::~Otto() {
    DetachServos();
}

unsigned long IRAM_ATTR millis() {
    return (unsigned long)(esp_timer_get_time() / 1000ULL);
}

void Otto::Init(int left_front, int right_front, int left_back, int right_back) {
    servo_pins_[SERVO_LF] = left_front;
    servo_pins_[SERVO_RF] = right_front;
    servo_pins_[SERVO_LB] = left_back;
    servo_pins_[SERVO_RB] = right_back;

    ESP_LOGI(TAG, "Initializing Otto with pins: LF=%d, RF=%d, LB=%d, RB=%d", 
             left_front, right_front, left_back, right_back);

    AttachServos();
    is_otto_resting_ = false;
}

///////////////////////////////////////////////////////////////////
//-- ATTACH & DETACH FUNCTIONS ----------------------------------//
///////////////////////////////////////////////////////////////////
void Otto::AttachServos() {
    ESP_LOGI(TAG, "Attaching servos...");
    for (int i = 0; i < SERVO_COUNT; i++) {
        if (servo_pins_[i] != -1) {
            ESP_LOGI(TAG, "Attaching servo %d to GPIO %d", i, servo_pins_[i]);
            servo_[i].Attach(servo_pins_[i]);
            ESP_LOGI(TAG, "Servo %d attached successfully", i);
        } else {
            ESP_LOGW(TAG, "Servo %d has invalid pin (-1)", i);
        }
    }
    ESP_LOGI(TAG, "All servos attached");
}

void Otto::DetachServos() {
    for (int i = 0; i < SERVO_COUNT; i++) {
        if (servo_pins_[i] != -1) {
            servo_[i].Detach();
        }
    }
}

///////////////////////////////////////////////////////////////////
//-- SERVO TRIMS & COMPENSATION ---------------------------------//
///////////////////////////////////////////////////////////////////
void Otto::SetTrims(int left_front, int right_front, int left_back, int right_back) {
    servo_trim_[SERVO_LF] = left_front;
    servo_trim_[SERVO_RF] = right_front;
    servo_trim_[SERVO_LB] = left_back;
    servo_trim_[SERVO_RB] = right_back;

    for (int i = 0; i < SERVO_COUNT; i++) {
        if (servo_pins_[i] != -1) {
            servo_[i].SetTrim(servo_trim_[i]);
        }
    }
}

///////////////////////////////////////////////////////////////////
//-- BASIC DOG-STYLE SERVO CONTROL FUNCTIONS -------------------//
///////////////////////////////////////////////////////////////////
void Otto::ServoWrite(int servo_id, float angle) {
    if (servo_id < 0 || servo_id >= SERVO_COUNT || servo_pins_[servo_id] == -1) {
        return;
    }
    
    // Apply compensation and trim
    angle += servo_compensate_[servo_id] + servo_trim_[servo_id];
    
    // Limit angle to 0-180 degrees
    if (angle < 0) angle = 0;
    if (angle > 180) angle = 180;
    
    // For right side servos, invert the angle (like DogMaster)
    if (servo_id == SERVO_RF || servo_id == SERVO_RB) {
        angle = 180 - angle;
    }
    
    servo_[servo_id].SetPosition(angle);
}

void Otto::ServoAngleSet(int servo_id, float angle, int delay_time) {
    ServoWrite(servo_id, angle);
    
    if (delay_time > 0) {
        vTaskDelay(pdMS_TO_TICKS(delay_time));
    }
}

void Otto::ServoInit(int lf_angle, int rf_angle, int lb_angle, int rb_angle, int delay_time) {
    ServoAngleSet(SERVO_LF, lf_angle, 0);
    ServoAngleSet(SERVO_RF, rf_angle, 0);
    ServoAngleSet(SERVO_LB, lb_angle, 0);
    ServoAngleSet(SERVO_RB, rb_angle, 0);
    
    if (delay_time > 0) {
        vTaskDelay(pdMS_TO_TICKS(delay_time));
    }
    
    ESP_LOGI(TAG, "Dog servo initialized - LF:%d RF:%d LB:%d RB:%d", 
             lf_angle, rf_angle, lb_angle, rb_angle);
}

void Otto::ExecuteDogMovement(int lf, int rf, int lb, int rb, int delay_time) {
    ServoAngleSet(SERVO_LF, lf, 0);
    ServoAngleSet(SERVO_RF, rf, 0);
    ServoAngleSet(SERVO_LB, lb, 0);
    ServoAngleSet(SERVO_RB, rb, delay_time);
}

void Otto::MoveToPosition(int target_angles[SERVO_COUNT], int move_time) {
    if (GetRestState() == true) {
        SetRestState(false);
    }

    final_time_ = millis() + move_time;
    if (move_time > 10) {
        for (int i = 0; i < SERVO_COUNT; i++) {
            if (servo_pins_[i] != -1) {
                increment_[i] = (target_angles[i] - servo_[i].GetPosition()) / (move_time / 10.0);
            }
        }

        for (int iteration = 1; millis() < final_time_; iteration++) {
            for (int i = 0; i < SERVO_COUNT; i++) {
                if (servo_pins_[i] != -1) {
                    ServoWrite(i, servo_[i].GetPosition() + increment_[i]);
                }
            }
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    } else {
        for (int i = 0; i < SERVO_COUNT; i++) {
            if (servo_pins_[i] != -1) {
                ServoWrite(i, target_angles[i]);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(move_time));
    }

    // Final adjustment to target
    for (int i = 0; i < SERVO_COUNT; i++) {
        if (servo_pins_[i] != -1) {
            ServoWrite(i, target_angles[i]);
        }
    }
}

///////////////////////////////////////////////////////////////////
//-- HOME & REST FUNCTIONS --------------------------------------//
///////////////////////////////////////////////////////////////////
void Otto::Home() {
    StandUp();
}

void Otto::StandUp() {
    ESP_LOGI(TAG, "Dog standing up to rest position");
    ServoInit(90, 90, 90, 90, 500);
    is_otto_resting_ = true;
    vTaskDelay(pdMS_TO_TICKS(200));
}

bool Otto::GetRestState() {
    return is_otto_resting_;
}

void Otto::SetRestState(bool state) {
    is_otto_resting_ = state;
}

///////////////////////////////////////////////////////////////////
//-- DOG-STYLE MOVEMENT FUNCTIONS (from DogMaster) -------------//
///////////////////////////////////////////////////////////////////

//-- Dog Walk Forward (adapted from DogMaster Action_Advance)
void Otto::DogWalk(int steps, int speed_delay) {
    ESP_LOGI(TAG, "Dog walking forward for %d steps", steps);
    
    // Preparation movement to avoid interference
    StandUp();
    vTaskDelay(pdMS_TO_TICKS(120));

    for (int i = 0; i < steps; i++) {
        // Step 1: DogMaster sequence - LF+RB diagonal, then RF+LB
        ServoAngleSet(SERVO_LF, 30, 0);
        ServoAngleSet(SERVO_RB, 30, speed_delay);
        ServoAngleSet(SERVO_RF, 150, 0);
        ServoAngleSet(SERVO_LB, 150, speed_delay);

        // Return to neutral with same sequence
        ServoAngleSet(SERVO_LF, 90, 0);
        ServoAngleSet(SERVO_RB, 90, speed_delay);
        ServoAngleSet(SERVO_RF, 90, 0);
        ServoAngleSet(SERVO_LB, 90, speed_delay);

        // Step 2: Opposite diagonal - RF+LB, then LF+RB  
        ServoAngleSet(SERVO_RF, 30, 0);
        ServoAngleSet(SERVO_LB, 30, speed_delay);
        ServoAngleSet(SERVO_LF, 150, 0);
        ServoAngleSet(SERVO_RB, 150, speed_delay);

        // Return to neutral
        ServoAngleSet(SERVO_RF, 90, 0);
        ServoAngleSet(SERVO_LB, 90, speed_delay);
        ServoAngleSet(SERVO_LF, 90, 0);
        ServoAngleSet(SERVO_RB, 90, speed_delay);
    }
    
    ESP_LOGI(TAG, "Dog walk forward completed");
}

//-- Dog Walk Backward (adapted from DogMaster Action_Back)
void Otto::DogWalkBack(int steps, int speed_delay) {
    ESP_LOGI(TAG, "Dog walking backward for %d steps", steps);
    
    // Preparation movement - same delay as forward
    StandUp();
    vTaskDelay(pdMS_TO_TICKS(120));

    for (int i = 0; i < steps; i++) {
        // Step 1: DogMaster sequence - LF+RB diagonal (reversed angles)
        ServoAngleSet(SERVO_LF, 150, 0);    // Reversed: from 30 → 150
        ServoAngleSet(SERVO_RB, 150, speed_delay);
        ServoAngleSet(SERVO_RF, 30, 0);     // Reversed: from 150 → 30
        ServoAngleSet(SERVO_LB, 30, speed_delay);

        // Return to neutral with same sequence
        ServoAngleSet(SERVO_LF, 90, 0);
        ServoAngleSet(SERVO_RB, 90, speed_delay);
        ServoAngleSet(SERVO_RF, 90, 0);
        ServoAngleSet(SERVO_LB, 90, speed_delay);

        // Step 2: Opposite diagonal - RF+LB (reversed angles)
        ServoAngleSet(SERVO_RF, 150, 0);    // Reversed: from 30 → 150
        ServoAngleSet(SERVO_LB, 150, speed_delay);
        ServoAngleSet(SERVO_LF, 30, 0);     // Reversed: from 150 → 30
        ServoAngleSet(SERVO_RB, 30, speed_delay);

        // Return to neutral
        ServoAngleSet(SERVO_RF, 90, 0);
        ServoAngleSet(SERVO_LB, 90, speed_delay);
        ServoAngleSet(SERVO_LF, 90, 0);
        ServoAngleSet(SERVO_RB, 90, speed_delay);
    }
    
    ESP_LOGI(TAG, "Dog walk backward completed");
}

//-- Dog Turn Left (adapted from DogMaster Action_TurnLeft)
void Otto::DogTurnLeft(int steps, int speed_delay) {
    ESP_LOGI(TAG, "Dog turning left for %d steps", steps);
    
    StandUp();
    vTaskDelay(pdMS_TO_TICKS(500));

    for (int i = 0; i < steps; i++) {
        // DogMaster sequence: RF+LB first, then LF+RB
        ServoAngleSet(SERVO_RF, 45, 0);
        ServoAngleSet(SERVO_LB, 135, speed_delay);
        ServoAngleSet(SERVO_LF, 45, 0);
        ServoAngleSet(SERVO_RB, 135, speed_delay);

        // Return to neutral
        ServoAngleSet(SERVO_RF, 90, 0);
        ServoAngleSet(SERVO_LB, 90, speed_delay);
        ServoAngleSet(SERVO_LF, 90, 0);
        ServoAngleSet(SERVO_RB, 90, speed_delay);
    }
    
    ESP_LOGI(TAG, "Dog turn left completed");
}

//-- Dog Turn Right (adapted from DogMaster Action_TurnRight)
void Otto::DogTurnRight(int steps, int speed_delay) {
    ESP_LOGI(TAG, "Dog turning right for %d steps", steps);
    
    StandUp();
    vTaskDelay(pdMS_TO_TICKS(500));

    for (int i = 0; i < steps; i++) {
        // DogMaster sequence: LF+RB first, then RF+LB  
        ServoAngleSet(SERVO_LF, 45, 0);
        ServoAngleSet(SERVO_RB, 135, speed_delay);
        ServoAngleSet(SERVO_RF, 45, 0);
        ServoAngleSet(SERVO_LB, 135, speed_delay);

        // Return to neutral
        ServoAngleSet(SERVO_LF, 90, 0);
        ServoAngleSet(SERVO_RB, 90, speed_delay);
        ServoAngleSet(SERVO_RF, 90, 0);
        ServoAngleSet(SERVO_LB, 90, speed_delay);
    }
    
    ESP_LOGI(TAG, "Dog turn right completed");
}

//-- Dog Sit Down (adapted from DogMaster Action_SitDown)
void Otto::DogSitDown(int delay_time) {
    ESP_LOGI(TAG, "Dog sitting down");
    
    // Front legs stay at 90°, back legs go to 30° to sit
    ExecuteDogMovement(90, 90, 30, 30, delay_time);
    
    ESP_LOGI(TAG, "Dog sit down completed");
}

//-- Dog Lie Down (adapted from DogMaster Action_LieDown)
void Otto::DogLieDown(int delay_time) {
    ESP_LOGI(TAG, "Dog lying down completely");
    
    // Gradually lower all legs to lie flat
    ExecuteDogMovement(5, 5, 5, 5, delay_time);
    
    vTaskDelay(pdMS_TO_TICKS(1000));  // Hold lying position
    
    ESP_LOGI(TAG, "Dog is now lying completely flat");
}

//-- Dog Jump (adapted from DogMaster Action_Jump)
void Otto::DogJump(int delay_time) {
    ESP_LOGI(TAG, "Dog jumping");
    
    // Prepare to jump - crouch down
    ExecuteDogMovement(60, 60, 60, 60, delay_time);
    
    // Jump up - extend all legs
    ExecuteDogMovement(120, 120, 120, 120, 100);
    
    vTaskDelay(pdMS_TO_TICKS(300));
    
    // Land - return to standing
    StandUp();
    
    ESP_LOGI(TAG, "Dog jump completed");
}

//-- Dog Bow (adapted from DogMaster Action_Bow)
void Otto::DogBow(int delay_time) {
    ESP_LOGI(TAG, "Dog bowing");
    
    // Bow - front legs down, back legs stay up
    ExecuteDogMovement(0, 0, 90, 90, 100);
    
    vTaskDelay(pdMS_TO_TICKS(delay_time));  // Hold bow position
    
    // Stand up again
    StandUp();
    
    ESP_LOGI(TAG, "Dog bow completed");
}

//-- Dog Dance (adapted from DogMaster Action_Dance)
void Otto::DogDance(int cycles, int speed_delay) {
    ESP_LOGI(TAG, "Dog dancing for %d cycles", cycles);
    
    for (int i = 0; i < cycles; i++) {
        // Step 1: Lean left (left side down, right side up)
        ExecuteDogMovement(60, 120, 60, 120, 200);
        
        // Step 2: Lean right (left side up, right side down)
        ExecuteDogMovement(120, 60, 120, 60, 200);
        
        // Step 3: Small jump - crouch down
        ExecuteDogMovement(75, 75, 105, 105, 150);
        vTaskDelay(pdMS_TO_TICKS(100));
        
        // Jump up
        ExecuteDogMovement(105, 105, 75, 75, 150);
    }
    
    // End with standing position
    StandUp();
    
    ESP_LOGI(TAG, "Dog dance completed");
}

//-- Dog Wave Right Foot (adapted from DogMaster Action_WaveRightFoot)
void Otto::DogWaveRightFoot(int waves, int speed_delay) {
    ESP_LOGI(TAG, "Dog waving right front foot %d times", waves);
    
    // Prepare position: other legs stable, right front starts at 90°
    ExecuteDogMovement(90, 90, 90, 90, 300);
    
    // Wave right front leg from 90° to 0° rapidly
    for (int wave_count = 0; wave_count < waves; wave_count++) {
        ESP_LOGI(TAG, "Wave %d", wave_count + 1);
        
        // Wave down from 90° to 0°
        for (int angle = 90; angle >= 0; angle -= 5) {
            ServoAngleSet(SERVO_RF, angle, 0);
            vTaskDelay(pdMS_TO_TICKS(8));
        }
        
        vTaskDelay(pdMS_TO_TICKS(speed_delay));
        
        // Wave up from 0° to 90°
        for (int angle = 0; angle <= 90; angle += 5) {
            ServoAngleSet(SERVO_RF, angle, 0);
            vTaskDelay(pdMS_TO_TICKS(8));
        }
        
        vTaskDelay(pdMS_TO_TICKS(speed_delay));
    }
    
    ESP_LOGI(TAG, "Right foot wave completed");
    
    // End with standing
    StandUp();
}

//-- Dog Dance 4 Feet (adapted from DogMaster Action_Dance4Feet)
void Otto::DogDance4Feet(int cycles, int speed_delay) {
    ESP_LOGI(TAG, "Dog dancing with 4 feet for %d cycles", cycles);
    
    StandUp();
    vTaskDelay(pdMS_TO_TICKS(200));
    
    for (int cycle = 0; cycle < cycles; cycle++) {
        // PHASE 1: All feet move forward together
        ESP_LOGI(TAG, "All feet forward");
        ExecuteDogMovement(60, 60, 60, 60, speed_delay);
        vTaskDelay(pdMS_TO_TICKS(400));
        
        // PHASE 2: All feet move backward together
        ESP_LOGI(TAG, "All feet backward");
        ExecuteDogMovement(120, 120, 120, 120, speed_delay);
        vTaskDelay(pdMS_TO_TICKS(400));
        
        // PHASE 3: Return to center (90°)
        ExecuteDogMovement(90, 90, 90, 90, speed_delay);
        vTaskDelay(pdMS_TO_TICKS(200));
    }
    
    // End with firm standing position
    StandUp();
    vTaskDelay(pdMS_TO_TICKS(500));
    
    ESP_LOGI(TAG, "4-feet dance completed");
}

//-- Dog Swing (adapted from DogMaster Action_Swing)
void Otto::DogSwing(int cycles, int speed_delay) {
    ESP_LOGI(TAG, "Dog swinging for %d cycles", cycles);
    
    StandUp();
    vTaskDelay(pdMS_TO_TICKS(500));

    // Initial lean to prepare
    for (int i = 90; i > 30; i--) {
        ExecuteDogMovement(i, i, i, i, 0);
        vTaskDelay(pdMS_TO_TICKS(speed_delay));
    }
    
    // Swing back and forth
    for (int temp = 0; temp < cycles; temp++) {
        for (int i = 30; i < 90; i++) {
            ExecuteDogMovement(i, 110 - i, i, 110 - i, 0);
            vTaskDelay(pdMS_TO_TICKS(speed_delay));
        }
        for (int i = 90; i > 30; i--) {
            ExecuteDogMovement(i, 110 - i, i, 110 - i, 0);
            vTaskDelay(pdMS_TO_TICKS(speed_delay));
        }
    }
    
    DogSitDown(0);
    
    ESP_LOGI(TAG, "Dog swing completed");
}

//-- Dog Stretch (adapted from DogMaster Action_Stretch)
void Otto::DogStretch(int cycles, int speed_delay) {
    ESP_LOGI(TAG, "Dog stretching for %d cycles", cycles);
    
    ExecuteDogMovement(90, 90, 90, 90, 80);

    for (int i = 0; i < cycles; i++) {
        // Stretch front legs down
        for (int j = 90; j > 10; j--) {
            ExecuteDogMovement(j, j, 90, 90, speed_delay);
        }
        for (int j = 10; j < 90; j++) {
            ExecuteDogMovement(j, j, 90, 90, speed_delay);
        }
        
        // Stretch back legs up
        for (int j = 90; j < 170; j++) {
            ExecuteDogMovement(90, 90, j, j, speed_delay);
        }
        for (int j = 170; j > 90; j--) {
            ExecuteDogMovement(90, 90, j, j, speed_delay);
        }
    }
    
    ESP_LOGI(TAG, "Dog stretch completed");
}

///////////////////////////////////////////////////////////////////
//-- LEGACY MOVEMENT FUNCTIONS (adapted for 4 servos) ----------//
///////////////////////////////////////////////////////////////////

//-- Otto Jump (simplified for 4 servos)
void Otto::Jump(float steps, int period) {
    ESP_LOGI(TAG, "Legacy jump function");
    DogJump(period / 2);
}

//-- Otto Walk (adapted for 4 servos)  
void Otto::Walk(float steps, int period, int dir) {
    ESP_LOGI(TAG, "Legacy walk function");
    int step_count = (int)steps;
    int speed_delay = period / 4;  // Convert period to speed delay
    
    if (dir == FORWARD) {
        DogWalk(step_count, speed_delay);
    } else {
        DogWalkBack(step_count, speed_delay);
    }
}

//-- Otto Turn (adapted for 4 servos)
void Otto::Turn(float steps, int period, int dir) {
    ESP_LOGI(TAG, "Legacy turn function");
    int step_count = (int)steps;
    int speed_delay = period / 4;
    
    if (dir == LEFT) {
        DogTurnLeft(step_count, speed_delay);
    } else {
        DogTurnRight(step_count, speed_delay);
    }
}

//-- Otto Bend (adapted for 4 servos)
void Otto::Bend(int steps, int period, int dir) {
    ESP_LOGI(TAG, "Legacy bend function");
    DogBow(period);
}

///////////////////////////////////////////////////////////////////
//-- SERVO LIMITER FUNCTIONS ------------------------------------//
///////////////////////////////////////////////////////////////////
void Otto::EnableServoLimit(int diff_limit) {
    for (int i = 0; i < SERVO_COUNT; i++) {
        if (servo_pins_[i] != -1) {
            servo_[i].SetLimiter(diff_limit);
        }
    }
}

void Otto::DisableServoLimit() {
    for (int i = 0; i < SERVO_COUNT; i++) {
        if (servo_pins_[i] != -1) {
            servo_[i].DisableLimiter();
        }
    }
}