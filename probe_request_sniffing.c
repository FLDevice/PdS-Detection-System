
#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "esp_wifi.h"
#include "esp_wifi_types.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_event_loop.h"
#include "nvs_flash.h"
#include "driver/gpio.h"

#define	LED_GPIO_PIN			GPIO_NUM_4
#define	WIFI_CHANNEL_MAX		(13)
#define	WIFI_CHANNEL_SWITCH_INTERVAL	(500)

static wifi_country_t wifi_country = {.cc="CN", .schan=1, .nchan=13, .policy=WIFI_COUNTRY_POLICY_AUTO};

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

static esp_err_t event_handler(void *ctx, system_event_t *event);
static void wifi_sniffer_init(void);
static void wifi_sniffer_set_channel(uint8_t channel);
static const char *wifi_sniffer_packet_type2str(wifi_promiscuous_pkt_type_t type);
static void wifi_sniffer_packet_handler(void *buff, wifi_promiscuous_pkt_type_t type);

void app_main(void) {
	uint8_t level = 0, channel = 1;

	/* setup */
	wifi_sniffer_init();
	gpio_set_direction(LED_GPIO_PIN, GPIO_MODE_OUTPUT);

	/* loop */
	while (true) {
		gpio_set_level(LED_GPIO_PIN, level ^= 1);
		vTaskDelay(WIFI_CHANNEL_SWITCH_INTERVAL / portTICK_PERIOD_MS);
		wifi_sniffer_set_channel(1); //seleziono solo il canale 1
		//wifi_sniffer_set_channel(channel);
		//channel = (channel % WIFI_CHANNEL_MAX) + 1;
    	}
}

esp_err_t event_handler(void *ctx, system_event_t *event) {
	return ESP_OK;
}

void wifi_sniffer_init(void) {
	nvs_flash_init();
    	tcpip_adapter_init();
    	ESP_ERROR_CHECK( esp_event_loop_init(event_handler, NULL) );
    	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
	ESP_ERROR_CHECK( esp_wifi_set_country(&wifi_country) ); /* set country for channel range [1, 13] */
	ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) );
    	ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_NULL) );
    	ESP_ERROR_CHECK( esp_wifi_start() );
	esp_wifi_set_promiscuous(true);
	esp_wifi_set_promiscuous_rx_cb(&wifi_sniffer_packet_handler);
}

void wifi_sniffer_set_channel(uint8_t channel) {
	esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
}

const char * wifi_sniffer_packet_type2str(wifi_promiscuous_pkt_type_t type) {
	switch(type) {
		case WIFI_PKT_MGMT: return "MGMT";
		case WIFI_PKT_DATA: return "DATA";
		default:
		case WIFI_PKT_MISC: return "MISC";
	}
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
	/*
	printf("%08d  PROBE  CHAN=%02d,  SEQ=%02x%02x,  RSSI=%02d, "
		" ADDR=%02x:%02x:%02x:%02x:%02x:%02x,  " ,
		ppkt->rx_ctrl.timestamp,
		ppkt->rx_ctrl.channel,
		hdr->seq_ctl[0], hdr->seq_ctl[1],
		ppkt->rx_ctrl.rssi,
		// ADDR2
		hdr->addr2[0],hdr->addr2[1],hdr->addr2[2],
		hdr->addr2[3],hdr->addr2[4],hdr->addr2[5]
	);
	printf("SSID=");
	c = 0;
	i = 2;
	while (((char)(ipkt->payload[i]) > ' ') && ((char)(ipkt->payload[i]) < '~') && (c < 32)) {
		printf("%c", (char)ipkt->payload[i]);
		i++;
		c++;
	}
	printf("\n"); */
}
