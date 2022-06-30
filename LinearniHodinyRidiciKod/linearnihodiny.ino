//Lineární hodiny
//Marek Firla

// připojení potřebných knihoven
//#include <Arduino.h> //knihovny použité při ladění programu
//#include <Wire.h>
#include <DS3231.h> //knihovna zajišťující funkci modulu reálného času
#include <TM1637Display.h> //knihovna pro ovládání displeje
#include <avr/sleep.h> //knihovny pro uspání Arduina
#include <avr/wdt.h>

// nastavení čísel propojovacích pinů
#define SLP_HOD 0
#define SLP_MIN 1
#define DOWN_TIME 2
#define SET_TIME 3
#define UP_TIME 4
#define CLK 5
#define DIO 6
#define DIR_HOD 7
#define STEP_HOD 8
#define DIR_MIN 9
#define STEP_MIN 10
#define KON_HOD 11
#define KON_MIN 12
#define BUZZER 13

TM1637Display display(CLK, DIO); // vytvoření instance displej z knihovny TM1637
DS3231 RTC; // inicializace RTC z knihovny

//deklarace proměnných
int Hodiny; // proměnná uchovávající aktuální Hodinu
int Minuty; // proměnná uchovávající aktuální minutu
bool h12; // logická proměnná pro nastavení 24 nebo 12 hodinového formátu
bool PM;  // logické proměnná pro rozlišení dopolední a odpoledního času
int HodinyPol; // proměnná uchovávající aktuální polohu hodinového ciferníku vůči indikátoru
int MinutyPol; // proměnná uchovávající aktuální polohu hodinového ciferníku vůči indikátoru
int HodinyAlarm; // proměnná uchovávající čas alarmu
int MinutyAlarm;
bool AlarmOvladani = false; //logická proměnná pro zapnutí nebo vypnutí alarmu
bool AlarmStav = false; //logická proměnná indikující stav zvukové signalizace

// proměnné sloužící pro ošetření zákmitů tlačítek
int enStiskSetTime = 1;      // proměnná povolení stisku
long stiskMilSetTime = 0;    // proměnná pro čas stisku
int stiskSetTime = LOW;      // proměnná pro kontrolu stisku
int stavTlacSetTime = 0;     // proměnná stavu tlačítka

int enStiskUpTime = 1;      // proměnná povolení stisku
long stiskMilUpTime = 0;    // proměnná pro čas stisku
int stiskUpTime = LOW;      // proměnná pro kontrolu stisku
int stavTlacUpTime = 0;     // proměnná stavu tlačítka

int enStiskDownTime = 1;      // proměnná povolení stisku
long stiskMilDownTime = 0;    // proměnná pro čas stisku
int stiskDownTime = LOW;      // proměnná pro kontrolu stisku
int stavTlacDownTime = 0;     // proměnná stavu tlačítka

int stavTlacKonHod = 0;       // proměnná stavu tlačítka
int stavTlacKonMin = 0;       // proměnná stavu tlačítka

int KrokyMin = 334;  //proměnná pro počet kroků motoru pro lineární posun o rozestup jednotlivých označení minut na ciferníku
int KrokyHod = 1672; //proměnná pro počet kroků motoru pro lineární posun o rozestup jednotlivých označení hodin na ciferníku

int Rychlost = 122; //přepočet na délku mezery mezi impulzy

int SetTimeMode = 0; //proměnná pro přepínání módů nastavení hodin

//hodnoty pro informativní nápisy na displeji
const uint8_t SEG_SET[] = {
  SEG_A | SEG_F | SEG_G | SEG_C | SEG_D ,           // S
  SEG_A | SEG_F | SEG_G | SEG_E | SEG_D ,           // E
  SEG_F | SEG_G | SEG_E | SEG_D ,                   // T
  0x00 ,
};

const uint8_t SEG_ON[] = {
  SEG_A | SEG_F | SEG_E | SEG_D | SEG_C | SEG_B ,   // O
  SEG_E | SEG_G | SEG_C | SEG_E ,                   // N
  0x00 ,
  0x00 ,
};

const uint8_t SEG_OFF[] = {
  SEG_A | SEG_F | SEG_E | SEG_D | SEG_C | SEG_B ,    // O
  SEG_E | SEG_F | SEG_A | SEG_G ,                   // F
  SEG_E | SEG_F | SEG_A | SEG_G ,                   // F
  0x00 ,
};

