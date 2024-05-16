#include <WiFiUdp.h>
#include <NTPClient.h>
#include <Firebase_ESP_Client.h>
#include <addons/TokenHelper.h>
#include <TimeLib.h>
#include <SoftwareSerial.h>
#include <ESP8266WiFi.h>

#define RXD2 13
#define TXD2 15

// Define WiFi credentials
#define WIFI_SSID "Galaxy"
#define WIFI_PASSWORD "galaksih"

// Define Firebase API Key, Project ID, and user credentials
#define API_KEY "AIzaSyBH0cPzGCSELbqEAcEu4xiGZoJZGkQDwqw"
#define FIREBASE_PROJECT_ID "obras-7eb0b"
#define USER_EMAIL "admin@admin.dev"
#define USER_PASSWORD "admin123"

// Define NTP Client to get time
WiFiUDP ntpUDP;
// Define the NTP Client with a time offset (7 hours in seconds)
NTPClient timeClient(ntpUDP, "pool.ntp.org"); // 25200 seconds = 7 hours

// Define Firebase Data object, Firebase authentication, and configuration
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

SoftwareSerial Serial2(RXD2, TXD2);
String prevTagId; // String to store the previous tag ID

// Function to convert epoch time to timestamp string
String epochToTimestamp(unsigned long epochTime) {
  tmElements_t tm;
  breakTime(epochTime, tm);
  char timestamp[21];
  sprintf(timestamp, "%04d-%02d-%02dT%02d:%02d:%02dZ", tm.Year + 1970, tm.Month, tm.Day, tm.Hour, tm.Minute, tm.Second);
  return String(timestamp);
}

// Function to read tag data
String readTag() {
  // Read data from RFID
  int data = Serial2.read();
  static String currentTag;
  String previousTag;

  char hex[3];
  sprintf(hex, "%02X", data);
  currentTag += hex;

  if (currentTag.length() == 36) {
    previousTag = currentTag;
    currentTag = ""; // Reset for the next tag
    return previousTag;
  }

  return "";
}

void sentToFirestore(String tagId, String timestamp) {
  // Create a FirebaseJson object for storing data
  FirebaseJson content;
  // Define the path to the Firestore document
  String documentPath = "tags/" + tagId;
  // Set the 'TagID' and 'timestamp' fields in the FirebaseJson object
  content.set("fields/TagID/stringValue", tagId);
  content.set("fields/timeStamp/timestampValue", timestamp); // RFC3339 UTC "Zulu" format

  Serial.print("Update/Add RFID Tag Data... ");

  // Use the patchDocument method to update the TagID and timeStamp Firestore document
  if (Firebase.Firestore.patchDocument(&fbdo, FIREBASE_PROJECT_ID, "", documentPath.c_str(), content.raw(), "TagID") && Firebase.Firestore.patchDocument(&fbdo, FIREBASE_PROJECT_ID, "", documentPath.c_str(), content.raw(), "timeStamp") ) {
    Serial.printf("ok\n%s\n\n", fbdo.payload().c_str());
  } else {
    Serial.println(fbdo.errorReason());
  }
}

void setup() {
  Serial.begin(115200);
  // Connect to Wi-Fi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(1000);
  }
  Serial.println();
  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());
  Serial.println();

  // Print Firebase client version
  Serial.printf("Firebase Client v%s\n\n", FIREBASE_CLIENT_VERSION);

  // Assign the API key
  config.api_key = API_KEY;

  // Assign the user sign-in credentials
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;

  // Assign the callback function for the long-running token generation task
  config.token_status_callback = tokenStatusCallback;  // see addons/TokenHelper.h

  // Begin Firebase with configuration and authentication
  Firebase.begin(&config, &auth);

  // Reconnect to Wi-Fi if necessary
  Firebase.reconnectWiFi(true);

  // Serial for rfid read
  Serial2.begin(57600);

  // start time
  timeClient.begin();

  Serial.println("Start loop");
}

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    if (Serial2.available()) {

      // Read TagID
      String tagId = readTag();

      // check time
      timeClient.update(); // Ensure NTP client is up-to-date
      unsigned long epochTime = timeClient.getEpochTime(); // Get epoch time

      // Convert epoch time to timestamp format
      String timestamp = epochToTimestamp(epochTime);

      if (!tagId.isEmpty()) {

        // Debug for check current and previous TagID
        Serial.println("prev tag: " + prevTagId);
        Serial.println("current tag: " + tagId);

        if (tagId != prevTagId) {
          // Sent data to firebase firestore
          sentToFirestore(tagId, timestamp);

          prevTagId = tagId;

          // Debug for check current and previous TagID
          Serial.println("current tag: " + tagId);
          Serial.println("prev tag: " + prevTagId);
        } else {
          Serial.println("same");
        }
      }
    }
  } else {
    Serial.println("Wifi Disconnected");
  }
}
