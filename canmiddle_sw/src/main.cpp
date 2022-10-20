#include <Arduino.h>
#include <CAN.h>
#include <Preferences.h>

#include <pb_encode.h>
#include <pb_decode.h>

#include "message.pb.h"

Preferences pref;

enum Role: uint32_t {
  MASTER=1,
  SLAVE=2,
};

Role r;

void program_as(Role r) {
  pref.begin("canmiddle", false);
  int x = pref.getUInt("role", 0);
  Serial.printf("read role: %d, want: %d\r\n", x, r);
  if (x == r) {
    Serial.println("already programmed as required.");
    while(1){}
  }
  delay(5000);

  pref.clear();
  pref.putUInt("role", r);
  pref.end();

  Serial.printf("programmed as: %d\r\n", r);
  ESP.restart();
}

void setup() {
  Serial.begin(115200);
  Serial.println("serial port initialized");

  //program_as(SLAVE);

  pref.begin("canmiddle", true);
  r = static_cast<Role>(pref.getUInt("role", 0));

  Serial2.begin(9600);

  CAN.setPins(GPIO_NUM_26, GPIO_NUM_25);
  if (!CAN.begin(500E3)) {
    Serial.println("can init failed");
    while(1){};
  }
  Serial.println("can port ready");

  pinMode(LED_BUILTIN, OUTPUT);

  Serial.printf("running as role: %d\r\n", r);
  if (r == MASTER) {
    digitalWrite(LED_BUILTIN, 1);
  } else if (r == SLAVE) {
    digitalWrite(LED_BUILTIN, 0);
  } else {
    Serial.printf("role unknown: %d\n", r);
    while(1) {}
  }
}

// Sends msg on Serial2.
size_t send_can_message(const CanMessage &msg) {
  uint8_t buffer[255];
  bool status;
  size_t message_length;

  pb_ostream_t stream = pb_ostream_from_buffer(buffer, sizeof(buffer));

  status = pb_encode(&stream, CanMessage_fields, &msg);
  if (!status) {
    Serial.printf("Encoding failed: %s\r\n", PB_GET_ERROR(&stream));
    return 0;
  }
  message_length = stream.bytes_written;

  if (message_length <= 0) {
    Serial.println("zero length message");
    return 0;
  }
  if (message_length > 255) {
    Serial.println("message too long");
    return 0;
  }

  size_t bytes_written = Serial2.write(static_cast<uint8_t>(message_length));
  if (bytes_written < 1) {
    Serial.printf("write length unexpected: read %d bytes, want 1\r\n", bytes_written);
    return 0;
  }
  bytes_written = Serial2.write(buffer, message_length);
  if (bytes_written < message_length) {
    Serial.printf("write length unexpected: wrote %d bytes, want %d\r\n", bytes_written, message_length);
    return 0;
  }
  Serial2.flush(true);
  return message_length;
}

size_t read_can_message(CanMessage *msg) {
  uint8_t buffer[255];
  bool status;
  uint8_t message_length;

  size_t bytes_read;
  bytes_read = Serial2.read(&message_length, 1);
  if (bytes_read < 1) {
    Serial.printf("read length unexpected: read %d bytes, want 1\r\n", bytes_read);
    return 0;
  }
  bytes_read = Serial2.read(buffer, message_length);
  if (bytes_read < message_length) {
    Serial.printf("read length unexpected: read %d bytes, want %d\r\n", bytes_read, message_length);
    return 0;
  }

  pb_istream_t stream = pb_istream_from_buffer(buffer, message_length);
        
  status = pb_decode(&stream, CanMessage_fields, msg);

  return message_length;
}

void loop_slave() {
#if 0
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
#endif
  if (millis() % 1000 == 0) {
    CanMessage msg = CanMessage_init_zero;

    msg.prop = 0x07;
    msg.has_prop = true;
    msg.has_value = true;
    msg.value.size = 0x02;
    msg.value.bytes[0] = 'h';
    msg.value.bytes[1] = 'i';

    digitalWrite(LED_BUILTIN, 1);
    int cnt = send_can_message(msg);
    digitalWrite(LED_BUILTIN, 0);
    Serial.printf("sent message of %d bytes\r\n", cnt);

    delay(1);
  }
}

void loop_master() {
  if (Serial2.available()) {
    int cnt = 0;
    CanMessage msg;
    cnt = read_can_message(&msg);
    Serial.printf("read %d bytes\r\n", cnt);
  }
}

#if 0
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
#endif

void loop() {
  if (r == MASTER) {
    loop_master();
  } else {
    loop_slave();
  }
}