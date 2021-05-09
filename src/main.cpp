#include <Arduino.h>
#include "WiFi.h"
#include "PubSubClient.h"
#include "DHT.h"
#include "time.h"
#include "Config.h"
#include "AnalogSensor.h"
#include "EEPROM.h"

// sensors
int getLux(int AR);
AnalogSensor lightSensor(LIGHT_SENSOR_PIN, getLux);
AnalogSensor soilMoistureSensor(MOISTURE_SENSOR_PIN, MOISTURE_SENSOR_MIN_VALUE, MOISTURE_SENSOR_MAX_VALUE, SENSOR_SWITCH_PIN);
AnalogSensor waterLevelSensor(WATER_LEVEL_SENSOR_PIN, WATER_LEVEL_SENSOR_MIN_VALUE, WATER_LEVEL_SENSOR_MAX_VALUE, SENSOR_SWITCH_PIN);
DHT dht(DHTPIN, DHTTYPE);

//NTP server
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 3600; //gtm +1
const int   daylightOffset_sec = 3600; //dayligh saving

//topics
char lightSensorTopic[80];
char soilMoistureSensorTopic[80];
char waterLevelSensorTopic[80];
char airTemperatureSensorTopic[80];
char airHumuditySensorTopic[80];
char waterPumpTopic[80];

//used to store message
char waterPumpTopicPayload[80];
int waterPumpTopicPayloadLenght = 0;

//used fore timeing
unsigned long miliSecLast;
unsigned long miliSecNext;

int err = 0; //wifi:1  MQTT:2

int led = 2;
char eRead[40];
byte len;

char ssid[40];
char pass[40];
char userHash[40];
char planterName[40];

WiFiClient espClient;
PubSubClient client(espClient);


//converts input from KY-018 module to lux
int getLux(int AR){ 
  Serial.print("AR:");
  Serial.println(AR);
  float voltage = AR * (3.3/4095) * 1000;
  Serial.println(voltage);
  float resistance = 10000 * ( voltage / ( 5000.0 - voltage) );
  Serial.println(resistance);
  int lux = (500.0 / resistance)*1000;
  Serial.println(lux);
  return lux > 100000 ? 100000 : lux;
}

// Saves string do EEPROM
void SaveString(int startAt, const char* id)
{
  for (byte i = 0; i <= strlen(id); i++)
  {
    EEPROM.write(i + startAt, (uint8_t) id[i]);
  }
  EEPROM.commit();
}

// Reads string from EEPROM
void ReadString(byte startAt, byte bufor, char* eRead)
{
  for (byte i = 0; i <= bufor; i++)
  {
    eRead[i] = (char)EEPROM.read(i + startAt);
  }
  len = bufor;
}

//Saves byte to EEPROM
void SaveByte(int startAt, byte val)
{
  EEPROM.write(startAt, val);
  EEPROM.commit();
}

//Reads byte from EEPROM
byte ReadByte(byte startAt)
{
  byte Read = -1;
  Read = EEPROM.read(startAt);
  return Read;
}

//loads preset from eprom
void loadFromEprom(){
  byte x = ReadByte(0);
  ReadString(1,x,ssid);

  x = ReadByte(41);
  ReadString(42,x,pass);

  x = ReadByte(83);
  ReadString(84,x,planterName);

  x = ReadByte(125);
  ReadString(126,x,userHash);
}

//reads input from serial port
void readSerial(char* var, int lenght = 40){
  while (Serial.available() == 0) {
    // wait
  }
  Serial.readBytesUntil(10, var, lenght);
}

