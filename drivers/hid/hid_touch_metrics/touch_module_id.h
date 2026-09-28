#ifndef __MODULE_ID_H
#define __MODULE_ID_H

#include "hid_touch_metrics.h"

/* module id*/
#define NT36523N_TG_INX 0
#define NT36523N_KD_HSD 5
#define ILI_9882T_KD_BOE 7
#define ILI_9882T_TG_HSD 3
#define FT8205_TG_INX 2
#define FT8205_KD_BOE 4
#define UNKNOWN_MODULE_ID 0xFF

void init_touch_module_info(void);

#endif //__MODULE_ID_H
