#include "Header.h"
#include "TransmitterMode.h"
#include "ReceiverMode.h"

void setup() {
  //initialize customSerial Monitor
  // customSerial.begin(9600);
  customSerial.begin(9600, SERIAL_8N1, 0, 1);

  pinMode(SW1, INPUT);
  pinMode(SW2, INPUT);
  pinMode(SELECT_BUTTON, INPUT);
  
  //initialize OLED
  Wire.begin(OLED_SDA, OLED_SCL);
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3c, false, false)) { // Address 0x3C for 128x32
    customSerial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }

  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(1);
  display.setCursor(0,0);

  //SPI LoRa pins
  LoRa.setSPI(LoraSpi);
  LoraSpi.begin(SCK, MISO, MOSI, SS);
  //setup LoRa transceiver module
  LoRa.setPins(SS, RST, DIO0);
  
  if (!LoRa.begin(BAND)) {
    restartDevice("Starting LoRa Failed", 5);
    while (1);
  }

  // initialize SD Card
  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH);
  // setup SDCard SPI
  SPI.begin(SD_SCK , SD_MISO, SD_MOSI);
  SPI.setClockDivider(SPI_CLOCK_DIV2); // Set the SPI clock speed
  delay(100);
  bool bypass = !digitalRead(SELECT_BUTTON);
  if(!bypass){
    customSerial.println("Reading SDCard ...");
    if (!SD.begin(SD_CS)) {
      restartDevice("Card Mount Failed!\n\n  Hold select button \n    to run system \n   without SDCard", 5);
      customSerial.println("Card Mount Failed");
    }
    else {
      customSerial.println("Card Mounted!!");
      uint8_t cardType = SD.cardType();
      if (cardType == CARD_NONE) {
        customSerial.println("No SD card attached");
      } else {
        customSerial.print("SD Card Type: ");
        if (cardType == CARD_MMC) {
          customSerial.println("MMC");
        } else if (cardType == CARD_SD) {
          customSerial.println("SDSC");
        } else if (cardType == CARD_SDHC) {
          customSerial.println("SDHC");
        } else {
          customSerial.println("UNKNOWN");
        }
        File configFile = SD.open("/config.ini", FILE_READ);
        if (!configFile) {
          customSerial.println("Failed to open config file");
          return;
        }
      
        int idNumber = 0;
      
        // Read each line from the file
        while (configFile.available()) {
          String configLine = configFile.readStringUntil('\n');
      
          // Extract values from each line
          if (configLine.startsWith("ID:")) {
            DEV_ID = configLine.substring(configLine.indexOf(':') + 1, configLine.indexOf(';')).toInt();
          } else if (configLine.startsWith("SSID:")) {
            configLine.substring(configLine.indexOf(':') + 2, configLine.length() - 3).toCharArray(ssid, sizeof(ssid));
          } else if (configLine.startsWith("PASS:")) {
            configLine.substring(configLine.indexOf(':') + 2, configLine.length() - 3).toCharArray(password, sizeof(password));
          }
        }
      
        configFile.close();
      
        customSerial.println("Read configuration from config.ini:");
        customSerial.print("ID: ");
        customSerial.println(DEV_ID);
        customSerial.print("SSID: ");
        customSerial.println(ssid);
        customSerial.print("Password: ");
        customSerial.println(password);
      }
    }
  }
  else {    
    display.clearDisplay();
    display.setCursor(0, 10);
    display.print("Skip Reading SDCard..");
    display.display();
    customSerial.println("Reading SDCard skipped");
  }
  
  display.clearDisplay();
  display.setCursor(0,0);
  
  node.begin(slaveID, customSerial);
  if(digitalRead(SW1)){
    mode = 0;
    customSerial.println("MODE TRANSMITTER");
    display.print("ID-" + String(DEV_ID));
    display.setCursor(60,0);
    display.println("TRANSMITTER");
    display.display();
  }
  else if(digitalRead(SW2)){
    mode = 1;
    customSerial.print("Connecting to WiFi");
    display.println("Connecting to WiFi..");
    display.println();
    display.println("SSID:" + String(ssid));
    display.println("Pass:" + String(password)); 
    display.display();
    
    // Connect to Wi-Fi
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
      customSerial.print(".");
      delay(1000);
    }

    customSerial.println("WiFi Connected");
    
    //connecting to a mqtt broker
    client.setClient(wifiClient);
    client.setServer(mqtt_server, mqtt_port);

    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    LocalTime();
  
    customSerial.println("MODE RECEIVER/WIFI");

    display.clearDisplay();
    display.setCursor(0,5);
    display.println("  MODE RECEIVER/WIFI");
    display.display();
  }
  else{
    mode = 2;
    
    customSerial.print("Connecting to LAN");
    display.println("Connecting to LAN..");
    display.display();
    
    Ethernet.init(W5500_CS);

    if (Ethernet.begin(mac)) { // Dynamic IP setup
      customSerial.println("DHCP OK!");
    } else {
      customSerial.println("Failed to configure Ethernet using DHCP");
      // Check for Ethernet hardware present
      if (Ethernet.hardwareStatus() == EthernetNoHardware) {
        customSerial.println("Ethernet shield was not found.  Sorry, can't run without hardware. :(");
        while (true) {
          delay(500); // do nothing, no point running without Ethernet hardware
          ESP.restart();
        }
      }
      if (Ethernet.linkStatus() == LinkOFF) {
        customSerial.println("Ethernet cable is not connected.");
      }
    }
    digitalWrite(W5500_CS, HIGH);
    
    //connecting to a mqtt broker
    client = PubSubClient(ethClient);
    client.setServer(mqtt_server, mqtt_port);
    while (!client.connected()) {
      customSerial.printf("The client %s connects to the public mqtt broker\n", client_id);
      if (client.connect("RECEIVER", mqtt_username, mqtt_password)) {
        customSerial.println("MQTT broker connected");
      } else {
        customSerial.print("failed with state ");
        customSerial.print(client.state());
        delay(2000);
      }
    }

    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    LocalTime();
  
    customSerial.println("MODE RECEIVER/LAN");

    display.clearDisplay();
    display.setCursor(0,5);
    display.println("   MODE RECEIVER/LAN");
    display.display();
  }
  if(mode == 0)
    setData();
}