//loads values from serial port and saves them to eprom
void setupThroughSerial(){
  pinMode(led, OUTPUT);
  digitalWrite(led, HIGH);

  delay(10000);

  Serial.println();
  Serial.print("Enter your WiFi credentials.\n (do not fill to retain old value)\n");

  Serial.print("SSID: ");
  readSerial(ssid);
  Serial.println(ssid);

  Serial.print("PASSWORD: ");
  readSerial(pass);
  Serial.println(pass);

  Serial.println();
  Serial.println("Enter planter configuration.\n (do not fill to retain old value)\n");

  Serial.print("PLANTER ID: ");
  readSerial(planterName);
  Serial.println(planterName);

  Serial.print("USER HASH: ");
  readSerial(userHash);
  Serial.println(userHash);



byte x;
if (strlen(ssid))
{
  SaveByte(0,strlen(ssid));
  SaveString(1,ssid);
}else{
  x = ReadByte(0);
  ReadString(1,x,ssid);
}

if (strlen(pass))
{
  SaveByte(41,strlen(pass));
  SaveString(42,pass);
}else{
  x = ReadByte(41);
  ReadString(42,x,pass);
}

if (strlen(planterName))
{
  SaveByte(83,strlen(planterName));
  SaveString(84,planterName);
}else{
  x = ReadByte(83);
  ReadString(84,x,planterName);
}

if (strlen(userHash))
{
  SaveByte(125,strlen(userHash));
  SaveString(126,userHash);
}else{
  x = ReadByte(125);
  ReadString(123,x,userHash);
}
  
  int counter = 0;

  WiFi.begin(ssid, pass);
  while (counter < 30 && WiFi.status() != WL_CONNECTED) {
    counter ++;
    delay(200);
    Serial.print(".");
  }
  Serial.println();  
  Serial.println(counter < 30 ? "wifi connected sucessfuly!" : "wifi did NOT connect sucessfuly!!!");

  while (true)
  {
    //wait for restart
  }
}

//callback function form MQTT client
void callback(char* topic, byte* payload, unsigned int length) {

  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  
  if (String(topic) == String(waterPumpTopic))
  {
    //max payload lenght 50
    waterPumpTopicPayloadLenght = length > 50 ? 50 : length;
    for (int i = 0; i < waterPumpTopicPayloadLenght; i++) {
      waterPumpTopicPayload[i] = (char)payload[i];  
    }
  } else {
  }
};

//updates led based on err and watersesor values
void updateLed(){
  if (waterLevelSensor.getLastValue() > 80)
  {
    digitalWrite(WATER_LED_R_PIN, LOW);
    digitalWrite(WATER_LED_G_PIN, HIGH);
    digitalWrite(WATER_LED_B_PIN, LOW);
  }else if (waterLevelSensor.getLastValue() > 25)
  {
    digitalWrite(WATER_LED_R_PIN, LOW);
    digitalWrite(WATER_LED_G_PIN, LOW);
    digitalWrite(WATER_LED_B_PIN, HIGH);
  }else{
    digitalWrite(WATER_LED_R_PIN, HIGH);
    digitalWrite(WATER_LED_G_PIN, LOW);
    digitalWrite(WATER_LED_B_PIN, LOW);
  }
  
  switch (err)
  {
  case 0:
    digitalWrite(ERR_LED_R_PIN, LOW);
    digitalWrite(ERR_LED_G_PIN, HIGH);
    digitalWrite(ERR_LED_B_PIN, LOW);
    break;
  case 2:
    digitalWrite(ERR_LED_R_PIN, HIGH);
    digitalWrite(ERR_LED_G_PIN, LOW);
    digitalWrite(ERR_LED_B_PIN, HIGH);
    break;
  default:
    digitalWrite(ERR_LED_R_PIN, HIGH);
    digitalWrite(ERR_LED_G_PIN, LOW);
    digitalWrite(ERR_LED_B_PIN, LOW);
    break;
  }
  
};



void publishData(){
  client.publish(lightSensorTopic, String(lightSensor.getValue()).c_str());
  client.publish(waterLevelSensorTopic, String(waterLevelSensor.getValue()).c_str());
  client.publish(soilMoistureSensorTopic, String(soilMoistureSensor.getValue()).c_str());
  client.publish(airTemperatureSensorTopic, String(dht.readTemperature()).c_str());
  client.publish(airHumuditySensorTopic, String(dht.readHumidity()).c_str());
};

void reconnectWifi() {

  if (WiFi.status() != WL_CONNECTED){  
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, pass);

    int counter = 0;
    while (WiFi.status() != WL_CONNECTED && counter < 60) {
      counter ++;
      delay(500);
    }
  }

  // update err
  if (WiFi.status() != WL_CONNECTED){
    err += err == 0 || err == 2 ? 1 : 0;
    Serial.println("WiFi failed");
  }else{
    err -= err == 1 || err == 3 ? 1 : 0;
  }

  randomSeed(micros());
}