const uint8_t SEG_ALARM[] = {
  SEG_E | SEG_F | SEG_A | SEG_B | SEG_C | SEG_G ,   // A
  SEG_F | SEG_E | SEG_D ,                           // L
  SEG_E | SEG_F | SEG_A | SEG_B | SEG_C | SEG_G ,   // A
  0x00 ,
};

uint8_t PomVypis[] = { 0x00, 0x00, 0x00, 0x00 };

//Hodnoty pro formátování displeje
long BlikaniMil;
bool StavBlikani = true;
long OdpocetMil;
bool OdpocetStav = true;

// funkce pro vypnutí vnitřních hodin
ISR (WDT_vect)
{
  wdt_disable();
}

void setup() {
  // nastavení pinů jako výstupních
  pinMode(DIR_MIN, OUTPUT);
  pinMode(STEP_MIN, OUTPUT);
  pinMode(DIR_HOD, OUTPUT);
  pinMode(STEP_HOD, OUTPUT);
  pinMode(SLP_HOD, OUTPUT);
  pinMode(SLP_MIN, OUTPUT);
  pinMode(BUZZER, OUTPUT);

  // připojení obou řídících pinů na zem
  digitalWrite(DIR_HOD, LOW);
  digitalWrite(STEP_HOD, LOW);
  digitalWrite(DIR_MIN, LOW);
  digitalWrite(STEP_MIN, LOW);

  digitalWrite(SLP_HOD, LOW);
  digitalWrite(SLP_MIN, LOW);

  // nastavení pinů jako vstupních
  pinMode(SET_TIME, INPUT_PULLUP);
  pinMode(UP_TIME, INPUT_PULLUP);
  pinMode(DOWN_TIME, INPUT_PULLUP);
  pinMode(KON_HOD, INPUT_PULLUP);
  pinMode(KON_MIN, INPUT_PULLUP);

  RTC.setClockMode(false); //nastavení 24 hodinového formátu času

  //funkce pro hardwarové přerušení právě probíhajícího programu
  //na základě stisknutí tlačítka SET
  attachInterrupt(digitalPinToInterrupt(SET_TIME), Vzbudit, RISING);

  //Nastavení výchozí polohy
  HomeHod();
  HomeMin();
}

void loop() {
  Mode(); //kontrola stisku tlačítka SET

  Debounce (); //funkce pro odstrnění zákmitů u tlačítek

  if (SetTimeMode == 0)
  {
    Synchronizace(); //funkce pro synchronizaci aktuálního času a polohy ciferníků
    Alarm(); //funkce pro ověření spuštění alarmu

    if (Hodiny == HodinyPol && Minuty == MinutyPol && AlarmStav == false)
    {
      enStiskSetTime == 1;
      Uspat(); //přechod do režimu s nízkým odběrem proudu
    }
  }

  else
  {
    NastaveniCasu(); //funkce pro nastavení času a alarmu
  }

}

//ošetření zákmitu tlačítek
//pokud je stisk tlačítka zakázaný tak ho povol po uplynutí 200 milisekund
void Debounce () {
  if (enStiskSetTime == 0) {
    if (millis() - stiskMilSetTime > 200) {
      enStiskSetTime = 1;
    }
  }

  if (enStiskUpTime == 0) {
    if (millis() - stiskMilUpTime > 200) {
      enStiskUpTime = 1;
    }
  }

  if (enStiskDownTime == 0) {
    if (millis() - stiskMilDownTime > 200) {
      enStiskDownTime = 1;
    }
  }
}

//stisk tlačítka SET
//nastavení prostředního tlačítka
// pokud je zvuková signalizace aktivní vypni ji, pokud ne nastav aktuální čas hodin
void Mode() {
  stavTlacSetTime = digitalRead(SET_TIME);
  if (stavTlacSetTime == LOW & enStiskSetTime == 1)
  {
    if (AlarmStav == true)
    {
      AlarmStav = false;
      AlarmOvladani = false;
    }

    else
    {
      SetTimeMode = SetTimeMode + 1;
      OdpocetMil = millis();
    }

    stiskSetTime = HIGH;
  }
  if (stiskSetTime == HIGH) {
    stiskMilSetTime = millis();
    enStiskSetTime = 0;
    stiskSetTime = LOW;
  }
}

