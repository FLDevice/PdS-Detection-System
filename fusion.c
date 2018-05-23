#include <stdio.h>
#include <string.h>    //strlen
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_wifi_types.h"
#include "esp_event.h"
#include "esp_event_loop.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_log.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "driver/gpio.h"

// WiFi Connection
#define SSID "FL-Lap"
#define PASSPHARSE "02M0157q"
#define MESSAGE "HelloTCPServer"
#define TCPServerIP "192.168.137.1"
// Packet Sniffing
#define	LED_GPIO_PIN			GPIO_NUM_4
#define	WIFI_CHANNEL_MAX		(13)
#define	WIFI_CHANNEL_SWITCH_INTERVAL	(500)

/***  Structs for packet sniffing***/

//BUFFER IN CUI SALVARE I DATI DA INVIARE AL SERVER
struct buffer {
	unsigned timestamp:32;
	unsigned channel:8;
	uint8_t seq_ctl[2];
	signed rssi:8;
	uint8_t addr[6];
	uint8_t ssid_length;
	uint8_t ssid[32];
};

struct buffer *buf;
int count = 0;

typedef struct {
	uint8_t frame_ctrl[2];
	uint8_t duration[2];
	//unsigned frame_ctrl:16;
	//unsigned duration_id:16;
	uint8_t addr1[6]; /* receiver address */
	uint8_t addr2[6]; /* sender address */
	uint8_t addr3[6]; /* filtering address */
	//unsigned sequence_ctrl:16;
	uint8_t seq_ctl[2];
	//uint8_t addr4[6]; /* optional */
} wifi_ieee80211_mac_hdr_t;

typedef struct {
	wifi_ieee80211_mac_hdr_t hdr;
	uint8_t payload[0]; /* network data ended with 4 bytes csum (CRC32) */
} wifi_ieee80211_packet_t;

//AGGIUNTO NEL CASO DI MANAGEMENT FRAMES
typedef struct {
	wifi_vendor_ie_type_t type;
	uint8_t length;
	vendor_ie_data_t data;
} wifi_ieee80211_ie_t;

/*** Elements for packet sniffing ***/

static wifi_country_t wifi_country = {.cc="CN", .schan=1, .nchan=13, .policy=WIFI_COUNTRY_POLICY_AUTO};

static EventGroupHandle_t wifi_event_group;
const int CONNECTED_BIT = BIT0;
static const char *TAG="tcp_client";

/** Packet sniffing functions **/

static esp_err_t event_handler(void *ctx, system_event_t *event);
static void wifi_sniffer_set_channel(uint8_t channel);
static void wifi_sniffer_packet_handler(void *buff, wifi_promiscuous_pkt_type_t type);
void my_wifi_init(void);

void wifi_connect(){
    wifi_config_t cfg = {
        .sta = {
            .ssid = SSID,
            .password = PASSPHARSE,
			.channel = 1,
        },
		/*.ap = {
			.ssid = "hotspotESP32",
			.channel = 1,
		},*/
    };
    ESP_ERROR_CHECK( esp_wifi_disconnect() );
    ESP_ERROR_CHECK( esp_wifi_set_config(ESP_IF_WIFI_STA, &cfg) );
    //ESP_ERROR_CHECK( esp_wifi_set_config(ESP_IF_WIFI_AP, &cfg) );
    ESP_ERROR_CHECK( esp_wifi_connect() );
}

static esp_err_t event_handler(void *ctx, system_event_t *event)
{
    switch(event->event_id) {
    case SYSTEM_EVENT_STA_START:
        wifi_connect();
        break;
    case SYSTEM_EVENT_STA_GOT_IP:
        xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
        break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
        esp_wifi_connect();
        xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
        break;
    default:
        break;
    }
    return ESP_OK;
}