void loop() {
  currentMillis = millis();
  
  if(mode == 0){
    if (currentMillis - previousMillisRead >= 5000) {
      previousMillisRead = currentMillis;
      setData();
    }
    
    int packetSize = LoRa.parsePacket();
    if (packetSize) {
      //received a packet
      customSerial.print("Received packet ");
      //read packet
      while (LoRa.available()) {
        LoRaData = LoRa.readString();
      }
      if (LoRaData.startsWith("ACK#")) {
        customSerial.print("ACK Message Received : ");
        customSerial.println(LoRaData);
        // Remove "ACK," from the beginning of the string
        LoRaData.remove(0, 4);
      
        // Split the remaining string by commas
        int delimiterPos;
        ack_status = false;
        while ((delimiterPos = LoRaData.indexOf('#')) != -1) {
          String valueStr = LoRaData.substring(0, delimiterPos);
          int value = valueStr.toInt();
          LoRaData.remove(0, delimiterPos + 1);
      
          if (value == (int)DEV_ID) {
            customSerial.println("DEV_ID is found in the ACK message. Send Data Success!");
            ack_status = true;
            
            display.fillRect(0, 50, 128, 40, SSD1306_BLACK);
            display.setCursor(0, 55);
            display.print("Send data : -OK-");
            display.display();
          }
        }
        if(ack_status == false){
          customSerial.print("ACK data not found! ");
          if(waiting == false){
            int generateDelay = random(1000, 15000);
            randomDelay = currentMillis + generateDelay;
            waiting = true;
            customSerial.println("Resend data in " + String(generateDelay) + "s");
          }
        }
      }
    }
    if(ack_status == false){
      if(waiting){
        display.fillRect(0, 55, 128, 40, SSD1306_BLACK);
        display.setCursor(0, 55);
        display.println("Waiting " + String(randomDelay-currentMillis) + " ms");
        display.display();
        if(currentMillis >= randomDelay){
          customSerial.print("Waiting time over, sending data..");
          appendFile(SD, "/log_transmitter.txt", dataLogJson);
          LoRa.beginPacket();
          LoRa.print(dataSendJson);
          LoRa.endPacket();
          customSerial.print(dataLogJson);
          
          display.fillRect(0, 50, 128, 40, SSD1306_BLACK);
          display.display();
          display.setCursor(0, 55);
          display.println("Sending data..");
          display.display();
          
          waiting = false;
        }
      }
    }
  }
  
  if((mode == 1)||(mode == 2)){
    int packetSize = LoRa.parsePacket();
    if (packetSize) {
      //received a packet
      customSerial.print("Received packet ");
      //read packet
      while (LoRa.available()) {
        LoRaData = LoRa.readString();
      }
      customSerial.println(LoRaData);
      DynamicJsonDocument doc(1024);
      DeserializationError error = deserializeJson(doc, LoRaData);
      if (error) {
        customSerial.print("ERROR : Invalid Data - ");
        customSerial.println(error.c_str());
      } else {
        SensorData received_data;
        received_data.id = doc["ID"];
        received_data.iradian = doc["Ir"];
        received_data.wspeed = doc["WS"];
        received_data.wdirect = doc["WD"];
        received_data.temp = doc["Temp"];
        received_data.hum = doc["Hum"];
        received_data.atmp = doc["AP"];
        received_data.rain = doc["Rain"];
        
        String result = "ID : " + String(received_data.id) + "\n" + 
                 "Iradian : " + String(received_data.iradian) + "\n" + 
                 "WindSpeed : " + String(received_data.wspeed) + "\n" + 
                 "WindDirection : " + String(received_data.wdirect) + "\n" + 
                 "Temperature : " + String(received_data.temp) + "\n" + 
                 "Humidity : " + String(received_data.hum) + "\n" + 
                 "AtmosphericPressure : " + String(received_data.atmp) + "\n" + 
                 "Rainfall : " + String(received_data.rain);
        customSerial.println(result);
        bool is_collected = false;
        // Output the data to the customSerial port
        for (const SensorData& entry : node_data) {
          if(received_data.id == entry.id){
            is_collected = true;
            customSerial.println("Data From This Node is Already Collected");
          }
          customSerial.print("ID: ");
          customSerial.println(entry.id);
          display.fillRect(0, 45, 128, 10, SSD1306_BLACK);
          display.setCursor(0, 45);
          display.println("Received from " + String(entry.id));
          display.display();
        }
        if(!is_collected){
          // is it new data?
          node_data.push_back(received_data);
        }
      }
    }
    if (currentMillis - previousAckMillisRead >= DELAY_ACK) {
      previousAckMillisRead = currentMillis;
      ack_message = "ACK#";
      display.fillRect(0, 15, 128, 50, SSD1306_BLACK);
      display.setCursor(0, 15);
      display.print("Received(ID): ");
      for (const SensorData &entry : node_data)
      {
        ack_message += String(entry.id) + "#";
        display.print(String(entry.id) + ", ");
      }
      display.display();
      
      LoRa.beginPacket();
      LoRa.print(ack_message);
      LoRa.endPacket();
      delay(100);
      LoRa.beginPacket();
      LoRa.print(ack_message);
      LoRa.endPacket();
      delay(100);
      LoRa.beginPacket();
      LoRa.print(ack_message);
      LoRa.endPacket();
      
      customSerial.print("Sending ACK message : ");
      customSerial.println(ack_message);

      display.fillRect(50, 55, 128, 20, SSD1306_BLACK);
      display.setCursor(50, 55);
      display.println("ACK Sent");
      display.display();
    }
    if (currentMillis - previousPublishMillisRead >= INTERVAL_MQTT) {
      previousPublishMillisRead = currentMillis;
      // save log to sdcard
      saveLog();
      // send data with mqtt protocol
      sendData();
      // clear data
      node_data.clear();
    }
    
    int remaining_s = ((INTERVAL_MQTT - (currentMillis - previousPublishMillisRead))/1000);
    int remaining_m = remaining_s/60;
    remaining_s = remaining_s - (remaining_m*60);
    display.fillRect(0, 55, 128, 20, SSD1306_BLACK);
    display.setCursor(0, 55);
    display.println(String(remaining_m) + "m" + String(remaining_s) + "s");
    display.setCursor(55, 55);
    display.println("ACK in " + String((DELAY_ACK - (currentMillis - previousAckMillisRead))/1000) + "s");
    display.display();
    
  }
}
