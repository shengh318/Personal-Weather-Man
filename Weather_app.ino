#include <WiFi.h> //Connect to WiFi Network
#include <TFT_eSPI.h> // Graphics and font library for ST7735 driver chip
#include <SPI.h> //Used in support of TFT Display
#include <string.h>  //used for some string handling and processing.
#include <ArduinoJson.h>

TFT_eSPI tft = TFT_eSPI();  // Invoke library, pins defined in User_Setup.h


const uint8_t TEMP_SCREEN = 0;
const uint8_t OTHER_SCREEN = 1;
const uint8_t TURN = 2;

const long period = 60000;
unsigned long previousMillis = 0;
unsigned long currentMillis = 0;

const long time_period = 3000;
unsigned long previous_time_millis= 0;
unsigned long current_time_millis = 0;

uint8_t state;
uint8_t previous_state;


uint8_t CHANGE_BUTTON = 34;

const int RESPONSE_TIMEOUT = 6000; //ms to wait for response from host
const int GETTING_PERIOD = 2000; //periodicity of getting a number fact.
const int BUTTON_TIMEOUT = 1000; //button timeout in milliseconds
const uint16_t IN_BUFFER_SIZE = 5000; //size of buffer to hold HTTP request
const uint16_t OUT_BUFFER_SIZE = 5000; //size of buffer to hold HTTP response
char request_buffer[IN_BUFFER_SIZE]; //char array buffer to hold HTTP request
char response_buffer[OUT_BUFFER_SIZE]; //char array buffer to hold HTTP response

char network[] = "TP-Link_5A24";
char password[] = "13030513";

uint8_t scanning = 0; //set to 1 if you'd like to scan for wifi networks 

uint8_t channel = 1; //network channel on 2.4 GHz
byte bssid[] = {0x04, 0x95, 0xE6, 0xAE, 0xDB, 0x41}; //6 byte MAC address of AP you're targeting.

double latitude = 43.158679;
double longitude = -76.33271;
const char API_KEY[] = "17fa85997281567e98a7dabfd46f9fa9";
const char units[] = "imperial";

char host[] = "api.openweathermap.org";

void setup(){
  tft.init();  //init screen
  tft.setRotation(1); //adjust rotation
  tft.setTextSize(3); //default font size
  tft.fillScreen(TFT_BLACK); //fill background
  tft.setTextColor(TFT_WHITE, TFT_BLACK); //set color of font to green foreground, black background
  Serial.begin(115200); //begin serial comms
  if (scanning){
    int n = WiFi.scanNetworks();
    Serial.println("scan done");
    if (n == 0) {
      Serial.println("no networks found");
    } else {
      Serial.print(n);
      Serial.println(" networks found");
      for (int i = 0; i < n; ++i) {
        Serial.printf("%d: %s, Ch:%d (%ddBm) %s ", i + 1, WiFi.SSID(i).c_str(), WiFi.channel(i), WiFi.RSSI(i), WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "open" : "");
        uint8_t* cc = WiFi.BSSID(i);
        for (int k = 0; k < 6; k++) {
          Serial.print(*cc, HEX);
          if (k != 5) Serial.print(":");
          cc++;
        }
        Serial.println("");
      }
    }
  }
  delay(100); //wait a bit (100 ms)

  //if using regular connection use line below:
  WiFi.begin(network, password);
  //if using channel/mac specification for crowded bands use the following:
  //WiFi.begin(network, password, channel, bssid);
  uint8_t count = 0; //count used for Wifi check times
  Serial.print("Attempting to connect to ");
  Serial.println(network);
  while (WiFi.status() != WL_CONNECTED && count<6) {
    delay(500);
    Serial.print(".");
    count++;
  }
  delay(2000);
  if (WiFi.isConnected()) { //if we connected then print our IP, Mac, and SSID we're on
    Serial.println("CONNECTED!");
    Serial.printf("%d:%d:%d:%d (%s) (%s)\n",WiFi.localIP()[3],WiFi.localIP()[2],
                                            WiFi.localIP()[1],WiFi.localIP()[0], 
                                          WiFi.macAddress().c_str() ,WiFi.SSID().c_str());
    delay(500);
  } else { //if we failed to connect just Try again.
    Serial.println("Failed to Connect :/  Going to restart");
    Serial.println(WiFi.status());
    ESP.restart(); // restart the ESP (proper way)
  }
  pinMode(CHANGE_BUTTON, INPUT_PULLUP);
  delay(2000);
  Serial.println("GETTING NEW DATA");
  get_all_values();
  get_all_time_values();
  previous_time_millis = millis();
  print_temp_screen();
  state = TEMP_SCREEN;
  previousMillis = millis();
}

