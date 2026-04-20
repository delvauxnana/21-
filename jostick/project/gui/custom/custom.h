#ifndef CUSTOM_H
#define CUSTOM_H

#ifdef __cplusplus
extern "C" {
#endif

#include "gui_guider.h"

void custom_init(lv_ui *ui);
void setup_scr_pid_tune(lv_ui *ui);
void custom_pid_tune_on_show(lv_ui *ui);
bool custom_pid_tune_handle_key(lv_ui *ui, uint32_t key, bool *request_back);

#ifdef __cplusplus
}
#endif

#endif /* CUSTOM_H */
