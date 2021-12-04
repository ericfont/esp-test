#include <stdio.h>
#include "esp_system.h"
#include "aoo/aoo.h"

void app_main(void)
{
    printf("aoo version: %d.%d.%d\n", AOO_VERSION_MAJOR, AOO_VERSION_MINOR, AOO_VERSION_BUGFIX);
}
