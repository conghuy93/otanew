# 🔧 Tóm tắt các vấn đề và giải pháp

## ✅ ĐÃ SỬA - News Tool Fix

### Vấn đề:
- Tool name sai format: `"self.news.get_vietnam_news"`
- LLM không nhận diện được tool này do prefix "self." không đúng chuẩn MCP

### Giải pháp:
```cpp
// TRƯỚC (SAI):
mcp_server.AddTool("self.news.get_vietnam_news", ...)

// SAU (ĐÚNG):
mcp_server.AddTool("news.get_vietnam_news", ...)
```

### Test:
Hãy nói với robot:
- "cho tôi biết tin công nghệ"
- "tin thể thao hôm nay"  
- "giá vàng bao nhiêu"

Robot sẽ gọi tool `news.get_vietnam_news` và trả về tin tức từ VnExpress.

---

## ⚠️ VẤN ĐỀ CẦN KIỂM TRA

### 1. Tool nạp endpoint trên UI không tác dụng

**Nguyên nhân có thể:**
- Web UI chưa có form để set assets download URL
- Endpoint handler chưa được implement trong `otto_webserver.cc`

**Tool hiện có:**
```cpp
// Trong mcp_server.cc line 292
AddUserOnlyTool("self.assets.set_download_url", ...)
```

**Cách kiểm tra:**
1. Mở web UI: http://192.168.0.38
2. Tìm form "Set Assets URL" hoặc tương tự
3. Nếu không có → cần thêm vào `otto_webserver.cc`

**Giải pháp tạm thời:**
Sử dụng MCP tool qua chat:
```
"Set assets download URL to http://example.com/assets.zip"
```

---

### 2. Tìm ESP32 không được (mDNS Discovery)

**Nguyên nhân có thể:**
- mDNS service chưa được enable
- ESP32 chưa broadcast hostname
- Router/Firewall chặn mDNS packets (UDP port 5353)

**Cách kiểm tra trong code:**

Tìm trong `application.cc` hoặc `main.cc`:
```cpp
// Nên có dòng này
mdns_init();
mdns_hostname_set("xiaozhi");
mdns_service_add("xiaozhi", "_http", "_tcp", 80, NULL, 0);
```

**Cách test manual:**
```bash
# Windows (cài Bonjour Print Services)
dns-sd -B _http._tcp local

# Linux/Mac
avahi-browse -a

# Hoặc dùng IP trực tiếp
http://192.168.0.38
```

---

## 🎯 DANH SÁCH KIỂM TRA

### Build hiện tại:
- [x] Otto GIF mode mặc định ✅
- [x] Keyword "bằng bằng" → emoji shocked (no text) ✅
- [x] Emotion lock ngăn LLM ghi đè ✅
- [x] News tool registered với đúng format ✅
- [ ] News tool hoạt động (cần test) ⏳
- [ ] Web UI assets endpoint (cần kiểm tra) ⏳
- [ ] mDNS discovery (cần kiểm tra) ⏳

### Các file đã sửa:
1. `main/application.h` - Added `emotion_locked_` flag
2. `main/application.cc` - Emotion lock logic, keyword detection
3. `main/boards/otto-robot/otto_emoji_display.cc` - Removed duplicate keyword handling
4. `main/boards/otto-robot/otto_controller.cc` - Fixed news tool name
5. `main/boards/otto-robot/otto_robot.cc` - Otto GIF default mode
6. `main/boards/otto-robot/otto_webserver.cc` - Updated button active state

---

## 📝 GHI CHÚ

### News Tool Categories:
- `technology` → VnExpress Số Hóa
- `sports` → VnExpress Thể Thao
- `entertainment` → VnExpress Giải Trí
- `gold` → VnExpress Kinh Doanh (giá vàng)

### News Tool Limits:
- Max 3 articles để tránh watchdog timeout
- Timeout HTTP: 10 seconds
- Sử dụng RSS2JSON service (free, no API key)

### Testing Commands:
```
# Test news tool
"cho tôi tin công nghệ"
"tin thể thao mới nhất"
"giá vàng hôm nay"

# Test keyword trigger
"súng nè"
"bằng bằng"

# Test emoji mode switch
Vào web UI → click "😊 Twemoji Text Mode"
Vào web UI → click "🤖 OTTO GIF MODE"
```

---

## 🔍 DEBUGGING TIPS

### 1. Kiểm tra MCP tools đã register:
Trong serial monitor, tìm:
```
I (xxx) MCP: Add tool: news.get_vietnam_news
```

### 2. Kiểm tra news tool được gọi:
```
I (xxx) OttoController: 📰 Fetching Vietnamese xxx news (limit: 3)...
I (xxx) OttoController: 📡 Fetching via RSS2JSON: https://...
I (xxx) OttoController: 📊 HTTP Status Code: 200
```

### 3. Kiểm tra emotion lock:
```
I (xxx) Application: 🔒 Emotion LOCKED for keyword sequence
I (xxx) Application: ⛔ Ignoring LLM emotion 'happy' (emotion locked for keyword)
I (xxx) Application: 🔓 Emotion UNLOCKED after keyword sequence
```

---

## 🎁 BONUS: Web UI Improvements Needed

Để fix "tool nạp endpoint không tác dụng", cần thêm vào `otto_webserver.cc`:

```html
<!-- Add this to web UI -->
<div class="section">
    <h2>📦 Assets Management</h2>
    <form id="assets-form">
        <label>Assets Download URL:</label>
        <input type="text" id="assets-url" placeholder="http://example.com/assets.zip">
        <button type="submit">Update URL</button>
    </form>
</div>

<script>
document.getElementById('assets-form').onsubmit = async (e) => {
    e.preventDefault();
    const url = document.getElementById('assets-url').value;
    await fetch('/api/assets/url', {
        method: 'POST',
        body: JSON.stringify({url: url})
    });
    alert('Assets URL updated!');
};
</script>
```

Và thêm handler trong C++:
```cpp
httpd_uri_t assets_url_handler = {
    .uri = "/api/assets/url",
    .method = HTTP_POST,
    .handler = [](httpd_req_t* req) {
        // Parse JSON body
        // Call self.assets.set_download_url tool
        return ESP_OK;
    }
};
```
