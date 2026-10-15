#include "dns_server.h"
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "dns_server";
#define DNS_PORT 53

#pragma pack(push, 1)
typedef struct {
    uint16_t id;
    uint16_t flags;
    uint16_t qdcount;
    uint16_t ancount;
    uint16_t nscount;
    uint16_t arcount;
} dns_header_t;
#pragma pack(pop)

static void dns_server_task(void *pvParameters) {
    uint8_t rx_buffer[512];
    uint8_t tx_buffer[512];
    struct sockaddr_in server_addr;
    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock < 0) {
        ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
        vTaskDelete(NULL);
        return;
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(DNS_PORT);

    if (bind(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
        close(sock);
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "DNS Captive Portal Server listening on port 53");

    while (1) {
        int len = recvfrom(sock, rx_buffer, sizeof(rx_buffer), 0,
                           (struct sockaddr *)&client_addr, &client_addr_len);
        if (len < (int)sizeof(dns_header_t)) {
            continue;
        }

        (void)rx_buffer;
        dns_header_t *resp_hdr = (dns_header_t *)tx_buffer;

        // Copy question section as-is
        memcpy(tx_buffer, rx_buffer, len);

        // Modify header for response
        resp_hdr->flags = htons(0x8180); // Response, Authoritative, Recursion Available
        resp_hdr->ancount = htons(1);    // 1 Answer
        resp_hdr->nscount = 0;
        resp_hdr->arcount = 0;

        int resp_len = len;

        // Append Answer Record (A record pointing to 192.168.4.1)
        // Name pointer to question domain name at offset 12 (0xC00C)
        tx_buffer[resp_len++] = 0xC0;
        tx_buffer[resp_len++] = 0x0C;

        // Type A (0x0001)
        tx_buffer[resp_len++] = 0x00;
        tx_buffer[resp_len++] = 0x01;

        // Class IN (0x0001)
        tx_buffer[resp_len++] = 0x00;
        tx_buffer[resp_len++] = 0x01;

        // TTL (60 seconds)
        tx_buffer[resp_len++] = 0x00;
        tx_buffer[resp_len++] = 0x00;
        tx_buffer[resp_len++] = 0x00;
        tx_buffer[resp_len++] = 0x3C;

        // Data length (4 bytes for IPv4)
        tx_buffer[resp_len++] = 0x00;
        tx_buffer[resp_len++] = 0x04;

        // IP Address: 192.168.4.1
        tx_buffer[resp_len++] = 192;
        tx_buffer[resp_len++] = 168;
        tx_buffer[resp_len++] = 4;
        tx_buffer[resp_len++] = 1;

        sendto(sock, tx_buffer, resp_len, 0,
               (struct sockaddr *)&client_addr, client_addr_len);
    }

    close(sock);
    vTaskDelete(NULL);
}

esp_err_t dns_server_start(void) {
    BaseType_t ret = xTaskCreate(dns_server_task, "dns_server", 4096, NULL, 5, NULL);
    return (ret == pdPASS) ? ESP_OK : ESP_FAIL;
}
