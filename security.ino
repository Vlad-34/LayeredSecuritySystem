#include <SPI.h>
#include <MFRC522.h>
#include <Keypad.h>

/* Pins
MFRC
  SDA  -> 53
  SCK  -> 52
  MOSI -> 51
  MISO -> 50
  RQ   ->
  GND  -> GND
  RST  -> 5
  3V3  -> 3V

Keypad 3x4
  R1 -> 13
  R2 -> 12
  R3 -> 11
  R4 -> 10
  C1 -> 9
  C2 -> 8
  C3 -> 7

Button
  + -> 4
  - -> GND

Buzzer
  + -> 3  
  - -> GND

LEDs
  +1 -> 2
  +2 -> 6
  -  -> GND
*/

const byte rst_pin = 5;
const byte ss_pin = 53;
MFRC522 mfrc522(ss_pin, rst_pin);  // Create MFRC522 instance

const byte keypad_rows = 4;
const byte keypad_cols = 3;
const byte row_pins[keypad_rows] = { 13, 12, 11, 10 };
const byte col_pins[keypad_cols] = { 9, 8, 7 };

// keypad LUT
const char keymap[keypad_rows][keypad_cols] = {
  { '1', '2', '3' },
  { '4', '5', '6' },
  { '7', '8', '9' },
  { '*', '0', '#' }
};
Keypad keypad = Keypad(makeKeymap(keymap), row_pins, col_pins, keypad_rows, keypad_cols);

const byte buzzer_pin = 3;
const byte button_pin = 4;

byte rfid_lock_led = 6;
byte keypad_lock_led = 2;

const byte passcode_size = 4;
char data[passcode_size + 1] = "";
char master[passcode_size + 1] = "1324";
byte data_count = 0;

volatile bool rfid_lock = true;
volatile bool keypad_lock = true;
bool hard_lock = false;
byte keypad_attempts = 0;

const long interval = 10000;

void rfid_auth() {
  // Look for new cards
  if (!mfrc522.PICC_IsNewCardPresent())
    return;

  // Select one of the cards
  if (!mfrc522.PICC_ReadCardSerial())
    return;

  String content = "";
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    content.concat(String(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " "));
    content.concat(String(mfrc522.uid.uidByte[i], HEX));
  }

  content.toUpperCase();
  if (content.substring(1) == "04 2E 4E 63 30 59 80") {  // Change here the UID of the card/cards that you want to give access
    rfid_lock = false;
    Serial.println("RFID bypassed!");
  } else {
    rfid_lock = true;
    Serial.println("Wrong RFID token!");
  }
}

void clear_data() {
  while (data_count != 0) {
    data[data_count--] = 0;
  }
}

void keypad_auth() {
  char current_key = keypad.getKey();  // build passcode
  if (current_key) {
    if (current_key == '*') {
      data[--data_count] = '\0';
      if (data_count < 0) {
        data_count = 0;
      }
    } else if (current_key == '#') {
      // strcpy(data, "");
      data[0] = '\0';
      data_count = 0;
    } else {
      data[data_count] = current_key;
      data_count++;
    }
    Serial.println(String(data_count) + ": " + data);
  }

  if (data_count == passcode_size) {  // check passcode
    if (!strcmp(data, master)) {
      keypad_lock = false;
      keypad_attempts = 0;
      Serial.println("Keypad bypassed!");
    } else {
      keypad_lock = true;
      keypad_attempts++;
      Serial.println(String(3 - keypad_attempts) + " attempts remaining. This incident will be reported!");
    }
    clear_data();
  }
}

void doorbell() {
  tone(buzzer_pin, 660);
  delay(500);
  tone(buzzer_pin, 550);
  delay(500);
  tone(buzzer_pin, 440);
  delay(500);
  noTone(buzzer_pin);
}

void alarm() {
  tone(buzzer_pin, 440);
  delay(250);
  tone(buzzer_pin, 880);
  delay(250);
  noTone(buzzer_pin);
}

void lock() {
  rfid_lock = true;
  keypad_lock = true;
}

void setup() {
  Serial.begin(9600);  // Initialize serial communications with the PC
  SPI.begin();         // Init SPI bus
  mfrc522.PCD_Init();  // Init MFRC522

  pinMode(buzzer_pin, OUTPUT);
  pinMode(button_pin, INPUT_PULLUP);
  pinMode(rfid_lock_led, OUTPUT);
  pinMode(keypad_lock_led, OUTPUT);
}

void loop() {
  if (!hard_lock) {
    rfid_auth();
    keypad_auth();
  }

  digitalWrite(rfid_lock_led, !rfid_lock);
  digitalWrite(keypad_lock_led, !keypad_lock);

  if (keypad_attempts == 3) {
    for (byte i = 0; i < 8; i++) {
      alarm();
    }
    keypad_attempts = 0;
    rfid_lock = true;
    keypad_lock = true;
    hard_lock = true;
  }

  if (!digitalRead(button_pin)) {
    doorbell();
  }

  if (!rfid_lock && !keypad_lock) {
    Serial.println("Access granted!");
    delay(10000);
    lock();
  }
}
