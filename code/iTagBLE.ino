#include <BLEDevice.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <String.h>

// Ρυθμίσεις για σύνδεση σε wifi SSID/Password 
const char* ssid = "Home1";
const char* password = "xxxxxxxxxx";

// Add your MQTT Broker IP address:
const char* mqtt_server = "test.mosquitto.org";
char msgArr[4];

WiFiClient espClient;
PubSubClient client(espClient);
long lastMsg = 0;
char msg[50];
int value = 0;


#define ADDRESS "d3:72:bb:fb:7e:34" //Bluetooth address που θα ελέγχει για παρουσία
#define RELAY_PIN 2 //pin ελέγχου
#define SCAN_INTERVAL 1000 //Χρόνος αναμονής για σάρωση scan
#define TARGET_RSSI -70 //RSSI όριο για ενεργοποίηση ενέργειας.
#define MAX_MISSING_TIME 5000 //Χρόνος αναμονής πριν να δηλωθεί απουσία του BLE

BLEScan* pBLEScan; //Μεταβλητή τύπου BLEScan
uint32_t lastScanTime = 0; //Πόσος χρόνος πέρασε από το τελευταίο scan
boolean found = false; //Έχει βρεθεί το BLE?
uint32_t lastFoundTime = 0; //Πόσος χρόνος πέρασε από την τελευταία παρουσία του BLE
int rssi = 0;

String addr = ADDRESS;
String mac;

//Callback class
class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks
{
    void onResult(BLEAdvertisedDevice advertisedDevice)
    {
     //   Όταν βρεθεί μία συσκευή BLE
     //   Serial.print("Device found: ");      
     //   Serial.println(advertisedDevice.toString().c_str());
     // Ποιο είναι το mac Address που έχει
    mac = advertisedDevice.getAddress().toString().c_str();
    
        //Έλεγχος αν είναι η συσκευή που ψάχνουμε
        if(mac == addr)
        {
            Serial.print("found-");
            Serial.println(advertisedDevice.getRSSI());
            //Βρέθηκε η συσκευή, πόσο RSSI δίνει και σταματάμε το scan
            found = true;
            advertisedDevice.getScan()->stop();
            rssi = advertisedDevice.getRSSI();
            Serial.println("RSSI: " + rssi);
        }
    }
};

void setup()
{
    Serial.begin(115200);
        
    setup_wifi();
    client.setServer(mqtt_server, 1883);
  

    //Αν θέλουμε να ενεργοποιήσουμε κάποιο Pin 
    pinMode(RELAY_PIN, OUTPUT);
    digitalWrite(RELAY_PIN, LOW);

    //Ενεργοποίηση σάρωσης
    BLEDevice::init(""); 
    pBLEScan = BLEDevice::getScan();
    pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
    pBLEScan->setActiveScan(true);
}

void setup_wifi() {
  delay(10);
  // Συνδεόμαστε στο wifi 
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void reconnect() {
  // Επανασύνδεση αν χαθεί η σύνδεση
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Σύνδεση με τυχαίο client name
    if (client.connect("ESP32Client"+random(300))) {
      Serial.println("connected");
      // Subscribe σε topic
      client.subscribe("gatakia/RSSI");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}
void loop()
{   
    if (!client.connected()) {
     reconnect();
    }
    client.loop();
  
    uint32_t now = millis(); //Χρόνος σε milisecond από την εκκίνηση

    if(found){ //Αν βρέθυηκε η BLE συσκευή
        lastFoundTime = millis(); //Χρόνος από την εύρεση της
        found = false;
        String tempString = String(rssi);
        tempString.toCharArray(msgArr, sizeof(msgArr));
        if(rssi > TARGET_RSSI){ //Αν το RSSI είναι δυνατότερο από το όριο που δηλώσαμε
            digitalWrite(RELAY_PIN, HIGH); //ενεργοποίηση ρελέ
            client.publish("gatakia/RSSI", msgArr);
            client.publish("gatakia/cmd", "open");
        }
        else{ //έχει απομακρυνθεί 
            digitalWrite(RELAY_PIN, LOW);
            client.publish("gatakia/RSSI", msgArr);
            client.publish("gatakia/cmd", "close");
        }
    }
    //Αν δεν βρέθηκε και ο χρόνος αναμονής έχει περάσει
    else if(now - lastFoundTime > MAX_MISSING_TIME){
        digitalWrite(RELAY_PIN, LOW);  //άνοιγμα ρελέ
        //client.publish("gatakia/RSSI", "000");
    }
    
    if(now - lastScanTime > SCAN_INTERVAL){ //Χρόνος για νέα σάρωση
        //Κρατάω το χρόνο και ξεκινάω σάρωση
        lastScanTime = now;
        pBLEScan->start(1);
    }
}
