#include <EEPROM.h>
#include <LiquidCrystal.h>
#include <OneWire.h>

#define pin 11
#define RPin 12
#define GPin 13
OneWire iButton (pin);
byte Addr[8];
byte CalcCRC;

LiquidCrystal lcd(8, 9, 4, 5, 6, 7);

#define LCDLedPin 10
int8_t LCDLedState = HIGH;

byte KeyDataForWrite[8];
byte ReadIButtonKey[8];
byte RecoveryKeyID[8] = { 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x2F }; // "Универсальный" ключ. Прошивается последовательность 01:FF:FF:FF:FF:FF:FF:2F
byte EEPROMKeyReadID[8];
byte InputKeyManual[8];

uint8_t ch = 0;
String val = "";
uint8_t CellNumber = 1;
uint8_t StepCode = 0;
uint8_t AnswerCode = 0;
uint8_t EventWrite = 1; // 1- Recovery; 2 - EEPROM to iButton; 3 - iButton to iButton.

void setup() {
  Serial.begin(9600);
  randomSeed(analogRead(5));
  pinMode(RPin, OUTPUT);
  pinMode(GPin, OUTPUT);
  pinMode(LCDLedPin, OUTPUT);
  digitalWrite(LCDLedPin, LCDLedState);
  lcd.begin(16, 2);
  Serial.println( F("iButton Copier RW1990"));
  Serial.println();
  WriteIButton ();
  lcd.setCursor(0, 0);
  lcd.print(F(" iButton Copier"));
  lcd.setCursor(0, 1);
  lcd.print(F("     RW1990     "));
  delay (3000);
  LCDPrintHomeScreen();
}

//Автоматическое восстановление ключа при перезагрузке устройства.
void WriteIButton() {
  //delay(50);
  if (EventWrite != 1) {
    while (!iButton.search (Addr)) {
      iButton.reset_search();
      RedLight ();
    }
  }
  iButton.skip(); iButton.reset(); iButton.write(0x33);
  if (EventWrite == 1) Serial.println(F ("Auto recovery key ID on boot device..."));
  iButton.skip();
  iButton.reset();
  iButton.write(0xD1);
  digitalWrite(pin, LOW); pinMode(pin, OUTPUT); delayMicroseconds(60);
  pinMode(pin, INPUT); digitalWrite(pin, HIGH); delay(10);
  Serial.print("Write iButton ID: ");
  iButton.skip();
  iButton.reset();
  iButton.write(0xD5);
  if (EventWrite == 1) memcpy(KeyDataForWrite, RecoveryKeyID, 8);
  else if (EventWrite == 2) memcpy(KeyDataForWrite, EEPROMKeyReadID, 8);
  else if (EventWrite == 3) memcpy(KeyDataForWrite, ReadIButtonKey, 8);

  for (byte x = 0; x < 8; x++) {
    writeByte(KeyDataForWrite[x]);
    Serial.print(KeyDataForWrite[x], HEX);
    if (x < 7) {
      Serial.print(":");
    } /*else {
      Serial.print("   ");
    }*/
  }
  Serial.println();
  iButton.reset();
  iButton.write(0xD1);
  digitalWrite(pin, LOW); pinMode(pin, OUTPUT); delayMicroseconds(10);
  pinMode(pin, INPUT); digitalWrite(pin, HIGH); delay(10);
  EventWrite = 2;
  RedGreenLightOff ();
}

int writeByte(byte data) {
  int data_bit;
  for (data_bit = 0; data_bit < 8; data_bit++) {
    if (data & 1) {
      digitalWrite(pin, LOW); pinMode(pin, OUTPUT);
      delayMicroseconds(60);
      pinMode(pin, INPUT); digitalWrite(pin, HIGH);
      delay(10);
    } else {
      digitalWrite(pin, LOW); pinMode(pin, OUTPUT);
      pinMode(pin, INPUT); digitalWrite(pin, HIGH);
      delay(10);
    }
    data = data >> 1;
  }
  return 0;
}

