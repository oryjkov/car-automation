#include <Arduino.h>
#include <CAN.h>
#include <Preferences.h>

#include <pb_encode.h>
#include <pb_decode.h>

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
    digitalWrite(LED_BUILTIN, 1);
    Serial2.println("123");
    digitalWrite(LED_BUILTIN, 0);
    delay(1);
  }
}

void loop_master() {
  int cnt = 0;
  while (Serial2.available()) {
    cnt += 1;
    Serial2.read();
  }
  if (cnt > 0) {
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