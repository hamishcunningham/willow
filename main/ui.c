#include "esp_err.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"

#include "config.h"
#include "shared.h"
#include "slvgl.h"

static const char *TAG = "WILLOW/UI";

void init_ui(void)
{
    if (ld == NULL) {
        ESP_LOGE(TAG, "lv_disp_t ld is NULL!!!!");
    } else {
        char *speech_rec_mode = config_get_char("speech_rec_mode", DEFAULT_SPEECH_REC_MODE);

        if (lvgl_port_lock(lvgl_lock_timeout)) {
            lv_obj_t *scr_act = lv_disp_get_scr_act(ld);
            lv_obj_t *lbl_hdr = lv_label_create(scr_act);
            btn_cancel = lv_btn_create(scr_act);
            lbl_btn_cancel = lv_label_create(btn_cancel);
            lbl_ln1 = lv_label_create(scr_act);
            lbl_ln2 = lv_label_create(scr_act);
            lbl_ln3 = lv_label_create(scr_act);
            lbl_ln4 = lv_label_create(scr_act);
            lbl_ln5 = lv_label_create(scr_act);
            lbl_sense = lv_label_create(scr_act);
            lv_obj_set_style_text_align(lbl_ln3, LV_TEXT_ALIGN_CENTER, 0);
            lv_label_set_recolor(lbl_ln4, true);
            lv_label_set_recolor(lbl_ln5, true);
            lv_obj_add_event_cb(scr_act, cb_scr, LV_EVENT_ALL, NULL);

#ifdef CONFIG_LV_USE_FS_POSIX
            // LV style
            static lv_style_t lv_st_big;
            lv_style_init(&lv_st_big);
            static lv_style_t lv_st_small;
            lv_style_init(&lv_st_small);

            // Willow colors
            lv_color_t lv_clr_willow = lv_color_hex(0x583759);
            lv_color_t lv_clr_willow_alt = lv_color_hex(0xfbe870);

            // Attach background color to screen
            lv_obj_set_style_bg_color(scr_act, lv_clr_willow, LV_PART_MAIN);

            // White text
            lv_obj_set_style_text_color(scr_act, lv_color_hex(0xffffff), LV_PART_MAIN);

            // Cancel button to alt with black text
            lv_obj_set_style_bg_color(btn_cancel, lv_clr_willow_alt, LV_PART_MAIN);
            lv_obj_set_style_text_color(btn_cancel, lv_color_hex(0x000000), LV_PART_MAIN);

            // LV font
            lv_style_set_text_font(&lv_st_small, &lv_font_montserrat_16); //enable lvgl font in config
            lv_obj_add_style(lbl_sense, &lv_st_small, 0);

            // Attach font to style
            lv_style_set_text_font(&lv_st_big, &lv_font_montserrat_24);

            // Set Willow style on objects
            lv_obj_add_style(lbl_hdr, &lv_st_big, 0);
            lv_obj_add_style(lbl_ln1, &lv_st_big, 0);
            lv_obj_add_style(lbl_ln2, &lv_st_big, 0);
            lv_obj_add_style(lbl_ln3, &lv_st_big, 0);
            lv_obj_add_style(lbl_ln4, &lv_st_big, 0);
            lv_obj_add_style(lbl_ln5, &lv_st_big, 0);
            lv_obj_add_style(lbl_btn_cancel, &lv_st_big, 0);
#endif

            lv_label_set_text_static(lbl_btn_cancel, "Cancel");
            lv_label_set_text_static(lbl_hdr, "Welcome to Willow!");
            lv_obj_add_flag(btn_cancel, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(lbl_ln1, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(lbl_ln2, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(lbl_ln3, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(lbl_ln4, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(lbl_ln5, LV_OBJ_FLAG_HIDDEN);
            lv_obj_align(btn_cancel, LV_ALIGN_BOTTOM_MID, 0, -10);
            lv_obj_align(lbl_btn_cancel, LV_ALIGN_CENTER, 0, 0);
            lv_obj_align(lbl_hdr, LV_ALIGN_TOP_MID, 0, 0);
            lv_obj_align(lbl_sense, LV_ALIGN_TOP_MID, 0, 30);
            lv_obj_align(lbl_ln1, LV_ALIGN_TOP_LEFT, 10, 50);
            lv_obj_align(lbl_ln2, LV_ALIGN_TOP_LEFT, 10, 80);
            lv_obj_align(lbl_ln3, LV_ALIGN_TOP_LEFT, 10, 110);
            lv_obj_align(lbl_ln4, LV_ALIGN_TOP_LEFT, 10, 140);
            lv_obj_align(lbl_ln5, LV_ALIGN_TOP_LEFT, 10, 170);
            lv_label_set_long_mode(lbl_ln2, LV_LABEL_LONG_SCROLL);
            lv_obj_set_width(lbl_ln1, 300);
            lv_obj_set_width(lbl_ln2, 300);
            lv_obj_set_width(lbl_ln3, 300);
            lv_obj_set_width(lbl_ln4, 300);
            lv_obj_set_width(lbl_ln5, 300);

            if (strcmp(speech_rec_mode, "Multinet") == 0) {
#if defined(WILLOW_SUPPORT_MULTINET)
                lv_label_set_text_static(lbl_ln3, "Starting up (local)...");
#else
                lv_label_set_text_static(lbl_ln3, "Multinet Not Supported");
#endif
            } else if (strcmp(speech_rec_mode, "WIS") == 0) {
                lv_label_set_text_static(lbl_ln3, "Starting up (server)...");
            }
            lv_obj_clear_flag(lbl_ln3, LV_OBJ_FLAG_HIDDEN);

            lvgl_port_unlock();
        }
        free(speech_rec_mode);
    }
}

void update_sensor_data(float temp_value, float humidity_value) {
    if (lvgl_port_lock(lvgl_lock_timeout)) {
        char sensor_text[50];
        snprintf(sensor_text, sizeof(sensor_text), "Temp: %.0f    Humidity: %.0f", temp_value, humidity_value);
        lv_label_set_text(lbl_sense, sensor_text);
        lv_obj_clear_flag(lbl_sense, LV_OBJ_FLAG_HIDDEN);
        lvgl_port_unlock();
    }
}

void ui_pr_err(char *ln3, char *ln4)
{
    if (ld == NULL) {
        ESP_LOGE(TAG, "display not initialized, writing error on console");
        if (ln3 != NULL) {
            ESP_LOGE(TAG, "%s", ln3);
        }
        if (ln4 != NULL) {
            ESP_LOGE(TAG, "%s", ln4);
        }
        return;
    }

    if (lvgl_port_lock(lvgl_lock_timeout)) {
        lv_obj_add_flag(lbl_ln1, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(lbl_ln2, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(lbl_ln5, LV_OBJ_FLAG_HIDDEN);

        if (ln3 == NULL) {
            lv_obj_add_flag(lbl_ln3, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_label_set_text(lbl_ln3, ln3);
            lv_obj_set_style_text_align(lbl_ln3, LV_TEXT_ALIGN_CENTER, 0);
            lv_obj_clear_flag(lbl_ln3, LV_OBJ_FLAG_HIDDEN);
        }
        if (ln4 == NULL) {
            lv_obj_add_flag(lbl_ln4, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_label_set_text(lbl_ln4, ln4);
            lv_obj_set_style_text_align(lbl_ln4, LV_TEXT_ALIGN_CENTER, 0);
            lv_obj_clear_flag(lbl_ln4, LV_OBJ_FLAG_HIDDEN);
        }

        lvgl_port_unlock();
    }
}
