#include "wifi_board.h"
#include "display/lcd_display.h"
#include "esp_lcd_sh8601.h"

#include "codecs/box_audio_codec.h"
#include "application.h"
#include "button.h"
#include "led/single_led.h"
#include "mcp_server.h"
#include "config.h"
#include "power_save_timer.h"
#include "axp2101.h"
#include "i2c_device.h"
#include <wifi_station.h>
#include "music_player.h"

#include <esp_log.h>
#include <esp_lcd_panel_vendor.h>
#include <driver/i2c_master.h>
#include <driver/spi_master.h>
#include "esp_io_expander_tca9554.h"
#include "settings.h"

#include <esp_lcd_touch_cst9217.h>
#include <esp_lvgl_port.h>
#include <lvgl.h>

#define TAG "WaveshareEsp32s3TouchAMOLED1inch75"

class Pmic : public Axp2101 {
public:
    Pmic(i2c_master_bus_handle_t i2c_bus, uint8_t addr) : Axp2101(i2c_bus, addr) {
        WriteReg(0x22, 0b110); // PWRON > OFFLEVEL as POWEROFF Source enable
        WriteReg(0x27, 0x10);  // hold 4s to power off

        // Disable All DCs but DC1
        WriteReg(0x80, 0x01);
        // Disable All LDOs
        WriteReg(0x90, 0x00);
        WriteReg(0x91, 0x00);

        // Set DC1 to 3.3V
        WriteReg(0x82, (3300 - 1500) / 100);

        // Set ALDO1 to 3.3V
        WriteReg(0x92, (3300 - 500) / 100);

        // Enable ALDO1(MIC)
        WriteReg(0x90, 0x01);

        WriteReg(0x64, 0x02); // CV charger voltage setting to 4.1V

        WriteReg(0x61, 0x02); // set Main battery precharge current to 50mA
        WriteReg(0x62, 0x08); // set Main battery charger current to 400mA ( 0x08-200mA, 0x09-300mA, 0x0A-400mA )
        WriteReg(0x63, 0x01); // set Main battery term charge current to 25mA
    }
};

#define LCD_OPCODE_WRITE_CMD (0x02ULL)
#define LCD_OPCODE_READ_CMD (0x03ULL)
#define LCD_OPCODE_WRITE_COLOR (0x32ULL)

static const sh8601_lcd_init_cmd_t vendor_specific_init[] = {
    // set display to qspi mode
    {0xFE, (uint8_t[]){0x20}, 1, 0},
    {0x19, (uint8_t[]){0x10}, 1, 0},
    {0x1C, (uint8_t[]){0xA0}, 1, 0},

    {0xFE, (uint8_t[]){0x00}, 1, 0},
    {0xC4, (uint8_t[]){0x80}, 1, 0},
    {0x3A, (uint8_t[]){0x55}, 1, 0},
    {0x35, (uint8_t[]){0x00}, 1, 0},
    {0x53, (uint8_t[]){0x20}, 1, 0},
    {0x51, (uint8_t[]){0xFF}, 1, 0},
    {0x63, (uint8_t[]){0xFF}, 1, 0},
    {0x2A, (uint8_t[]){0x00, 0x06, 0x01, 0xD7}, 4, 0},
    {0x2B, (uint8_t[]){0x00, 0x00, 0x01, 0xD1}, 4, 600},
    {0x11, NULL, 0, 600},
    {0x29, NULL, 0, 0},
};

// 在waveshare_amoled_1_75类之前添加新的显示类
class CustomLcdDisplay : public SpiLcdDisplay {
public:
    static void rounder_event_cb(lv_event_t* e) {
        lv_area_t* area = (lv_area_t* )lv_event_get_param(e);
        uint16_t x1 = area->x1;
        uint16_t x2 = area->x2;

        uint16_t y1 = area->y1;
        uint16_t y2 = area->y2;

        // round the start of coordinate down to the nearest 2M number
        area->x1 = (x1 >> 1) << 1;
        area->y1 = (y1 >> 1) << 1;
        // round the end of coordinate up to the nearest 2N+1 number
        area->x2 = ((x2 >> 1) << 1) + 1;
        area->y2 = ((y2 >> 1) << 1) + 1;
    }

