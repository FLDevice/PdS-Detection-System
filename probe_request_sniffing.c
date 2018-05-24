

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

#include "esp_types.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "soc/timer_group_struct.h"
#include "driver/periph_ctrl.h"
#include "driver/timer.h"

#define TIMER_DIVIDER         16  //  Hardware timer clock divider
#define TIMER_SCALE           (TIMER_BASE_CLK / TIMER_DIVIDER)  // convert counter value to seconds
#define TIMER_INTERVAL0_SEC   (10.0) // sample test interval for the first timer
//#define TIMER_INTERVAL1_SEC   (5.78)   // sample test interval for the second timer
#define TEST_WITHOUT_RELOAD   0        // testing will be done without auto reload
#define TEST_WITH_RELOAD      1        // testing will be done with auto reload

#define	LED_GPIO_PIN			GPIO_NUM_4
#define	WIFI_CHANNEL_MAX		(13)
#define	WIFI_CHANNEL_SWITCH_INTERVAL	(500)

static wifi_country_t wifi_country = {.cc="CN", .schan=1, .nchan=13, .policy=WIFI_COUNTRY_POLICY_AUTO};

typedef struct {
    int type;  // the type of timer's event
    int timer_group;
    int timer_idx;
    uint64_t timer_counter_value;
} timer_event_t;

xQueueHandle timer_queue;

//BUFFER IN CUI SALVARE I DATI DA INVIARE AL SERVER
struct buffer {
	unsigned timestamp:32;
	unsigned channel:8;
	uint8_t seq_ctl[2];
	signed rssi:8;
	uint8_t addr[6];
	uint8_t ssid_length;
	uint8_t ssid[32];
	uint8_t crc[4];
};

struct buffer *buf;
int count = 0;
int flag_time = 0; //quando il timer scatta --> flag_time = 1  ==> stop sniffing

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

//       T     I     M     E     R     !     !     !     !     !
/*
 * A simple helper function to print the raw timer counter value
 * and the counter value converted to seconds
 */
static void inline print_timer_counter(uint64_t counter_value) {
    printf("Counter: 0x%08x%08x\n", (uint32_t) (counter_value >> 32), (uint32_t) (counter_value));
    printf("Time   : %.8f s\n", (double) counter_value / TIMER_SCALE);
}

/*
 * Timer group0 ISR handler
 *
 * Note:
 * We don't call the timer API here because they are not declared with IRAM_ATTR.
 * If we're okay with the timer irq not being serviced while SPI flash cache is disabled,
 * we can allocate this interrupt without the ESP_INTR_FLAG_IRAM flag and use the normal API.
 */
void IRAM_ATTR timer_group0_isr(void *para) {
    int timer_idx = (int) para;

    /* Retrieve the interrupt status and the counter value
       from the timer that reported the interrupt */
    uint32_t intr_status = TIMERG0.int_st_timers.val;
    TIMERG0.hw_timer[timer_idx].update = 1;
    uint64_t timer_counter_value =
        ((uint64_t) TIMERG0.hw_timer[timer_idx].cnt_high) << 32
        | TIMERG0.hw_timer[timer_idx].cnt_low;

    /* Prepare basic event data
       that will be then sent back to the main program task */
    timer_event_t evt;
    evt.timer_group = 0;
    evt.timer_idx = timer_idx;
    evt.timer_counter_value = timer_counter_value;

    /* Clear the interrupt
       and update the alarm time for the timer with without reload */
    if ((intr_status & BIT(timer_idx)) && timer_idx == TIMER_0) {
        evt.type = TEST_WITHOUT_RELOAD;
        TIMERG0.int_clr_timers.t0 = 1;
        timer_counter_value += (uint64_t) (TIMER_INTERVAL0_SEC * TIMER_SCALE);
        TIMERG0.hw_timer[timer_idx].alarm_high = (uint32_t) (timer_counter_value >> 32);
        TIMERG0.hw_timer[timer_idx].alarm_low = (uint32_t) timer_counter_value;
    } else if ((intr_status & BIT(timer_idx)) && timer_idx == TIMER_1) {
        evt.type = TEST_WITH_RELOAD;
        TIMERG0.int_clr_timers.t1 = 1;
    } else {
        evt.type = -1; // not supported even type
    }

    /* After the alarm has been triggered
      we need enable it again, so it is triggered the next time */
    TIMERG0.hw_timer[timer_idx].config.alarm_en = TIMER_ALARM_EN;

    /* Now just send the event data back to the main program task */
    xQueueSendFromISR(timer_queue, &evt, NULL);
}

