#include <uxr/client/transport.h>

#include "microros_transports.h"
#include <version.h>

#if ZEPHYR_VERSION_CODE >= ZEPHYR_VERSION(3,1,0)
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/sys/printk.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/sys/ring_buffer.h>
#include <zephyr/posix/unistd.h>
#else
#include <zephyr.h>
#include <device.h>
#include <sys/printk.h>
#include <drivers/uart.h>
#include <sys/ring_buffer.h>
#include <posix/unistd.h>
#endif

#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#define RING_BUF_SIZE 2048
#define UART_NODE DT_NODELABEL(usart6)

char lexxhard_uart_in_buffer[RING_BUF_SIZE];
char lexxhard_uart_out_buffer[RING_BUF_SIZE];

struct ring_buf lexxhard_out_ringbuf, lexxhard_in_ringbuf;

// --- micro-ROS Serial Transport for Zephyr ---

static void uart_fifo_callback(const struct device * dev, void * args){
    while (uart_irq_update(dev) && uart_irq_is_pending(dev)) {
        if (uart_irq_rx_ready(dev)) {
            int recv_len;
            char buffer[64];
            size_t len = MIN(ring_buf_space_get(&lexxhard_in_ringbuf), sizeof(buffer));

            if (len > 0){
                recv_len = uart_fifo_read(dev, buffer, len);
                ring_buf_put(&lexxhard_in_ringbuf, buffer, recv_len);
            }

        }
    }
}


bool lexxhard_transport_open(struct uxrCustomTransport * transport){
    lexxhard_transport_params_t * params = (lexxhard_transport_params_t*) transport->args;

    params->uart_dev = DEVICE_DT_GET(UART_NODE);
    if (!params->uart_dev) {
        printk("Serial device not found\n");
        return false;
    }

    ring_buf_init(&lexxhard_in_ringbuf, sizeof(lexxhard_uart_in_buffer), lexxhard_uart_out_buffer);

    uart_irq_callback_set(params->uart_dev, uart_fifo_callback);

    /* Enable rx interrupts */
    uart_irq_rx_enable(params->uart_dev);

    return true;
}

bool lexxhard_transport_close(struct uxrCustomTransport * transport){
    (void) transport;
    // TODO: close serial transport here
    return true;
}

size_t lexxhard_transport_write(struct uxrCustomTransport* transport, const uint8_t * buf, size_t len, uint8_t * err){
    lexxhard_transport_params_t * params = (lexxhard_transport_params_t*) transport->args;

    for (size_t i = 0; i < len; i++)
    {
        uart_poll_out(params->uart_dev, buf[i]);
    }

    return len;
}

size_t lexxhard_transport_read(struct uxrCustomTransport* transport, uint8_t* buf, size_t len, int timeout, uint8_t* err){
    lexxhard_transport_params_t * params = (lexxhard_transport_params_t*) transport->args;

    size_t read = 0;
    int spent_time = 0;

    while(ring_buf_is_empty(&lexxhard_in_ringbuf) && spent_time < timeout){
        usleep(1000);
        spent_time++;
    }

    uart_irq_rx_disable(params->uart_dev);
    read = ring_buf_get(&lexxhard_in_ringbuf, buf, len);
    uart_irq_rx_enable(params->uart_dev);

    return read;
}

#ifdef FW_HAS_MICROROS_REAL
/* --- per-port transports (unit h): each port has its own rings and IRQ callback --- */

lexxhard_port_t lexxhard_port_uart6 = {.dev = DEVICE_DT_GET(DT_NODELABEL(usart6))};
lexxhard_port_t lexxhard_port_uart2 = {.dev = DEVICE_DT_GET(DT_NODELABEL(usart2))};

/* user_data of the IRQ callback is the port the callback was installed for */
static void port_irq_callback(const struct device * dev, void * user_data){
    lexxhard_port_t * port = (lexxhard_port_t *) user_data;

    while (uart_irq_update(dev) && uart_irq_is_pending(dev)) {
        if (uart_irq_rx_ready(dev)) {
            char buffer[64];
            const size_t len = MIN(ring_buf_space_get(&port->in_ringbuf), sizeof(buffer));

            if (len > 0){
                const int recv_len = uart_fifo_read(dev, buffer, len);
                if (recv_len > 0){
                    ring_buf_put(&port->in_ringbuf, buffer, recv_len);
                }
            }
        }
    }
}

bool lexxhard_port_open(struct uxrCustomTransport * transport){
    lexxhard_port_t * port = (lexxhard_port_t *) transport->args;

    if (!device_is_ready(port->dev)) {
        printk("Serial device not ready\n");
        return false;
    }

    ring_buf_init(&port->in_ringbuf, sizeof(port->in_buffer), (uint8_t *) port->in_buffer);

    uart_irq_callback_user_data_set(port->dev, port_irq_callback, port);
    uart_irq_rx_enable(port->dev);

    return true;
}

bool lexxhard_port_close(struct uxrCustomTransport * transport){
    lexxhard_port_t * port = (lexxhard_port_t *) transport->args;

    uart_irq_rx_disable(port->dev);
    return true;
}

size_t lexxhard_port_write(struct uxrCustomTransport * transport, const uint8_t * buf, size_t len, uint8_t * err){
    lexxhard_port_t * port = (lexxhard_port_t *) transport->args;

    for (size_t i = 0; i < len; i++)
    {
        uart_poll_out(port->dev, buf[i]);
    }

    return len;
}

size_t lexxhard_port_read(struct uxrCustomTransport * transport, uint8_t * buf, size_t len, int timeout, uint8_t * err){
    lexxhard_port_t * port = (lexxhard_port_t *) transport->args;

    size_t read = 0;
    int spent_time = 0;

    while(ring_buf_is_empty(&port->in_ringbuf) && spent_time < timeout){
        usleep(1000);
        spent_time++;
    }

    uart_irq_rx_disable(port->dev);
    read = ring_buf_get(&port->in_ringbuf, buf, len);
    uart_irq_rx_enable(port->dev);

    return read;
}
#endif /* FW_HAS_MICROROS_REAL */
