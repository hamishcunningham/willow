#include "board.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "esp_random.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"
#include "sdkconfig.h"

#include "shared.h"
#include "slvgl.h"
#include "system.h"

// for sensor vals (read, send) ///
#include "driver/i2c.h"
#include "aht20.h"
#include "esp_http_client.h"
#include "ui.h"

#define I2C_MASTER_FREQ_HZ  100000
#define I2C_MASTER_SCL_IO   40  /*!< gpio number for I2C master clock  40 8*/
#define I2C_MASTER_SDA_IO   41  /*!< gpio number for I2C master data    41 18*/

i2c_bus_handle_t hdl_i2c_aht_bus;
aht20_dev_handle_t aht20 = NULL;
uint32_t temperature_raw;
uint32_t humidity_raw;
float temperature;
float humidity;
//////

static const char *TAG = "WILLOW/SYSTEM";
static const char *willow_hw_t[WILLOW_HW_MAX] = {
    [WILLOW_HW_UNSUPPORTED] = "HW-UNSUPPORTED",
    [WILLOW_HW_ESP32_S3_BOX] = "ESP32-S3-BOX",
    [WILLOW_HW_ESP32_S3_BOX_LITE] = "ESP32-S3-BOX-Lite",
    [WILLOW_HW_ESP32_S3_BOX_3] = "ESP32-S3-BOX-3",
};

i2c_bus_handle_t hdl_i2c_bus;
volatile bool restarting = false;

const char *str_hw_type(int id)
{
    if (id < 0 || id >= WILLOW_HW_MAX || !willow_hw_t[id]) {
        return "Invalid hardware type.";
    }
    return willow_hw_t[id];
}

static void set_hw_type(void)
{
#if defined(CONFIG_ESP32_S3_BOX_BOARD)
    hw_type = WILLOW_HW_ESP32_S3_BOX;
#elif defined(CONFIG_ESP32_S3_BOX_LITE_BOARD)
    hw_type = WILLOW_HW_ESP32_S3_BOX_LITE;
#elif defined(CONFIG_ESP32_S3_BOX_3_BOARD)
    hw_type = WILLOW_HW_ESP32_S3_BOX_3;
#else
    hw_type = WILLOW_HW_UNSUPPORTED;
#endif
    ESP_LOGD(TAG, "hardware type %d (%s)", hw_type, str_hw_type(hw_type));
}

static esp_err_t init_ev_loop()
{
    esp_err_t ret = esp_event_loop_create_default();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to initialize default event loop: %s", esp_err_to_name(ret));
    }
    return ret;
}

static void init_i2c(void)
{
    int ret = ESP_OK;
    i2c_config_t i2c_cfg = {
        .mode = I2C_MODE_MASTER,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 100000,
    };
    ret = get_i2c_pins(I2C_NUM_0, &i2c_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to get I2C pins");
    }
    hdl_i2c_bus = i2c_bus_create(I2C_NUM_0, &i2c_cfg);
}

static void init_i2c_aht(void)
{
    i2c_config_t i2c_conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = (gpio_num_t)I2C_MASTER_SDA_IO,
        .sda_pullup_en = GPIO_PULLUP_DISABLE,
        .scl_io_num = (gpio_num_t)I2C_MASTER_SCL_IO,
        .scl_pullup_en = GPIO_PULLUP_DISABLE,
        .master.clk_speed = 400000
    };

    i2c_param_config(I2C_NUM_1, &i2c_conf);
    hdl_i2c_aht_bus = i2c_bus_create(I2C_NUM_1, &i2c_conf);
}

void aht20_init(void)
{
    aht20_i2c_config_t i2c_conf = {
        .i2c_port = I2C_NUM_1,
        .i2c_addr = AHT20_ADDRRES_0,
    };
    aht20_new_sensor(&i2c_conf, &aht20);
    aht20_read_temperature_humidity(aht20, &temperature_raw, &temperature, &humidity_raw, &humidity);
    ESP_LOGI(TAG, "%-20s: %2.2f %%", "humidity is", humidity);
    ESP_LOGI(TAG, "%-20s: %2.2f degC", "temperature is", temperature);
}

char* extract_ip_address(const char* url) {
    const char* ip_start = strstr(url, "//");
    if (ip_start == NULL) {
        return NULL; // Invalid URL format
    }
    ip_start += 2; // Move pointer past "//"
    const char* ip_end = strchr(ip_start, ':');
    if (ip_end == NULL) {
        return NULL; // Invalid URL format
    }

    size_t ip_length = ip_end - ip_start;
    char* ip_address = (char*)malloc(ip_length + 1); // +1 for null terminator
    if (ip_address == NULL) {
        return NULL; // Memory allocation failed
    }
    strncpy(ip_address, ip_start, ip_length);
    ip_address[ip_length] = '\0'; // Null-terminate the string
    return ip_address;
}

void send_sensor_data_task(void *pvParameters) {
    char* ip_address = extract_ip_address(was_url);
    while (true) {
        aht20_read_temperature_humidity(aht20, &temperature_raw, &temperature, &humidity_raw, &humidity);
        char server_url[100];
        snprintf(server_url, sizeof(server_url), "http://%s:19093/update_sensors?temperature=%.2f&humidity=%.2f", ip_address, temperature, humidity);
        ESP_LOGI(TAG, "Sensor task: %s", server_url);
        esp_http_client_config_t config = {
            .url = server_url,
        };
        esp_http_client_handle_t client = esp_http_client_init(&config);
        esp_err_t err = esp_http_client_perform(client);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "HTTP GET request failed: %d", err);
        }
        esp_http_client_cleanup(client);
        update_sensor_data(temperature, humidity);
        vTaskDelay(30000 / portTICK_PERIOD_MS); // Send data every X ms
    }
    aht20_del_sensor(aht20);
    i2c_driver_delete(I2C_NUM_1);
}

void init_system(void)
{
    set_hw_type();
    init_i2c();
    init_i2c_aht();
    aht20_init();
    ESP_ERROR_CHECK(init_ev_loop());
}

void init_sensor_task(void)
{
    xTaskCreate(&send_sensor_data_task, "send_sensor_data_task", 4096, NULL, 0, NULL);
}

void restart_delayed(void)
{
    uint32_t delay = esp_random() % 9;
    if (delay < 3) {
        delay = 3;
    } else if (delay > 6) {
        delay = 6;
    }

    ESP_LOGI(TAG, "restarting after %" PRIu32 " seconds", delay);

    if (lvgl_port_lock(lvgl_lock_timeout)) {
        lv_label_set_text_fmt(lbl_ln4, "Restarting in %" PRIu32 " seconds", delay);
        lv_obj_clear_flag(lbl_ln4, LV_OBJ_FLAG_HIDDEN);
        lvgl_port_unlock();
    }

    delay *= 1000;
    vTaskDelay(delay / portTICK_PERIOD_MS);
    esp_restart();
}
