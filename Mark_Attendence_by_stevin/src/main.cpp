#include <Arduino.h>

#include "secrets.h"

#include <ESP8266WiFi.h>

#include <FirebaseESP8266.h>

#include <SPI.h>

#include <MFRC522.h>

#include <TimeLib.h>

#include <WiFiUdp.h>

#include <NTPClient.h>



FirebaseData fbdo;

FirebaseAuth auth;

FirebaseConfig config;

const int relayPin = D2;

const int buzzerPin = D1;

const int wifiWaitTime = 15000;

#define SS_PIN D4

#define RST_PIN -1

MFRC522 rfid(SS_PIN, RST_PIN);

String cardID = "";

WiFiUDP ntpUDP;

NTPClient timeClient(ntpUDP, "in.pool.ntp.org");

void beep(int type)

{

	if (type == 1)

	{

		digitalWrite(buzzerPin, HIGH);

		delay(250);

		digitalWrite(buzzerPin, LOW);

		delay(250);

		digitalWrite(buzzerPin, HIGH);

		delay(250);

		digitalWrite(buzzerPin, LOW);

		delay(250);
	}

	else if (type == 2)

	{

		digitalWrite(buzzerPin, HIGH);

		delay(500);

		digitalWrite(buzzerPin, LOW);

		delay(500);
	}
}

void markAttendance(String cardID)
{
	timeClient.update();
	unsigned long epochTime = timeClient.getEpochTime();
	String currentDate = String(day(epochTime)) + "-" + String(month(epochTime)) + "-" + String(year(epochTime));
	String month_year = String(month(epochTime)) + "-" + String(year(epochTime));
	String access_time = Firebase.getString(fbdo, "access/" + cardID + "/last_access") ? fbdo.to<const char *>() : fbdo.errorReason().c_str();

	if (currentDate != access_time)
	{
		if (Firebase.getString(fbdo, "access/" + cardID + "/Name"))
		{
			String name = fbdo.to<const char *>();
			String path = "/attendance/" + month_year + "/" + name;

			if (Firebase.getInt(fbdo, path))
			{

				int currentCount = fbdo.to<int>();

				currentCount++;

				if (Firebase.setInt(fbdo, path, currentCount))
				{

					Serial.println("Attendance marked for: " + cardID + ". New Count: " + String(currentCount));
					Firebase.setString(fbdo, "access/" + cardID + "/last_access/", currentDate);
					Firebase.setString(fbdo, "access/" + cardID + "/entry_time/", timeClient.getFormattedTime());
					Firebase.setString(fbdo, "access/" + cardID + "/exit_time/", "null");
				}
				else
				{

					Serial.println("Error updating attendance:");
				}
			}
			else
			{

				if (Firebase.setInt(fbdo, path, 1))
				{

					Serial.println("Attendance marked for: " + cardID + ". New Count: 1");
					Firebase.setString(fbdo, "access/" + cardID + "/last_access/", currentDate);
					Firebase.setString(fbdo, "access/" + cardID + "/entry_time/", timeClient.getFormattedTime());
					Firebase.setString(fbdo, "access/" + cardID + "/exit_time/", "null");
				}
				else
				{

					Serial.println("Error updating attendance:");
				}
			}
		}
		else
		{
			Serial.println("Name not found");
		}
	}
	else
	{
		Serial.println("Attendance is already marked");
		Firebase.setString(fbdo, "access/" + cardID + "/exit_time/", timeClient.getFormattedTime());
	}
}
void setup()

