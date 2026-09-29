/*
  Smart Collar - ESP32 integrated sketch
  - DS18B20 (OneWire)         -> GPIO4
  - MPU6050 (I2C)             -> SDA=21, SCL=22
  - NEO-6M GPS (UART1)        -> RX=16 (GPS TX), TX=17 (GPS RX)
  - microSD (SPI)             -> CS=5, MOSI=23, MISO=19, SCK=18
  - Blynk Virtuals:
       V1 -> GPS string "Lat:...,Lng:..."
       V2 -> Temperature (float)
       V3 -> Activity (0/1)
       V9 -> Geofence status/message (string)
  - Note: replace WIFI_SSID, WIFI_PASS, BLYNK_AUTH below
*/

#define BLYNK_TEMPLATE_ID   "TMPL3w0DWuwGl"
#define BLYNK_TEMPLATE_NAME "Smart Collar"
#define BLYNK_AUTH_TOKEN    "h7liaui6QTL5Q62qcB8xfSlcpQ3DeqTO"


#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <TinyGPSPlus.h>
#include <HardwareSerial.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <SD.h>
#include <SPI.h>
#include <WiFi.h>
#include <BlynkSimpleEsp32.h>

// ---------------- CONFIG ----------------
#define WIFI_SSID     "Vaishuu"
#define WIFI_PASS     "Vaishuu7104"
#define BLYNK_AUTH    "h7liaui6QTL5Q62qcB8xfSlcpQ3DeqTO"

// Pins / hardware config
#define DS18B20_PIN    4      // OneWire data pin
#define SD_CS_PIN      5      // SD card CS
// MPU6050 uses Wire default (SDA=21, SCL=22)
// GPS uses Serial1 -> RX=16, TX=17

// Temperature threshold (degC)
float TEMP_THRESHOLD = 39.0; // set according to animal baseline; you can tune

// Motion threshold: acceleration magnitude (m/s^2) above which we say "high activity"
float MOTION_THRESHOLD = 6.0; // ~0.6g (adjust as needed)

// Geofence: center lat/lng and radius in meters
const double GEOFENCE_LAT = 18.649545;  // example: replace with farmer center lat
const double GEOFENCE_LNG = 73.762111;  // example: replace with farmer center lng
const double GEOFENCE_RADIUS_M = 200.0; // 200 meters radius (adjust)

// Logging interval (ms)
const unsigned long LOOP_INTERVAL_MS = 1000UL;

// ---------------- Globals ----------------
Adafruit_MPU6050 mpu;
TinyGPSPlus gps;
HardwareSerial GPSserial(1);
OneWire oneWire(DS18B20_PIN);
DallasTemperature ds18(&oneWire);

File logFile;

bool lastTempAbove = false;      // for rise/fall detection
bool isBlynkConnected = false;

unsigned long lastLoop = 0;
unsigned long lastBlynkCheck = 0;
const unsigned long BLYNK_CHECK_INTERVAL = 5000;

// ---------------- Blynk setup ----------------
BlynkTimer timer; // not strictly needed, we'll use loop timing
// Virtual pins mapping (keep consistent with your dashboard)
const int VP_GPS = V2;
const int VP_TEMP = V0;
const int VP_ACTIVITY = V1;
const int VP_STATUS = V5;

// ---------------- Helper functions ----------------
double haversineDistanceMeters(double lat1, double lon1, double lat2, double lon2) {
  // returns distance in meters between two lat/lon points
  const double R = 6371000.0; // Earth radius (m)
  double dLat = radians(lat2 - lat1);
  double dLon = radians(lon2 - lon1);
  double a = sin(dLat/2.0)*sin(dLat/2.0) +
             cos(radians(lat1))*cos(radians(lat2)) *
             sin(dLon/2.0)*sin(dLon/2.0);
  double c = 2.0 * atan2(sqrt(a), sqrt(1.0 - a));
  return R * c;
}

