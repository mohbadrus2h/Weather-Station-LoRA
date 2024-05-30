// Device ID (every device must have unique device ID)
int DEV_ID = 14;
// WiFi Configuration
char ssid[32] = "Change SSID Here"; 
char password[32] = "Change Password Here"; 

#include <WiFi.h>

#include <Ethernet.h>
#define W5500_CS 25

HardwareSerial customSerial(1);

#include <SPI.h>
#include <LoRa.h>
#define SCK 5
#define MISO 19
#define MOSI 27
#define SS 18
#define RST 23
#define DIO0 26
// 433E6 for Asia
// 866E6 for Europe
// 915E6 for North America
#define BAND 433E6

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
// OLED pins
#define OLED_SDA 21
#define OLED_SCL 22
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

#include "SD.h"
#include "FS.h"
#define SD_CS 13
#define SD_SCK 14
#define SD_MISO 2
#define SD_MOSI 15

// LoRaSPI
SPIClass LoraSpi(HSPI);

// Switching PIN
#define SW1 34
#define SW2 35

// Select button
#define SELECT_BUTTON 4
int mode = 0;

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

//======== Timing ========
unsigned long currentMillis;
unsigned long previousMillisRead = 0;
unsigned long previousAckMillisRead = 0;
unsigned long previousPublishMillisRead = 0;

// Global Variable
float wspeed, wdirect, temp, hum, rain;
int iradian, atmp;

// Append log data to SDCard
void appendFile(fs::FS &fs, const char *path, const char *message)
{
    customSerial.printf("Appending to file: %s\n", path);

    File file = fs.open(path, FILE_APPEND);
    if (!file)
    {
        // If the file doesn't exist, create it
        file = fs.open(path, FILE_WRITE);
        if (!file)
        {
            customSerial.println("Failed to create or open file");
            display.fillRect(0, 50, 128, 40, SSD1306_BLACK);
            display.display();
            display.setCursor(0, 55);
            display.print("Failed to write log");
            display.display();
            return;
        }
    }

    if (file.print(message))
    {
        customSerial.println("Message appended");
    }
    else
    {
        display.fillRect(0, 50, 128, 40, SSD1306_BLACK);
        display.display();
        display.setCursor(0, 55);
        display.print("Failed to write log");
        display.display();
        customSerial.println("Append failed");
    }

    file.close();
}

// restart if init module failed
void restartDevice(String Message, int delay_s)
{
    customSerial.println(Message);

    display.setCursor(0, 10);
    display.print(Message);
    display.display();
    customSerial.println("Restart in : ");
    for(int i=delay_s; i>=0; i--){
      display.fillRect(0, 50, 128, 30, SSD1306_BLACK);
      display.setCursor(0, 55);
      display.print("      Restart in " + String(i) + "s");
      display.display();
      customSerial.print(" " + String(i));
      delay(1000);
    }
    display.clearDisplay();
    display.display();
    ESP.restart();
}
