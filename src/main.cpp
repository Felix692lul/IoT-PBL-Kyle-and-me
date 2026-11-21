#include <Arduino.h> // Libraries
#include <SPI.h>
#include <Wire.h>
#include <MFRC522.h>
#include <Adafruit_GFX.h> 
#include <Adafruit_SSD1306.h>
#include <Adafruit_Sensor.h> 
#include <Adafruit_BMP280.h>

// Global variables
//PINS NEED TO BE DETERMINED BEFORE WIRING ANY CIRCUIT
#define RFID_SS 22
#define RFID_RST 23
#define GREEN_LED 16
#define RED_LED 17
#define HELP_BUTTON 33 // help button (needs to be determined)
#define POT_PIN 34 
#define SCREEN_WIDTH 128 
#define SCREEN_HEIGHT 64 
#define OLED_MOSI 11 
#define OLED_CLK 13 
#define OLED_DC 9 
#define OLED_CS 10 
#define OLED_RST 8 

//FOR OLED DISPLAY
Adafruit_SSD1306 display( 
SCREEN_WIDTH, 
SCREEN_HEIGHT, 
OLED_MOSI, 
OLED_CLK, 
OLED_DC, 
OLED_RST, 
OLED_CS 
);  

//BMP SENSOR ARRAY
float tempreading[10];

//BMP sensor initialization
Adafruit_BMP280 bmp; 

//BMP temperature variable
float temperature;

//Sets previous UID reading
String lastUID = "";
 
//millis Timers:
unsigned long lastScanTime = 0;
unsigned long Start = 0;
unsigned long Time = 0;
unsigned long lastblinktime = 0;
unsigned long lasttemptime = 0;

MFRC522 rfid(RFID_SS, RFID_RST); //RFID initialization
String authorizedUIDs[] = {"9C 2B 4D 05"}; //authorized UIDs to be changed if neccessary
int numAuthorized = 1; //Nb of authorized UIDs
bool roomOccupied = false;
bool indicatorOn = false;
bool blinking = false;



// Events and Status Byte

 // Status Byte Bit Masks
  const byte OCCUPIED_MASK = 0b00000001; // Bit 0: Room occupied
  const byte TOO_HOT_MASK = 0b00000010; // Bit 1: Room too hot
  const byte HELP_MASK = 0b00000100; // Bit 2: Help button pressed

  byte statusByte = 0; // Status byte starting value = 0 (all bits are 0, meaning room is free, temperature is normal, and help button is not pressed)
  byte lastPrintedStatus = 0; // Last printed status byte value

 // Help button variables
  volatile bool helpFlag = false; // Flag to show if the help button has been pressed
  volatile unsigned long lastHelpTime = 0; // Time when the help button was pressed

 // Help button timing
  const unsigned long HELP_TIME = 10000; // help stays on for 10 secs
  unsigned long helpStart = 0; // help button start time when it was called



//Functions:
String getUID() //This functions turns the card's UID into a readable text format
{
  String uid = "";
  for (byte i = 0; i < rfid.uid.size; i++) {
    if (rfid.uid.uidByte[i] < 0x10) {
      uid = uid + "0";
    }
    uid = uid + String(rfid.uid.uidByte[i], HEX);
    if (i < rfid.uid.size - 1) {
      uid = uid + " ";
    }
  }
  uid.toUpperCase();
  return uid;
}

bool isAuthorized(String uid) { 
  for (int i = 0; i < numAuthorized; i++) {  
    if (uid == authorizedUIDs[i]) {  
      return true; 
    } 
  } 
  return false; 
}

void Startindicatortimer(unsigned long duration)
{ 
 Start = millis();
 Time = duration;
 indicatorOn = true;
}  
void allIndicatorsOff() 
{ 
  digitalWrite(GREEN_LED, LOW); 
  digitalWrite(RED_LED, LOW); 
} 

void AccessGranted() 
{

  allIndicatorsOff();
  roomOccupied = !roomOccupied; //Sets the room occupied to true
  if(roomOccupied == true){ //if room is occupied, make green LED high
    Serial.println("Room occupied!"); 
    digitalWrite(GREEN_LED,HIGH);
    }else{        
      Serial.println("Room free!");
      digitalWrite(GREEN_LED,LOW);
    }
    blinking = false;
    digitalWrite(RED_LED, LOW);
    indicatorOn = false;
}