/*
 * Initialize selected timer of the timer group 0
 *
 * timer_idx - the timer number to initialize
 * auto_reload - should the timer auto reload on alarm?
 * timer_interval_sec - the interval of alarm to set
 */
static void example_tg0_timer_init(int timer_idx, bool auto_reload, double timer_interval_sec)
{
    /* Select and initialize basic parameters of the timer */
    timer_config_t config;
    config.divider = TIMER_DIVIDER;
    config.counter_dir = TIMER_COUNT_UP;
    config.counter_en = TIMER_PAUSE;
    config.alarm_en = TIMER_ALARM_EN;
    config.intr_type = TIMER_INTR_LEVEL;
    config.auto_reload = auto_reload;
    timer_init(TIMER_GROUP_0, timer_idx, &config);

    /* Timer's counter will initially start from value below.
       Also, if auto_reload is set, this value will be automatically reload on alarm */
    timer_set_counter_value(TIMER_GROUP_0, timer_idx, 0x00000000ULL);

    /* Configure the alarm value and the interrupt on alarm. */
    timer_set_alarm_value(TIMER_GROUP_0, timer_idx, timer_interval_sec * TIMER_SCALE);
    timer_enable_intr(TIMER_GROUP_0, timer_idx);
    timer_isr_register(TIMER_GROUP_0, timer_idx, timer_group0_isr,
        (void *) timer_idx, ESP_INTR_FLAG_IRAM, NULL);

    timer_start(TIMER_GROUP_0, timer_idx);
}

/*
 * The main task of this example program
 */
static void timer_example_evt_task(void *arg)
{
    while (1) {
        timer_event_t evt;
        xQueueReceive(timer_queue, &evt, portMAX_DELAY);

        /* Print information that the timer reported an event */
        if (evt.type == TEST_WITHOUT_RELOAD) {
            printf("\n    Example timer without reload\n");
        } else if (evt.type == TEST_WITH_RELOAD) {
            printf("\n    Example timer with auto reload\n");
        } else {
            printf("\n    UNKNOWN EVENT TYPE\n");
        }
        printf("Group[%d], timer[%d] alarm event\n", evt.timer_group, evt.timer_idx);

        /* Print the timer values passed by event */
        printf("------- EVENT TIME --------\n");
        print_timer_counter(evt.timer_counter_value);

        /* Print the timer values as visible by this task */
        printf("-------- TASK TIME --------\n");
        uint64_t task_counter_value;
        timer_get_counter_value(evt.timer_group, evt.timer_idx, &task_counter_value);
        print_timer_counter(task_counter_value);

        flag_time = 1; //!!!!!!!!
    }
}
//      F     I     N     E                     T     I     M     E      R

void app_main(void) {
	uint8_t level = 0, channel = 1;

	//3 ISTRUZIONI GESTIONE TIMER
	timer_queue = xQueueCreate(1, sizeof(timer_event_t));
	example_tg0_timer_init(TIMER_0, TEST_WITHOUT_RELOAD, TIMER_INTERVAL0_SEC);
	xTaskCreate(timer_example_evt_task, "timer_evt_task", 2048, NULL, 5, NULL);

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

	if (flag_time == 1) {
		esp_wifi_set_promiscuous(false);
		printf("STOP SNIFFING\n");
		return;
	}

	if (type != WIFI_PKT_MGMT)
		return;

	const wifi_promiscuous_pkt_t *ppkt = (wifi_promiscuous_pkt_t *)buff;
	const wifi_ieee80211_packet_t *ipkt = (wifi_ieee80211_packet_t *)ppkt->payload;
	const wifi_ieee80211_mac_hdr_t *hdr = &ipkt->hdr;
	
	if( hdr->frame_ctrl[0] != 0x40) //tengo solo i PROBE REQUEST
		return;

	int packet_length = ppkt->rx_ctrl.sig_len;

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
	for (int i=0, j=packet_length-(buf->ssid_length); i<4; i++, j++)
			buf->crc[i] = ipkt->payload[j];

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
	printf("  CRC=");
	for (int i=0; i<4; i++)
		printf("%02x", buf->crc[i]);
	//printf("    %d", packet_length);
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



