#ifndef __IO_ABSTRACTION_H__
#define __IO_ABSTRACTION_H__

#include "esp_abstraction.h"

#if defined(ARDUINO)
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

struct IOAbstraction {
  // Callback for receiving messages.
  typedef void cb(QueueElement *);

  IOAbstraction(cb *f);

  // Puts message on the UART tx queue.
  void SendOverUART(QueueElement *e);
  // Puts message on the TWAI tx queue.
  void SendOverTWAI(QueueElement *e);

  // Outgoing queues.
  QueueHandle_t twai_tx_queue;
  QueueHandle_t uart_tx_queue;
  // Incoming queue.
  QueueHandle_t rx_queue;

  // This callback is called for every incoming TWAI and UART message.
  // It is called from a separate thread.
  cb *recv_cb;
};
#else
 
#endif  // defined(ARDUINO)

#endif  // __IO_ABSTRACTION_H__