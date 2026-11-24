#include <Arduino.h> // Libraries
#include <SPI.h>
#include <Wire.h>
#include <MFRC522.h>
#include <Adafruit_GFX.h> 
#include <Adafruit_SSD1306.h>
#include <Adafruit_Sensor.h> 
#include <Adafruit_BMP280.h>
#include <SD.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>

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
#define FAN_PIN 25

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

//BMP sensor variables:
int tempindex = 0; //Tracks current position
int readingcount = 0; //Tracks how many readings I have so far

//BMP sensor initialization
Adafruit_BMP280 bmp; 

//BMP average temperature variable
float avgtemp = 0;

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


// WiFi credentials

 const char* ssid="Kyle ESP32"; //WiFi SSID
 const char* password="kyle123!"; //WiFi password

 WebServer server(80); // Create a web server object that listens for HTTP request on port 80

 // TEMPORARY VARIABLE FOR TESTING PURPOSES (will change later)
 int fanLevel = 2; // Fan level (0-3) to be changed based on temperature readings
 

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


// Data logging variables
  const int chipselect = 4; // SD card CS pin (NEEDS TO BE DETERMINED)
  File dataFile; // File object for SD card


//Functions:


// starts SD card and creates csv file
  void startLogging() {
    Serial.println("Starting SD card...");
    if (SD.begin(chipselect) == true) {
      Serial.println("SD Card Found!");

    // Write the header to the CSV file if it doesn't exist
      if (SD.exists("/datalog.csv") == false) {
        dataFile = SD.open("/datalog.csv", FILE_WRITE);
        dataFile.println("Time_ms,Event");
        dataFile.close();
    }
  }
  else 
  {
    Serial.println("SD Card not found");
  }
}

// Logs event to the csv file
  void logEvent(String event) {
    dataFile = SD.open("/datalog.csv", FILE_APPEND);
    if (dataFile) {
      dataFile.print(millis());
      dataFile.print(",");
      dataFile.println(event);
      dataFile.close();
      Serial.print("Logged: ");
      Serial.println(event);
  }
  else 
  {
    Serial.println("Could not open datalog.csv");
  }
}



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
    logEvent("CHECKIN"); // Log the check-in event to the SD card
    digitalWrite(GREEN_LED,HIGH);
    statusByte |= OCCUPIED_MASK; //Set bit 0, links statusByte to room state
    }else{        
      Serial.println("Room free!");
      logEvent("CHECKOUT"); // Log the check-out event to the SD card
      digitalWrite(GREEN_LED,LOW);
      statusByte &= ~OCCUPIED_MASK; //the ~ is a NOT operator, clear bit 0, links statusByte to room state
    }
    blinking = false;
    digitalWrite(RED_LED, LOW);
    indicatorOn = false;
    OLEDupdate();
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
    logEvent("HELP"); // Log the help event to the SD card
  }

void OLEDupdate(){ //For the OLED update, parameter of avgtemp
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  if(roomOccupied){
    display.println("Room: Occupied");
    }else{
    display.println("Room: Free");
  }
  display.setCursor(0,20); //To be modified?
  display.println("Avg Temp:");
  display.print(avgtemp,1); //the 1 is to tell how much decimal is to be shown, 1 is 23.2 and 2 23.22 for example
  display.print(" C");
  display.display(); //Put the text on the screen
  }

//PWM FUNCTION
void PWM(){
  int potValue = analogRead(POT_PIN);
  float setpoint = map(potValue, 0, 4095, 180, 300) / 10.0; //Map 0-4095 to 18 and 30 degree Celsius, keep 1 decimal place
  if(avgtemp > setpoint){
    statusByte |= TOO_HOT_MASK; //Set the bit
     }else{
      statusByte &= ~TOO_HOT_MASK; //Clears the bit
    }
    int fanspeed = 0; //Create a variable for the speed of the fan
    if(roomOccupied && avgtemp > setpoint){
    fanspeed = constrain((avgtemp-setpoint)* 51, 0, 255); //the 0 next to the 51 is the degree, so if it's 1, then 1*51 = 51 out of 255(20% speed)
                                                          //constrain caps the value at 255, so if there's a 6 degree difference, it will not go past 255(100% speed)                                   
     }
  ledcWrite(0, fanspeed); //attach the fanspeed variable, IF ERROR, CHANGE IT TO ledcWrite(FAN_PIN, fanspeed);
}

// this runs when someone opens /data, and sends the room data as JSON
void handleData() {
  Serial.println("data route accessed");
  JsonDocument doc;

  if (roomOccupied == true) { // is room occupied?
    doc ["room"] = "occupied"; // Yes: add room occupied to the JSON document
  } else {
    doc ["room"] = "free"; // No: add room free to the JSON document
  }
  doc ["avg_temp"] = avgtemp; // Add average temperature to the JSON document
  doc ["fan_level"] = fanLevel; // Add fan level to the JSON document
  doc ["status_byte"] = statusByte; // Add status byte to the JSON document

  String out; // text holds the finished json
  serializeJson(doc, out); // turn the document into JSON text and put it in the out variable
  server.send(200, "application/json", out); // send it back to the client with a 200 OK and the browser knows it's JSON data
}



void setup() {
Serial.begin(115200);
SPI.begin(); //initalize SPI communication
Wire.begin(); //For I2C communication
display.begin(SSD1306_SWITCHCAPVCC); //Starts the OLED display
rfid.PCD_Init();
startLogging(); // Start SD card logging
delay(10);


// Wi-Fi access point
  WiFi.softAP(ssid, password); // creates the wifi network with name/password
    Serial.print("IP address: ");
    Serial.println(WiFi.softAPIP()); // prints the esp32's ip address

// Routes for the web server
  server.on("/data", handleData); // when browser asks for /data, call handleData();

// Server start
  server.begin(); // Start the web server
    Serial.println("Server started");


//PWM settings, IF THERE IS ERROR HERE, SWAP TWO LINES BELOW FOR ledcAttach(FAN_PIN, 5000, 8); 
ledcSetup(0, 5000, 8);  //Set PWM channel 0 to 5 kHz, 8-bit resolution
ledcAttachPin(FAN_PIN, 0); //Attach the fan pin to PWM channel 0

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

//Call OLED function to show room free and 0.0C
OLEDupdate();
}



void loop() {

server.handleClient(); // check if a browser is asking for a page at every loop

//BMP sensor tempature reading with arrays.
if (millis() - lasttemptime >= 2000) {
  lasttemptime = millis();
  tempreading[tempindex] = bmp.readTemperature();
  tempindex++;
if (tempindex >= 10) {
  tempindex = 0;
}
//Prevents the first few reading from being averaged with 0.0 slots
if(readingcount < 10){ //Counts the number of readings, does not surpass 10
  readingcount++;
  }
  float sum = 0;
  for(int i = 0; i < readingcount; i++){
    sum = sum + tempreading[i];
    }
    avgtemp = sum/readingcount; //Takes the sum of the reading and divides it by 10
    OLEDupdate();
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
  PWM(); //CALL THE PWM FUNCTION, SO IT LOOPS CONTINIOUSLY
}