    CustomLcdDisplay(esp_lcd_panel_io_handle_t io_handle,
                     esp_lcd_panel_handle_t panel_handle,
                     int width,
                     int height,
                     int offset_x,
                     int offset_y,
                     bool mirror_x,
                     bool mirror_y,
                     bool swap_xy)
        : SpiLcdDisplay(io_handle, panel_handle,
                        width, height, offset_x, offset_y, mirror_x, mirror_y, swap_xy) {
        DisplayLockGuard lock(this);
        lv_obj_set_style_pad_left(status_bar_, LV_HOR_RES*  0.1, 0);
        lv_obj_set_style_pad_right(status_bar_, LV_HOR_RES*  0.1, 0);
        lv_display_add_event_cb(display_, rounder_event_cb, LV_EVENT_INVALIDATE_AREA, NULL);
    }
};

class CustomBacklight : public Backlight {
public:
    CustomBacklight(esp_lcd_panel_io_handle_t panel_io) : Backlight(), panel_io_(panel_io) {}

protected:
    esp_lcd_panel_io_handle_t panel_io_;

    virtual void SetBrightnessImpl(uint8_t brightness) override {
        auto display = Board::GetInstance().GetDisplay();
        DisplayLockGuard lock(display);
        uint8_t data[1] = {((uint8_t)((255*  brightness) / 100))};
        int lcd_cmd = 0x51;
        lcd_cmd &= 0xff;
        lcd_cmd <<= 8;
        lcd_cmd |= LCD_OPCODE_WRITE_CMD << 24;
        esp_lcd_panel_io_tx_param(panel_io_, lcd_cmd, &data, sizeof(data));
    }
};

class WaveshareEsp32s3TouchAMOLED1inch75 : public WifiBoard {
private:
    i2c_master_bus_handle_t i2c_bus_;
    Pmic* pmic_ = nullptr;
    Button boot_button_;
    CustomLcdDisplay* display_;
    CustomBacklight* backlight_;
    esp_io_expander_handle_t io_expander = NULL;
    PowerSaveTimer* power_save_timer_;
    MusicPlayer* music_player_;

