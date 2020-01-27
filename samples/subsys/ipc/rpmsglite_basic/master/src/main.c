/*
 * Copyright (c) 2020, Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <logging/log.h>
#include <rpmsg_lite.h>
#include "../../example_config.h"

LOG_MODULE_REGISTER(master, LOG_LEVEL_DBG);

int rx_cb(void * payload, int payload_len, unsigned long src, void * priv)
{
    LOG_INF("Got %d bytes from 0x%x.", payload_len, (unsigned)src);

    return RL_SUCCESS;
}

void main(void)
{
    LOG_INF("Starting master");

    struct rpmsg_lite_instance static_ctx;
    struct rpmsg_lite_instance * p_inst = rpmsg_lite_master_init((void*)SHARED_MEM_START,
                                                                 SHARED_MEM_SIZE,
                                                                 0,
                                                                 0,
                                                                 &static_ctx);

    if (!p_inst)
    {
        LOG_ERR("Failed to create master instance");
        return;
    }

    struct rpmsg_lite_ept_static_context static_ept;
    struct rpmsg_lite_endpoint * p_ept = rpmsg_lite_create_ept(p_inst,
                                                               MASTER_ADDR,
                                                               rx_cb,
                                                               NULL,
                                                               &static_ept);
    if (!p_ept)
    {
        LOG_ERR("Failed to create endpoint");
        return;
    }
    else
    {
        LOG_INF("Created endpoint with address: 0x%x", MASTER_ADDR);
    }

    // Wait for Remote to setup
    k_sleep(1000);

    char msg[] = "Message from master";
    int retval = rpmsg_lite_send(p_inst, p_ept, REMOTE_ADDR, msg, sizeof(msg), 0);
    if (retval != RL_SUCCESS)
    {
        LOG_ERR("Failed to send %u bytes. Err code %d", sizeof(msg), retval);
    }
    else
    {
        LOG_INF("Sent message to 0x%x", REMOTE_ADDR);
    }

    while (1)
    {
        k_sleep(1000);
    }
}
