
///////////////////////////////////////////////////////////////////////////
//For IotCore
#include "FS.h"
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
//////////////////////////////////////////////////////////////////////////
//For RFID MFRC522, and Load Cell Amplifier HX711
#include <MFRC522.h>//RFID sensor
#include <SPI.h>    //SPI protocol
#include <HX711.h>  //Load Cell Amplifier

#define LED_MODULE D0 //LED

#define DOUT D3          //for load cell amplifier
#define CLK D4           //for load cell amplifier
#define KG_CONVERSION ( 0.453592F )
#define TRUE ( 1U )
#define FALSE ( 0U )
#define SS_PIN 4         //D2 for RFID
#define RST_PIN 5        //D1 for RFID
#define ACCESS_GRANTED 1 //for RFID
#define ACCESS_DENIED 0  //for RFID
#define ACCESS_ERROR 2   //for RFID

HX711 scale; //Create load sensor instance
MFRC522 mfrc522( SS_PIN, RST_PIN ); //Create MFRC522 instance

float calibration_factor = -10100; //Load Cell
boolean serial_flag = FALSE;        //Load Cell
boolean display_flag = FALSE;       //Load Cell
const char* ssid = "RAMZEP1";      //WiFi
const char* password = "7Tlapehue-";//WiFi
uint32_t one_minute = 60000;

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");

//MQTT broker ID
const char* AWS_endpoint = "a1dy2r3jmn1gcz-ats.iot.us-east-1.amazonaws.com"; //MQTT broker ip

///////////////////////////////////////////////////////////////////////////////////
void callback(char* topic, byte* payload, unsigned int length) 
{
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) 
  {
  Serial.print((char)payload[i]);
  }
  Serial.println();

}
///////////////////////////////////////////////////////////////////////////////////

WiFiClientSecure espClient;
PubSubClient client(AWS_endpoint, 8883, callback, espClient); //set MQTT port number to 8883 as per //standard

void setup() 
{
  /////////////////////////////////////////////////////////////////////
  //IoT Core and WiFi connection
  Serial.begin(9600);//Init UART 9600
  Serial.setDebugOutput(true);
  
  // initialize digital pin LED_BUILTIN as an output.
  pinMode(LED_BUILTIN, OUTPUT);
  
  setup_wifi();
  delay(1000);
  if (!SPIFFS.begin()) 
  {
   Serial.println("Failed to mount file system");
   return;
  }
  load_security_files();

  /////////////////////////////////////////////////////////////////////
  // RFID sensor 
  SPI.begin();            // Init SPI bus
  mfrc522.PCD_Init();     // Init MFRC522
  ShowReaderDetails();    // Show details of PCD - MFRC522 Card Reader details

  /////////////////////////////////////////////////////////////////////
  // Load Cell Amplifier
  Serial.println("Remove all weight from scale");
  Serial.println("After readings begin, place known weight on scale");
  Serial.println("Press a to increase calibration factor");
  Serial.println("Press z to decrease calibration factor");
  scale.begin( DOUT, CLK );
  scale.set_scale();
  scale.tare();//Reset scale to 0
  long zero_factor = scale.read_average();//Get a baseline reading
  Serial.print("Zero_factor: ");//This can be used to remove the need to tare the scale
  Serial.println(zero_factor);

  ///////////////////////////////////////////////////////////////
  //Built in LED
  pinMode( LED_MODULE, OUTPUT );
  digitalWrite( LED_MODULE, HIGH );
}

void loop() 
{
  scale.set_scale(calibration_factor);//Adjust to calibration factor
  if (!client.connected()) 
  {
    reconnect();
  }
  client.loop();//to allow client to process incoming messages to send publish data and make a refresh on the connection
  if( !mfrc522.PICC_IsNewCardPresent() )
  {
    return;
  }
  //Select one of the cards
  if( !mfrc522.PICC_ReadCardSerial() )
  {
    return;
  }
  String RFID = Display_UID();
  float weight = 0;
  uint8_t flagAccess = Access( RFID );
  if( flagAccess )
  {
    digitalWrite( LED_MODULE, LOW );//turn LED on indicating weight scanning
    flagAccess = FALSE;
    weight = check_weight();
    Send_to_Cloud( RFID, weight );
    delay(10000);
    if (!client.connected()) 
    {
      reconnect();
    }
    delay(10000);
    if (!client.connected()) 
    {
      reconnect();
    }
    weight = check_weight();
    Send_to_Cloud( RFID, weight );
    digitalWrite( LED_MODULE, HIGH );//turn LED off
  }
  /////////////////////////////////////////////////////////////////////////////
  //Load Cell, adjust calibration factor
  char temp = 0;
  scale.set_scale(calibration_factor);//Adjust to calibration factor
  if(Serial.available())
  {
    serial_flag = TRUE;
    temp = Serial.read();
  }
  if(serial_flag)
  {
    serial_flag = FALSE;
    if( temp == '+' || temp == 'a' )
    {
      calibration_factor += 10;
      display_flag = TRUE;
    }
    else if( temp == '-' || temp == 'z' )
    {
      calibration_factor -= 10;
      display_flag = TRUE;
    }
    else if( temp == 'r' )
    {
      display_flag = TRUE;
    }
  }
    if(display_flag == TRUE)
  { //This is only to read scale on console, does not send anything
    display_flag = FALSE;
    Serial.println("Reading: ");
    Serial.print( (scale.get_units())*KG_CONVERSION, 1);
    Serial.println(" kg");
    //Change to kg and re-adjust calibration factor if following SI
    Serial.print("Calibration factor");
    Serial.print(calibration_factor);
    Serial.println();
  }
//////////////////////////////////////////////////////////////////////////////
}

