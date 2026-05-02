/* WiFi softAP + TCP Server + UART 转发
   ESP32开启热点，手机连接后：
   1. 每100ms自动发送自增数字（0-999循环）
   2. 接收UART数据（USB转TTL）并通过WiFi转发到手机
   3. 接收手机命令控制ESP32
*/

#include <string.h>
#include <sys/errno.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_mac.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "driver/uart.h"
#include "driver/gpio.h"

// ==================== UART 配置 ====================
// USB转TTL 连接到 ESP32 的 UART1（避开 UART0 的日志输出）
#define UART_PORT_NUM      UART_NUM_1
#define UART_TXD_PIN       GPIO_NUM_17   // 连接 USB转TTL 的 RX
#define UART_RXD_PIN       GPIO_NUM_16   // 连接 USB转TTL 的 TX
#define UART_BAUD_RATE     115200        // 波特率，与电脑端设置一致
#define UART_BUF_SIZE      1024

// ==================== WiFi热点配置 ====================
#define EXAMPLE_ESP_WIFI_SSID      "ESP32_HotSpot"
#define EXAMPLE_ESP_WIFI_PASS      "12345678"
#define EXAMPLE_ESP_WIFI_CHANNEL   1
#define EXAMPLE_MAX_STA_CONN       5

// ==================== TCP服务器配置 ====================
#define TCP_PORT 12345
#define RX_BUFFER_SIZE 512

// ==================== 定时发送配置 ====================
#define SEND_INTERVAL_MS 3000      // 发送间隔 1000ms
#define MAX_COUNT 1000             // 最大值 1000

static const char *TAG = "ESP32_UART_WIFI";
static int send_counter = 0;
static int client_connected = 0;
static int client_socket = -1;
static int auto_send_enabled = 0;

