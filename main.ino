#include <FS.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266HTTPClient.h>
#include <SPI.h> 
//#include <WiFi.h>
#include <WiFiUdp.h>


const int led1Pin = 2;
const int led2Pin = 16;

const int inputButton1Pin = 5;
const int inputButton2Pin = 4;

const char configFileName[16] = "/config.bin";

static String wifi_ssid = "";
static String wifi_passwd = "";

static String device1 = "";
static String device2 = "";

static bool led1Status = false;
static int totalSec = 0;
static WiFiUDP Udp;


static bool LoadUserConfig();
static void ApplyUserConfig();
static void KeepInputSignalPinHigh();

static void SetupInputPins();
ICACHE_RAM_ATTR void OnButton1Clicked();
ICACHE_RAM_ATTR void OnButton2Clicked();

void setup() 
{
  Serial.begin(9600);
  Serial.println("");
  Serial.println("Welcome to StairLight serial console!");
  Serial.print("#");

  if (LoadUserConfig())
  {
    ApplyUserConfig();
  }

  Udp.begin(8888);

  SetupInputPins();
}

static void SetupInputPins()
{
  pinMode(led1Pin, OUTPUT);
  digitalWrite(led1Pin, HIGH);

  pinMode(inputButton1Pin, INPUT_PULLUP);
  pinMode(inputButton2Pin, INPUT_PULLUP);

  attachInterrupt(digitalPinToInterrupt(inputButton1Pin), OnButton1Clicked, FALLING);
  attachInterrupt(digitalPinToInterrupt(inputButton2Pin), OnButton2Clicked, FALLING);
}

static void ApplyUserConfig()
{
  WiFi.disconnect();
  WiFi.begin(wifi_ssid, wifi_passwd);
}

static bool LoadUserConfig()
{
  if (!SPIFFS.begin())
  {
    return false;
  }

  auto f = SPIFFS.open(configFileName, "r");
  if (!f)
  {
    return false;
  }

  int idx = 0;
  String* ar[6] = { 
    &wifi_ssid, 
    &wifi_passwd, 
    &device1,
    &device2,
  };

  auto s = ar[0];
  (*s) = "";
  
  auto n = f.size();
  for (int i = 0; i < n; ++i)
  {
    auto c = (char)f.read();
    if (0 == c)
    {
      ++idx;
      s = ar[idx];
      if (0 == s)
      {
        break;
      }      
      (*s) = "";
    }
    else
    {
      (*s) += c;
    }
  }
  f.close();
  return true;
}

static bool SaveUserConfig()
{
  if (!SPIFFS.begin())
  {
    return false;
  }

  auto f = SPIFFS.open(configFileName, "w");
  if (!f)
  {
    return false;
  }

  auto z = (char)0;
  f.print(wifi_ssid);
  f.print(z);
  f.print(wifi_passwd);
  f.print(z);
  
  f.print(device1);
  f.print(z);
  f.print(device2);
  f.print(z);

  f.close();
  return true;
}

