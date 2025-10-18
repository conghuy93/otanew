# 📰 News API MCP Tool - Vietnamese News

## Overview
MCP tool để lấy tin tức tiếng Việt từ NewsAPI.org, hỗ trợ các danh mục: công nghệ, giải trí, thể thao, và giá vàng.

## Tool Information

### Tool Name
`news.get_vietnam_news`

### Description
Get latest Vietnamese news articles by category from NewsAPI. Returns a formatted list of news articles with titles, descriptions, and URLs.

## Parameters

| Parameter | Type | Default | Range | Description |
|-----------|------|---------|-------|-------------|
| category | string | "technology" | - | News category: 'technology', 'entertainment', 'sports', or 'gold' |
| limit | integer | 5 | 1-10 | Maximum number of articles to return |

## Supported Categories

### 1. Technology (Công nghệ) 🧠
- **Category**: `technology`
- **Endpoint**: `top-headlines?country=vn&category=technology`
- **Use case**: AI, gadgets, software, hardware news

### 2. Entertainment (Giải trí) 🎬
- **Category**: `entertainment`
- **Endpoint**: `top-headlines?country=vn&category=entertainment`
- **Use case**: Movies, music, celebrities, showbiz

### 3. Sports (Thể thao) ⚽
- **Category**: `sports`
- **Endpoint**: `top-headlines?country=vn&category=sports`
- **Use case**: Football, olympics, tournaments, athletes

### 4. Gold Price (Giá vàng) 💰
- **Category**: `gold`
- **Endpoint**: `everything?q=giá%20vàng%20Việt%20Nam&language=vi`
- **Use case**: Gold prices, market updates, financial news

## Usage Examples

### Voice Commands
```
"Get me tech news"
"Show me sports news"
"What's the gold price today?"
"Get entertainment news"
```

### MCP Call Examples

#### Get 5 technology articles
```json
{
  "tool": "news.get_vietnam_news",
  "arguments": {
    "category": "technology",
    "limit": 5
  }
}
```

#### Get 3 gold price updates
```json
{
  "tool": "news.get_vietnam_news",
  "arguments": {
    "category": "gold",
    "limit": 3
  }
}
```

#### Get 10 sports articles
```json
{
  "tool": "news.get_vietnam_news",
  "arguments": {
    "category": "sports",
    "limit": 10
  }
}
```

## API Configuration

### NewsAPI Key
```cpp
const char* api_key = "87ad9881b871439c8c687d1fcb143eea";
```

### URL Templates

#### Top Headlines (technology, entertainment, sports)
```
https://newsapi.org/v2/top-headlines?country=vn&category={category}&pageSize={limit}&apiKey={api_key}
```

#### Everything Search (gold)
```
https://newsapi.org/v2/everything?q=giá%20vàng%20Việt%20Nam&language=vi&pageSize={limit}&apiKey={api_key}
```

## Response Format

### Successful Response
```
📰 News fetching tool registered!
Category: technology
Limit: 5 articles
API URL: https://newsapi.org/v2/top-headlines?country=vn&category=technology&pageSize=5&apiKey=...

[Article 1]
Title: ...
Description: ...
URL: ...

[Article 2]
...
```

### Error Response
```
⚠️ Note: HTTP client implementation needed to fetch actual news.
Please implement HTTP GET request using esp_http_client or similar.
```

## Implementation Status

### ✅ Completed
- MCP tool registration
- Category parameter handling
- URL generation for all categories
- API key integration
- Parameter validation

### ⏳ TODO
- HTTP client implementation using `esp_http_client`
- JSON parsing for NewsAPI response
- Article formatting
- Error handling for network failures
- Cache mechanism to reduce API calls
- Rate limiting (100 requests/day for free tier)

## HTTP Client Implementation Guide

### Required Headers
```cpp
#include "esp_http_client.h"
#include "esp_tls.h"
#include "cJSON.h"
```