//funkce pro nastavení aktuálního času
void NastaveniCasu() {
  //Načtení potřebných hodnot
  if (SetTimeMode == 1)
  {
    Hodiny = RTC.getHour(h12, PM);
    Minuty = RTC.getMinute();
    display.setBrightness(7, true);
    //display.setBrightness(0x0f);
    display.clear();
    display.setSegments(SEG_SET);
    delay(1000);
    SetTimeMode = 2;
  }
  //Nastavení hodin
  if (SetTimeMode == 2)
  {
    Blikani();
    UpHodiny();
    DownHodiny();
    Odpocet();
  }
  //Nastavení minut
  if (SetTimeMode == 3)
  {
    Blikani();
    UpMinuty();
    DownMinuty();
    Odpocet();
  }
  //Uložení hodnot do RTC modulu
  if (SetTimeMode == 4)
  {
    RTC.setHour(Hodiny);
    RTC.setMinute(Minuty);
    RTC.setClockMode(false);
    display.showNumberDecEx(RTC.getHour(h12, PM), 0b01000000, true, 2, 0);
    display.showNumberDecEx(RTC.getMinute(), 0b01000000, true, 2, 2);
    delay(1000);
    display.clear();
    display.setSegments(SEG_ALARM);
    Hodiny = HodinyAlarm;
    Minuty = MinutyAlarm;
    delay(1000);
    SetTimeMode = 5;
  }
  //Nastavení alarmu - hodiny
  if (SetTimeMode == 5)
  {
    Blikani();
    UpHodiny();
    DownHodiny();
    Odpocet();
  }
  //Nastavení alarmu - minuty
  if (SetTimeMode == 6)
  {
    Blikani();
    UpMinuty();
    DownMinuty();
    Odpocet();
  }
  //Nastavení alarmu - zapnutí
  if (SetTimeMode == 7)
  {
    if (AlarmOvladani == false)
    {
      display.setSegments(SEG_OFF);
    }
    if (AlarmOvladani == true)
    {
      display.setSegments(SEG_ON);
    }
    ToggleOnOff();
    Odpocet();
  }
  //Uložení nastavení alarmu
  if (SetTimeMode == 8)
  {
    HodinyAlarm = Hodiny;
    MinutyAlarm = Minuty;
    display.showNumberDecEx(HodinyAlarm, 0b01000000, true, 2, 0);
    display.showNumberDecEx(MinutyAlarm, 0b01000000, true, 2, 2);
    delay(2000);
    display.clear();
    SetTimeMode = 0;
  }
}

//zvyšuje nastavení hodinové hodnoty
void UpHodiny() {
  stavTlacUpTime = digitalRead(UP_TIME);
  if (stavTlacUpTime == LOW & enStiskUpTime == 1)
  {
    Hodiny = Hodiny + 1;
    if (Hodiny > 23)
    {
      Hodiny = 0;
    }
    OdpocetMil = millis();
    stiskUpTime = HIGH;
  }
  if (stiskUpTime == HIGH) {
    stiskMilUpTime = millis();
    enStiskUpTime = 0;
    stiskUpTime = LOW;
  }
}

//snižuje nastavení hodinové hodnoty
void DownHodiny() {
  stavTlacDownTime = digitalRead(DOWN_TIME);
  if (stavTlacDownTime == LOW & enStiskDownTime == 1)
  {
    Hodiny = Hodiny - 1;
    if (Hodiny < 0)
    {
      Hodiny = 23;
    }
    OdpocetMil = millis();
    stiskDownTime = HIGH;
  }
  if (stiskDownTime == HIGH) {
    stiskMilDownTime = millis();
    enStiskDownTime = 0;
    stiskDownTime = LOW;
  }
}

//zvyšuje nastavení minutové hodnoty
void UpMinuty() {
  stavTlacUpTime = digitalRead(UP_TIME);
  if (stavTlacUpTime == LOW & enStiskUpTime == 1)
  {
    Minuty = Minuty + 1;
    if (Minuty > 59)
    {
      Minuty = 0;
    }
    OdpocetMil = millis();
    stiskUpTime = HIGH;
  }
  if (stiskUpTime == HIGH) {
    stiskMilUpTime = millis();
    enStiskUpTime = 0;
    stiskUpTime = LOW;
  }
}

