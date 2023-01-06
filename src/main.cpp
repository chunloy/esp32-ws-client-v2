#include <Arduino.h>
#include <WiFiMulti.h>
#include <secrets.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>

// ----------------------------------------------------------------------------------------------------
// constants
// ----------------------------------------------------------------------------------------------------
#define LED_PIN 14
#define MSG_SIZE 256
#define JSON_SIZE 2048

// ----------------------------------------------------------------------------------------------------
// globals
// ----------------------------------------------------------------------------------------------------
WiFiMulti wifiMulti;
WebSocketsClient wsClient;

// ----------------------------------------------------------------------------------------------------
// function definitions
// ----------------------------------------------------------------------------------------------------
void initWiFi()
{
  wifiMulti.addAP(SSID, PASS);

  while (wifiMulti.run() != WL_CONNECTED)
  {
    delay(100);
  }

  Serial.println("Connected");
}

void sendOkMessage()
{
  wsClient.sendTXT("{\"action\":\"msg\",\"type\":\"status\",\"body\":\"ok\"}");
}

void sendErrorMessage(const char *error)
{
  char msg[MSG_SIZE];

  // format error message as json
  sprintf(msg, "{\"action\":\"msg\",\"type\":\"error\",\"body\":\"%s\"}", error);
  wsClient.sendTXT(msg);
}

uint8_t toMode(const char *mode)
{
  if (strcmp(mode, "output") == 0)
    return OUTPUT;
  if (strcmp(mode, "input_pullup") == 0)
    return INPUT_PULLUP;

  return INPUT;
}

void messageHandler(uint8_t *payload)
{
  StaticJsonDocument<JSON_SIZE> doc;

  // Deserialize the JSON document
  DeserializationError error = deserializeJson(doc, payload);

  // Test if parsing succeeds.
  if (error)
  {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.f_str());

    // send error message
    sendErrorMessage(error.c_str());
    return;
  }

  // check that event is a string
  if (!doc["type"].is<const char *>())
  {
    sendErrorMessage("Invalid message type");
    return;
  }

  // validate command format is json
  if (strcmp(doc["type"], "cmd") == 0)
  {
    if (!doc["body"].is<JsonObject>())
    {
      sendErrorMessage("Invalid command body");
      return;
    }

    // set pin mode
    if (strcmp(doc["body"]["type"], "pinMode") == 0)
    {
      // check that mode is a string
      if (!doc["body"]["mode"].is<const char *>())
      {
        sendErrorMessage("invalid pinMode mode type");
        return;
      }

      // validate mode type
      if (strcmp(doc["body"]["mode"], "input") != 0 &&
          strcmp(doc["body"]["mode"], "input_pullup") != 0 &&
          strcmp(doc["body"]["mode"], "output") != 0)
      {
        sendErrorMessage("Invalid pinMode mode value");
        return;
      }

      pinMode(doc["body"]["pin"], toMode(doc["body"]["mode"]));
      sendOkMessage();
      return;
    }

    // set pin state
    if (strcmp(doc["body"]["pin"], "digitalWrite") == 0)
    {
      digitalWrite(doc["body"]["pin"], doc["body"]["value"]);
      sendOkMessage();
      return;
    }

    // read pin state
    if (strcmp(doc["body"]["pin"], "digitalRead") == 0)
    {
      int value = digitalRead(doc["body"]["pin"]);
      char msg[MSG_SIZE];

      sprintf(msg, "{\"action\":\"msg\",\"type\":\"output\",\"body\":%d}", value);
      wsClient.sendTXT(msg);
      return;
    }

    sendErrorMessage("Unsupported command type");
    return;
  }

  sendErrorMessage("Unsopported message type");
  return;
}

void onWSEvent(WStype_t type, uint8_t *payload, size_t length)
{
  switch (type)
  {
  case WStype_CONNECTED:
    Serial.println("WS connected");
    break;
  case WStype_DISCONNECTED:
    Serial.println("WS disconnected");
    break;
  case WStype_TEXT:
    Serial.printf("WS Message: %s\n", payload);
    break;
  }
}

void initClient()
{
  wsClient.beginSSL(WS_HOST, WS_PORT, WS_URL, "", "wss");
  wsClient.onEvent(onWSEvent);
}

void setup()
{
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);

  initWiFi();
  initClient();
}

void loop()
{
  digitalWrite(LED_BUILTIN, WiFi.status() == WL_CONNECTED); // turn on builtin led once conncted to wifi
  wsClient.loop();                                          // keep client connection alive
}