uint8_t char_append(char* buff, char c, uint16_t buff_size) {
  int len = strlen(buff);
  if (len > buff_size) return false;
  buff[len] = c;
  buff[len + 1] = '\0';
  return true;
}

void do_http_GET(char* host, char* request, char* response, uint16_t response_size, uint16_t response_timeout, uint8_t serial){
  WiFiClient client; //instantiate a client object
  if (client.connect(host, 80)) { //try to connect to host on port 80
    if (serial) Serial.print(request);//Can do one-line if statements in C without curly braces
    client.print(request);
    memset(response, 0, response_size); //Null out (0 is the value of the null terminator '\0') entire buffer
    uint32_t count = millis();
    while (client.connected()) { //while we remain connected read out data coming back
      client.readBytesUntil('\n',response,response_size);
      if (serial) Serial.println(response);
      if (strcmp(response,"\r")==0) { //found a blank line!
        break;
      }
      memset(response, 0, response_size);
      if (millis()-count>response_timeout) break;
    }
    memset(response, 0, response_size); 
    count = millis();
    while (client.available()) { //read out remaining text (body of response)
      char_append(response,client.read(),OUT_BUFFER_SIZE);
    }
    if (serial) Serial.println(response);
    client.stop();
    if (serial) Serial.println("-----------"); 
  }else{
    if (serial) Serial.println("connection failed :/");
    if (serial) Serial.println("wait 0.5 sec...");
    client.stop();
  }
}           


void get_world_clock_json(){
  sprintf(request_buffer,"GET http://worldclockapi.com/api/json/est/now HTTP/1.1\r\n");
  strcat(request_buffer, "Host: worldclockapi.com\r\n"); //add more to the end
  strcat(request_buffer, "\r\n"); //add blank line!
  char host[] = "worldclockapi.com";
  do_http_GET(host, request_buffer, response_buffer, OUT_BUFFER_SIZE, RESPONSE_TIMEOUT, true);
}

void get_weather_json(){
  sprintf(request_buffer,"GET https://api.openweathermap.org/data/2.5/weather?lat=43.158679&lon=-76.33271&appid=17fa85997281567e98a7dabfd46f9fa9&units=imperial HTTP/1.1\r\n");
  strcat(request_buffer, "Host: api.openweathermap.org\r\n"); //add more to the end
  strcat(request_buffer, "\r\n"); //add blank line!
  char host[] = "api.openweathermap.org";
  do_http_GET(host, request_buffer, response_buffer, OUT_BUFFER_SIZE, RESPONSE_TIMEOUT, true);
}

double current_temperature = 0.0;
double feels_like = 0.0;
double min_temp = 0.0;
double max_temp = 0.0;
double pressure = 0.0;
double humidity = 0.0;
double wind_speed = 0.0;
char weather_condition[50];


char day_of_the_week[10];
char current_date[50];
char current_time[50];
char total_date_time[100];
char total_day_of_week[50];

void get_all_time_values(){
    memset(current_date, 0, 50);
    memset (current_time, 0, 50);
    memset(day_of_the_week, 0, 3);
 
    get_world_clock_json();
    StaticJsonDocument<5000> doc;
    DeserializationError error = deserializeJson(doc, response_buffer);
    if (error) {
      Serial.print(F("deserializeJson() failed: "));
      Serial.println(error.f_str());
      return;
    }
    //day_of_the_week = doc["dayofTheWeek"]
    sprintf(total_day_of_week, doc["dayOfTheWeek"]);
    for (int i = 0; i<3; i++){
      char temp = total_day_of_week[i];
      day_of_the_week[i] = temp;
    }
    //sprintf(day_of_the_week, doc["dayOfTheWeek"]);
    
    sprintf(total_date_time, doc["currentDateTime"]);
    //2022-06-03T09:07-04:00
    int length_of_string = strlen(total_date_time);
    int t_index;
    
    for (int i = 0; i < length_of_string; i++){
      char temp = total_date_time[i];
      if (temp == 'T'){
        t_index = i;
        break;
      }
      else{
        current_date[i] = temp;
      }
    }


    for (int i = t_index+1; i < length_of_string; i++){
      char temp = total_date_time[i];
      if (temp == '-'){
        break;  
      }
      else{
        current_time[i - t_index-1] = total_date_time[i];
      }  
    }
    
//    Serial.println(day_of_the_week);
//    Serial.println(total_date_time);
//    Serial.println(current_date);
//    Serial.println(current_time);
}

