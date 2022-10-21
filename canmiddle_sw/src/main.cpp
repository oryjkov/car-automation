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
  // Sends on CAN and prints what it gets on CAN.
  DEBUGGER=3,
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
}

void setup() {
  Serial.begin(115200);
  Serial.println("serial port initialized");

  //program_as(SLAVE);

  pref.begin("canmiddle", true);
  r = static_cast<Role>(pref.getUInt("role", 0));

  Serial2.begin(500000);

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
  } else if (r == DEBUGGER) {
    digitalWrite(LED_BUILTIN, 0);
  } else {
    Serial.printf("role unknown: %d\n", r);
    while(1) {}
  }
}

// Sends msg on Serial2.
size_t send_can_serial(const CanMessage &msg) {
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
  if (!status) {
    Serial.printf("Decoding failed: %s\r\n", PB_GET_ERROR(&stream));
    return 0;
  }

  return message_length;
}

bool CAN_to_msg(CanMessage *msg) {
  if (CAN.packetExtended()) {
    msg->has_extended = true;
    msg->extended = true;
    Serial2.print("extended, ");
  }
  msg->has_prop = true;
  msg->prop = CAN.packetId();
  msg->has_value = true;
  msg->value.size = CAN.packetDlc();
  size_t bytes_read = 0;
  CAN.readBytes(msg->value.bytes, msg->value.size);
 
  return true;
}

void dump_msg(const CanMessage &msg) {
  if (msg.has_prop) {
    Serial.printf("prop: %d, ", msg.prop);
  }
  if (msg.has_extended) {
    Serial.printf("ext: %d, ", msg.extended);
  }
  if (msg.has_value) {
    Serial.printf("data_len: %d, data: [ ", msg.value.size);
    for (int i = 0; i < msg.value.size; i++) {
      Serial.printf("0x%x, ", msg.value.bytes[i]);
    }
    Serial.print("]");
  }
  Serial.println();
}

bool send_can_can(const CanMessage &msg) {
  if (!msg.has_prop) {
    Serial.println("can property is required");
    return false;
  }
  if (!msg.has_value) {
    Serial.println("can value is required");
    return false;
  }
  if (msg.value.size < 1  || msg.value.size > 8) {
    Serial.println("can value size is wrong");
    return false;
  }

  if (msg.extended) {
    CAN.beginExtendedPacket(msg.prop, msg.value.size, false);
  } else {
    CAN.beginPacket(msg.prop);
  }
  CAN.write(msg.value.bytes, msg.value.size);
  CAN.endPacket();

  return true;
}

void loop_slave() {
  int packetSize = CAN.parsePacket();

  if (packetSize > 0) {
    CanMessage msg = CanMessage_init_zero;
    CAN_to_msg(&msg);

    digitalWrite(LED_BUILTIN, 1-digitalRead(LED_BUILTIN));
    int cnt = send_can_serial(msg);
    digitalWrite(LED_BUILTIN, 1-digitalRead(LED_BUILTIN));
#if DEBUG
    Serial.print("message from CAN: ");
    dump_msg(msg);
#endif
  }

  if (Serial2.available()) {
    CanMessage msg;
    digitalWrite(LED_BUILTIN, 1-digitalRead(LED_BUILTIN));
    size_t cnt = read_can_message(&msg);
    digitalWrite(LED_BUILTIN, 1-digitalRead(LED_BUILTIN));
    if (cnt <= 0) {
      Serial.println("zero len message");
      return;
    }
    bool status = send_can_can(msg);
    if (!status) {
      Serial.println("can send failed");
      return;
    }
#if DEBUG
    Serial.print("message from Serial: ");
    dump_msg(msg);
#endif
  }
}

uint32_t gen_send_millis = 0;
void loop_debugger() {
  int packetSize = CAN.parsePacket();

  if (packetSize > 0) {
    CanMessage msg = CanMessage_init_zero;
    digitalWrite(LED_BUILTIN, 1-digitalRead(LED_BUILTIN));
    CAN_to_msg(&msg);
    digitalWrite(LED_BUILTIN, 1-digitalRead(LED_BUILTIN));

    Serial.print("message from CAN: ");
    dump_msg(msg);
  }

  if (millis() % 1000 == 0 && millis() != gen_send_millis) {
    CanMessage msg = CanMessage_init_zero;

    msg.prop = 0x11;
    msg.has_prop = true;
    msg.has_value = true;
    msg.value.size = 8;
    msg.value.bytes[0] = 'g';
    msg.value.bytes[1] = 'e';
    msg.value.bytes[2] = 'n';
    msg.value.bytes[3] = 'e';
    msg.value.bytes[4] = 'r';
    msg.value.bytes[5] = 'a';
    msg.value.bytes[6] = 't';
    msg.value.bytes[7] = 'o';

    digitalWrite(LED_BUILTIN, 1-digitalRead(LED_BUILTIN));
    bool status = send_can_can(msg);
    if (!status) {
      Serial.println("can send failed");
    } else {
      Serial.println("can send success");
    }
    digitalWrite(LED_BUILTIN, 1-digitalRead(LED_BUILTIN));

    gen_send_millis=millis();
  }
}

void loop_master() {
  int packetSize = CAN.parsePacket();

  if (packetSize > 0) {
    CanMessage msg = CanMessage_init_zero;

    digitalWrite(LED_BUILTIN, 1-digitalRead(LED_BUILTIN));
    CAN_to_msg(&msg);
    int cnt = send_can_serial(msg);
    digitalWrite(LED_BUILTIN, 1-digitalRead(LED_BUILTIN));
    Serial.print("message from CAN: ");
    dump_msg(msg);
  }

  if (Serial2.available()) {
    int cnt = 0;
    CanMessage msg;
    digitalWrite(LED_BUILTIN, 1-digitalRead(LED_BUILTIN));
    cnt = read_can_message(&msg);
    digitalWrite(LED_BUILTIN, 1-digitalRead(LED_BUILTIN));
    if (cnt <= 0) {
      Serial.println("zero len message");
      return;
    }
    bool status = send_can_can(msg);
    if (!status) {
      Serial.println("can send failed");
      return;
    }
    Serial.print("message from Serial: ");
    dump_msg(msg);
  }
}

void loop() {
  if (r == MASTER) {
    loop_master();
  } else if (r == SLAVE) {
    loop_slave();
  } else if (r == DEBUGGER) {
    loop_debugger();
  } else {
    while(1) {}
  }
}