/*
 * Copyright (c) 2020, Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <logging/log.h>
#include <rpmsg_lite.h>
#include "../../example_config.h"

LOG_MODULE_REGISTER(remote, LOG_LEVEL_DBG);

struct rpmsg_lite_instance * p_inst;
struct rpmsg_lite_endpoint * p_ept;

int rx_cb(void * payload, int payload_len, unsigned long src, void * priv)
{
    LOG_INF("Got %d bytes from 0x%x.", payload_len, (unsigned)src);

    char msg[] = "Message from remote";
    int retval = rpmsg_lite_send(p_inst, p_ept, MASTER_ADDR, msg, sizeof(msg), 0);
    if (retval != RL_SUCCESS)
    {
        LOG_INF("Failed to send %u bytes. Err code %d", sizeof(msg), retval);
    }
    else
    {
        LOG_INF("Sent message to 0x%x", MASTER_ADDR);
    }

    return RL_SUCCESS;
}

void main(void)
{
    LOG_INF("Starting remote");

    struct rpmsg_lite_instance static_ctx;
    p_inst = rpmsg_lite_remote_init((void *)SHARED_MEM_START, 0, 0, &static_ctx);
    if (!p_inst)
    {
        LOG_INF("Failed to create remote instance");
    }

    struct rpmsg_lite_ept_static_context static_ept;
    p_ept = rpmsg_lite_create_ept(p_inst, REMOTE_ADDR, rx_cb, NULL, &static_ept);
    if (!p_ept)
    {
        LOG_INF("Failed to create endpoint");
    }
    else
    {
        LOG_INF("Created endpoint with address: 0x%x", REMOTE_ADDR);
    }

    while (1)
    {
        k_sleep(1000);
    }
}