{

	Serial.begin(9600);

	WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

	Serial.print("Connecting to Wi-Fi");

	int i = 0;

	while (WiFi.status() != WL_CONNECTED && i < wifiWaitTime)

	{

		Serial.print(".");

		delay(300);

		i += 300;
	}

	if (WiFi.status() == WL_CONNECTED)

	{

		Serial.println();

		Serial.print("Connected with IP: ");

		Serial.println(WiFi.localIP());

		Serial.println();
	}

	if (i >= wifiWaitTime)

	{

		Serial.println();

		Serial.println("Connection failed");

		Serial.println();
	}

	config.signer.test_mode = true;

	config.database_url = DATABASE_URL;

	// hiden code id : 2
	config.signer.tokens.legacy_token = FIREBASE_LEGACY_TOKEN;

	Firebase.begin(&config, &auth);
// 	if (Firebase.setString(fbdo, "connection_test", "ESP_OK")) {
//     Serial.println("Firebase Write Success");
// } else {
//     Serial.print("Firebase Error: ");
//     Serial.println(fbdo.errorReason());
// }


	Firebase.reconnectWiFi(true);

	Firebase.setDoubleDigits(5);

	pinMode(relayPin, OUTPUT);

	digitalWrite(relayPin, LOW);

	pinMode(buzzerPin, OUTPUT);

	digitalWrite(buzzerPin, LOW);

	SPI.begin(); // Init SPI bus

	rfid.PCD_Init(); // Init MFRC522 module
	timeClient.begin();
	timeClient.setTimeOffset(19800); // UTC+5:30 (Central European Time)

	Serial.println("Scan an RFID tag...");
}

void loop()

{

	cardID = "";
	if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial())

	{

		// Convert UID to a string

		Serial.print("Card UID: ");

		for (byte i = 0; i < rfid.uid.size; i++)

		{

			// Add each byte to the cardID string in HEX format

			cardID += String(rfid.uid.uidByte[i] < 0x10 ? "0" : "");

			cardID += String(rfid.uid.uidByte[i], HEX);
		}

		Serial.println(cardID); // Print the card ID stored in the string

		if (cardID == "43a20d2e" || cardID == "a1c79100" || cardID == "c9d19100" || cardID == "73f09100" || cardID == "734d6030" || cardID == "f31df106")

		{

			digitalWrite(relayPin, HIGH);

			beep(1);

			delay(3000);

			digitalWrite(relayPin, LOW);

			markAttendance(cardID);
		}

		else

		{

			String access = Firebase.getString(fbdo, "access/" + cardID + "/status") ? fbdo.to<const char *>() : fbdo.errorReason().c_str();

			Serial.println(access);

			if (access == "granted")

			{

				digitalWrite(relayPin, HIGH);
				beep(1);
				delay(3000);
				digitalWrite(relayPin, LOW);
				markAttendance(cardID);
			}

			else

			{

				if (access == "path not exist")

				{

					Firebase.setString(fbdo, "access/" + cardID + "/status", "denied") ? "ok" : fbdo.errorReason().c_str();
					Firebase.setString(fbdo, "access/" + cardID + "/Name", "Guest") ? "ok" : fbdo.errorReason().c_str();
					Firebase.setString(fbdo, "access/" + cardID + "/last_access/", "");
				}

				beep(2);
			}
		}

		// Halt PICC and stop encryption on PCD

		rfid.PICC_HaltA();

		rfid.PCD_StopCrypto1();
	}

	String lock = Firebase.getString(fbdo, "lock") ? fbdo.to<const char *>() : fbdo.errorReason().c_str();

	if (lock == "open")

	{

		digitalWrite(relayPin, HIGH);

		beep(1);

		delay(3000);

		digitalWrite(relayPin, LOW);

		Firebase.setString(fbdo, "lock", "close") ? "ok" : fbdo.errorReason().c_str();
	}
	if (Serial.available()) {
    String receivedData = Serial.readStringUntil('\n');  // Read the entire string
    
    Serial.print("Received: ");
    Serial.println(receivedData);

    // to check karna sirf ek hi string ho aur access
    if (receivedData == "arc@123") {
    	digitalWrite(relayPin, HIGH);
		beep(1);
		delay(3000);
		digitalWrite(relayPin, LOW);
    } else {
		beep(2);
      Serial.print("Invalid input: ");
      Serial.println(receivedData);
    }
  }
}