//snižuje nastavení minutové hodnoty
void DownMinuty() {
  stavTlacDownTime = digitalRead(DOWN_TIME);
  if (stavTlacDownTime == LOW & enStiskDownTime == 1)
  {
    Minuty = Minuty - 1;
    if (Minuty < 0)
    {
      Minuty = 59;
    }
    OdpocetMil = millis();
    stiskDownTime = HIGH;
  }
  if (stiskDownTime == HIGH) {
    stiskMilDownTime = millis();
    enStiskDownTime = 0;
    stiskDownTime = LOW;
  }
}

//přepíná mezi zapnutím a vypnutím alarmu
void ToggleOnOff() {
  stavTlacDownTime = digitalRead(DOWN_TIME);
  if (stavTlacDownTime == LOW & enStiskDownTime == 1)
  {
    display.clear();
    AlarmOvladani = !AlarmOvladani;
    OdpocetMil = millis();
    stiskDownTime = HIGH;
  }
  if (stiskDownTime == HIGH) {
    stiskMilDownTime = millis();
    enStiskDownTime = 0;
    stiskDownTime = LOW;
  }

  stavTlacUpTime = digitalRead(UP_TIME);
  if (stavTlacUpTime == LOW & enStiskUpTime == 1)
  {
    display.clear();
    AlarmOvladani = !AlarmOvladani;
    OdpocetMil = millis();
    stiskUpTime = HIGH;
  }
  if (stiskUpTime == HIGH) {
    stiskMilUpTime = millis();
    enStiskUpTime = 0;
    stiskUpTime = LOW;
  }
}

//nastavení výchozí polohy hodinového ciferníku
void HomeHod() {
  digitalWrite(DIR_HOD, LOW);
  digitalWrite(SLP_HOD, HIGH);

  while (1)
  {
    stavTlacKonHod = digitalRead(KON_HOD);

    if (stavTlacKonHod == LOW)
    {
      HodinyPol = 0;
      digitalWrite(SLP_HOD, LOW);
      break;
    }
    digitalWrite(STEP_HOD, HIGH);
    delayMicroseconds(Rychlost);
    digitalWrite(STEP_HOD, LOW);
    delayMicroseconds(Rychlost);
  }
}

//nastavení výchozí polohy minutového ciferníku
void HomeMin() {
  digitalWrite(DIR_MIN, HIGH);
  digitalWrite(SLP_MIN, HIGH);

  while (1)
  {
    stavTlacKonMin = digitalRead(KON_MIN);

    if (stavTlacKonMin == LOW)
    {
      MinutyPol = 0;
      digitalWrite(SLP_MIN, LOW);
      break;
    }

    digitalWrite(STEP_MIN, HIGH);
    delayMicroseconds(Rychlost);
    digitalWrite(STEP_MIN, LOW);
    delayMicroseconds(Rychlost);
  }
}

//zajišťuje synchronizaci aktuálního a zobrazovaného času
void Synchronizace() {
  Hodiny = RTC.getHour(h12, PM);
  Minuty = RTC.getMinute();

  if (Hodiny > 12)
  {
    Hodiny = RTC.getHour(h12, PM) - 12;
  }

  if (Hodiny > HodinyPol)
  {
    digitalWrite(DIR_HOD, HIGH);
    digitalWrite(SLP_HOD, HIGH);
    delay(1);
    while (Hodiny > HodinyPol)
    {
      HodPosun();
    }
    digitalWrite(SLP_HOD, LOW);
  }

  if (Minuty > MinutyPol)
  {
    digitalWrite(DIR_MIN, LOW);
    digitalWrite(SLP_MIN, HIGH);
    delay(1);
    while (Minuty > MinutyPol)
    {
      MinPosun();
    }
    digitalWrite(SLP_MIN, LOW);
  }

  if (Hodiny < HodinyPol)
  {
    HomeHod();
  }

  if (Minuty < MinutyPol)
  {
    HomeMin();
  }
}

//posun o 1 minutu
void MinPosun() {
  for (int i = 0; i <= KrokyMin; i++)
  {
    digitalWrite(STEP_MIN, HIGH);
    delayMicroseconds(Rychlost);
    digitalWrite(STEP_MIN, LOW);
    delayMicroseconds(Rychlost);
  }
  MinutyPol = MinutyPol + 1;
}