static void DoConsoleTask()
{
  if (!Serial.available())
  {
    return;
  }

  static int commandLen = 0;
  static String command;
  static String* currentParam = 0;
  static bool autoApplyUserConfig = false;
  
  bool noValue = false;
  int x = Serial.read();
  if (10 == x || 13 == x)
  {
    Serial.println("");
    if (0 == currentParam)
    {
      autoApplyUserConfig = false;
      currentParam = 0;
      if (command.equals("wifi.ssid"))
      {
        currentParam = &wifi_ssid;
        autoApplyUserConfig = true;
      }
      else if (command.equals("wifi.password"))
      {
        currentParam = &wifi_passwd;
        autoApplyUserConfig = true;
      }
      else if (command.equals("device1"))
      {
        currentParam = &device1;
        autoApplyUserConfig = true;
      }
      else if (command.equals("device2"))
      {
        currentParam = &device2;
        autoApplyUserConfig = true;
      }
      else if (command.equals("info"))
      {
        noValue = true;
        Serial.print("TotalSec: ");
        Serial.println(totalSec);

        Serial.print("SSID: ");
        Serial.println(wifi_ssid);
        Serial.print("Password: ");
        Serial.println(wifi_passwd);
        Serial.print("Status: ");
        Serial.println(WiFi.status());
        Serial.print("IP address: ");
        Serial.println(WiFi.localIP());

        Serial.print("Device 1: ");
        Serial.println(device1);
        Serial.print("Device 2: ");
        Serial.println(device2);
      }
      else if (command.equals("save"))
      {
        noValue = true;
        bool saveOK = SaveUserConfig();
        Serial.println((saveOK ? "OK" : "Failed"));
      }
      else if (command.equals("load"))
      {
        noValue = true;
        LoadUserConfig();
      }
  
      if (noValue || 0 == currentParam)
      {
        if (noValue)
        {
          currentParam = 0;
        }
        else
        {
          Serial.println("?");
        }
        Serial.print("#");
      }
      else
      {
        (*currentParam) = "";
        Serial.print(":");
      }
      command = "";
    }
    else
    {
      currentParam = 0;
      if (autoApplyUserConfig)
      {
        ApplyUserConfig();
      }
      Serial.print("#");
    }
    commandLen = 0;
  }
  else
  {
    char c = (char)x;
    if (commandLen < 32)
    {
      Serial.print(c);
      if (0 == currentParam)
      {
        command += c;
      }
      else
      {
        (*currentParam) += c;
      }
      ++commandLen;
    }
    //
  }
  //
}

static String SendHttpRequest(String url)
{
    WiFiClient wifiClient;
    HTTPClient http;
    http.begin(wifiClient, url);
    http.GET();
    auto s = http.getString();
    return s;
}

static void ToggleDevice(int v, String deviceBaseURL)
{
  if (HIGH == v)
  {
    return;
  }
  
  //Serial.println("ToggleDevice " + deviceBaseURL); return;
  
  if (deviceBaseURL.length() > 0)
  {  
    String url = deviceBaseURL + "/cm?cmnd=power%20TOGGLE";
    SendHttpRequest(url);
  }
}

static void DoTask1()
{
  static int prevX = 0;
  static int tick = 0;
  ++tick;
  if (tick >= 20)
  {
    tick = 0;
    int x = digitalRead(inputButton1Pin);
    if (x != prevX)
    {
      prevX = x; 
      ToggleDevice(x, device1);
    }
    //
  }
}

static void DoTask2()
{
  static int prevX = 0;
  static int tick = 0;
  ++tick;
  if (tick >= 20)
  {
    tick = 0;
    int x = digitalRead(inputButton2Pin);
    if (x != prevX)
    {
      prevX = x; 
      ToggleDevice(x, device2);
    }
    //
  }
}

ICACHE_RAM_ATTR void OnButton1Clicked()
{
  //
}

ICACHE_RAM_ATTR void OnButton2Clicked()
{
  //
}

static void DoClockTask()
{
  static int totalMiliSec = 0;
  
  static char buf[4];
  int packetSize = Udp.parsePacket();
  if (packetSize > 0)
  {
    int n = Udp.read(buf, 4);
    if (4 == n)
    {
      n = buf[2];
      n *= 256;
      n += buf[1];
      n *= 256;
      n += buf[0];
      totalSec = n;
    }
  }

  bool isDark = (totalSec < 21600 || totalSec >= 64800);
  if (led1Status)
  {
    if (!isDark)
    {
      led1Status = false;
      digitalWrite(led1Pin, HIGH);
    }    
  }
  else
  {
    if (isDark)
    {
      led1Status = true;
      digitalWrite(led1Pin, LOW);
    }    
  }

  totalMiliSec += 10;
  if (totalMiliSec >= 1000)
  {
    totalMiliSec = 0;
    ++totalSec;
  }
}

void loop() 
{
  DoConsoleTask();

  DoTask1();
  DoTask2();

  DoClockTask();
  
  delay(10);
}