void reconnectMqtt() {
  
  

  if (!client.connected())
  {
    int counter = 0;
    String clientId = "ESP8266Client-";
    do {
      counter ++;
      clientId += String(random(0xffff), HEX);
      // Attempt to connect
      if (client.connect(clientId.c_str(), MQTT_USER, MQTT_PASSWD)) {
        client.subscribe(waterPumpTopic);
      } else {
        // Wait 5 seconds before retrying
        delay(5000);
      }
    } while (!client.connected() && counter < 3);
  }

  
  

  if (!client.connected()){
    err += err == 0 || err == 1 ? 2 : 0;
  }else{
    err -= err == 2 || err == 3 ? 2 : 0;
  }
      
}

void runPump(){
  if (waterPumpTopicPayloadLenght == 1 && waterPumpTopicPayload[0] == '1')
  {
    digitalWrite(WATER_PUMP_PIN, HIGH);
    delay(WATER_PUMP_TIME);
    digitalWrite(WATER_PUMP_PIN, LOW);
    client.publish(waterPumpTopic,"2",true);
  }
  
}

void setup() {
  Serial.begin(9600);

  pinMode(MODE_PIN, INPUT);
  EEPROM.begin(512);
  if (digitalRead(MODE_PIN) == HIGH)
  {
    setupThroughSerial();
  }

  loadFromEprom();

   sprintf(lightSensorTopic,"%s/%s/%s", userHash, planterName,"lightSensor");
   sprintf(waterLevelSensorTopic,"%s/%s/%s", userHash, planterName,"waterLevelSensor");
   sprintf(soilMoistureSensorTopic,"%s/%s/%s", userHash, planterName,"soilMoistureSensor");
   sprintf(airHumuditySensorTopic,"%s/%s/%s", userHash, planterName,"airHumiditySensor");
   sprintf(airTemperatureSensorTopic,"%s/%s/%s", userHash, planterName,"airTemperatureSensor");
   sprintf(waterPumpTopic,"%s/%s/%s", userHash, planterName,"waterPump");
   
  // setup inputs and outputs
  pinMode(WATER_PUMP_PIN, OUTPUT);
  pinMode(WATER_LED_R_PIN, OUTPUT);
  pinMode(WATER_LED_G_PIN, OUTPUT);
  pinMode(WATER_LED_B_PIN, OUTPUT);
  pinMode(ERR_LED_R_PIN, OUTPUT);
  pinMode(ERR_LED_G_PIN, OUTPUT);
  pinMode(ERR_LED_B_PIN, OUTPUT);
  pinMode(SENSOR_SWITCH_PIN, OUTPUT);
  digitalWrite(SENSOR_SWITCH_PIN,LOW);

  pinMode(LIGHT_SENSOR_PIN, INPUT);
  pinMode(WATER_LEVEL_SENSOR_PIN, INPUT);
  pinMode(MOISTURE_SENSOR_PIN, INPUT);

  dht.begin();

  reconnectWifi();
  client.setServer(MQTT_SERVER, 1883);
  client.setCallback(callback);
  reconnectMqtt();
  client.subscribe(waterPumpTopic);
  Serial.println(lightSensorTopic);
  Serial.println(waterLevelSensorTopic);
  Serial.println(soilMoistureSensorTopic);
  Serial.println(airTemperatureSensorTopic);
  Serial.println(airHumuditySensorTopic);
  Serial.println(waterPumpTopic);
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  miliSecLast = miliSecNext = micros();
}

int NextPublishSec(){
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    return UPDATE_INTERVAL*60;
  }
  
  char t[3];
  strftime(t,3, "%M", &timeinfo);
  int min = atoi( t );
  strftime(t,3, "%S", &timeinfo);
  int sec = atoi( t );
  
  int result = (UPDATE_INTERVAL - min % UPDATE_INTERVAL) * 60 - sec;
  Serial.print("result: ");
  Serial.println(result);
  return result;
}


bool nextPublish(){
  unsigned long now = micros();

  if (miliSecLast > miliSecNext)
  {
    return (now < miliSecLast && now >= miliSecNext);
  }else{
    return (now < miliSecLast || now >= miliSecNext);
  }
}

void loop() {  
  reconnectWifi();
  reconnectMqtt();
  client.loop();
  if (!err)
  {
    if (nextPublish()){
      publishData();
      miliSecLast = micros();
      miliSecNext = miliSecLast + (NextPublishSec() * 1000000);
    }
    runPump();
  }
  updateLed();
  delay(500); //wait 0.5 sec
};