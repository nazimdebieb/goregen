/*---------------------------------------------------------------------*
  This file comes pre-installed in your Regenbox, it is responsible
  for communicating with the main executable "goregen"

  In case of a major upgrade, or if you need to install it on a new
  Arduino board, please refer to the wiki section of the project :

      https://github.com/solar3s/goregen/wiki/Upgrading-firmware
-----------------------------------------------------------------------*/

/*---------------------------------------------------------------------*
  Provides direct pin access via simple serial protocol

  Takes a 1byte instruction input, output can be:
    - On toggle instructions: write 1 single boolean byte for new state
        (LED_TOGGLE)
    - On other pin instructions: write 1 single 0 byte (always success)
        (LED_0/1, PIN_DISCHARGE_0/1, PIN_CHARGE_0/1, MODE_*)
    - On uint readings: write ascii repr of value
        (READ_A0, READ_V)

  All communication end with STOP_BYTE

-----------------------------------------------------------------------*/


// -------------------------------------------
// Input instructions (waiting on serial read)
//
// READ_* writes string response
#define READ_A0         0x00 // read A0 pin
#define READ_V          0x01 // fancy A0 reads and compute voltage
#define READ_VERSION    0x02 // returns current firmware version

// LED_TOGGLE writes boolean response (led state)
#define LED_TOGGLE      0x12 // led toggle

// all other commands return a single null byte
#define LED_0           0x10 // led off
#define LED_1           0x11 // led on
#define PIN_DISCHARGE_0 0x30 // pin discharge off
#define PIN_DISCHARGE_1 0x31 // pin discharge on
#define PIN_CHARGE_0    0x40 // pin charge off
#define PIN_CHARGE_1    0x41 // pin charge on

#define MODE_IDLE       0x50 // enable idle mode
#define MODE_CHARGE     0x51 // enable charge mode
#define MODE_DISCHARGE  0x52 // enable discharge mode

#define PING            0xA0 // just a ping

#define STOP_BYTE       0xff // sent after all communication

// ---------------------------
// Internal address and config
#define PIN_CHARGE    4        // output pin address (charge)
#define PIN_DISCHARGE 3        // output pin address (discharge)
#define PIN_LED       13       // output pin address (arduino led)
#define PIN_ANALOG    A0       // analog pin on battery-0 voltage

// config parameters for getVoltage()
#define CAN_REF       2410 // tension de reference du CAN
#define CAN_BITSIZE   1023 // précision du CAN
#define NB_ANALOG_RD  204  // how many analog read to measure average

// Averaging parameters
#define VOLTAGE_HISTORY_NUM  10                  // Number of samples for averaging
unsigned long gVoltageHist[VOLTAGE_HISTORY_NUM]; // Voltage history
unsigned long gHistCounter = 0;                  // Voltage measurement counter

// computeAvgVoltage retreive the previous last
// VOLTAGE_HISTORY_NUM measures and averages on that
unsigned long computeAvgVoltage() {
  unsigned long avgVoltage = 0;
  byte sz = gHistCounter < VOLTAGE_HISTORY_NUM?
    gHistCounter: VOLTAGE_HISTORY_NUM;
  for (byte i = 0; i < sz; i++) {
    avgVoltage += gVoltageHist[i];
  }
  avgVoltage = floor(avgVoltage / sz);
  return avgVoltage;
}

void setCharge(boolean b) {
  digitalWrite(PIN_CHARGE, !b);
}

void setDischarge(boolean b) {
  digitalWrite(PIN_DISCHARGE, b);
}

void setLed(boolean b) {
  digitalWrite(PIN_LED, b);
}

boolean toggleLed() {
  boolean b = !digitalRead(PIN_LED);
  setLed(b);
  return b;
}

unsigned long getAnalog() {
  return analogRead(PIN_ANALOG);
}

unsigned long getVoltage() {
  unsigned long tmp, sum;
  sum = 0;
  for(byte i=0; i < NB_ANALOG_RD; i++){
    tmp = getAnalog();
    sum = sum + tmp;
    delay(1);
  }
  sum = sum / NB_ANALOG_RD;
  // convert using CAN specs and ref value
  sum = (sum * CAN_REF) / CAN_BITSIZE;

  gVoltageHist[gHistCounter % VOLTAGE_HISTORY_NUM] = sum;
  gHistCounter++;
  
  return computeAvgVoltage();
}

void setup() {
  Serial.begin(57600);
  // reference de tension pour les mesures
  analogReference(EXTERNAL);

  pinMode(PIN_CHARGE, OUTPUT);
  pinMode(PIN_DISCHARGE, OUTPUT);
  pinMode(PIN_LED, OUTPUT);

  setCharge(0);
  setDischarge(0);
  setLed(1);
}


// simple talk protocol
void loop() {
  if (!Serial.available()) {
    return;
  }

  byte in = Serial.read();
  switch (in) {
    case READ_A0:
      Serial.print(getAnalog());
      break;
    case READ_V:
      Serial.print(getVoltage());
      break;

    case LED_0:
      setLed(0);
      break;
    case LED_1:
      setLed(1);
      break;
    case LED_TOGGLE:
      Serial.write(toggleLed());
      break;

    case PIN_DISCHARGE_0:
      setDischarge(0);
      break;
    case PIN_DISCHARGE_1:
      setDischarge(1);
      break;

    case PIN_CHARGE_0:
      setCharge(0);
      break;
    case PIN_CHARGE_1:
      setCharge(1);
      break;

    case MODE_IDLE:
      setDischarge(0);
      setCharge(0);
      break;
    case MODE_CHARGE:
      setDischarge(0);
      setCharge(1);
      break;
    case MODE_DISCHARGE:
      setCharge(0);
      setDischarge(1);
      break;
    case PING:
      break;
    default:
      // do not talk to strangers
      return;
  }

  // end communication
  Serial.write(STOP_BYTE);
}
