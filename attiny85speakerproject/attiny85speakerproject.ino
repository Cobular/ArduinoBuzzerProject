/* Four Sample Player

   David Johnson-Davies - www.technoblogy.com - 21st January 2020
   ATtiny85 @ 8 MHz (internal oscillator; BOD disabled)
      
   CC BY 4.0
   Licensed under a Creative Commons Attribution 4.0 International license: 
   http://creativecommons.org/licenses/by/4.0/
*/

#include <avr/sleep.h>

// Winbond DataFlash commands
#define PAGEPROG      0x02
#define READSTATUS    0x05
#define READDATA      0x03
#define WRITEENABLE   0x06
#define CHIPERASE     0x60
#define READID        0x90
#define POWERDOWN     0xB9
#define RELEASEPD     0xAB

// ATtiny85 pins used for dataflash
const int sck = 2, miso = 1, mosi = 0, cs = 3;

class DF {
  public:
    boolean Setup();
    void BeginRead(uint32_t addr);
    void BeginWrite(void);
    uint8_t ReadByte(void);
    void WriteByte(uint8_t data);
    void EndRead(void);
    void EndWrite(void);
    void PowerDown(boolean);
  private:
    unsigned long addr;
    uint8_t Read(void);
    void Write(uint8_t);
    void Busy(void);
    void WriteEnable(void);
};

boolean DF::Setup () {
  uint8_t manID, devID;
  pinMode(cs, OUTPUT); digitalWrite(cs, HIGH); 
  pinMode(sck, OUTPUT);
  pinMode(mosi, OUTPUT);
  pinMode(miso, INPUT);
  digitalWrite(sck, LOW); digitalWrite(mosi, HIGH);
  delay(1);
  digitalWrite(cs, LOW);
  delay(100);
  Write(READID); Write(0);Write(0);Write(0);
  manID = Read();
  devID = Read();
  digitalWrite(cs, HIGH);
  return (devID == 0x15); // Found correct device
}

void DF::Write (uint8_t data) {
  uint8_t bit = 0x80;
  while (bit) {
    if (data & bit) PORTB = PORTB | (1<<mosi);
    else PORTB = PORTB & ~(1<<mosi);
    PINB = 1<<sck;                        // sck high
    bit = bit>>1;
    PINB = 1<<sck;                        // sck low
  }
}

void DF::Busy () {
  digitalWrite(cs, 0);
  Write(READSTATUS);
  while (Read() & 1 != 0);
  digitalWrite(cs, 1);
}

void DF::WriteEnable () {
  digitalWrite(cs, 0);
  Write(WRITEENABLE);
  digitalWrite(cs, 1);
}

void DF::PowerDown (boolean on) {
  digitalWrite(cs, 0);
  if (on) Write(POWERDOWN); else Write(RELEASEPD);
  digitalWrite(cs, 1);
}

void DF::BeginRead (uint32_t start) {
  addr = start;
  digitalWrite(cs, 0);
  Write(READDATA);
  Write(addr>>16);
  Write(addr>>8);
  Write(addr);
}

uint8_t DF::Read () {
  uint8_t data = 0;
  uint8_t bit = 0x80;
  while (bit) {
    PINB = 1<<sck;                        // sck high
    if (PINB & 1<<miso) data = data | bit;
    PINB = 1<<sck;                        // sck low
    bit = bit>>1;
  }
  return data;
}

void DF::EndRead(void) {
  digitalWrite(cs, 1);
}

void DF::BeginWrite () {
  addr = 0;
  // Erase DataFlash
  WriteEnable();
  digitalWrite(cs, 0);
  Write(CHIPERASE);
  digitalWrite(cs, 1);
  Busy();
}

uint8_t DF::ReadByte () {
  return Read();
}

void DF::WriteByte (uint8_t data) {
  // New page
  if ((addr & 0xFF) == 0) {
    digitalWrite(cs, 1);
    Busy();
    WriteEnable();
    digitalWrite(cs, 0);
    Write(PAGEPROG);
    Write(addr>>16);
    Write(addr>>8);
    Write(0);
  }
  Write(data);
  addr++;
}

void DF::EndWrite (void) {
  digitalWrite(cs, 1);
  Busy();
}

DF DataFlash;

// Audio player **********************************************

volatile boolean StayAwake;
const int speaker = 4;
volatile int Play = 0;
volatile uint32_t Count;
uint32_t Sizes[5] = { 0, 761853, 2964179, 10291, 1415837 };

// Sample interrupt
ISR (TIMER0_COMPA_vect) {
  char sample = DataFlash.ReadByte();
  OCR1B = sample;
  // End of data? Go to sleep
  Count--;
  if (Count == 0) {
    DataFlash.EndRead();
    TIMSK = 0;                            // Turn off interrupt
    StayAwake = false;
  }
}

// Pin change interrupt
ISR (PCINT0_vect) {
  int Buttons = PINB;
  if ((Buttons & 1<<miso) == 0) Play = 1;
  else if ((Buttons & 1<<sck) == 0) Play = 2;
  else if ((Buttons & 1<<mosi) == 0) Play = 3;
  else Play = 0;
  GIMSK = 0;                              // Disable interrupts
}

void setup() {
  // Enable 64 MHz PLL and use as source for Timer1
  PLLCSR = 1<<PCKE | 1<<PLLE;

  // Set up Timer/Counter1 for PWM output
  TIMSK = 0;                              // Timer interrupts OFF
  TCCR1 = 1<<CS10;                        // 1:1 prescale
  GTCCR = 1<<PWM1B | 2<<COM1B0;           // PWM B, clear on match
  OCR1B = 128;                            // 50% duty at start

  // Set up Timer/Counter0 for 8kHz interrupt to output samples.
  TCCR0A = 3<<WGM00;                      // Fast PWM
  TCCR0B = 1<<WGM02 | 2<<CS00;            // 1/8 prescale
  OCR0A = 124;                            // Divide by 1000

  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  ADCSRA = ADCSRA & ~(1<<ADEN);           // Disable ADC to save power
  PCMSK = 1<<mosi | 1<<miso | 1<<sck;     // Set up pin-change interrupts
}

void loop() {
  pinMode(speaker, OUTPUT);
  DataFlash.Setup();
  DataFlash.PowerDown(false);
  StayAwake = true;
  Count = Sizes[Play+1] - Sizes[Play];
  DataFlash.BeginRead(Sizes[Play]);
  TIMSK = 1<<OCIE0A;                      // Enable compare match
  while (StayAwake);
  DataFlash.PowerDown(true);
  // Set up pin-change interrupts on PB0, PB1, and PB2
  pinMode(mosi, INPUT_PULLUP); pinMode(miso, INPUT_PULLUP); pinMode(sck, INPUT_PULLUP);
  pinMode(speaker, INPUT);                // Avoid click
  GIFR = 1<<PCIF;                         // Clear flag
  GIMSK = 1<<PCIE;                        // Enable interrupts
  sleep_enable();
  sleep_cpu();
  // Continue after pin-change interrupt
}