//posun o 1 hodinu
void HodPosun() {
  for (int i = 0; i <= KrokyHod; i++)
  {
    digitalWrite(STEP_HOD, HIGH);
    delayMicroseconds(Rychlost);
    digitalWrite(STEP_HOD, LOW);
    delayMicroseconds(Rychlost);
  }
  HodinyPol = HodinyPol + 1;
}

//Mód šetření energií
void Uspat() {
  digitalWrite(DIR_HOD, LOW);
  digitalWrite(STEP_HOD, LOW);
  digitalWrite(DIR_MIN, LOW);
  digitalWrite(STEP_MIN, LOW);
  digitalWrite(SLP_HOD, LOW);
  digitalWrite(SLP_MIN, LOW);
  display.clear();
  // vypne ADC
  ADCSRA = 0;
  // clear various "reset" flags
  MCUSR = 0;
  // allow changes, disable reset
  WDTCSR = bit (WDCE) | bit (WDE);
  // Nastavení přerušení a časový interval
  WDTCSR = bit (WDIE) | bit (WDP3) | bit (WDP0);    // nastav WDIE, a 8 sekundový delay
  wdt_reset();
  set_sleep_mode (SLEEP_MODE_PWR_DOWN);
  noInterrupts ();
  sleep_enable();
  attachInterrupt (0, Vzbudit, FALLING);
  EIFR = bit (INTF0);
  MCUCR = bit (BODS) | bit (BODSE);
  MCUCR = bit (BODS);
  interrupts ();
  sleep_cpu ();

  sleep_disable();
}

// vypnout mód pro šetření energií
void Vzbudit()
{
  sleep_disable();
  detachInterrupt (0);
}

//podprogram zajišťující problikávání právně nastavované hodnoty na displeji
void Blikani() {
  if (StavBlikani)
  {
    BlikaniMil = millis();
    StavBlikani = false;
  }

  if ((millis() - BlikaniMil) < 500)
  {
    if (SetTimeMode == 2 || SetTimeMode == 5)
    {
      PomVypis[0] = 0x00;
      PomVypis[1] = 0x00;
      PomVypis[2] = display.encodeDigit(Minuty / 10);
      PomVypis[3] = display.encodeDigit(Minuty % 10);
      display.setSegments(PomVypis);
    }
    if (SetTimeMode == 3 || SetTimeMode == 6)
    {
      PomVypis[0] = display.encodeDigit(Hodiny / 10);
      PomVypis[1] = display.encodeDigit(Hodiny % 10);
      PomVypis[2] = 0x00;
      PomVypis[3] = 0x00;
      display.setSegments(PomVypis);
    }
  }
  if ((millis() - BlikaniMil) > 500)
  {
    display.showNumberDecEx(Hodiny, 0b01000000, true, 2, 0);
    display.showNumberDecEx(Minuty, 0b01000000, true, 2, 2);

    if ((millis() - BlikaniMil) > 1000)
    {
      StavBlikani = true;
    }
  }
}

//vypne režim pro nastavení aktuálního času při absenci vstupu, která trvá déle než jednu minutu
void Odpocet() {
  if ((millis() - OdpocetMil) > 60000)
  {
    if (SetTimeMode < 4)
    {
      RTC.setHour(Hodiny);
      RTC.setMinute(Minuty);
      RTC.setClockMode(false);
      Hodiny = RTC.getHour(h12, PM);
      Minuty = RTC.getMinute();
      display.showNumberDecEx(Hodiny, 0b01000000, true, 2, 0);
      display.showNumberDecEx(Minuty, 0b01000000, true, 2, 2);
      delay(1000);
      display.clear();
      SetTimeMode = 0;
    }
    else
    {
      SetTimeMode = 8;
    }
  }
}

//funkce alarmu
//pokud je alarm aktivní srovnej aktuální a nastavený čas pro alarm, pokud jsou shodné  zapni zvukovou signalizaci
void Alarm() {
  if (RTC.getHour(h12, PM) == HodinyAlarm && RTC.getMinute() == MinutyAlarm && AlarmOvladani == true)
  {
    AlarmStav = true;
  }

  if (AlarmStav == true)
  {
    for (int i = 0; i <= 3; i++)
    {
      tone(BUZZER, 500);
      delay(50);
      noTone(BUZZER);
      delay(50);
    }
    delay(1000);
  }
}