String getTimestamp() {
  // Prefer GPS time if valid; otherwise use millis since boot (seconds)
  if (gps.time.isValid() && gps.date.isValid()) {
    char buf[32];
    // format: YYYY-MM-DD HH:MM:SS (UTC from GPS)
    sprintf(buf, "%04d-%02d-%02d %02d:%02d:%02d",
            gps.date.year(), gps.date.month(), gps.date.day(),
            gps.time.hour(), gps.time.minute(), gps.time.second());
    return String(buf);
  } else {
    unsigned long s = millis() / 1000UL;
    unsigned long hh = s / 3600UL;
    unsigned long mm = (s % 3600UL) / 60UL;
    unsigned long ss = s % 60UL;
    char buf[32];
    sprintf(buf, "uptime %02lu:%02lu:%02lu", hh, mm, ss);
    return String(buf);
  }
}

bool sdWriteLine(const String &line) {
  // Append a line to the log file
  if (!SD.begin(SD_CS_PIN)) {
    Serial.println(F("SD.begin failed during write"));
    return false;
  }
  File f = SD.open("/datalog.csv", FILE_APPEND);
  if (!f) {
    Serial.println(F("Failed to open datalog.csv for append"));
    return false;
  }
  f.println(line);
  f.close();
  return true;
}

// ---------------- Setup ----------------
void setup() {
  Serial.begin(115200);
  delay(100);

  Serial.println(F("Smart Collar starting..."));

  // Initialize I2C and MPU6050
  Wire.begin(21, 22);
  if (!mpu.begin()) {
    Serial.println(F("Failed to find MPU6050 chip!"));
    // continue but MPU readings will be invalid
  } else {
    Serial.println(F("MPU6050 found"));
    mpu.setAccelerometerRange(MPU6050_RANGE_4_G);
    mpu.setGyroRange(MPU6050_RANGE_500_DEG);
    mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
  }

  // GPS serial (UART1)
  GPSserial.begin(9600, SERIAL_8N1, 16, 17); // RX, TX
  Serial.println(F("GPS serial begun"));

  // DS18B20
  ds18.begin();
  Serial.println(F("DS18B20 initialized"));

  // SD init
  if (!SD.begin(SD_CS_PIN)) {
    Serial.println(F("Warning: SD init failed - check connections and card"));
  } else {
    Serial.println(F("SD card initialized"));
    // Create header if file not present
    if (!SD.exists("/datalog.csv")) {
      File f = SD.open("/datalog.csv", FILE_WRITE);
      if (f) {
        f.println("timestamp,lat,lng,temperatureC,accel_magnitude_g,activity");
        f.close();
      }
    }
  }

  // Blynk + WiFi
  Blynk.begin(BLYNK_AUTH, WIFI_SSID, WIFI_PASS);
  // Blynk.begin blocks until connected; if you want non-blocking, use Blynk.config + Blynk.connect approach.
  Serial.println(F("Attempting Blynk connection..."));

  lastLoop = millis();
}

