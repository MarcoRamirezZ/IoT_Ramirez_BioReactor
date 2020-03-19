#include <ESP8266WiFi.h>
#include <SPI.h>
#include <MFRC522.h>


#define LED_MODULE D0
#define SS_PIN 4  //D2
#define RST_PIN 5 //D1
#define ACCESS_GRANTED 1
#define ACCESS_DENIED 0
#define ACCESS_ERROR 2

WiFiClient WIFI_CLIENT;
MFRC522 mfrc522(SS_PIN, RST_PIN);   // Create MFRC522 instance
char data = '0';


void setup() {
  //////////////////////////////////////////////
  // Serial communication
  Serial.begin(9600);

  //////////////////////////////////////////////
  //WiFi communication
  WiFi.begin("RAMZEP1", "7Tlapehue-");
  while(WiFi.status() != WL_CONNECTED)
  {
    delay(500);
  }
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  ///////////////////////////////////////////////
  // RFID sensor 
  SPI.begin();            // Init SPI bus
  mfrc522.PCD_Init();     // Init MFRC522
  ShowReaderDetails();    // Show details of PCD - MFRC522 Card Reader details
  Serial.println(F("Scan PICC to see UID, type, and data blocks..."));

  //extras
  pinMode(LED_MODULE, OUTPUT);
  digitalWrite(LED_MODULE, HIGH);
  Serial.println("Antenna Off");
  mfrc522.PCD_AntennaOff();
  
}

void loop() {
   /////////////////////////////////////////
   //turn on and off RFID Antenna with COM3
   data = Serial.read();
   if(data == '1')
   {
    Serial.println("Antenna On");
    digitalWrite(LED_MODULE, LOW);
    mfrc522.PCD_AntennaOn();
    
   }
   else if(data == '0')
   {
    Serial.println("Antenna Off");
    digitalWrite(LED_MODULE, HIGH);
    mfrc522.PCD_AntennaOff();
   }
   
   ///////////////////////////////////////////
   //RFID 
   if ( ! mfrc522.PICC_IsNewCardPresent()) 
   {
    return;
   }
   // Select one of the cards
   if ( ! mfrc522.PICC_ReadCardSerial()) 
   {
    return;
   }
  String content = Display_UID();
  uint8_t flagAccess = Access( content );
  //revisar de la base de datos de tarjetas UID permitidas
  //conceder acceso y empezar a medir peso
}
String Display_UID()
{
   Serial.print(" UID Tag: " );
   String content = "";
   byte letter;
   for(byte i = 0; i < mfrc522.uid.size; i++)
   {
    Serial.print(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " ");
    Serial.print(mfrc522.uid.uidByte[i], HEX);
    content.concat(String(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " "));
    content.concat(String(mfrc522.uid.uidByte[i], HEX));
   }
   Serial.println();
   delay(1000);
   return ( content );
}

uint8_t Access( String content )
{
  bool access = ACCESS_DENIED;
  Serial.print("Message: ");
  content.toUpperCase();

  if( content.substring(1) == "24 31 64 2B" )
  {
    access = ACCESS_GRANTED;
  }
  if( access )
  {
    Serial.println("Authorized Access");
  }
  else
  {
    Serial.println( "Denied Access" );
  }
  return access;      
}

void ShowReaderDetails() 
{
    // Get the MFRC522 software version
    byte v = mfrc522.PCD_ReadRegister(mfrc522.VersionReg);
    Serial.print(F("MFRC522 Software Version: 0x"));
    Serial.print(v, HEX);
    if (v == 0x91)
    {
        Serial.print(F(" = v1.0"));
    }
    else if (v == 0x92)
    {
        Serial.print(F(" = v2.0"));
    }
    else
    {
        Serial.print(F(" (unknown)"));
    }
    Serial.println("");
    // When 0x00 or 0xFF is returned, communication probably failed
    if ((v == 0x00) || (v == 0xFF)) 
    {
        Serial.println(F("WARNING: Communication failure, is the MFRC522 properly connected?"));
    }
}
/*
 * Displays all the information inside the UID card, probably not needed for 
void Display_Content()
{
  // Dump debug info about the card; PICC_HaltA() is automatically called
  mfrc522.PICC_DumpToSerial(&(mfrc522.uid));
}
*/