void AccessDenied() 
{ 
  Serial.println("Unknown card detected!");
  allIndicatorsOff();
  blinking = true;
  digitalWrite(RED_LED, HIGH);
  lastblinktime = millis();
  Startindicatortimer(2000); //The RED LED will only blink for 2 second, adjust if needed
}

void Timerexpired()
{
allIndicatorsOff();
indicatorOn = false;
blinking = false;
//GREEN_LED MATCHES THE ROOM STATE
if (roomOccupied == true) {
  digitalWrite(GREEN_LED, HIGH);
  } else {
    digitalWrite(GREEN_LED, LOW);
  }
}

void onCardScanned(String uid) 
{ 
  if (uid == lastUID && millis() - lastScanTime < 2000){  
    Serial.println("Repeat ignored"); 
    return; 
  } 
  lastUID = uid;
  lastScanTime = millis();
  Serial.print("Card UID: "); 
  Serial.println(uid); 
  if (isAuthorized(uid) == true) 
  { 
    AccessGranted(); 
  } 
  else 
  { 
    AccessDenied(); 
  } 
} 


// Interrupt Service ROutines (ISRs) for the help button
  void IRAM_ATTR helpButton_ISR() {

    // Ignore presses less than 200 ms apart (debounce)
    if (millis() - lastHelpTime > 200) {
      lastHelpTime = millis();
      helpFlag = true; // Set the help flag to true
    }
  }


// Prints the status byte as 8 binary digits
  void printStatusByte() {
    Serial.print("Status Byte: ");
    for (int i =7; i >= 0; i--) {
      Serial.print((statusByte >> i) & 1);
    }
    Serial.println();
  }

// Event handler for help request - called from loop()
  void onHelpRequested(){
    statusByte |= HELP_MASK; // set bit to 2
    helpStart = millis(); // start the help timer
    Serial.println("Help requested!");
  }




void setup() {
Serial.begin(115200);
SPI.begin(); //initalize SPI communication
Wire.begin(); //For I2C communication
display.begin(SSD1306_SWITCHCAPVCC); //Starts the OLED display
rfid.PCD_Init();
delay(10);
//For bmp sensor
 if (bmp.begin(0x76)){ 
    Serial.println("BMP280 Found at 0x76"); 
  }
 else if(bmp.begin(0x77))
 { 
    Serial.println("BMP280 Found at 0x77");
  }
  else
  {
   Serial.println("BMP280 Not found");
  }  
rfid.PCD_DumpVersionToSerial(); //prints the firmware version to serial monitor IF RFID IS PROPERLLY WIRED
pinMode(GREEN_LED, OUTPUT);
pinMode(RED_LED, OUTPUT);

// Help button setup
  pinMode(HELP_BUTTON, INPUT_PULLUP); // help button pin set to input with pullup resistor
  attachInterrupt(digitalPinToInterrupt(HELP_BUTTON), helpButton_ISR, FALLING); // attach interrupt to help button pin, triggers on falling edge (button press)
  printStatusByte(); //show starting value of status byte
}

void loop() {
//BMP sensor tempature reading:
if (millis() - lasttemptime >= 2000) {
  lasttemptime = millis();
  temperature = bmp.readTemperature(); // Read BMP280, update array, calculate average, and refresh OLED here
}


if (rfid.PICC_IsNewCardPresent() == true && rfid.PICC_ReadCardSerial() == true) 
{ 
  String uid = getUID();
  onCardScanned(uid);  
  rfid.PICC_HaltA(); //puts the card to sleep
  rfid.PCD_StopCrypto1(); //ends the authentication
}
  if (indicatorOn  == true && millis() - Start >= Time) { // Timer 
    Timerexpired(); //calls the function timer expired
  } 
  
  //BLINKING LOGIC FOR RED LED
  if(blinking == true && millis()-lastblinktime >= 300){
   lastblinktime = millis();
   digitalWrite(RED_LED,!digitalRead(RED_LED));
  }

 // Check for help button press
  if (helpFlag == true) {
    helpFlag = false; // reset the help flag
    onHelpRequested(); // call the help request handler
  }


 // Timer: help request ends after help time
  if ((statusByte & HELP_MASK) && millis() - helpStart >= HELP_TIME) 
  {
    statusByte &= ~HELP_MASK;
  }
  

 // Print the byte only when it changes
  if (statusByte != lastPrintedStatus){
    lastPrintedStatus = statusByte;
    printStatusByte();
  }
}

