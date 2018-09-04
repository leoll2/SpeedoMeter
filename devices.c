#include "devices.h"

int devices_create(void)
{   
    int ret;
    
    ret = dev_speed_create();
    if (ret)
        return ret;
    
    ret = dev_screen_create(dev_speed_get_ptr()->this_device);
    if (ret)
        return ret;
    
    return 0;
}

void devices_destroy(void)
{
    dev_screen_destroy();
    dev_speed_destroy();
}