    void InitializeMusicPlayer() {
        music_player_ = new MusicPlayer();
        if (music_player_->init()) {
            ESP_LOGI(TAG, "Music Player initialized with Music Tree support");
            
            // Add Vietnamese music tracks to the library
            MusicTrack track1 = {
                .title = "Lạc Trôi",
                .artist = "Sơn Tùng",
                .album = "Sơn Tùng Collection",
                .file_path = "/spiffs/music/lac_troi.mp3",
                .duration_ms = 240000,
                .genre = "V-Pop"
            };
            
            MusicTrack track2 = {
                .title = "Hãy Trao Cho Anh",
                .artist = "Sơn Tùng",
                .album = "Sơn Tùng Collection",
                .file_path = "/spiffs/music/hay_trao_cho_anh.mp3",
                .duration_ms = 220000,
                .genre = "V-Pop"
            };
            
            MusicTrack track3 = {
                .title = "Chúng Ta Của Hiện Tại",
                .artist = "Sơn Tùng",
                .album = "Sơn Tùng Collection", 
                .file_path = "/spiffs/music/chung_ta_cua_hien_tai.mp3",
                .duration_ms = 210000,
                .genre = "V-Pop"
            };
            
            MusicTrack track4 = {
                .title = "Em Của Ngày Hôm Qua",
                .artist = "Sơn Tùng",
                .album = "Sơn Tùng Collection",
                .file_path = "/spiffs/music/em_cua_ngay_hom_qua.mp3",
                .duration_ms = 235000,
                .genre = "V-Pop"
            };
            
            MusicTrack track5 = {
                .title = "Muộn Rồi Mà Sao Còn",
                .artist = "Sơn Tùng",
                .album = "Sơn Tùng Collection",
                .file_path = "/spiffs/music/muon_roi_ma_sao_con.mp3",
                .duration_ms = 205000,
                .genre = "V-Pop"
            };
            
            MusicTrack track6 = {
                .title = "远方",
                .artist = "Classical Artist",
                .album = "远方专辑",
                .file_path = "/spiffs/music/yuanfang.mp3", 
                .duration_ms = 180000,
                .genre = "Chinese"
            };
            
            MusicTrack track7 = {
                .title = "逐梦",
                .artist = "EDM Producer", 
                .album = "逐梦专辑",
                .file_path = "/spiffs/music/zhumeng.mp3",
                .duration_ms = 240000,
                .genre = "Chinese"
            };
            
            // Add tracks to library
            music_player_->add_track(track1);  // Lạc Trôi
            music_player_->add_track(track2);  // Hãy Trao Cho Anh
            music_player_->add_track(track3);  // Chúng Ta Của Hiện Tại
            music_player_->add_track(track4);  // Em Của Ngày Hôm Qua
            music_player_->add_track(track5);  // Muộn Rồi Mà Sao Còn
            music_player_->add_track(track6);  // 远方
            music_player_->add_track(track7);  // 逐梦
            
            // Create playlists
            music_player_->create_playlist("Sơn Tùng Hits");
            music_player_->create_playlist("V-Pop Favorites");
            music_player_->create_playlist("Chinese Songs");
            music_player_->create_playlist("All Songs");
            
            // Add tracks to playlists
            // Sơn Tùng playlist
            music_player_->add_to_playlist("Sơn Tùng Hits", track1);
            music_player_->add_to_playlist("Sơn Tùng Hits", track2);
            music_player_->add_to_playlist("Sơn Tùng Hits", track3);
            music_player_->add_to_playlist("Sơn Tùng Hits", track4);
            music_player_->add_to_playlist("Sơn Tùng Hits", track5);
            
            // V-Pop playlist
            music_player_->add_to_playlist("V-Pop Favorites", track1);
            music_player_->add_to_playlist("V-Pop Favorites", track2);
            music_player_->add_to_playlist("V-Pop Favorites", track3);
            music_player_->add_to_playlist("V-Pop Favorites", track4);
            music_player_->add_to_playlist("V-Pop Favorites", track5);
            
            // Chinese playlist
            music_player_->add_to_playlist("Chinese Songs", track6);
            music_player_->add_to_playlist("Chinese Songs", track7);
            
            // All songs
            music_player_->add_to_playlist("All Songs", track1);
            music_player_->add_to_playlist("All Songs", track2);
            music_player_->add_to_playlist("All Songs", track3);
            music_player_->add_to_playlist("All Songs", track4);
            music_player_->add_to_playlist("All Songs", track5);
            music_player_->add_to_playlist("All Songs", track6);
            music_player_->add_to_playlist("All Songs", track7);
            
            ESP_LOGI(TAG, "Music library initialized with Vietnamese and Chinese content");
        } else {
            ESP_LOGE(TAG, "Failed to initialize Music Player");
        }
    }

    void InitializePowerSaveTimer() {
        power_save_timer_ = new PowerSaveTimer(-1, 60, 300);
        power_save_timer_->OnEnterSleepMode([this]() {
            GetDisplay()->SetPowerSaveMode(true);
            GetBacklight()->SetBrightness(20); });
        power_save_timer_->OnExitSleepMode([this]() {
            GetDisplay()->SetPowerSaveMode(false);
            GetBacklight()->RestoreBrightness(); });
        power_save_timer_->OnShutdownRequest([this](){ 
            pmic_->PowerOff(); });
        power_save_timer_->SetEnabled(true);
    }

