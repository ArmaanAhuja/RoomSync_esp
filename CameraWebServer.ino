#include "esp_camera.h"
#include <Arduino.h>
#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <Wire.h>
#include "time.h"

#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"

#define CAMERA_MODEL_AI_THINKER
#define API_KEY "AIzaSyDgDperrtprIFnPtoTCuLnYuxPpqQ6ytcI"
#define USER_EMAIL "cheifyofficial@gmail.com"
#define USER_PASSWORD "roomautomation"
#define DATABASE_URL "https://room-automation-a1ce8-default-rtdb.asia-southeast1.firebasedatabase.app/"
#include "camera_pins.h"

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config_firebase;


String uid;
String databasePath;
String lightPath = "/light";
String timePath = "/timestamp";
String parentPath;

int timestamp;
FirebaseJson json;

const char* ssid = "amit munjal";
const char* password = "munjal123";
 
const char* ntpServer = "pool.ntp.org";
 


int val= 0;
int prevVal = 1; 
int light_pin = 13; 
int fan_pin = 12; 
int motion_pin = 14; 
int motion_val = 0; 
String led_status = "off";  
String fan_val_from_app; 
String val_from_app; 
String prev_val_from_app; 
String prev_fan_val_from_app; 

unsigned long sendDataPrevMillis = 0;
unsigned long timerDelay = 20000;
 
 

void startCameraServer();
void setupLedFlash(int pin);

void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println();

  
  WiFi.begin(ssid, password);
  WiFi.setSleep(false);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
   Serial.println(WiFi.localIP());
  Serial.println();

  configTime(0, 0, ntpServer);
  config_firebase.api_key = API_KEY; 

  auth.user.email = USER_EMAIL; 
  auth.user.password = USER_PASSWORD;

  config_firebase.database_url = DATABASE_URL; 
  Firebase.reconnectWiFi(true);
  fbdo.setResponseSize(4096);

  config_firebase.token_status_callback = tokenStatusCallback; 
  config_firebase.max_token_generation_retry = 5; 

  Firebase.begin(&config_firebase, &auth); 

  Serial.println("Getting User UID");
  while ((auth.token.uid) == "") {
    Serial.print('.');
    delay(1000);
  }

  uid = auth.token.uid.c_str();
  Serial.print("User UID: ");
  Serial.println(uid);

  databasePath = "/UsersData/" + uid + "/readings/light_readings_from_esp";
  pinMode(light_pin,OUTPUT);
  pinMode(fan_pin, OUTPUT); 
  pinMode(motion_pin, INPUT); 
  // parentPath = databasePath; 
  if (Firebase.ready()){
    if(Firebase.RTDB.getString(&fbdo, "/UsersData/" +uid +"/readings/light_readings_from_app/light")){
      if(fbdo.dataType() == "string"){
        prev_val_from_app = fbdo.stringData(); 
        Serial.println(prev_val_from_app);
      
    }}
    if(Firebase.RTDB.getString(&fbdo, "/UsersData/" +uid +"/readings/fan_readings_from_app/fan")){
      if(fbdo.dataType() == "string"){
        prev_val_from_app = fbdo.stringData(); 
        Serial.println(prev_fan_val_from_app);
    }}
    }

  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 10000000;
  config.frame_size = FRAMESIZE_VGA;
  config.pixel_format = PIXFORMAT_JPEG; // for streaming
  //config.pixel_format = PIXFORMAT_RGB565; // for face detection/recognition
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.jpeg_quality = 12;
  config.fb_count = 1;
  
  // if PSRAM IC present, init with UXGA resolution and higher JPEG quality
  //                      for larger pre-allocated frame buffer.
  if(config.pixel_format == PIXFORMAT_JPEG){
    if(psramFound()){
      config.jpeg_quality = 40;
      config.fb_count = 2;
      config.grab_mode = CAMERA_GRAB_LATEST;
    } else {
      // Limit the frame size when PSRAM is not available
      config.frame_size = FRAMESIZE_SVGA;
      config.fb_location = CAMERA_FB_IN_DRAM;
    }
  } else {
    // Best option for face detection/recognition
    config.frame_size = FRAMESIZE_240X240;
#if CONFIG_IDF_TARGET_ESP32S3
    config.fb_count = 2;
#endif
  }