void loop() {
  int x;
  x = analogRead (0);

  if (x < 60) { //RIGHT
    if   (StepCode == 2) {
      lcd.setCursor(0, 1);
      lcd.print(" ");
      lcd.setCursor(9, 1);
      lcd.print("*");
      AnswerCode = 1;
    } else {
      lcd.clear();
      LCDPrintWriteCell();
    }
  }
  else if (x < 200) { //UP
    EventWrite = 2;
    RedGreenLightOff ();
    if (StepCode == 2) {
      ResetStepAnswer();
    }
    if (CellNumber < 99) {
      CellNumber++;
      LCDPrintHomeScreen();
    }
  }
  else if (x < 400) { //DOWN
    EventWrite = 2;
    RedGreenLightOff ();
    if   (StepCode == 2) {
      ResetStepAnswer();
    }
    if (CellNumber > 1) {
      CellNumber--;
      LCDPrintHomeScreen();
    }
  }
  else if (x < 600) { //LEFT
    if (StepCode == 2) {
      lcd.setCursor(9, 1);
      lcd.print(" ");
      lcd.setCursor(0, 1);
      lcd.print("*");
      AnswerCode = 2;
    }
    else if  (StepCode == 0 && AnswerCode == 0) {
      if (LCDLedState == LOW)
        LCDLedState = HIGH;
      else
        LCDLedState = LOW;

      delay(300);
      digitalWrite(LCDLedPin, LCDLedState);
    }
    else lcd.clear();
  }
  else if (x < 800) { //SELECT
    if (StepCode == 2 && AnswerCode == 1) {
      ResetStepAnswer();
      LCDPrintHomeScreen();
    }
    else if  (StepCode == 2 && AnswerCode == 2) {
      ResetStepAnswer();
      EEPROMKeyWrite();
      LCDPrintHomeScreen();
    }
    else if  (StepCode == 0 && AnswerCode == 0) {
      lcd.setCursor(0, 0);
      lcd.print(F("                "));
      lcd.setCursor(0, 0);
      lcd.print(F("Attach the key"));
      WriteIButton();
    }
    else {
      ResetStepAnswer();
      lcd.clear();
    }
  }
  ReadIButton();
  if (Serial.available()) {
    while (Serial.available()) {
      ch = Serial.read();
      val += char(ch);
      delay(20);
    }
    ConsoleCommands();
    val = "";
  }
}


void ResetStepAnswer() {
  StepCode = 0;
  AnswerCode = 0;
}

void EEPROMKeyRead() {
  EEPROM.get((CellNumber * 10), EEPROMKeyReadID);
  Serial.print(F("Memory Cell: "));
  Serial.print(CellNumber);
  Serial.print(F(" -> iButton ID: "));
  for (byte x = 0; x < 8; x++) {
    Serial.print(EEPROMKeyReadID[x], HEX);
    if (x < 7)
    {
      Serial.print(":") ;
    }
    else {
      Serial.println();
    }
    lcd.setCursor(x * 2, 1);
    lcd.print (EEPROMKeyReadID[x], HEX);
  }
}

void EEPROMKeyWrite() {
  EEPROM.put((CellNumber * 10), ReadIButtonKey);
  Serial.print(F("Memory Cell: "));
  Serial.print(CellNumber);
  Serial.println(F(" -> iButton ID saved"));
  lcd.setCursor(0, 1);
  lcd.print(F("Saved in EEPROM"));
  delay(1000);
}

void LCDPrintHomeScreen() {
  lcd.clear();
  EEPROMKeyRead();
  LCDPrintReadCell();
}

void LCDPrintReadCell() {
  lcd.setCursor(0, 0);
  lcd.print(F("Selected cell: "));
  lcd.setCursor(14, 0);
  lcd.print(CellNumber);
  delay (200);
}