void tcp_client(void *pvParam){
    ESP_LOGI(TAG,"tcp_client task started \n");
    struct sockaddr_in tcpServerAddr;
    tcpServerAddr.sin_addr.s_addr = inet_addr(TCPServerIP);
    tcpServerAddr.sin_family = AF_INET;
    tcpServerAddr.sin_port = htons( 3010 );
    int s, r;
    char recv_buf[64];
    while(1){
        xEventGroupWaitBits(wifi_event_group,CONNECTED_BIT,false,true,portMAX_DELAY);
        s = socket(AF_INET, SOCK_STREAM, 0);
        if(s < 0) {
            ESP_LOGE(TAG, "... Failed to allocate socket.\n");
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            continue;
        }
        ESP_LOGI(TAG, "... allocated socket\n");
         if(connect(s, (struct sockaddr *)&tcpServerAddr, sizeof(tcpServerAddr)) != 0) {
            ESP_LOGE(TAG, "... socket connect failed errno=%d \n", errno);
            close(s);
            vTaskDelay(4000 / portTICK_PERIOD_MS);
            continue;
        }
        ESP_LOGI(TAG, "... connected \n");
        if( write(s , MESSAGE , strlen(MESSAGE)) < 0)
        {
            ESP_LOGE(TAG, "... Send failed \n");
            close(s);
            vTaskDelay(4000 / portTICK_PERIOD_MS);
            continue;
        }
        ESP_LOGI(TAG, "... socket send success");
        do {
            bzero(recv_buf, sizeof(recv_buf));
            r = read(s, recv_buf, sizeof(recv_buf)-1);
            for(int i = 0; i < r; i++) {
                putchar(recv_buf[i]);
            }
        } while(r > 0);
        ESP_LOGI(TAG, "... done reading from socket. Last read return=%d errno=%d\r\n", r, errno);
        close(s);
        ESP_LOGI(TAG, "... new request in 5 seconds");
        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
    ESP_LOGI(TAG, "...tcp_client task closed\n");
}

void app_main()
{
  uint8_t level = 0, channel = 1;

  /*** SetUp Phase ***/
  // Initialize NVS flash storage with layout given in the partition table.
	esp_err_t ret = nvs_flash_init();
	if (ret == ESP_ERR_NVS_NO_FREE_PAGES) {
			ESP_ERROR_CHECK(nvs_flash_erase());
			ret = nvs_flash_init();
	}
	ESP_ERROR_CHECK( ret );

  ESP_ERROR_CHECK( esp_event_loop_init(event_handler, NULL) ); // Create a system Event task and initialize an application event’s callback function.

  wifi_event_group = xEventGroupCreate();

  /* WiFI SetUp and Initialization */
  my_wifi_init();

  wifi_sniffer_set_channel(1);

  // See FreeRTOS API - Create a new task and add it to the list of tasks that are ready to run
  // Arguments:
  // Pointer to function - Name of the task - Size of task stack - Parameters for the task
  // Priority of the task - Handle by which the created task can be referenced.
  xTaskCreate(&tcp_client,"tcp_client",4048,NULL,5,NULL);
}

void
my_wifi_init(void)
{
	/*** Init Wifi ***/
	tcpip_adapter_init();	// Creates an LwIP core task and initialize LwIP-related work.
	// Create the Wi-Fi driver task and initialize the Wi-Fi driver.
	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK( esp_wifi_init(&cfg) );

	/*** Configure WiFi ***/
	ESP_ERROR_CHECK( esp_wifi_set_country(&wifi_country) ); // Set country for channel range [1, 13]
	ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) );
	ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );

	/*** Start WiFi ***/
	ESP_LOGI(TAG,"Starting wifi \n");
	ESP_ERROR_CHECK( esp_wifi_start() );
	// Set promiscuous mode and register the RX callback function.
	// Each time a packet is received, the registered callback function will be called.
	esp_wifi_set_promiscuous(true);
	esp_wifi_set_promiscuous_rx_cb(&wifi_sniffer_packet_handler);
}

void wifi_sniffer_set_channel(uint8_t channel) {
	esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
}

void wifi_sniffer_packet_handler(void* buff, wifi_promiscuous_pkt_type_t type) {

	if (type != WIFI_PKT_MGMT)
		return;

	const wifi_promiscuous_pkt_t *ppkt = (wifi_promiscuous_pkt_t *)buff;
	const wifi_ieee80211_packet_t *ipkt = (wifi_ieee80211_packet_t *)ppkt->payload;
	const wifi_ieee80211_mac_hdr_t *hdr = &ipkt->hdr;

	if( hdr->frame_ctrl[0] != 0x40) //tengo solo i PROBE REQUEST
		return;

	buf = malloc(sizeof(struct buffer));

	buf->timestamp = ppkt->rx_ctrl.timestamp;
	buf->channel = ppkt->rx_ctrl.channel;
	buf->seq_ctl[0] = hdr->seq_ctl[0]; buf->seq_ctl[1] = hdr->seq_ctl[1];
	buf->rssi = ppkt->rx_ctrl.rssi;
	for (int j=0; j<6; j++)
		buf->addr[j] = hdr->addr2[j];
	buf->ssid_length = ipkt->payload[1];
	for (int i=0; i<buf->ssid_length; i++)
		buf->ssid[i] = (char)ipkt->payload[i+2];

	printf("%08d  PROBE  CHAN=%02d,  SEQ=%02x%02x,  RSSI=%02d, "
			" ADDR=%02x:%02x:%02x:%02x:%02x:%02x,  " ,
			buf->timestamp,
			buf->channel,
			buf->seq_ctl[0], buf->seq_ctl[1],
			buf->rssi,
			buf->addr[0],buf->addr[1],buf->addr[2],
			buf->addr[3],buf->addr[4],buf->addr[5]
	);
	printf("SSID=");
	for (int i=0; i<buf->ssid_length; i++)
		printf("%c", (char)buf->ssid[i]);
	printf("\n");

	count++;
	buf++;
}