#if defined(CAMERA_MODEL_ESP_EYE)
  pinMode(13, INPUT_PULLUP);
  pinMode(14, INPUT_PULLUP);
#endif

  // camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  sensor_t * s = esp_camera_sensor_get();
  // initial sensors are flipped vertically and colors are a bit saturated
  if (s->id.PID == OV3660_PID) {
    s->set_vflip(s, 1); // flip it back
    s->set_brightness(s, 1); // up the brightness just a bit
    s->set_saturation(s, -2); // lower the saturation
  }
  // drop down frame size for higher initial frame rate
  if(config.pixel_format == PIXFORMAT_JPEG){
    s->set_framesize(s, FRAMESIZE_QVGA);
  }

#if defined(CAMERA_MODEL_M5STACK_WIDE) || defined(CAMERA_MODEL_M5STACK_ESP32CAM)
  s->set_vflip(s, 1);
  s->set_hmirror(s, 1);
#endif

#if defined(CAMERA_MODEL_ESP32S3_EYE)
  s->set_vflip(s, 1);
#endif

// Setup LED FLash if LED pin is defined in camera_pins.h
#if defined(LED_GPIO_NUM)
  setupLedFlash(LED_GPIO_NUM);
#endif


  startCameraServer();

  Serial.print("Camera Ready! Use 'http://");
  Serial.print(WiFi.localIP());
  Serial.println("' to connect");


}

void loop() {

//  camera_fb_t *fb = esp_camera_fb_get(); 
//    if(!fb){
//      Serial.println("Camera capture failed");
//      esp_camera_fb_return(fb); 
//      return; 
//    }
//    if(fb->format != PIXFORMAT_JPEG){
//      Serial.println("non jpeg data not implemented");
//      return;
//    }
//    client.sendBinary((const char*) fb->buf, fb->len); 
//    esp_camera_fb_return(fb); 


    motion_val = digitalRead(motion_pin); 

    if(motion_val == 1){
      digitalWrite(light_pin, HIGH); 
      digitalWrite(fan_pin, HIGH); 
      Firebase.RTDB.setString(&fbdo, "/UsersData/" +uid +"/readings/light_readings_from_app/light", "on"); 
      Firebase.RTDB.setString(&fbdo, "/UsersData/" +uid +"/readings/fan_readings_from_app/fan", "on"); 

    }
    if(motion_val == 0){
      digitalWrite(light_pin, LOW); 
      digitalWrite(fan_pin, LOW); 
      Firebase.RTDB.setString(&fbdo, "/UsersData/" +uid +"/readings/light_readings_from_app/light", "off"); 
      Firebase.RTDB.setString(&fbdo, "/UsersData/" +uid +"/readings/fan_readings_from_app/fan", "off"); 

    }
  if (Firebase.ready()){
    if(Firebase.RTDB.getString(&fbdo, "/UsersData/" +uid +"/readings/light_readings_from_app/light")){
      if(fbdo.dataType() == "string"){
        val_from_app = fbdo.stringData(); 
        Serial.print("light: ");
        Serial.println(val_from_app);
      
    }}

    if(val_from_app == "off"){
      digitalWrite(light_pin,  LOW);
      prev_val_from_app = val_from_app;  
    }
    else if(val_from_app == "on"){
      digitalWrite(light_pin, HIGH); 
      prev_val_from_app = val_from_app; 
    }



     if(Firebase.RTDB.getString(&fbdo, "/UsersData/" +uid +"/readings/fan_readings_from_app/fan")){
      if(fbdo.dataType() == "string"){
        fan_val_from_app = fbdo.stringData(); 
        Serial.print("fan: ");
        Serial.println(fan_val_from_app);
      
    }

    if(fan_val_from_app == "off"){
      digitalWrite(fan_pin,  LOW);
      prev_fan_val_from_app = fan_val_from_app;  
    }
    else if(fan_val_from_app == "on"){
      digitalWrite(fan_pin, HIGH); 
      prev_fan_val_from_app = fan_val_from_app; 
    }
  }
     
  }


  
}
