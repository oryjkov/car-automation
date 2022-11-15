#ifndef __IO_ABSTRACTION_H__
#define __IO_ABSTRACTION_H__

#include "esp_abstraction.h"
#include "snoop_buffer.h"

#if defined(ARDUINO)
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

// Provides a way to send and receive messages via TWAI and UART busses.
struct IOAbstraction {
  // Callback for receiving messages.
  typedef void RecvCallback(QueueElement *);

  // Callback must free() its argument when done.
  IOAbstraction(RecvCallback *f, SnoopBuffer<LockAbstraction> *snoop_buffer);

  // Puts message on the UART tx queue.
  void SendOverUART(QueueElement *e);
  // Puts message on the TWAI tx queue.
  void SendOverTWAI(QueueElement *e);

  // Outgoing queues. Messages to be sent over TWAI or UART are placed here
  // to be handled from the corresponding TX task.
  QueueHandle_t twai_tx_queue;
  QueueHandle_t uart_tx_queue;
  // Incoming queue. All messages received on TWAI and UARt are placed here to be
  // delivered as callbacks from the event_loop task.
  QueueHandle_t rx_queue;

  // This callback is called for every incoming TWAI and UART message.
  // It is called from a separate thread.
  RecvCallback *recv_cb;

  SnoopBuffer<LockAbstraction> *snoop_buffer;
};

#endif  // defined(ARDUINO)

#endif  // __IO_ABSTRACTION_H__