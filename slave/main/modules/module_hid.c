#include "module_hid.h"
#include <string.h>

#define NAMESPACE_PREFIX "HID_"

bool module_hid_owns(const char *cmd)
{
    return strncmp(cmd, NAMESPACE_PREFIX, strlen(NAMESPACE_PREFIX)) == 0;
}

void module_hid_handle(const wackos_frame_t *request)
{
    /* TODO: HID_PRESS, HID_TYPE и т.п. Пока заглушка. */
    wackos_proto_send_err(request, WACKOS_ERR_NOT_SUPPORTED);
}
