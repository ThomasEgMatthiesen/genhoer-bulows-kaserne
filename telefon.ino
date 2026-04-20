#include <Bounce2.h>
#include <AltSoftSerial.h>
#include <DFRobotDFPlayerMini.h>

// ----- Pins -----
const int hookPin = 4;
const int dialPin = 5;
const int numberPin = 6;
const int busyPin = 7;

// ----- Phone logic -----
const int maxPhoneNumberLength = 3;
char phoneNumber[maxPhoneNumberLength + 1];
int currentDigit = 0;
int pulseCount = 0;

enum State { Idle, Dialtone, Dialling, Playback, PlaybackStarting };
State state = Idle;

// ----- Bounce -----
Bounce hookSwitch = Bounce();
Bounce dialSwitch = Bounce();
Bounce numberSwitch = Bounce();

// ----- DFPlayer -----
AltSoftSerial dfSerial;
DFRobotDFPlayerMini dfPlayer;

// ----- Timer -----
unsigned long playbackStartTime = 0;
const unsigned long playbackStartupDelay = 1000; // ms

// ----- Number mapping -----
struct PhoneMapping {
  const char* number;
  uint8_t file;
};

const PhoneMapping phoneMappings[] = {
  {"347", 4}, // 0003.mp3, FUT
  {"619", 5}, // 0004.mp3, Kommandanten
  {"140", 6}, // 0005.mp3, Kontoret
  {"391", 7}, // 0006.mp3, Indkvartering
  {"462", 8}, // 0007.mp3, På Fælleden
  {"918", 9}, // 0008.mp3, Gymnastik i gården
  {"800", 10}, // 0009.mp3, Efter morgenmaden
  {"165", 11}, // 0010.mp3, Angreb på ubåde
};

const int mappingCount = sizeof(phoneMappings) / sizeof(phoneMappings[0]);

void resetPhone() {
  memset(phoneNumber, 0, sizeof(phoneNumber));
  currentDigit = 0;
  pulseCount = 0;
}

void setup() {
  Serial.begin(9600);

  pinMode(hookPin, INPUT_PULLUP);
  hookSwitch.attach(hookPin);
  hookSwitch.interval(5);

  pinMode(dialPin, INPUT_PULLUP);
  dialSwitch.attach(dialPin);
  dialSwitch.interval(5);

  pinMode(numberPin, INPUT_PULLUP);
  numberSwitch.attach(numberPin);
  numberSwitch.interval(5);

  pinMode(busyPin, INPUT);

  dfSerial.begin(9600);
  delay(500);

  if (!dfPlayer.begin(dfSerial)) {
    // Serial.println("DFPlayer fejl!");
    while (1);
  }

  dfPlayer.volume(20);
  resetPhone();
  // Serial.println("System klar");
}

void loop() {
  hookSwitch.update();
  dialSwitch.update();
  numberSwitch.update();

  // ----- RØRET LAGT PÅ -----
  if (hookSwitch.fell()) {
    state = Idle;
    dfPlayer.stop();
    resetPhone();
    // Serial.println("Røret lagt på → Idle");
    return;
  }

  // ----- RØRET LØFTET -----
  if (state == Idle && hookSwitch.rose()) {
    dfPlayer.loop(1);
    state = Dialtone;
    // Serial.println("Dialtone start");
  }

  // ----- STATE MACHINE -----
  switch (state) {

    case Dialtone:
      if (dialSwitch.fell()) {
        state = Dialling;
        // Serial.println("Dialling start");
      }
      break;

    case Dialling:
      if (numberSwitch.rose()) pulseCount++;

      if (dialSwitch.rose()) {
        if (pulseCount == 10) pulseCount = 0;

        phoneNumber[currentDigit] = '0' + pulseCount;
        currentDigit++;
        phoneNumber[currentDigit] = 0;
        pulseCount = 0;

        // Serial.print("Nummer: ");
        // Serial.println(phoneNumber);

        // ----- MATCH NUMBER → start playback -----
        bool found = false;
        for (int i = 0; i < mappingCount; i++) {
          if (strcmp(phoneNumber, phoneMappings[i].number) == 0) {
            dfPlayer.play(phoneMappings[i].file);
            playbackStartTime = millis();
            state = PlaybackStarting;
            // Serial.print("→ spiller fil ");
            // Serial.println(phoneMappings[i].file);
            found = true;
            break;
          }
        }

        if (!found && currentDigit >= maxPhoneNumberLength) {
          dfPlayer.play(3);  // fejltone
          playbackStartTime = millis();
          state = PlaybackStarting;
          // Serial.println("→ forkert nummer, fil 2");
        }
      }
      break;

    case PlaybackStarting:
      if (millis() - playbackStartTime >= playbackStartupDelay) {
        state = Playback;
      }
      break;

    case Playback:
      int busyState = digitalRead(busyPin);

      if (busyState == HIGH) { // Afspilning færdig
        dfPlayer.loop(1);       // tilbage til dialtone
        resetPhone();
        state = Dialtone;
        // Serial.println("Playback slut → dialtone");
      }
      break;
  }
}