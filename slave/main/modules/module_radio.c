#include "module_radio.h"
#include <string.h>

#define NAMESPACE_PREFIX "RADIO_"

bool module_radio_owns(const char *cmd)
{
    return strncmp(cmd, NAMESPACE_PREFIX, strlen(NAMESPACE_PREFIX)) == 0;
}

void module_radio_handle(const wackos_frame_t *request)
{
    /*
     * TODO: сюда встанет реальная реализация (RADIO_SCAN, RADIO_TX,
     * RADIO_RX и т.п.), см. пример долгих операций в docs/PROTOCOL.md
     * (REQ -> RES OK ACCEPTED -> поток EVT с результатами).
     * Пока namespace существует, но ни одна команда не реализована.
     */
    wackos_proto_send_err(request, WACKOS_ERR_NOT_SUPPORTED);
}