void LCDPrintWriteCell() {
  lcd.setCursor(0, 0);
  lcd.print(F("Write to cell: "));
  lcd.setCursor(14, 0);
  lcd.print(CellNumber);
  delay (200);
  StepCode = 1;
  LCDPrintYesNo();
}

void LCDPrintYesNo() {
  lcd.setCursor(0, 1);
  lcd.print(F(" Yes    "));
  lcd.setCursor(9, 1);
  lcd.print(F(" No     "));
  delay (200);
  StepCode = 2;
}

void ReadIButton() {
  while (!iButton.search (Addr)) {
    iButton.reset_search();
    delay(50);
    return;
  }
  Serial.print("Read iButton ID: ");
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Read iButton ID:");
  for (byte x = 0; x < 8; x++) {
    Serial.print(Addr[x], HEX);
    ReadIButtonKey[x] = (Addr[x]);
    if (x < 7) {
      Serial.print(":");
    } else {
      Serial.print("   ");
    }
    lcd.setCursor(x * 2, 1);
    lcd.print (ReadIButtonKey[x], HEX);
  }
  byte crc;
  crc = iButton.crc8(Addr, 7);
  Serial.print("CRC: ");
  Serial.println(crc, HEX);
  EventWrite = 3;
  GreenLight ();
}

void RedLight () {
  digitalWrite(RPin, HIGH);
  digitalWrite(GPin, LOW);
}
void GreenLight () {
  digitalWrite(RPin, LOW);
  digitalWrite(GPin, HIGH);
}

void RedGreenLightOff () {
  digitalWrite(RPin, LOW);
  digitalWrite(GPin, LOW);
}

// ----- Soft Reset
void(* Reset) (void) = 0;

// ----- Тестирование памяти EEPROM
void memtest() {
  uint8_t x;
  x = random(2, 255);
  Serial.print( F ("EEPROM: "));
  Serial.print(EEPROM.length());
  Serial.println( F (" Bytes"));
  Serial.print( F ("Test char hex value: "));
  Serial.println(x, HEX);
  for (uint16_t i = 0 ; i < EEPROM.length() ; i++) {
    EEPROM.write(i, x);
    delay(5);
  }
  uint16_t y;
  y = 0;
  for (uint16_t i = 0 ; i < EEPROM.length() ; i++) {
    if (EEPROM.read(i) != x) {
      //lineprint();
      Serial.print( F ("Failed cell: "));
      Serial.println(i + " -> " + EEPROM.read(i));
      y++;
    }
    delay(5);
  }
  Serial.print( F ("Test "));
  if (y == 0) {
    Serial.println( F ("passed."));
    delay(500);
    Serial.println();
    Reset();
  }
  if (y != 0) Serial.println( F ("failed."));
  Serial.println();
}

// ----- Обработка команд
void ConsoleCommands() {
  Serial.println(val);
  val.replace("\r", "");
  val.replace("\n", "");
  val.toLowerCase();
  if ((val.indexOf( F ("memtest")) > -1)) {
    memtest();
  }
  else if ((val.indexOf( F ("keyforcesave")) > -1)) {
    KeyForceSave(val);
  }
  else if ((val.indexOf( F ("keysave")) > -1)) {
    KeySave(val);
  }
}

