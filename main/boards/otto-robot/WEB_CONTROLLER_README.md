# Otto Robot Web Controller

## 🌐 Web Interface Setup (No Password Required!)

Otto Robot now includes a modern web interface for controlling the robot without needing passwords or complex authentication.

### ✨ Features

- **🚫 No Password Required** - Direct access to robot controls
- **📱 Mobile Responsive** - Works on phones, tablets, and computers  
- **🎮 Real-time Control** - Instant robot movement commands
- **🎪 Fun Actions** - Dance, jump, bow, wave, and more
- **🎯 Direction Pad** - Intuitive movement controls
- **📊 Status Monitoring** - Real-time robot status updates

### 🔧 Setup Instructions

1. **Configure WiFi** (edit `otto_webserver.h`):
   ```cpp
   #define WIFI_SSID      "YourWiFiName"
   #define WIFI_PASS      "YourWiFiPassword"
   ```

2. **Flash the firmware** with web server enabled

3. **Find the IP address** in the serial monitor:
   ```
   OttoWeb: Got IP: 192.168.1.XXX
   ```

4. **Open your browser** and go to: `http://192.168.1.XXX`

### 🎮 Web Controls

#### Movement Controls
- **⬆️ Forward** - Walk forward 3 steps
- **⬅️ Left** - Turn left 2 steps  
- **➡️ Right** - Turn right 2 steps
- **⬇️ Backward** - Walk backward 3 steps
- **🛑 STOP** - Emergency stop

#### Fun Actions
- **💃 Dance** - 3 cycle dance routine
- **🦘 Jump** - Single jump movement
- **🙇 Bow** - Polite bow (2 second hold)
- **🪑 Sit** - Sit down position
- **🛏️ Lie Down** - Full lying position
- **👋 Wave** - Wave right foot 5 times
- **🎯 Swing** - Left-right swinging motion
- **🧘 Stretch** - Stretching exercise
- **🏠 Home** - Return to standing position

### 🔗 API Endpoints

The web interface uses these REST endpoints:

- `GET /` - Main control page
- `GET /action?cmd={action}&p1={param1}&p2={param2}` - Execute action
- `GET /status` - Get robot status

#### Example API Calls:
```bash
# Make Otto dance
curl "http://192.168.1.XXX/action?cmd=dog_dance&p1=3&p2=200"

# Make Otto walk forward
curl "http://192.168.1.XXX/action?cmd=dog_walk&p1=5&p2=150"

# Make Otto jump
curl "http://192.168.1.XXX/action?cmd=dog_jump&p1=1&p2=200"
```

### 🛠️ Customization

#### Add New Actions
Edit `otto_webserver.cc` to add new buttons:

```cpp
// Add to HTML
httpd_resp_sendstr_chunk(req, "<button class='btn' onclick='sendAction(\"my_action\", 1, 500)'>My Action</button>");

// Add to action handler
void otto_execute_web_action(const char* action, int param1, int param2) {
    if (strcmp(action, "my_action") == 0) {
        // Your custom action here
    }
}
```

#### Modify Styling
Update the CSS in `send_otto_control_page()` function for custom themes.

### 🔧 Troubleshooting

#### Web Page Not Loading
1. Check WiFi connection in serial monitor
2. Verify IP address is correct
3. Ensure firewall allows port 80

#### Actions Not Working
1. Check serial monitor for error messages
2. Verify MCP tools are registered correctly
3. Try the `/status` endpoint to check robot status

#### Servo Issues
1. Verify GPIO pin connections (17, 18, 8, 38)
2. Check power supply to servos
3. Use test servo tool: `/action?cmd=test_servo&p1=0&p2=90`

### 📱 Mobile Usage

The web interface is fully responsive and works great on mobile devices:

- **Portrait mode** - Stacked controls for easy thumb navigation
- **Landscape mode** - Side-by-side layout for two-handed control
- **Touch friendly** - Large buttons optimized for touch input

### 🔒 Security Note

This web interface has **no authentication** for ease of use. Only connect to trusted WiFi networks. For production use, consider adding authentication if needed.

### 🎯 Advanced Usage

#### Integration with Home Automation
The REST API can be integrated with home automation systems:

```yaml
# Home Assistant example
rest_command:
  otto_dance:
    url: "http://192.168.1.XXX/action?cmd=dog_dance&p1=3&p2=200"
  otto_patrol:
    url: "http://192.168.1.XXX/action?cmd=dog_walk&p1=10&p2=100"
```

#### Scheduled Actions
Use cron jobs or task schedulers to create automated routines:

```bash
# Morning routine
0 8 * * * curl "http://192.168.1.XXX/action?cmd=dog_stretch&p1=3&p2=15"
0 8 * * * curl "http://192.168.1.XXX/action?cmd=dog_dance&p1=5&p2=150"
```

### 🚀 Next Steps

- Try all the movement controls
- Create custom action sequences
- Integrate with your smart home system
- Share the web interface with family members

Enjoy controlling your Otto Robot! 🤖✨