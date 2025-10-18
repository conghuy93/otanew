#include "music_player.h"
#include <esp_log.h>

static const char* TAG = "music_demo";

// Demo function to show Music Player usage
void music_demo() {
    ESP_LOGI(TAG, "Starting Music Player Demo");
    
    // Create Music Player instance
    MusicPlayer* player = new MusicPlayer();
    
    if (!player->init()) {
        ESP_LOGE(TAG, "Failed to initialize Music Player");
        return;
    }
    
    // Add demo tracks
    MusicTrack track1 = {
        .title = "Lạc Trôi",
        .artist = "Sơn Tùng", 
        .album = "Collection",
        .file_path = "/sdcard/lac_troi.mp3",
        .duration_ms = 240000,
        .genre = "V-Pop"
    };
    
    MusicTrack track2 = {
        .title = "Hãy Trao Cho Anh",
        .artist = "Sơn Tùng",
        .album = "Collection", 
        .file_path = "/sdcard/hay_trao_cho_anh.mp3",
        .duration_ms = 220000,
        .genre = "V-Pop"
    };
    
    // Add tracks to library
    player->add_track(track1);
    player->add_track(track2);
    ESP_LOGI(TAG, "Added tracks to library");
    
    // Create playlist
    player->create_playlist("Demo Playlist");
    player->add_to_playlist("Demo Playlist", track1);
    player->add_to_playlist("Demo Playlist", track2);
    ESP_LOGI(TAG, "Created demo playlist");
    
    // Play music
    player->play_track("Lạc Trôi");
    ESP_LOGI(TAG, "Playing: Lạc Trôi");
    
    // Control playback
    vTaskDelay(pdMS_TO_TICKS(5000)); // Play for 5 seconds
    player->pause();
    ESP_LOGI(TAG, "Paused playback");
    
    vTaskDelay(pdMS_TO_TICKS(2000)); // Pause for 2 seconds  
    player->resume();
    ESP_LOGI(TAG, "Resumed playback");
    
    // Audio effects
    player->set_volume(80);
    player->set_bass(5);
    player->set_treble(3);
    player->enable_reverb(true);
    ESP_LOGI(TAG, "Applied audio effects");
    
    // Next track
    player->next_track();
    ESP_LOGI(TAG, "Playing next track");
    
    ESP_LOGI(TAG, "Music demo completed");
}

// Alternative simple app_main for testing music only
extern "C" void app_main_music_demo(void) {
    // Initialize NVS flash
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    // Run music demo
    music_demo();
}