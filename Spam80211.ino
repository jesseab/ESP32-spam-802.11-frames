#include <WiFi.h>
#include <esp_wifi.h>
#include "esp_private/wifi.h"

#define BEACON_SSID_OFFSET 38
#define SRCADDR_OFFSET 10
#define BSSID_OFFSET 16
#define SEQNUM_OFFSET 22
#define TOTAL_LINES (sizeof(rick_ssids) / sizeof(char *))

uint8_t beacon_raw[] = {
  0x80, 0x00,             // 0-1: Frame Control
  0x00, 0x00,             // 2-3: Duration
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff,       // 4-9: Destination address (broadcast)
  0xba, 0xde, 0xaf, 0xfe, 0x00, 0x06,       // 10-15: Source address
  0xba, 0xde, 0xaf, 0xfe, 0x00, 0x06,       // 16-21: BSSID
  0x00, 0x00,             // 22-23: Sequence / fragment number
  0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,     // 24-31: Timestamp (GETS OVERWRITTEN TO 0 BY HARDWARE)
  0x64, 0x00,             // 32-33: Beacon interval
  0x31, 0x04,             // 34-35: Capability info
  0x00, 0x00, /* FILL CONTENT HERE */       // 36-38: SSID parameter set, 0x00:length:content
  0x01, 0x08, 0x82, 0x84, 0x8b, 0x96, 0x0c, 0x12, 0x18, 0x24, // 39-48: Supported rates
  0x03, 0x01, 0x01,           // 49-51: DS Parameter set, current channel 1 (= 0x01),
  0x05, 0x04, 0x01, 0x02, 0x00, 0x00,       // 52-57: Traffic Indication Map
};

char *rick_ssids[] = {
  "01 Never gonna give you up",
  "02 Never gonna let you down",
  "03 Never gonna run around",
  "04 and desert you",
  "05 Never gonna make you cry",
  "06 Never gonna say goodbye",
  "07 Never gonna tell a lie",
  "08 and hurt you"
};

char ssid1[] = "Test";
char pass1[] = "testpassword";


void setup() {
  Serial.begin(115200);
  
  esp_netif_init();
  esp_event_loop_create_default();
  esp_netif_create_default_wifi_ap();

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

  esp_wifi_init(&cfg);
  esp_wifi_set_storage(WIFI_STORAGE_RAM);

  esp_wifi_set_mode(WIFI_MODE_AP);
  wifi_config_t ap_config = {
    .ap = {
      .ssid_len = 4,
      .channel = 1,
      .authmode = WIFI_AUTH_WPA2_PSK,
      .ssid_hidden = 0,
      .max_connection = 4,
      .beacon_interval = 60000,
      .pairwise_cipher = WIFI_CIPHER_TYPE_TKIP,
      .ftm_responder = 0
    }
  };

  // Workaround, had compilation problems
  memcpy(ap_config.ap.ssid, ssid1, sizeof(ssid1));
  memcpy(ap_config.ap.password, pass1, sizeof(pass1));

  esp_wifi_set_config(WIFI_IF_AP, &ap_config); // Each of these esp functions returns an error code that can be useful for debugging

  esp_wifi_start();
  esp_wifi_set_ps(WIFI_PS_NONE);

  xTaskCreate(&spam_task, "spam_task", 2048, NULL, 5, NULL);
}


void loop(){
  yield(); // Not sure if useful
}


void spam_task(void *pvParameter) {
  uint8_t line = 0;

  // Keep track of beacon sequence numbers on a per-songline-basis
  uint16_t seqnum[TOTAL_LINES] = { 0 };

  for (;;) {
    vTaskDelay(100 / TOTAL_LINES / portTICK_PERIOD_MS);

    // Insert line of Rick Astley's "Never Gonna Give You Up" into beacon packet
    Serial.printf("%i %i %s\r\n", strlen(rick_ssids[line]), TOTAL_LINES, rick_ssids[line]);

    uint8_t beacon_rick[200];
    memcpy(beacon_rick, beacon_raw, BEACON_SSID_OFFSET - 1);
    beacon_rick[BEACON_SSID_OFFSET - 1] = strlen(rick_ssids[line]);
    memcpy(&beacon_rick[BEACON_SSID_OFFSET], rick_ssids[line], strlen(rick_ssids[line]));
    memcpy(&beacon_rick[BEACON_SSID_OFFSET + strlen(rick_ssids[line])], &beacon_raw[BEACON_SSID_OFFSET], sizeof(beacon_raw) - BEACON_SSID_OFFSET);

    // Last byte of source address / BSSID will be line number - emulate multiple APs broadcasting one song line each
    beacon_rick[SRCADDR_OFFSET + 5] = line;
    beacon_rick[BSSID_OFFSET + 5] = line;

    // Update sequence number
    beacon_rick[SEQNUM_OFFSET] = (seqnum[line] & 0x0f) << 4;
    beacon_rick[SEQNUM_OFFSET + 1] = (seqnum[line] & 0xff0) >> 4;
    seqnum[line]++;
    if (seqnum[line] > 0xfff)
      seqnum[line] = 0;

    esp_wifi_80211_tx(WIFI_IF_AP, beacon_rick, sizeof(beacon_raw) + strlen(rick_ssids[line]), false);

    if (++line >= TOTAL_LINES)
      line = 0;
  }
}
