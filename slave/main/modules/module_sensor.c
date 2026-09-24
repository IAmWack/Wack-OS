#include "module_sensor.h"
#include <string.h>

#define NAMESPACE_PREFIX "SENSOR_"

bool module_sensor_owns(const char *cmd)
{
    return strncmp(cmd, NAMESPACE_PREFIX, strlen(NAMESPACE_PREFIX)) == 0;
}

void module_sensor_handle(const wackos_frame_t *request)
{
    /* TODO: SENSOR_READ и т.п. Пока заглушка. */
    wackos_proto_send_err(request, WACKOS_ERR_NOT_SUPPORTED);
}