    void InitializeCodecI2c() {
        // Initialize I2C peripheral
        i2c_master_bus_config_t i2c_bus_cfg = {
            .i2c_port = I2C_NUM_0,
            .sda_io_num = AUDIO_CODEC_I2C_SDA_PIN,
            .scl_io_num = AUDIO_CODEC_I2C_SCL_PIN,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .flags = {
                .enable_internal_pullup = 1,
            },
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &i2c_bus_));
    }

    void InitializeTca9554(void) {
        esp_err_t ret = esp_io_expander_new_i2c_tca9554(i2c_bus_, I2C_ADDRESS, &io_expander);
        if (ret != ESP_OK)
            ESP_LOGE(TAG, "TCA9554 create returned error");
        ret = esp_io_expander_set_dir(io_expander, IO_EXPANDER_PIN_NUM_4, IO_EXPANDER_INPUT);
        ESP_ERROR_CHECK(ret);
    }

    void InitializeAxp2101() {
        ESP_LOGI(TAG, "Init AXP2101");
        pmic_ = new Pmic(i2c_bus_, 0x34);
    }

    void InitializeSpi() {
        spi_bus_config_t buscfg = {};
        buscfg.sclk_io_num = EXAMPLE_PIN_NUM_LCD_PCLK;
        buscfg.data0_io_num = EXAMPLE_PIN_NUM_LCD_DATA0;
        buscfg.data1_io_num = EXAMPLE_PIN_NUM_LCD_DATA1;
        buscfg.data2_io_num = EXAMPLE_PIN_NUM_LCD_DATA2;
        buscfg.data3_io_num = EXAMPLE_PIN_NUM_LCD_DATA3;
        buscfg.max_transfer_sz = DISPLAY_WIDTH*  DISPLAY_HEIGHT*  sizeof(uint16_t);
        buscfg.flags = SPICOMMON_BUSFLAG_QUAD;
        ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO));
    }

    void InitializeButtons() {
        boot_button_.OnClick([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting && !WifiStation::GetInstance().IsConnected()) {
                ResetWifiConfiguration();
            }
            app.ToggleChatState();
        });

#if CONFIG_USE_DEVICE_AEC
        boot_button_.OnDoubleClick([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateIdle) {
                app.SetAecMode(app.GetAecMode() == kAecOff ? kAecOnDeviceSide : kAecOff);
            }
        });
