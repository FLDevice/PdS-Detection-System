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
#include "esp_timer.h"

#include "includes/sniffing.h"
#include "includes/wifi_settings.h"
#include "includes/timer.h"

/******************************************
 CONSTANTS, GLOBAL VARIABLES AND FUNCTIONS
*******************************************/

const char INIT_MSG_H[5]= "INIT";
const char READY_MSG_H[6] = "READY";
uint8_t mac_address[6];
int ready = 0;
int READY_PORT = 0;

const int CONNECTED_BIT = BIT0;
const uint8_t CHANNEL_TO_SNIFF = 1;
static const char *TAG="tcp_client";
static const char *MEM_ERR="memory_error";

struct buffer *buf;
struct buffer_list *head, *curr;
uint8_t count = 0;
static wifi_country_t wifi_country = {.cc="CN", .schan=1, .nchan=13, .policy=WIFI_COUNTRY_POLICY_AUTO};
static EventGroupHandle_t wifi_event_group;

/*** TIMER ***/
xQueueHandle timer_queue;

/*** Variables for Peterson's algorithm ***/
int in1 = false; // clear_probe_buffer
int in2 = false; // wifi_sniffer_packet_handler
int turn = 1; // 1: clear_probe_buffer | 2: wifi_sniffer_packet_handler

void esp_initialization();
void esp_is_ready();

void wifi_sniffer_init(void);
static esp_err_t event_handler(void *ctx, system_event_t *event);
void wifi_connect();
void tcp_client(void *pvParam);
static void wifi_sniffer_packet_handler(void *buff, wifi_promiscuous_pkt_type_t type);
void print_proberequest(struct buffer* buf);
ssize_t send_probe_buffer(int sock);
void clear_probe_buffer(void);
void check_list_size();
void tcp_client_timer_version();

/*
 * A simple helper function to print the raw timer counter value
 * and the counter value converted to seconds
 */
static void inline print_timer_counter(uint64_t counter_value)
{
    printf("Counter: 0x%08x%08x\n", (uint32_t) (counter_value >> 32),
                                    (uint32_t) (counter_value));
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
void IRAM_ATTR timer_group0_isr(void *para)
{
    int timer_idx = (int) para;

    /* Retrieve the interrupt status and the counter value
       from the timer that reported the interrupt */
    uint32_t intr_status = TIMERG0.int_st_timers.val;
    TIMERG0.hw_timer[timer_idx].update = 1;     // For TIMERG0 info, go to https://github.com/espressif/esp-idf/blob/221eced06daff783afde6e378f6f84a82867f758/components/soc/esp32/include/soc/timer_group_struct.h
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
static void example_tg0_timer_init(int timer_idx,
    bool auto_reload, double timer_interval_sec)
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
    printf("Timer started \n");
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

        tcp_client_timer_version();
    }
}




/******************************************
			START OF THE PROGRAM
*******************************************/

void app_main()
{
  wifi_event_group = xEventGroupCreate();

  /* WiFI SetUp and Initialization */
  wifi_sniffer_init();
  esp_wifi_set_channel( CHANNEL_TO_SNIFF, WIFI_SECOND_CHAN_NONE);; //channel

  timer_queue = xQueueCreate(10, sizeof(timer_event_t));
  //example_tg0_timer_init(TIMER_0, TEST_WITHOUT_RELOAD, TIMER_INTERVAL0_SEC);
  example_tg0_timer_init(TIMER_1, TEST_WITH_RELOAD,    TIMER_INTERVAL1_SEC);
  // See FreeRTOS API - Create a new task and add it to the list of tasks that are ready to run
  // Arguments:
  // Pointer to function - Name of the task - Size of task stack - Parameters for the task
  // Priority of the task - Handle by which the created task can be referenced.
  xTaskCreate(timer_example_evt_task, "timer_evt_task", 2048, NULL, 5, NULL);

  //xTaskCreate(&tcp_client,"tcp_client",4048,NULL,5,NULL);
}



/******************************************
			INITIALIZATION
*******************************************/

void wifi_sniffer_init(void)
{

	printf("wifi_sniffer_init\n");

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

}

