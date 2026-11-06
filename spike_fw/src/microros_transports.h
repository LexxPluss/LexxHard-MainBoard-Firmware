// Copyright 2021 Proyectos y Sistemas de Mantenimiento SL (eProsima).
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef LEXXHARD_MICROROS_TRANSPORT_H_
#define LEXXHARD_MICROROS_TRANSPORT_H_

#include <unistd.h>
#include <version.h>

#if ZEPHYR_VERSION_CODE >= ZEPHYR_VERSION(3,1,0)
#include <zephyr/device.h>
#else
#include <device.h>
#endif

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct {
    size_t fd;
    const struct device * uart_dev;
} lexxhard_transport_params_t;

#define LEXXHARD_MICRO_ROS_FRAMING_REQUIRED true
volatile static lexxhard_transport_params_t lexxhard_default_params = {.fd = 1};

#ifdef FW_HAS_MICROROS_REAL
#include <zephyr/sys/ring_buffer.h>

#define LEXXHARD_PORT_RING_SIZE 2048

/* One micro-ROS serial port: device, own ring buffers and the input ring object. */
typedef struct {
    const struct device * dev;
    char in_buffer[LEXXHARD_PORT_RING_SIZE];
    char out_buffer[LEXXHARD_PORT_RING_SIZE];
    struct ring_buf in_ringbuf;
} lexxhard_port_t;

extern lexxhard_port_t lexxhard_port_uart6;
extern lexxhard_port_t lexxhard_port_uart2;

bool lexxhard_port_open(struct uxrCustomTransport * transport);
bool lexxhard_port_close(struct uxrCustomTransport * transport);
size_t lexxhard_port_write(struct uxrCustomTransport * transport, const uint8_t * buf, size_t len, uint8_t * err);
size_t lexxhard_port_read(struct uxrCustomTransport * transport, uint8_t * buf, size_t len, int timeout, uint8_t * err);
#endif /* FW_HAS_MICROROS_REAL */

bool lexxhard_transport_open(struct uxrCustomTransport * transport);
bool lexxhard_transport_close(struct uxrCustomTransport * transport);
size_t lexxhard_transport_write(struct uxrCustomTransport* transport, const uint8_t * buf, size_t len, uint8_t * err);
size_t lexxhard_transport_read(struct uxrCustomTransport* transport, uint8_t* buf, size_t len, int timeout, uint8_t* err);

#ifdef __cplusplus
}
#endif

#endif //LEXXHARD_MICROROS_TRANSPORT_H_