#endif
    }

    void InitializeSH8601Display() {
        esp_lcd_panel_io_handle_t panel_io = nullptr;
        esp_lcd_panel_handle_t panel = nullptr;

        // 液晶屏控制IO初始化
        ESP_LOGD(TAG, "Install panel IO");
        esp_lcd_panel_io_spi_config_t io_config = SH8601_PANEL_IO_QSPI_CONFIG(
            EXAMPLE_PIN_NUM_LCD_CS,
            nullptr,
            nullptr);
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI2_HOST, &io_config, &panel_io));

        // 初始化液晶屏驱动芯片
        ESP_LOGD(TAG, "Install LCD driver");
        const sh8601_vendor_config_t vendor_config = {
            .init_cmds = &vendor_specific_init[0],
            .init_cmds_size = sizeof(vendor_specific_init) / sizeof(sh8601_lcd_init_cmd_t),
            .flags = {
                .use_qspi_interface = 1,
            }};

        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = EXAMPLE_PIN_NUM_LCD_RST;
        panel_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
        panel_config.bits_per_pixel = 16;
        panel_config.vendor_config = (void* )&vendor_config;
        ESP_ERROR_CHECK(esp_lcd_new_panel_sh8601(panel_io, &panel_config, &panel));
        esp_lcd_panel_set_gap(panel, 0x06, 0);
        esp_lcd_panel_reset(panel);
        esp_lcd_panel_init(panel);
        esp_lcd_panel_invert_color(panel, false);
        esp_lcd_panel_mirror(panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);
        esp_lcd_panel_disp_on_off(panel, true);
        display_ = new CustomLcdDisplay(panel_io, panel,
                                        DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY);
        backlight_ = new CustomBacklight(panel_io);
        backlight_->RestoreBrightness();
    }

    void InitializeTouch() {
        esp_lcd_touch_handle_t tp;
        esp_lcd_touch_config_t tp_cfg = {
            .x_max = DISPLAY_WIDTH - 1,
            .y_max = DISPLAY_HEIGHT - 1,
            .rst_gpio_num = GPIO_NUM_40,
            .int_gpio_num = GPIO_NUM_NC,
            .levels = {
                .reset = 0,
                .interrupt = 0,
            },
            .flags = {
                .swap_xy = 0,
                .mirror_x = 1,
                .mirror_y = 1,
            },
        };
        esp_lcd_panel_io_handle_t tp_io_handle = NULL;
        esp_lcd_panel_io_i2c_config_t tp_io_config = ESP_LCD_TOUCH_IO_I2C_CST9217_CONFIG();
        tp_io_config.scl_speed_hz = 400*  1000;
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c(i2c_bus_, &tp_io_config, &tp_io_handle));
        ESP_LOGI(TAG, "Initialize touch controller");
        ESP_ERROR_CHECK(esp_lcd_touch_new_i2c_cst9217(tp_io_handle, &tp_cfg, &tp));
        const lvgl_port_touch_cfg_t touch_cfg = {
            .disp = lv_display_get_default(),
            .handle = tp,
        };
        lvgl_port_add_touch(&touch_cfg);
        ESP_LOGI(TAG, "Touch panel initialized successfully");
    }

    // 初始化工具
    void InitializeTools() {
        auto &mcp_server = McpServer::GetInstance();
        
        // System tools
        mcp_server.AddTool("self.system.reconfigure_wifi",
            "Reboot the device and enter WiFi configuration mode.\n"
            "**CAUTION** You must ask the user to confirm this action.",
            PropertyList(), [this](const PropertyList& properties) {
                ResetWifiConfiguration();
                return true;
            });

        // Basic Music Controls
        mcp_server.AddTool("self.music.play",
            "Play a music file",
            PropertyList(), [this](const PropertyList& properties) {
                return music_player_->play("/spiffs/music/demo.mp3");
            });

        mcp_server.AddTool("self.music.stop", "Stop music playback",
            PropertyList(), [this](const PropertyList& properties) {
                music_player_->stop();
                return true;
            });

        mcp_server.AddTool("self.music.pause", "Pause music playback",
            PropertyList(), [this](const PropertyList& properties) {
                music_player_->pause();
                return true;
            });

        mcp_server.AddTool("self.music.resume", "Resume music playback",
            PropertyList(), [this](const PropertyList& properties) {
                music_player_->resume();
                return true;
            });

        mcp_server.AddTool("self.music.next", "Play next track in queue",
            PropertyList(), [this](const PropertyList& properties) {
                music_player_->next_track();
                return true;
            });

        mcp_server.AddTool("self.music.previous", "Play previous track in queue",
            PropertyList(), [this](const PropertyList& properties) {
                music_player_->previous_track();
                return true;
            });

        mcp_server.AddTool("self.music.is_playing", "Check if music is playing",
            PropertyList(), [this](const PropertyList& properties) {
                bool playing = music_player_->is_playing();
                ESP_LOGI(TAG, "Music playing: %s", playing ? "yes" : "no");
                return true;
            });

        // Playlist management
        mcp_server.AddTool("self.music.list_playlists", "List all available playlists",
            PropertyList(), [this](const PropertyList& properties) {
                auto playlists = music_player_->get_playlists();
                ESP_LOGI(TAG, "Available playlists (%d):", (int)playlists.size());
                for (const auto& playlist : playlists) {
                    ESP_LOGI(TAG, "  - %s", playlist.c_str());
                }
                return true;
            });

        // Library browsing
        mcp_server.AddTool("self.music.list_artists", "List all artists in library",
            PropertyList(), [this](const PropertyList& properties) {
                auto artists = music_player_->get_artists();
                ESP_LOGI(TAG, "Artists in library (%d):", (int)artists.size());
                for (const auto& artist : artists) {
                    ESP_LOGI(TAG, "  - %s", artist.c_str());
                }
                return true;
            });

        mcp_server.AddTool("self.music.get_current_track", "Get information about currently playing track",
            PropertyList(), [this](const PropertyList& properties) {
                auto track = music_player_->get_current_track();
                ESP_LOGI(TAG, "Current track: %s by %s from %s", 
                        track.title.c_str(), track.artist.c_str(), track.album.c_str());
                return true;
            });

        // Audio effects
        mcp_server.AddTool("self.music.enable_shuffle", "Enable shuffle mode",
            PropertyList(), [this](const PropertyList& properties) {
                music_player_->shuffle(true);
                return true;
            });

        mcp_server.AddTool("self.music.disable_shuffle", "Disable shuffle mode",
            PropertyList(), [this](const PropertyList& properties) {
                music_player_->shuffle(false);
                return true;
            });

        mcp_server.AddTool("self.music.enable_repeat", "Enable repeat mode",
            PropertyList(), [this](const PropertyList& properties) {
                music_player_->repeat(true);
                return true;
            });

        mcp_server.AddTool("self.music.disable_repeat", "Disable repeat mode",
            PropertyList(), [this](const PropertyList& properties) {
                music_player_->repeat(false);
                return true;
            });

        mcp_server.AddTool("self.music.enable_reverb", "Enable reverb effect",
            PropertyList(), [this](const PropertyList& properties) {
                music_player_->enable_reverb(true);
                return true;
            });

        mcp_server.AddTool("self.music.disable_reverb", "Disable reverb effect",
            PropertyList(), [this](const PropertyList& properties) {
                music_player_->enable_reverb(false);
                return true;
            });

        mcp_server.AddTool("self.music.enable_echo", "Enable echo effect",
            PropertyList(), [this](const PropertyList& properties) {
                music_player_->enable_echo(true);
                return true;
            });

        mcp_server.AddTool("self.music.disable_echo", "Disable echo effect",
            PropertyList(), [this](const PropertyList& properties) {
                music_player_->enable_echo(false);
                return true;
            });
    }

