#include <Arduino.h> // Libraries
#include <SPI.h>
#include <MFRC522.h>


// Global variables
 //Pins to be determined
 #define RFID_SS 22
 #define RFID_RST 23
 #define GREEN_LED 16
 #define RED_LED 17

 //Sets previous UID reading
 String lastUID = "";
 
//millis Timers:
unsigned long lastScanTime = 0; 

MFRC522 rfid(RFID_SS, RFID_RST); //RFID initialization
String authorizedUIDs[] = {"9C 2B 4D 05"}; //authorized UIDs to be changed if neccessary
int numAuthorized = 1; //Nb of authorized UIDs
bool roomOccupied = false;


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
}
void AccessDenied() 
{ 
  Serial.println("Unknown card detected!");
  allIndicatorsOff(); 
  digitalWrite(RED_LED, HIGH);
  delay(500);
  digitalWrite(RED_LED, LOW);
  delay(500); 
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



void setup() {
Serial.begin(115200);
SPI.begin(); //initalize SPI communication
rfid.PCD_Init();
delay(10);
rfid.PCD_DumpVersionToSerial(); //prints the firmware version to serial monitor IF RFID IS PROPERLLY WIRED
pinMode(GREEN_LED, OUTPUT);
pinMode(RED_LED, OUTPUT);
}

void loop() {
if (rfid.PICC_IsNewCardPresent() == true && rfid.PICC_ReadCardSerial() == true) 
{ 
  String uid = getUID();
  onCardScanned(uid);  
  rfid.PICC_HaltA(); //puts the card to sleep
  rfid.PCD_StopCrypto1(); //ends the authentication
}
}