static esp_err_t event_handler(void *ctx, system_event_t *event)
{
	bool check;
    switch(event->event_id) {
    case SYSTEM_EVENT_STA_START:
		printf("\nSYSTEM_EVENT_STA_START\n");
        wifi_connect();
        break;
    case SYSTEM_EVENT_STA_GOT_IP:
		printf("\nSYSTEM_EVENT_STA_GOT_IP\n");
        xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);

		//Initialization of the ESP --> connection with the server
		esp_initialization();
		//Check if the server is ready every 5 seconds
		while (!ready) {
			esp_is_ready();
			sleep(5000);
		}
		// Set promiscuous mode and register the RX callback function.
		// Each time a packet is received, the registered callback function will be called.
		esp_wifi_set_promiscuous(true);
		esp_wifi_set_promiscuous_rx_cb(&wifi_sniffer_packet_handler);
        break;
	case SYSTEM_EVENT_STA_CONNECTED:
		printf("\nSYSTEM_EVENT_STA_CONNECTED\n");
		break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
		printf("\nSYSTEM_EVENT_STA_DISCONNECTED\n");
    	esp_wifi_get_promiscuous(&check);
    	//if (check) {
    		esp_wifi_connect();
    		xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
    	//}
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

//                                                                ADDED BY ROBY
void esp_initialization() {
	printf("Waiting for initialization...\n");
	ESP_ERROR_CHECK(esp_efuse_mac_get_default(mac_address)); //getting MAC address and saving it in mac_address array
	printf("MAC ADDRESS:   ");
	for (int i = 0; i < 6; i++) {
		printf("%02x", mac_address[i]);
		if (i < 5)
			printf(":");
	}
	printf("\n");

	//GETTING INITIALIZATION CONFIRM BY SERVER
    struct sockaddr_in tcpServerAddr;
    tcpServerAddr.sin_addr.s_addr = inet_addr(TCPServerIP);
    tcpServerAddr.sin_family = AF_INET;
    tcpServerAddr.sin_port = htons(TCPServerPort);
    int s;

    for(int i = 0; i < 3; i++) {
		s = socket(AF_INET, SOCK_STREAM, 0);
		if(s < 0) {
			ESP_LOGE(TAG, "Failed to allocate socket");
			vTaskDelay(1000 / portTICK_PERIOD_MS);
			continue;
		}
		ESP_LOGI(TAG, "Socket allocated");
		 if (connect(s, (struct sockaddr *)&tcpServerAddr, sizeof(tcpServerAddr)) != 0) {
			ESP_LOGE(TAG, "Socket connect failed, errno=%d\n", errno);
			close(s);
			vTaskDelay(1000 / portTICK_PERIOD_MS);
			continue;
		}
		ESP_LOGI(TAG, "Connected to the server\n");

		//SENDING INIT MESSAGE TO THE SERVER
		if (write(s, INIT_MSG_H, 4) < 0){
			ESP_LOGE(TAG, "Sending of INIT failed\n");
			close(s);
			vTaskDelay(1000 / portTICK_PERIOD_MS);
			continue;
		}
		else
			ESP_LOGI(TAG, "INIT sent\n");

		//SENDING MAD ADDRESS TO THE SERVER
		if (write(s, mac_address, 6) < 0){
			ESP_LOGE(TAG, "Sending of MAC address failed\n");
			close(s);
			vTaskDelay(1000 / portTICK_PERIOD_MS);
			continue;
		}
		else
			ESP_LOGI(TAG, "MAC address sent\n");

		//READING THE INIT CONFIRM FROM THE SERVER
		char recv[8];
		if ((read(s, recv, 8)) < 0) {
			ESP_LOGE(TAG, "read failed\n");
			printf("recv: %s\n", recv);
			close(s);
			vTaskDelay(1000 / portTICK_PERIOD_MS);
			continue;
		}
		else if (strncmp(recv, "INIT", 4) == 0) {
			ESP_LOGI(TAG, "INIT confirm received from server\n");
			READY_PORT = (recv[4]-'0')*1000 + (recv[5]-'0')*100 + (recv[6]-'0')*10 + recv[7]-'0';
			printf("Ready port: %i \n", READY_PORT);
			close(s); //close
		}
		else{
			ESP_LOGI(TAG, "received something else\n");
			printf("recv: %s\n", recv);
		}
		close(s);
		break;
    }
}

void esp_is_ready() {
	struct sockaddr_in tcpServerAddr;
    tcpServerAddr.sin_addr.s_addr = inet_addr(TCPServerIP);
    tcpServerAddr.sin_family = AF_INET;
    tcpServerAddr.sin_port = htons(READY_PORT);
    int s;

	s = socket(AF_INET, SOCK_STREAM, 0);
	if(s < 0) {
		ESP_LOGE(TAG, "Failed to allocate socket");
		vTaskDelay(1000 / portTICK_PERIOD_MS);
		return;
	}
	ESP_LOGI(TAG, "Socket allocated");
	if (connect(s, (struct sockaddr *)&tcpServerAddr, sizeof(tcpServerAddr)) != 0) {
		ESP_LOGE(TAG, "Socket connect failed, errno=%d\n", errno);
		close(s);
		vTaskDelay(1000 / portTICK_PERIOD_MS);
		return;
	}
	ESP_LOGI(TAG, "Connected to the server\n");

	//SENDING READY REQUEST MESSAGE TO THE SERVER
	if (write(s, READY_MSG_H, 5) == -1){
		ESP_LOGE(TAG, "Sending of READY failed\n");
		close(s);
		vTaskDelay(1000 / portTICK_PERIOD_MS);
		return;
	}
	else
		ESP_LOGI(TAG, "READY sent\n");

	//READING THE READY STATE FROM THE SERVER
	char recv[5];
	if (read(s, recv, 5) == -1) {
		ESP_LOGE(TAG, "read failed\n");
		printf("%s\n", recv);
		close(s);
		vTaskDelay(1000 / portTICK_PERIOD_MS);
		return;
	}
	else if (strncmp(recv, "READY", 5) == 0) {
		ESP_LOGI(TAG, "READY state received from server\n");
		ready = 1;
	}
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

void tcp_client_timer_version(){
    ESP_LOGI(TAG,"tcp_client task started \n");
    struct sockaddr_in tcpServerAddr;
    tcpServerAddr.sin_addr.s_addr = inet_addr(TCPServerIP);
    tcpServerAddr.sin_family = AF_INET;
    tcpServerAddr.sin_port = htons(TCPServerPort);
    int s, r;
    char recv_buf[64];

    for(int i = 0; i < 3; i++) {
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
				vTaskDelay(1000 / portTICK_PERIOD_MS);
				continue;
			}
			ESP_LOGI(TAG, "Connected to the server");



			if(write(s, &INIT_MSG_H, 4) != 1){
				ESP_LOGE(TAG, "Send of INIT failed");
				close(s);
				vTaskDelay(1000 / portTICK_PERIOD_MS);
				continue;
			}
			else
				ESP_LOGE(TAG, "INIT sent");

			/*

			char c = count + '0';
			if(write(s, &c, 1) != 1){
				ESP_LOGE(TAG, "Send of *count* failed");
				close(s);
				vTaskDelay(1000 / portTICK_PERIOD_MS);
				continue;
			}

			ssize_t send_res;
			if((send_res = send_probe_buffer(s)) < 0){
				ESP_LOGE(TAG, "Send of buffer failed");
				close(s);
				vTaskDelay(1000 / portTICK_PERIOD_MS);
				continue;
			}*/

      /* Clear the probe buffer. In order to guarantee concurrency,
      *  Peterson's Algorithm is used.
      */
      in1 = true;
      turn = 2;
      while (in2 && turn == 2) // Start of the critical section
            ;
      clear_probe_buffer();
      ESP_LOGI(TAG, "Socket send success");
      in1 = false; // End of the critical section

	    /*if(send_res != 0){
				do {
					bzero(recv_buf, sizeof(recv_buf));
					r = read(s, recv_buf, PACKET_SIZE);
					printf("Received from server: ");  */
					/*for(int i = 0; i < r; i++) {
						putchar(recv_buf[i]);
					}*/
					/*print_proberequest((struct buffer *)recv_buf);
					printf("\n");
				} while(r < PACKET_SIZE);

				ESP_LOGI(TAG, "Done reading from socket. Last read return=%d errno=%d\r", r, errno);
	    }*/

	    close(s);
	    ESP_LOGI(TAG, "New request in 5 seconds");
      break;
    }

    ESP_LOGI(TAG, "tcp_client task closed");
}

/******************************************
			SNIFFING
*******************************************/

void wifi_sniffer_packet_handler(void* buff, wifi_promiscuous_pkt_type_t type)
{
	if (ready == 0) {
		return;
	}
	// If count == 20, stop sniffing
	/*if (count >= 10) {
		//esp_wifi_set_promiscuous(false);
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

  // Instead of using the time since boot, i register the time since the last connection with the server
  uint64_t timer_val;
  timer_get_counter_value(0, TIMER_1, &timer_val);
	b.timestamp = ((double) timer_val / TIMER_SCALE)*1000000;//ppkt->rx_ctrl.timestamp;

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

  /* In order to guarantee concurrency, Peterson's Algorithm is used.
  */
  in2 = true;
  turn = 1;
  while(in1 && turn == 1) // Start of the critical section
    ;

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

  in2 = false; // End of the critical section
}


/******************************************
			PROBE BUFFER HANDLING
*******************************************/

ssize_t send_probe_buffer(int sock){
	int buf_size = 0;
	int temp;
	if (count == 0)
		return 0;

	struct buffer_list *ptr;
	for(ptr=head; ptr->next!=NULL; ptr = ptr->next) {
		if( (temp = write(sock, &(ptr->data), sizeof(struct buffer))) < 0 )
			return -1;
		buf_size = buf_size + temp;
	}
	printf("Buffer size sent: %d \n", buf_size);
	return 1;
}

void clear_probe_buffer(){
	struct buffer_list *ptr, *temp;

	//check_list_size(); // DEBUGGING: Prints the current count variable and the list size

	if( count==0 )
		return;

	printf("%d Packets sniffed \n", count);

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

	count = 0;
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