float check_weight( void )
{
  float weight = 0;
  weight = ( scale.get_units() )*KG_CONVERSION;//weight displays in pounds without conversion
  return ( weight );
  
}

//debe de enviar la información en formato JSON:
/*{"container": "nombre del contenedor", "value": "weight", "rfid": "rfid" }*/
void Send_to_Cloud( String RFID, float weight )
{
  Serial.println( "Sending data to Cloud..." );
  
  if( client.connect( "IoT_ESP8266" ) )
  {
    String weight_s;
    weight_s = String(weight);
    char json_char[80] = {0};
    String json_string = "{\"container\": \"organico\", \"value\": \"";
    json_string = json_string + weight_s;
    json_string = json_string + "\", \"rfid\": \"";
    json_string = json_string + RFID;
    json_string = json_string + "\" }";
    json_string.toCharArray( json_char, 80 );
    client.publish( "outTopic", json_char );
  }
  else
  {
    Serial.print( "failed to send to Cloud, rc = " );
    Serial.println( client.state() );
  }
}
///////////////////////////////////////////////////////////////////////////////////
//RFID controller
String Display_UID()
{
   Serial.print( " UID Tag: " );
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

  if( ( content.substring(1) == "24 31 64 2B" ) || ( content.substring(1) == "37 71 B3 7A" ) )
  {
    access = ACCESS_GRANTED;
  }
  if( access )
  {
    Serial.println( "Access Granted" );
  }
  else
  {
    Serial.println( "Access Denied" );
  }
  return access;      
}

void ShowReaderDetails() 
{
    // Get the MFRC522 software version
    byte v = mfrc522.PCD_ReadRegister( mfrc522.VersionReg );
    Serial.print( F( "MFRC522 Software Version: 0x" ) );
    Serial.print( v, HEX );
    if ( 0x91 == v )
    {
        Serial.print( F( " = v1.0" ) );
    }
    else if ( 0x92 == v )
    {
        Serial.print( F( " = v2.0") );
    }
    else
    {
        Serial.print( F( " (unknown)" ) );
    }
    Serial.println("");
    // When 0x00 or 0xFF is returned, communication probably failed
    if ( ( 0x00 == v ) || ( 0xFF == v ) ) 
    {
        Serial.println( F( "WARNING: Communication failure, is the MFRC522 properly connected?" ) );
    }
}

///////////////////////////////////////////////////////////////////////////////////
void reconnect()
{
  Serial.println( "Attempting MQTT connection..." );
  if( client.connect( "IoT_ESP8266" ) )
  {
    Serial.println( "Connected..." );
    client.subscribe( "outTopic" );
    client.publish( "outTopic", "IoT_ESP8266 Connected" );
    Serial.println( "Subscribing..." );
    client.subscribe( "inTopic" );
  }
  else
  {
    Serial.print( "failed, rc = " );
    Serial.println( client.state() );
    Serial.println( "Try again in 5 seconds..." );
    delay( 500 );
  }
}

///////////////////////////////////////////////////////////////////////////////////
void setup_wifi() 
{
  delay(10);
  // We start by connecting to a WiFi network
  espClient.setBufferSizes(512, 512);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) 
  {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  timeClient.begin();
  while(!timeClient.update())
  {
    timeClient.forceUpdate();
  }

  espClient.setX509Time(timeClient.getEpochTime());

}

///////////////////////////////////////////////////////////////////////////////////
void load_security_files( void )
{
    Serial.print("Heap: "); Serial.println(ESP.getFreeHeap());

  // Load certificate file
  File cert = SPIFFS.open("/cert.der", "r");
  if (!cert) 
    {Serial.println("Failed to open cert file");}
  else
    {Serial.println("Success to open cert file");}   
  delay(1000);  
  if (espClient.loadCertificate(cert))
    {Serial.println("cert loaded");}
  else
    {Serial.println("cert not loaded");}

  // Load private key file
  File private_key = SPIFFS.open("/private.der", "r");
  if (!private_key) 
    {Serial.println("Failed to open private cert file");}
  else
    {Serial.println("Success to open private cert file");}  
  delay(1000);
  if (espClient.loadPrivateKey(private_key))
    {Serial.println("private key loaded");}
  else
    {Serial.println("private key not loaded");}

  // Load CA file
  File ca = SPIFFS.open("/ca.der", "r"); //replace ca eith your uploaded file name
  if (!ca) 
    {Serial.println("Failed to open ca ");}
  else
    {Serial.println("Success to open ca");}  
  delay(1000);
  if(espClient.loadCACert(ca))
    {Serial.println("ca loaded");}
  else
    {Serial.println("ca failed");}
  
  Serial.print("Heap: "); Serial.println(ESP.getFreeHeap());
}