void KeyForceSave (String val) {
  uint8_t CheckBeforeWrite = 1;
  lcd.clear();
  String Text = val.substring(String(val.lastIndexOf(":")).toInt() + 1);
  //Serial.println("Text = " + Text);
  String Cell = Text.substring(0, String(Text.indexOf("#")).toInt());
  //Serial.println("Cell = " + Cell);
  if (Cell.toInt() < 0 || Cell.toInt() > 99) CheckBeforeWrite = 0;
  String TextKeyData = Text.substring(String(Text.lastIndexOf("#")).toInt() + 1, String(Text.lastIndexOf("#")).toInt() + 17);
  //Serial.println("TextKeyData = " + TextKeyData);

  if (CheckBeforeWrite == 1) {
    byte ByteToWrieInEEPROM[8];
    char CharFromTextKeyData[16];
    TextKeyData.toCharArray(CharFromTextKeyData, 17);
    for (int i = 0; i < 8; i++)
      sscanf(&CharFromTextKeyData[i * 2], "%2x", &ByteToWrieInEEPROM[i]);
    //KeyForceSave:2#1234567890abcdef
    EEPROM.put((Cell.toInt() * 10), ByteToWrieInEEPROM);
    Serial.print(F("Memory Cell: "));
    Serial.print(Cell);
    Serial.print(F(" -> iButton ID "));
    for (byte x = 0; x < 8; x++) {
      Serial.print(ByteToWrieInEEPROM[x], HEX);
      if (x < 7) Serial.print(":") ;
    }
    Serial.println(F(" saved"));
    lcd.setCursor(0, 0);
    lcd.print(F("Saved in cell "));
    lcd.setCursor(14, 0);
    lcd.print(Cell);
    lcd.setCursor(0, 1);
    lcd.print(TextKeyData);
    delay(3000);
  } else {
    Serial.println(F("Error"));
  };
  LCDPrintHomeScreen();
}

void KeySave (String val) {
  uint8_t CheckBeforeWrite = 1;
  lcd.clear();
  String Text = val.substring(String(val.lastIndexOf(":")).toInt() + 1);
  //Serial.println("Text = " + Text);
  String Cell = Text.substring(0, String(Text.indexOf("#")).toInt());
  //Serial.println("Cell = " + Cell);
  if (Cell.toInt() < 0 || Cell.toInt() > 99) CheckBeforeWrite = 0;
  String TextKeyData = "01";
  TextKeyData += Text.substring(String(Text.lastIndexOf("#")).toInt() + 3, String(Text.lastIndexOf("#")).toInt() + 17);
  //Serial.println("TextKeyData = " + TextKeyData);
  if (CheckBeforeWrite == 1) {
    byte ByteToWrieInEEPROM[8];
    char CharFromTextKeyData[16];
    TextKeyData.toCharArray(CharFromTextKeyData, 17);
    for (int i = 0; i < 8; i++)
      sscanf(&CharFromTextKeyData[i * 2], "%2x", &ByteToWrieInEEPROM[i]);
    //KeySave:2#1234567890abcdef
    CalcCRC = crc8(ByteToWrieInEEPROM, 7);
    //Serial.print("CRC: ");
    //Serial.println(CalcCRC, HEX);
    ByteToWrieInEEPROM[7] = CalcCRC;
    EEPROM.put((Cell.toInt() * 10), ByteToWrieInEEPROM);
    Serial.print(F("Memory Cell: "));
    Serial.print(Cell);
    Serial.print(F(" -> iButton ID "));
    for (byte x = 0; x < 8; x++) {
      Serial.print(ByteToWrieInEEPROM[x], HEX);
      if (x < 7) Serial.print(":") ;
    }
    Serial.println(F(" saved"));
    lcd.setCursor(0, 0);
    lcd.print(F("Saved in cell "));
    lcd.setCursor(14, 0);
    lcd.print(Cell);
    lcd.setCursor(0, 1);
    lcd.print(TextKeyData);
    delay(3000);
  } else {
    Serial.println(F("Error"));
  };
  LCDPrintHomeScreen();
}


uint8_t crc8( uint8_t *addr, uint8_t len) {
  uint8_t crc = 0;

  for (uint8_t i = 0; i < len; i++)
  {
    uint8_t inbyte = addr[i];
    for (uint8_t j = 0; j < 8; j++)
    {
      uint8_t mix = (crc ^ inbyte) & 0x01;
      crc >>= 1;
      if (mix)
        crc ^= 0x8C;

      inbyte >>= 1;
    }
  }
  return crc;
}