### Basic HTTP GET Request
```cpp
esp_http_client_config_t config = {
    .url = url.c_str(),
    .method = HTTP_METHOD_GET,
    .timeout_ms = 5000,
    .buffer_size = 4096
};

esp_http_client_handle_t client = esp_http_client_init(&config);
esp_err_t err = esp_http_client_perform(client);

if (err == ESP_OK) {
    int status_code = esp_http_client_get_status_code(client);
    int content_length = esp_http_client_get_content_length(client);
    
    // Read response data
    char* buffer = (char*)malloc(content_length + 1);
    esp_http_client_read_response(client, buffer, content_length);
    buffer[content_length] = '\0';
    
    // Parse JSON response
    cJSON* root = cJSON_Parse(buffer);
    // ... process articles ...
    
    free(buffer);
    cJSON_Delete(root);
}

esp_http_client_cleanup(client);
```

## NewsAPI Response Structure

### Top Headlines Response
```json
{
  "status": "ok",
  "totalResults": 38,
  "articles": [
    {
      "source": {
        "id": null,
        "name": "VnExpress"
      },
      "author": "Author Name",
      "title": "Article Title",
      "description": "Article description...",
      "url": "https://...",
      "urlToImage": "https://...",
      "publishedAt": "2025-10-13T10:00:00Z",
      "content": "Full article content..."
    }
  ]
}
```

## Rate Limits & Restrictions

### Free Tier (Developer Plan)
- **100 requests per day**
- **HTTPS only**
- **Attribution required**: Must mention "Powered by NewsAPI.org"
- **Refresh**: Every 15 minutes for top-headlines
- **History**: Up to 1 month for everything endpoint

### Best Practices
1. Cache responses locally for 15 minutes
2. Implement request counter to avoid hitting limit
3. Handle rate limit errors gracefully
4. Display "Powered by NewsAPI.org" attribution

## Integration with Web UI

### Add News Button to otto_webserver.cc
```cpp
<button onclick="getNews('technology')">📰 Tin công nghệ</button>
<button onclick="getNews('sports')">⚽ Tin thể thao</button>
<button onclick="getNews('entertainment')">🎬 Tin giải trí</button>
<button onclick="getNews('gold')">💰 Giá vàng</button>

<div id="news-container"></div>

<script>
async function getNews(category) {
    const response = await fetch('/mcp/call', {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify({
            tool: 'news.get_vietnam_news',
            arguments: {category: category, limit: 5}
        })
    });
    const data = await response.json();
    document.getElementById('news-container').innerHTML = data.result;
}
</script>
```

## Testing

### Test Commands
```bash
# Technology news
curl -X POST http://192.168.1.100/mcp/call \
  -H "Content-Type: application/json" \
  -d '{"tool":"news.get_vietnam_news","arguments":{"category":"technology","limit":5}}'

# Gold price
curl -X POST http://192.168.1.100/mcp/call \
  -H "Content-Type: application/json" \
  -d '{"tool":"news.get_vietnam_news","arguments":{"category":"gold","limit":3}}'
```

## Future Enhancements

1. **Multi-language Support**: Add English news sources
2. **Custom Keywords**: Allow searching by custom keywords
3. **Date Range Filter**: Filter by publish date
4. **Source Selection**: Choose specific news sources
5. **TTS Integration**: Read news headlines aloud via speaker
6. **Display Integration**: Show news on OLED/LCD screen
7. **Scheduled Updates**: Auto-fetch news every hour

## Related Files

- **Controller**: `main/boards/otto-robot/otto_controller.cc`
- **Web Server**: `main/boards/otto-robot/otto_webserver.cc`
- **HTML Demo**: `docs/news-api-demo.html`

## References

- [NewsAPI Documentation](https://newsapi.org/docs)
- [ESP HTTP Client](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/protocols/esp_http_client.html)
- [cJSON Library](https://github.com/DaveGamble/cJSON)
