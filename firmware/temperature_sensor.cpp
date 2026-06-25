#include <Wire.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>

const char* WIFI_SSID = "CHANGE THIS";
const char* WIFI_PASSWORD = "CHANGE THIS";

const char* THINGSPEAK_WRITE_API_KEY = "CHANGE THIS";

const float LATITUDE = CHANGE THIS;
const float LONGITUDE = CHANGE THIS;

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define DHTPIN 4
#define DHTTYPE DHT22

DHT dht(DHTPIN, DHTTYPE);

#define DISPLAY_ON_TIME 5000
#define SLEEP_TIME_MINUTES 15 

#define WINDOW_MARGIN 1.0

void setup() {
  setCpuFrequencyMhz(80);

  Serial.begin(115200);
  delay(200);

  WiFi.mode(WIFI_OFF);
  btStop();

  dht.begin();
  Wire.begin(21, 22);

  bool oledOK = display.begin(SSD1306_SWITCHCAPVCC, 0x3C);

  if (oledOK) {
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.dim(true);
    showSmallMessage("Starting...", "Reading sensor");
  }

  delay(2000);

  float indoorHumidity = dht.readHumidity();
  float indoorTemp = dht.readTemperature();

  float outsideTemp = NAN;
  bool uploadOK = false;

  if (!isnan(indoorTemp) && !isnan(indoorHumidity)) {
    if (oledOK) {
      showSmallMessage("Connecting WiFi", "Getting weather");
    }

    bool wifiOK = connectWiFi();

    if (wifiOK) {
      outsideTemp = getOutsideTemperature();

      if (oledOK) {
        showSmallMessage("Uploading...", "ThingSpeak");
      }

      uploadOK = sendToThingSpeak(indoorTemp, indoorHumidity, outsideTemp);
    }
  }

  if (oledOK) {
    showWeatherScreen(indoorTemp, indoorHumidity, outsideTemp, uploadOK);
    delay(DISPLAY_ON_TIME);

    display.ssd1306_command(SSD1306_DISPLAYOFF);
  }

  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);

  goToSleep();
}

void loop() {

}

bool connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.print("Connecting to WiFi");

  unsigned long startAttempt = millis();

  while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 12000) {
    delay(250);
    Serial.print(".");
  }

  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi connected");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    return true;
  }

  Serial.println("WiFi failed");
  return false;
}

float getOutsideTemperature() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("No WiFi for weather");
    return NAN;
  }

  String url = "http://api.open-meteo.com/v1/forecast?latitude=";
  url += String(LATITUDE, 4);
  url += "&longitude=";
  url += String(LONGITUDE, 4);
  url += "&current=temperature_2m&temperature_unit=celsius";

  HTTPClient http;
  http.begin(url);

  int httpCode = http.GET();

  if (httpCode != 200) {
    Serial.print("Weather HTTP error: ");
    Serial.println(httpCode);
    http.end();
    return NAN;
  }

  String payload = http.getString();
  http.end();

  StaticJsonDocument<768> doc;
  DeserializationError error = deserializeJson(doc, payload);

  if (error) {
    Serial.println("Weather JSON error");
    return NAN;
  }

  float temp = doc["current"]["temperature_2m"];

  Serial.print("Outside temp: ");
  Serial.println(temp);

  return temp;
}

bool sendToThingSpeak(float indoorTemp, float indoorHumidity, float outsideTemp) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("No WiFi for ThingSpeak");
    return false;
  }

  int windowAdvice = 0;

  if (!isnan(outsideTemp) && outsideTemp + WINDOW_MARGIN < indoorTemp) {
    windowAdvice = 1;
  }

  String url = "http://api.thingspeak.com/update?api_key=";
  url += THINGSPEAK_WRITE_API_KEY;

  url += "&field1=";
  url += String(indoorTemp, 1);

  url += "&field2=";
  url += String(indoorHumidity, 1);

  if (!isnan(outsideTemp)) {
    url += "&field3=";
    url += String(outsideTemp, 1);
  }

  url += "&field4=";
  url += String(windowAdvice);

  HTTPClient http;
  http.begin(url);

  int httpCode = http.GET();

  if (httpCode != 200) {
    Serial.print("ThingSpeak HTTP error: ");
    Serial.println(httpCode);
    http.end();
    return false;
  }

  String response = http.getString();
  http.end();

  Serial.print("ThingSpeak response: ");
  Serial.println(response);

  if (response.toInt() > 0) {
    Serial.println("ThingSpeak upload successful");
    return true;
  } else {
    Serial.println("ThingSpeak upload failed");
    return false;
  }
}

void showWeatherScreen(float indoorTemp, float indoorHumidity, float outsideTemp, bool uploadOK) {
  display.clearDisplay();

  if (isnan(indoorTemp) || isnan(indoorHumidity)) {
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("Sensor error");
    display.println("Check SIG wiring");
    display.display();
    return;
  }

  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("Night Logger");

  display.setCursor(0, 12);
  display.print("In:  ");
  display.print(indoorTemp, 1);
  display.print(" C ");
  display.print(indoorHumidity, 0);
  display.println("%");

  display.setCursor(0, 24);
  display.print("Out: ");

  if (isnan(outsideTemp)) {
    display.println("No data");
  } else {
    display.print(outsideTemp, 1);
    display.println(" C");
  }

  display.setCursor(0, 38);

  if (isnan(outsideTemp)) {
    display.println("Weather failed");
  } else if (outsideTemp + WINDOW_MARGIN < indoorTemp) {
    display.println("Open the window");
  } else {
    display.println("Keep window closed");
  }

  display.setCursor(0, 52);

  if (uploadOK) {
    display.println("Uploaded. Sleep 15m");
  } else {
    display.println("Upload failed. Sleep");
  }

  display.display();
}

void showSmallMessage(const char* line1, const char* line2) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println(line1);
  display.setCursor(0, 16);
  display.println(line2);
  display.display();
}

void goToSleep() {
  uint64_t sleepTime = (uint64_t)SLEEP_TIME_MINUTES * 60ULL * 1000000ULL;

  Serial.println("Going to sleep");
  Serial.flush();

  esp_sleep_enable_timer_wakeup(sleepTime);
  esp_deep_sleep_start();
}
