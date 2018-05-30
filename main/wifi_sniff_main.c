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

#include "includes/sniffing.h"
#include "includes/wifi_settings.h"


/******************************************
 CONSTANTS, GLOBAL VARIABLES AND FUNCTIONS
*******************************************/

const int CONNECTED_BIT = BIT0;
const uint8_t CHANNEL_TO_SNIFF = 1;
static const char *TAG="tcp_client";
static const char *MEM_ERR="memory_error";

struct buffer *buf;
struct buffer_list *head, *curr;
uint8_t count = 0;
static wifi_country_t wifi_country = {.cc="CN", .schan=1, .nchan=13, .policy=WIFI_COUNTRY_POLICY_AUTO};
static EventGroupHandle_t wifi_event_group;

void wifi_sniffer_init(void);
static esp_err_t event_handler(void *ctx, system_event_t *event);
void wifi_connect();
void tcp_client(void *pvParam);
static void wifi_sniffer_packet_handler(void *buff, wifi_promiscuous_pkt_type_t type);
void print_proberequest(struct buffer* buf);
ssize_t send_probe_buffer(int sock);
void clear_probe_buffer(void);
void check_list_size();


/******************************************
			START OF THE PROGRAM
*******************************************/

void app_main()
{
  wifi_event_group = xEventGroupCreate();

  /* WiFI SetUp and Initialization */
  wifi_sniffer_init();
  esp_wifi_set_channel( CHANNEL_TO_SNIFF, WIFI_SECOND_CHAN_NONE);; //channel

  // See FreeRTOS API - Create a new task and add it to the list of tasks that are ready to run
  // Arguments:
  // Pointer to function - Name of the task - Size of task stack - Parameters for the task
  // Priority of the task - Handle by which the created task can be referenced.
  xTaskCreate(&tcp_client,"tcp_client",4048,NULL,5,NULL);
}


/******************************************
			INITIALIZATION
*******************************************/

void wifi_sniffer_init(void)
{
	esp_err_t ret = nvs_flash_init();
	if (ret == ESP_ERR_NVS_NO_FREE_PAGES) {
			ESP_ERROR_CHECK(nvs_flash_erase());
			ret = nvs_flash_init();
	}
	ESP_ERROR_CHECK( ret );

	/*** Init buffer list **/
	head = malloc(sizeof(struct buffer_list));
	if( head == NULL ) {
		ESP_ERROR_CHECK(nvs_flash_erase());
		ESP_LOGE(MEM_ERR,"Failed to allocate head of the buffer list\n");
		return;
	}
	head->next = NULL;
	curr = head;

	/*** Init Wifi ***/
	tcpip_adapter_init();	// Creates an LwIP core task and initialize LwIP-related work.
	// Create the Wi-Fi driver task and initialize the Wi-Fi driver.
	ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL)); // Create a system Event task and initialize an application event�s callback function.
	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_wifi_init(&cfg));

	/*** Configure WiFi ***/
	ESP_ERROR_CHECK(esp_wifi_set_country(&wifi_country)); // Set country for channel range [1, 13]
	ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

	/*** Start WiFi ***/
	ESP_LOGI(TAG,"Starting wifi\n");
	ESP_ERROR_CHECK(esp_wifi_start());
	// Set promiscuous mode and register the RX callback function.
	// Each time a packet is received, the registered callback function will be called.
	esp_wifi_set_promiscuous(true);
	esp_wifi_set_promiscuous_rx_cb(&wifi_sniffer_packet_handler);
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

void wifi_connect(){
    wifi_config_t cfg = {
        .sta = {
            .ssid = SSID,
            .password = PASSPHARSE,
						.channel = 1,
        },
    };
    ESP_ERROR_CHECK( esp_wifi_disconnect() );
    ESP_ERROR_CHECK( esp_wifi_set_config(ESP_IF_WIFI_STA, &cfg) );
    //ESP_ERROR_CHECK( esp_wifi_set_config(ESP_IF_WIFI_AP, &cfg) );
    ESP_ERROR_CHECK( esp_wifi_connect() );
}


/******************************************
			TCP CONNECTION
*******************************************/

