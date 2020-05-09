#include "freertos/FreeRTOS.h"
#include "esp_wifi.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_event_loop.h"
#include "nvs_flash.h"
#include "freertos/portmacro.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "tcpip_adapter.h"

#include "lwip/err.h"
#include "string.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/api.h"

static EventGroupHandle_t wifi_event_group;
const int CONNECTED_BIT = BIT0;


bool isWifiEnabled;

const static char http_html_hdr[] =
    "HTTP/1.1 200 OK\r\nContent-type: text/html\r\n\r\n";
const static char http_index_hml[] ="<!DOCTYPE html>"
		"<html>"
		"<body>"
		"<form action=/IP>"
		"IP <input type=\"text\" placeholder=\"0.0.0.0\" name=\"IP\" required pattern=\"^([0-9]{1,3}\\.){3}[0-9]{1,3}$\" ><br>"
		"<input type=\"submit\" value=\"Submit\">"
		"</form>"
		"</body>"
		"</html>";


esp_err_t event_handler(void *ctx, system_event_t *event)
{
    switch(event->event_id) {

    case SYSTEM_EVENT_STA_START:
        esp_wifi_connect();
        break;

    case SYSTEM_EVENT_STA_GOT_IP:
    	ESP_LOGI("WIFI","SYSTEM_EVENT_STA_GOT_IP");
        xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
        isWifiEnabled = true;
        break;

    case SYSTEM_EVENT_STA_DISCONNECTED:
    	ESP_LOGI("WIFI","SYSTEM_EVENT_STA_DISCONNECTED");

    	isWifiEnabled = false;
        esp_wifi_connect();
        xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
        break;

    default:
        break;
    }
    return ESP_OK;
}

void initialise_wifi(void)
{
    tcpip_adapter_init();
    wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK( esp_event_loop_init(event_handler, NULL) );
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
    ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) );
    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
    wifi_config_t sta_config = {
        .sta = {
            .ssid = "somesh",
            .password = "1234567890",
            .bssid_set = false
        }
    };

    ESP_ERROR_CHECK( esp_wifi_set_config(WIFI_IF_STA, &sta_config) );
    ESP_ERROR_CHECK( esp_wifi_start() );
}

void webserver_action(struct netconn *conn)
{
  struct netbuf *inbuf;
  char *recBuffer;
  u16_t buflen;
  err_t err;

  err = netconn_recv(conn, &inbuf);

  if (err == ERR_OK) {

    netbuf_data(inbuf, (void**)&recBuffer, &buflen);
    recBuffer[buflen]='\0';
	 printf("recBuffer = %s\n", recBuffer);

    if ( strstr(recBuffer,"GET / HTTP") != NULL) {

      netconn_write(conn, http_html_hdr, sizeof(http_html_hdr)-1, NETCONN_NOCOPY);
	  netconn_write(conn, http_index_hml, sizeof(http_index_hml)-1, NETCONN_NOCOPY);
    }
    else if( strstr(recBuffer,"GET /IP") != NULL) {

    	netconn_write(conn, http_html_hdr, sizeof(http_html_hdr)-1, NETCONN_NOCOPY);
    	netconn_write(conn, "Thank you!!!", sizeof("Thank you!!!")-1, NETCONN_NOCOPY);
    }
  }

  netconn_close(conn);
  netbuf_delete(inbuf);
}

void webserver(void *pvParameters)
{
  struct netconn *conn, *newconn;
  err_t err;
  conn = netconn_new(NETCONN_TCP);
  netconn_bind(conn, NULL, 80);
  netconn_listen(conn);
  while( 1 ) {

 		err = netconn_accept(conn, &newconn);
 		if (err == ERR_OK) {

 			webserver_action(newconn);

 			netconn_delete(newconn);
 		}

 		while (  isWifiEnabled == false ) {

 			ESP_LOGI("HTTP SERVER","Waiting For wifi connection : %d", isWifiEnabled);
 			vTaskDelay(10);
 		}

 		vTaskDelay(1);
  }

   netconn_close(conn);
   netconn_delete(conn);
   vTaskDelete(NULL);
}

void app_main(void)
{
    nvs_flash_init();
    initialise_wifi();
    xTaskCreate(&webserver, "webserver", 2048, NULL, 5, NULL);
}
