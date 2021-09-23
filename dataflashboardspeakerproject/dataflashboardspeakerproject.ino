/* SD Card to DataFlash

   David Johnson-Davies - www.technoblogy.com - 21st January 2020
   Arduino Uno
   
   CC BY 4.0
   Licensed under a Creative Commons Attribution 4.0 International license: 
   http://creativecommons.org/licenses/by/4.0/
*/

#include <SPI.h>
#include <SD.h>

File myFile;

// Winbond DataFlash commands
#define PAGEPROG      0x02
#define READSTATUS    0x05
#define READDATA      0x03
#define WRITEENABLE   0x06
#define CHIPERASE     0x60
#define READID        0x90

// Connect SD card interface to:
// CLK = 13, DO = 12, DI = 11, CS = 10

// Connect DataFlash to:
const int sck = 3, miso = 4, mosi = 5, cs = 6, power = 7;

class DF {
  public:
    boolean Setup();
    void BeginRead(uint32_t addr);
    void BeginWrite(void);
    uint8_t ReadByte(void);
    void WriteByte(uint8_t data);
    void EndRead(void);
    void EndWrite(void);
  private:
    unsigned long addr;
    uint8_t Read(void);
    void Write(uint8_t);
    void Busy(void);
    void WriteEnable(void);
};

boolean DF::Setup () {
  uint8_t manID, devID;
  pinMode(power, OUTPUT); digitalWrite(power, HIGH);
  digitalWrite(cs, HIGH); pinMode(cs, OUTPUT); 
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
  shiftOut(mosi, sck, MSBFIRST, data);
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

void DF::BeginRead (uint32_t start) {
  addr = start;
  digitalWrite(cs, 0);
  Write(READDATA);
  Write(addr>>16);
  Write(addr>>8);
  Write(addr);
}

uint8_t DF::Read () {
  return shiftIn(miso, sck, MSBFIRST);
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

// Process a filename

uint32_t Offset = 0;

void DoFile (const char *filename) {
  Serial.print(filename); Serial.print(" ");
  myFile = SD.open(filename, FILE_READ);
  if (!myFile) {
    Serial.println("Error with file");
    for(;;);
  }

  // Skip WAV header
  for (int j=0; j<44; j++) myFile.read();
  
  for (;;) {
    int Byte = myFile.read();
    if (Byte == -1) break;
    DataFlash.WriteByte(Byte);
    Offset++;
  };
  Serial.println(Offset);
  myFile.close();
}

// Test program
void setup() {
  // Open serial communications and wait for port to open:
  Serial.begin(9600);
  if (!DataFlash.Setup()) {
    Serial.println("DataFlash not found");
    for(;;);
  }

  if (!SD.begin(10)) {
    Serial.println("Initialization failed!");
    for(;;);
  }

  DataFlash.BeginWrite();
  Serial.println("Start 0");

  // Change these to the filenames on your SD card
  DoFile("admir.wav");
  DoFile("thetop.wav");

  
  DataFlash.EndWrite();
  Serial.println("Finished");
}

void loop() {
}