void tcp_client(void *pvParam){
    ESP_LOGI(TAG,"tcp_client task started \n");
    struct sockaddr_in tcpServerAddr;
    tcpServerAddr.sin_addr.s_addr = inet_addr(TCPServerIP);
    tcpServerAddr.sin_family = AF_INET;
    tcpServerAddr.sin_port = htons(TCPServerPort);
    int s, r;
    char recv_buf[64];

    while(1){
    	xEventGroupWaitBits(wifi_event_group,CONNECTED_BIT,false,true,portMAX_DELAY);
			s = socket(AF_INET, SOCK_STREAM, 0);
			if(s < 0) {
				ESP_LOGE(TAG, "Failed to allocate socket");
				vTaskDelay(1000 / portTICK_PERIOD_MS);
				continue;
			}
			ESP_LOGI(TAG, "Socket allocated");
			 if(connect(s, (struct sockaddr *)&tcpServerAddr, sizeof(tcpServerAddr)) != 0) {
				ESP_LOGE(TAG, "Socket connect failed, errno=%d\n", errno);
				close(s);
				vTaskDelay(4000 / portTICK_PERIOD_MS);
				continue;
			}
			ESP_LOGI(TAG, "Connected to the server");
			/*
	        if( write(s , MESSAGE , strlen(MESSAGE)) < 0)
	        {
	            ESP_LOGE(TAG, "Send failed");
	            close(s);
	            vTaskDelay(4000 / portTICK_PERIOD_MS);
	            continue;
	        }*/

			char c = count + '0';
			if(write(s, &c, 1) != 1){
				ESP_LOGE(TAG, "Send of *count* failed");
				close(s);
				vTaskDelay(4000 / portTICK_PERIOD_MS);
				continue;
			}

			ssize_t send_res;
			if((send_res = send_probe_buffer(s)) < 0){
				ESP_LOGE(TAG, "Send of buffer failed");
				close(s);
				vTaskDelay(4000 / portTICK_PERIOD_MS);
				continue;
			}
			/* WARN!!! Does it require a lock? */
			clear_probe_buffer();
	    ESP_LOGI(TAG, "Socket send success");

	    if(send_res != 0){
				do {
					bzero(recv_buf, sizeof(recv_buf));
					r = read(s, recv_buf, PACKET_SIZE);
					printf("Received from server: ");
					/*for(int i = 0; i < r; i++) {
						putchar(recv_buf[i]);
					}*/
					print_proberequest((struct buffer *)recv_buf);
					printf("\n");
				} while(r < PACKET_SIZE);

				ESP_LOGI(TAG, "Done reading from socket. Last read return=%d errno=%d\r", r, errno);
	    }

	    close(s);
	    ESP_LOGI(TAG, "New request in 5 seconds");
	    /*for(int i = 0; i < count; i++)
	    	print_proberequest(&(buf[i]));*/
	    vTaskDelay(5000 / portTICK_PERIOD_MS);
  	}
  	ESP_LOGI(TAG, "tcp_client task closed");
}


/******************************************
			SNIFFING
*******************************************/

void wifi_sniffer_packet_handler(void* buff, wifi_promiscuous_pkt_type_t type)
{
	// If count == 20, stop sniffing
	/*if(count==20) {
		esp_wifi_set_promiscuous(false);
		return;
	}*/
	if (type != WIFI_PKT_MGMT)
		return;

	const wifi_promiscuous_pkt_t *ppkt = (wifi_promiscuous_pkt_t *)buff;
	const wifi_ieee80211_packet_t *ipkt = (wifi_ieee80211_packet_t *)ppkt->payload;
	const wifi_ieee80211_mac_hdr_t *hdr = &ipkt->hdr;

	if( hdr->frame_ctrl[0] != PROBE_REQUEST_SUBTYPE) //tengo solo i PROBE REQUEST
		return;

	int packet_length = ppkt->rx_ctrl.sig_len;

	//struct buffer* b = malloc(sizeof(struct buffer));
	struct buffer b;

	b.timestamp = ppkt->rx_ctrl.timestamp;
	b.channel = ppkt->rx_ctrl.channel;
	b.seq_ctl/*[0]*/ = hdr->seq_ctl/*[0]; b->seq_ctl[1] = hdr->seq_ctl[1]*/;
	b.rssi = ppkt->rx_ctrl.rssi;
	for (int j=0; j<6; j++)
		b.addr[j] = hdr->addr2[j];
	b.ssid_length = ipkt->payload[1];
	for (int i=0; i<b.ssid_length; i++)
		b.ssid[i] = (char)ipkt->payload[i+2];
	for (int i=0, j=packet_length-4; i<4; i++, j++)
		b.crc[i] = ipkt->payload[j];

	// Add buffer to buffer_list
	curr->data = b;

	print_proberequest(&(curr->data));
	struct buffer_list *temp = malloc(sizeof(struct buffer_list));
	if( temp != NULL ) {
		temp->next = NULL;
		curr->next = temp;
		curr = temp;

		count++;
	} else {
		ESP_LOGE(MEM_ERR,"Couldn't allocate new element of the buffer list.\n");
	}
}


/******************************************
			PROBE BUFFER HANDLING
*******************************************/

ssize_t send_probe_buffer(int sock){
	if (count == 0)
		return 0;

	struct buffer_list *ptr;
	for(ptr=head; ptr->next!=NULL; ptr = ptr->next) {
		if( write(sock, &(ptr->data), sizeof(struct buffer)) < 0 )
			return -1;
	}

	return 1;
}

void clear_probe_buffer(){
	struct buffer_list *ptr, *temp;

	//check_list_size(); // DEBUGGING: Prints the current count variable and the list size

	if( count==0 )
		return;

	/* If at least a packet was read, then empty the list*/
	ptr = head->next;
	memset( &(head->data), 0, sizeof(struct buffer));
	head->next = NULL;

	for(; ptr->next!=NULL;) {
		 temp = ptr->next;
		 free(ptr);
		 ptr = temp;
	}

	curr = head;

	count=0;
}


/******************************************
			DEBUGGING AND PRINTING FUNCTIONS
*******************************************/

/*
 *	Prints the sniffed probe request
 */
void print_proberequest(struct buffer* buf){
	printf("%08d  PROBE  CHAN=%02d,  SEQ=%04x,  RSSI=%02d, "
			" ADDR=%02x:%02x:%02x:%02x:%02x:%02x,  " ,
			buf->timestamp,
			buf->channel,
			buf->seq_ctl/*[0], buf->seq_ctl[1]*/,
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
	printf("\n");
}

/*
 * Debugging function used to test whether the length of the list is equal to
 * the count variable
 */
void check_list_size(){
	int list_size = 0;
	struct buffer_list *ptr = head;

	while(ptr->next != NULL) {
		list_size++;
		ptr = ptr->next;
	}

	printf("Count: %d\nList_size: %d\n", count, list_size);
}
