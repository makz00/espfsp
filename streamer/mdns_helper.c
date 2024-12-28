/*
 * Home monitoring system
 * Author: Maksymilian Komarnicki
 */

#include "esp_err.h"
#include "esp_log.h"

#include "esp_netif.h"
#include <arpa/inet.h>
#include <netdb.h>

#include "mdns.h"

#include "mdns_helper.h"

// This should be configurable in later version
#define CONFIG_STREAMER_WAITTIME_FOR_SERVER_MILISEC 2000

static const char *TAG = "MDNS_HELPER";

esp_err_t wait_for_server(const char *host_name, struct esp_ip4_addr *addr, char *addr_str, int addr_str_len)
{
    while (1)
    {
        esp_err_t ret = mdns_query_a(host_name, CONFIG_STREAMER_WAITTIME_FOR_SERVER_MILISEC, addr);
        if (ret != ESP_OK)
        {
            if (ret == ESP_ERR_NOT_FOUND)
            {
                ESP_LOGI(TAG, "Host '%s' was not found. Trying again...", host_name);
                continue;
            }
            else
            {
                ESP_LOGE(TAG, "MDNS unknown error");
                return ESP_FAIL;
            }
        }

        ESP_LOGI(TAG, "Successfully found host '%s'", host_name);

        const char *s = inet_ntop(AF_INET, &addr->addr, addr_str, addr_str_len);
        if (s == NULL)
        {
            ESP_LOGE(TAG, "Unable to convert address from bin to text format : errno %d", errno);
            return ESP_FAIL;
        }

        ESP_LOGI(TAG, "Host has IP '%s'", addr_str);

        break;
    }

    return ESP_OK;
}