// ==================== UART 初始化 ====================
static void uart_init(void)
{
    uart_config_t uart_config = {
        .baud_rate = UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    
    // 安装 UART 驱动
    ESP_ERROR_CHECK(uart_driver_install(UART_PORT_NUM, UART_BUF_SIZE * 2, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(UART_PORT_NUM, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(UART_PORT_NUM, UART_TXD_PIN, UART_RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    
    ESP_LOGI(TAG, "UART 初始化完成: 端口=%d, 波特率=%d, TX=%d, RX=%d", 
             UART_PORT_NUM, UART_BAUD_RATE, UART_TXD_PIN, UART_RXD_PIN);
}

// ==================== WiFi事件处理 ====================
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                    int32_t event_id, void* event_data)
{
    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        ESP_LOGI(TAG, "手机已连接，MAC: "MACSTR", AID=%d",
                 MAC2STR(event->mac), event->aid);
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        ESP_LOGI(TAG, "手机已断开，MAC: "MACSTR", AID=%d, reason=%d",
                 MAC2STR(event->mac), event->aid, event->reason);
    }
}

// ==================== WiFi热点初始化 ====================
void wifi_init_softap(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = EXAMPLE_ESP_WIFI_SSID,
            .ssid_len = strlen(EXAMPLE_ESP_WIFI_SSID),
            .channel = EXAMPLE_ESP_WIFI_CHANNEL,
            .password = EXAMPLE_ESP_WIFI_PASS,
            .max_connection = EXAMPLE_MAX_STA_CONN,
            .authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    
    if (strlen(EXAMPLE_ESP_WIFI_PASS) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "WiFi热点已启动: %s", EXAMPLE_ESP_WIFI_SSID);
    ESP_LOGI(TAG, "ESP32 IP: 192.168.4.1, 端口: %d", TCP_PORT);
}

// ==================== 定时发送任务 ====================
static void send_task(void *pvParameters)
{
    char send_buffer[32];
    
    ESP_LOGI(TAG, "定时发送任务已启动，间隔: %dms", SEND_INTERVAL_MS);
    
    while (1) {
        if (auto_send_enabled && client_connected && client_socket >= 0) {
            snprintf(send_buffer, sizeof(send_buffer), "count: %d\r\n", send_counter);
            
            int len = send(client_socket, send_buffer, strlen(send_buffer), 0);
            if (len > 0) {
                ESP_LOGI(TAG, "发送: %d", send_counter);
            }
            
            send_counter++;
            if (send_counter >= MAX_COUNT) {
                send_counter = 0;
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(SEND_INTERVAL_MS));
    }
}

// ==================== UART 接收并转发到 WiFi 任务 ====================
static void uart_to_wifi_task(void *pvParameters)
{
    uint8_t *uart_data = (uint8_t *)malloc(UART_BUF_SIZE);
    if (uart_data == NULL) {
        ESP_LOGE(TAG, "UART 缓冲区分配失败");
        vTaskDelete(NULL);
        return;
    }
    
    ESP_LOGI(TAG, "UART 转发任务已启动，等待接收数据...");
    
    while (1) {
        // 从 UART 读取数据（阻塞 100ms）
        int len = uart_read_bytes(UART_PORT_NUM, uart_data, UART_BUF_SIZE - 1, pdMS_TO_TICKS(100));
        
        if (len > 0) {
            uart_data[len] = '\0';
            ESP_LOGI(TAG, "UART 收到 %d 字节: %s", len, (char *)uart_data);
            
            // 通过 WiFi 转发到手机
            if (client_connected && client_socket >= 0) {
                // 添加前缀标识这是来自 UART 的数据
                char wifi_buffer[UART_BUF_SIZE + 32];
                snprintf(wifi_buffer, sizeof(wifi_buffer), "[UART] %s\r\n", (char *)uart_data);
                int sent = send(client_socket, wifi_buffer, strlen(wifi_buffer), 0);
                if (sent > 0) {
                    ESP_LOGI(TAG, "已转发到手机: %s", (char *)uart_data);
                } else {
                    ESP_LOGE(TAG, "WiFi 发送失败");
                }
            } else {
                ESP_LOGW(TAG, "手机未连接，数据丢弃");
            }
        }
    }
    
    free(uart_data);
    vTaskDelete(NULL);
}

// ==================== 处理手机发来的命令 ====================
static void handle_command(const char *cmd, int client_sock)
{
    char response[256];
    
    if (strcmp(cmd, "start") == 0) {
        auto_send_enabled = 1;
        snprintf(response, sizeof(response), "Auto send enabled\r\n");
        ESP_LOGI(TAG, "命令: 开启自动发送");
    }
    else if (strcmp(cmd, "stop") == 0) {
        auto_send_enabled = 0;
        snprintf(response, sizeof(response), "Auto send disabled\r\n");
        ESP_LOGI(TAG, "命令: 关闭自动发送");
    }
    else if (strcmp(cmd, "reset") == 0) {
        send_counter = 0;
        snprintf(response, sizeof(response), "Counter reset to 0\r\n");
        ESP_LOGI(TAG, "命令: 重置计数器");
    }
    else if (strcmp(cmd, "query") == 0) {
        snprintf(response, sizeof(response), "Count: %d, Auto send: %s, Client: %s\r\n", 
                 send_counter, auto_send_enabled ? "ON" : "OFF", 
                 client_connected ? "Connected" : "Disconnected");
        ESP_LOGI(TAG, "命令: 查询状态");
    }
    else if (strncmp(cmd, "set ", 4) == 0) {
        int new_value = atoi(cmd + 4);
        if (new_value >= 0 && new_value < MAX_COUNT) {
            send_counter = new_value;
            snprintf(response, sizeof(response), "Counter set to %d\r\n", new_value);
            ESP_LOGI(TAG, "命令: 设置计数器为 %d", new_value);
        } else {
            snprintf(response, sizeof(response), "Invalid! Range: 0-%d\r\n", MAX_COUNT - 1);
        }
    }
    else if (strcmp(cmd, "help") == 0) {
        const char *help_msg = 
            "\r\n=== Available Commands ===\r\n"
            "  start  - Enable auto send\r\n"
            "  stop   - Disable auto send\r\n"
            "  reset  - Reset counter to 0\r\n"
            "  query  - Query current status\r\n"
            "  set X  - Set counter to X (0-999)\r\n"
            "  help   - Show this help\r\n"
            "===========================\r\n";
        send(client_sock, help_msg, strlen(help_msg), 0);
        return;
    }
    else {
        snprintf(response, sizeof(response), "Echo: %s\r\n", cmd);
        ESP_LOGI(TAG, "普通消息: %s", cmd);
    }
    
    send(client_sock, response, strlen(response), 0);
}

// ==================== TCP服务器任务（修复版 - 立即响应）====================
static void tcp_server_task(void *pvParameters)
{
    char rx_buffer[RX_BUFFER_SIZE];
    char client_ip[16];
    int listen_sock = -1;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    
    listen_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_sock < 0) {
        ESP_LOGE(TAG, "Socket创建失败");
        vTaskDelete(NULL);
        return;
    }
    
    int opt = 1;
    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(TCP_PORT);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    
    if (bind(listen_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        ESP_LOGE(TAG, "绑定失败");
        close(listen_sock);
        vTaskDelete(NULL);
        return;
    }
    
    if (listen(listen_sock, 5) < 0) {
        ESP_LOGE(TAG, "监听失败");
        close(listen_sock);
        vTaskDelete(NULL);
        return;
    }
    
    ESP_LOGI(TAG, "TCP服务器已启动，端口: %d", TCP_PORT);
    
    while (1) {
        int new_sock = accept(listen_sock, (struct sockaddr *)&client_addr, &client_addr_len);
        if (new_sock < 0) {
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }
        
        inet_ntoa_r(client_addr.sin_addr, client_ip, sizeof(client_ip));
        ESP_LOGI(TAG, ">>> 手机已连接! IP: %s <<<", client_ip);
        
        // ========== 关键修复：禁用 Nagle 算法 ==========
        int flag = 1;
        setsockopt(new_sock, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));
        
        client_socket = new_sock;
        client_connected = 1;
        
        const char *welcome = 
            "Hello! Connected to ESP32 TCP Server\r\n"
            "Type 'help' for commands, any message will be echoed\r\n";
        send(new_sock, welcome, strlen(welcome), 0);
        
        ESP_LOGI(TAG, "新连接已建立，等待命令...");
        
        // 数据处理循环：阻塞等待 socket 可读，避免轮询
        while (1) {
            memset(rx_buffer, 0, sizeof(rx_buffer));
            int len = recv(new_sock, rx_buffer, sizeof(rx_buffer) - 1, 0);
            
            if (len > 0) {
                rx_buffer[len] = '\0';
                
                // 去除换行符
                if (len > 0 && rx_buffer[len-1] == '\n') {
                    rx_buffer[len-1] = '\0';
                    if (len > 1 && rx_buffer[len-2] == '\r') {
                        rx_buffer[len-2] = '\0';
                    }
                }
                
                // 去除空格
                char *cmd = rx_buffer;
                while (*cmd == ' ') cmd++;
                
                if (strlen(cmd) > 0) {
                    ESP_LOGI(TAG, "处理: '%s'", cmd);
                    handle_command(cmd, new_sock);
                }
            } 
            else if (len == 0) {
                ESP_LOGI(TAG, "手机断开连接");
                break;
            }
            else {
                if (errno == EINTR) {
                    continue;
                }
                ESP_LOGE(TAG, "接收错误, errno=%d", errno);
                break;
            }
        }
        
        client_connected = 0;
        client_socket = -1;
        close(new_sock);
        ESP_LOGI(TAG, "等待新连接...");
    }
    
    close(listen_sock);
    vTaskDelete(NULL);
}


// ==================== 主函数 ====================
void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, "ESP32 UART -> WiFi 转发模式启动");
    
    // 初始化 UART
    uart_init();
    
    // 初始化 WiFi 热点
    wifi_init_softap();
    
    // 创建 TCP 服务器任务
    xTaskCreate(tcp_server_task, "tcp_server", 6144, NULL, 8, NULL);
    ESP_LOGI(TAG, "TCP服务器任务创建成功");
    
    // // 创建定时发送任务
    xTaskCreate(send_task, "send_task", 2048, NULL, 6, NULL);
    ESP_LOGI(TAG, "定时发送任务创建成功");
    
    // 创建 UART 转发任务
    xTaskCreate(uart_to_wifi_task, "uart_to_wifi", 4096, NULL, 6, NULL);
    ESP_LOGI(TAG, "UART转发任务创建成功");
    
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "硬件连接:");
    ESP_LOGI(TAG, "  ESP32 GPIO17 -- USB转TTL RX");
    ESP_LOGI(TAG, "  ESP32 GPIO16 -- USB转TTL TX");
    ESP_LOGI(TAG, "  ESP32 GND    -- USB转TTL GND");
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "手机连接热点: %s", EXAMPLE_ESP_WIFI_SSID);
    ESP_LOGI(TAG, "TCP连接: 192.168.4.1:%d", TCP_PORT);
    ESP_LOGI(TAG, "========================================");
}