// ---------------- Main loop ----------------
void loop() {
  // GPS input read (non-blocking)
  while (GPSserial.available() > 0) {
    gps.encode(GPSserial.read());
  }

  // Blynk handling (if connected)
  if (Blynk.connected()) {
    Blynk.run();
    isBlynkConnected = true;
  } else {
    // try to reconnect periodically
    isBlynkConnected = false;
    if (millis() - lastBlynkCheck > BLYNK_CHECK_INTERVAL) {
      lastBlynkCheck = millis();
      Serial.println(F("Blynk not connected, trying to connect..."));
      Blynk.connect(5000); // try connect for 5s
    }
  }

  // perform main measurements at interval
  if (millis() - lastLoop >= LOOP_INTERVAL_MS) {
    lastLoop = millis();

    String timestamp = getTimestamp();

    // --- Temperature ---
    ds18.requestTemperatures();
    float tempC = ds18.getTempCByIndex(0);
    if (tempC == DEVICE_DISCONNECTED_C) {
      Serial.println(F("DS18B20 not responding"));
      tempC = -999.0;
    }

    // temp threshold rise/fall detection
    bool nowAbove = (tempC >= TEMP_THRESHOLD);
    if (tempC != -999.0) {
      if (nowAbove && !lastTempAbove) {
        // rising above threshold
        Serial.println(F("ALERT: Temperature rose above threshold"));
        if (isBlynkConnected) Blynk.virtualWrite(VP_STATUS, "Temp RISE above threshold");
      } else if (!nowAbove && lastTempAbove) {
        Serial.println(F("INFO: Temperature fell below threshold"));
        if (isBlynkConnected) Blynk.virtualWrite(VP_STATUS, "Temp FALL below threshold");
      }
      lastTempAbove = nowAbove;
    }

    // --- MPU6050 read ---
    sensors_event_t a, g, temp;
    mpu.getEvent(&a, &g, &temp); // acceleration in m/s^2
    // convert acceleration magnitude to "g"
    float ax = a.acceleration.x;
    float ay = a.acceleration.y;
    float az = a.acceleration.z;
    float accel_magnitude = sqrt(ax*ax + ay*ay + az*az); // m/s^2
    float accel_g = accel_magnitude / 9.80665; // g units

    // decide activity 1/0
    int activity = (accel_g >= (MOTION_THRESHOLD / 9.80665)) ? 1 : 0;
    // Note: MOTION_THRESHOLD originally in m/s^2; we used convert to g above. If you prefer threshold in g, adjust.

    // --- GPS location ---
    double lat = 0.0, lng = 0.0;
    bool gpsValid = false;
    if (gps.location.isValid()) {
      lat = gps.location.lat();
      lng = gps.location.lng();
      gpsValid = true;
    }

    // --- Geofence check ---
    bool outsideGeofence = false;
    String geoMsg = "Unknown";
    if (gpsValid) {
      double dist = haversineDistanceMeters(lat, lng, GEOFENCE_LAT, GEOFENCE_LNG);
      if (dist > GEOFENCE_RADIUS_M) {
        outsideGeofence = true;
        geoMsg = "OUTSIDE geofence! dist(m): " + String((int)dist);
        Serial.println(geoMsg);
        if (isBlynkConnected) Blynk.virtualWrite(VP_STATUS, geoMsg);
      } else {
        outsideGeofence = false;
        geoMsg = "Inside geofence, dist(m): " + String((int)dist);
      }
    } else {
      geoMsg = "GPS invalid";
    }

    // --- Compose CSV line and write to SD ---
    // timestamp,lat,lng,temperatureC,accel_magnitude_g,activity
    String csv = "";
    csv += timestamp + ",";
    if (gpsValid) {
      csv += String(lat, 6) + "," + String(lng, 6) + ",";
    } else {
      csv += "NA,NA,";
    }
    if (tempC != -999.0) csv += String(tempC, 2) + ",";
    else csv += "NA,";
    csv += String(accel_g, 3) + ",";
    csv += String(activity);

    if (!sdWriteLine(csv)) {
      Serial.println(F("Failed to write log to SD"));
    }

    // --- Serial debug output ---
    Serial.print("TS: "); Serial.print(timestamp);
    Serial.print(" | Temp: "); Serial.print(tempC, 2);
    Serial.print(" C | Activity: "); Serial.print(activity);
    Serial.print(" | Acc_g: "); Serial.print(accel_g, 3);
    if (gpsValid) {
      Serial.print(" | Lat: "); Serial.print(lat, 6);
      Serial.print(" Lng: "); Serial.print(lng, 6);
    } else {
      Serial.print(" | GPS: NA");
    }
    Serial.print(" | Geo: "); Serial.println(geoMsg);

    // --- Send to Blynk if connected ---
    if (isBlynkConnected) {
      // GPS string
      String gpsStr = "NA";
      if (gpsValid) gpsStr = "Lat:" + String(lat,6) + ",Lng:" + String(lng,6);
      Blynk.virtualWrite(VP_GPS, gpsStr);
      // Temp float
      if (tempC != -999.0) Blynk.virtualWrite(VP_TEMP, tempC);
      else Blynk.virtualWrite(VP_TEMP, "NA");
      // Activity
      Blynk.virtualWrite(VP_ACTIVITY, activity);
      // Status (geofence or messages)
      Blynk.virtualWrite(VP_STATUS, geoMsg);
    }
  } // end timed block
}
