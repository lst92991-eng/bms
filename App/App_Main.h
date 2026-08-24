#ifndef APP_MAIN_H
#define APP_MAIN_H

#include <stdbool.h>

#ifndef BMS_ENGINEERING_BUILD
#define BMS_ENGINEERING_BUILD 0
#endif

void App_Main(bool boot_early_bq_safe);

#endif /* APP_MAIN_H */