public:
    WaveshareEsp32s3TouchAMOLED1inch75() : boot_button_(BOOT_BUTTON_GPIO) {
        InitializePowerSaveTimer();
        InitializeCodecI2c();
        InitializeTca9554();
        InitializeAxp2101();
        InitializeSpi();
        InitializeSH8601Display();
        InitializeTouch();
        InitializeButtons();
        InitializeMusicPlayer();
        InitializeTools();
    }

    virtual ~WaveshareEsp32s3TouchAMOLED1inch75() {
        delete music_player_;
    }

    virtual MusicPlayer* GetMusicPlayer() override {
        return music_player_;
    }

    virtual AudioCodec* GetAudioCodec() override {
        static BoxAudioCodec audio_codec(
            i2c_bus_, 
            AUDIO_INPUT_SAMPLE_RATE, 
            AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_MCLK, 
            AUDIO_I2S_GPIO_BCLK, 
            AUDIO_I2S_GPIO_WS, 
            AUDIO_I2S_GPIO_DOUT, 
            AUDIO_I2S_GPIO_DIN,
            AUDIO_CODEC_PA_PIN, 
            AUDIO_CODEC_ES8311_ADDR, 
            AUDIO_CODEC_ES7210_ADDR, 
            AUDIO_INPUT_REFERENCE);
        return &audio_codec;
    }

    virtual Display* GetDisplay() override {
        return display_;
    }

    virtual Backlight* GetBacklight() override {
        return backlight_;
    }

    virtual bool GetBatteryLevel(int &level, bool &charging, bool &discharging) override {
        static bool last_discharging = false;
        charging = pmic_->IsCharging();
        discharging = pmic_->IsDischarging();
        if (discharging != last_discharging)
        {
            power_save_timer_->SetEnabled(discharging);
            last_discharging = discharging;
        }

        level = pmic_->GetBatteryLevel();
        return true;
    }

    virtual void SetPowerSaveMode(bool enabled) override {
        if (!enabled)
        {
            power_save_timer_->WakeUp();
        }
        WifiBoard::SetPowerSaveMode(enabled);
    }
};

DECLARE_BOARD(WaveshareEsp32s3TouchAMOLED1inch75);
