#include <Arduino.h>
#include <CAN.h>


void setup() {
  Serial.begin(115200);
  Serial.println("serial port initialized");
  Serial2.begin(9600);

  CAN.setPins(GPIO_NUM_26, GPIO_NUM_25);
  if (!CAN.begin(500E3)) {
    Serial.println("can init failed");
    while(1){};
  }
  Serial.println("can port ready");

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, 1);
}


void loop_slave() {
  int packetSize = CAN.parsePacket();

  if (packetSize) {
    Serial.print("received: ");
    Serial.println(packetSize);

    if (CAN.packetExtended()) {
      Serial2.print("extended, ");
    }
    Serial2.print("pkt id: ");
    Serial2.print(CAN.packetId());
    Serial2.print(", pkt len: ");
    Serial2.print(CAN.packetDlc());
    Serial2.print(", data: ");
    while(CAN.available()) {
      Serial2.printf("0x%x", CAN.read());
    }
    Serial2.println();
  }
  if (Serial2.available()) {
  }
}

void loop_master() {
  while (Serial2.available()) {
    Serial.print(Serial2.read());
  }
}

void loop_sender() {
  Serial.println("sending packet");
  CAN.beginPacket(0x12);
  CAN.write('h');
  CAN.write('e');
  CAN.write('l');
  CAN.write('l');
  CAN.write('o');
  CAN.endPacket();
  Serial.println("packet sent");
  delay(1000);
  digitalWrite(LED_BUILTIN, 1-digitalRead(LED_BUILTIN));
}

//#define MASTER

void loop() {
#ifdef MASTER
  loop_master();
#else
  loop_slave();
#endif
}