void get_all_values(){
    get_weather_json();
    StaticJsonDocument<5000> doc;
    DeserializationError error = deserializeJson(doc, response_buffer);
    if (error) {
      Serial.print(F("deserializeJson() failed: "));
      Serial.println(error.f_str());
      return;
    }
    current_temperature = doc["main"]["temp"];
    feels_like = doc["main"]["feels_like"];
    min_temp = doc["main"]["temp_min"];
    max_temp = doc["main"]["temp_max"];
    humidity = doc["main"]["humidity"];
    wind_speed = doc["wind"]["speed"];
    sprintf(weather_condition, doc["weather"][0]["main"]);
}

void print_temp_screen(){
  tft.setCursor(0,0,1);
  tft.setTextSize(2);
  tft.println(current_date);
  tft.print(day_of_the_week);
  tft.print(":");
  tft.println(weather_condition);
  tft.println(current_time);
  tft.println("--------");
  tft.print("Now:");
  tft.println(current_temperature);
  tft.println("--------");
  tft.print("Feels:");
  tft.println(feels_like);
}

void print_other_info_screen(){
  tft.setCursor(0,0,1);
  tft.setTextSize(2); //default font size
  tft.print("Low:");
  tft.println(min_temp);
  tft.println("--------");
  tft.print("High:");
  tft.println(max_temp);
  tft.println("--------");
  tft.print("humid:");
  tft.println(humidity);
  tft.println("--------");
  tft.print("wind:");
  tft.println(wind_speed);
}

char previous_date[50];
char previous_time[50];
char previous_day[50];

void loop() {
  uint8_t change_button = digitalRead(CHANGE_BUTTON);
  currentMillis = millis();
  current_time_millis = millis();
  

  
  
  
  switch(state){
    case TEMP_SCREEN:

      if (current_time_millis - previous_time_millis >= time_period){
          sprintf(previous_date, current_date);
          sprintf(previous_time, current_time);
          sprintf(previous_day, day_of_the_week);
          get_all_time_values();
          if (strcmp(previous_date, current_date)!=0){
            tft.fillScreen(TFT_BLACK);
            print_temp_screen();
          }
          else if (strcmp(previous_time, current_time)!=0){
            tft.fillScreen(TFT_BLACK);
            print_temp_screen();
          }
          
          else if (strcmp(previous_day, day_of_the_week)!=0){
            tft.fillScreen(TFT_BLACK);
            print_temp_screen();
          }
            previous_time_millis = current_time_millis;
      }
  
      if (currentMillis - previousMillis >= period){
        Serial.println("GETTING NEW DATA");
        get_all_values();
        tft.fillScreen(TFT_BLACK);
        print_temp_screen();
        previousMillis = currentMillis;
      }

      if (change_button == 0){
          previous_state = TEMP_SCREEN;
          state = TURN;
      }
    break;

    case OTHER_SCREEN:
      if (currentMillis - previousMillis >= period){
        Serial.println("GETTING NEW DATA");
        get_all_values();
        tft.fillScreen(TFT_BLACK);
        print_other_info_screen();
        previousMillis = currentMillis;
      }

      if (change_button == 0){
          previous_state = OTHER_SCREEN;
          state = TURN;
        }
    break;

    case TURN:
      if (change_button == 1){
        if (previous_state == TEMP_SCREEN){
                  tft.fillScreen(TFT_BLACK);
                  state = OTHER_SCREEN;  
                  print_other_info_screen();
        }
        else if (previous_state == OTHER_SCREEN){
                  tft.fillScreen(TFT_BLACK);
                  state = TEMP_SCREEN;
                  print_temp_screen();
          }
      }
    break;
    
    }

}
