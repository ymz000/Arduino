// WiFiAutoCloudDHTMqtt.ino
// Company: KMP Electronics Ltd, Bulgaria
// Web: http://kmpelectronics.eu/
// Supported boards:
//    KMP ProDino WiFi-ESP WROOM-02 (http://www.kmpelectronics.eu/en-us/products/prodinowifi-esp.aspx)
// Description:
//    Cloud MQTT example with DHT support. In this example we show how to connect KMP ProDino WiFi-ESP WROOM-02 with Amazon cloudmqtt.com service and measure humidity and temperature with DHT22 sensor.
// Example link: http://www.kmpelectronics.eu/en-us/examples/prodinowifi-esp/wifiwebrelayserverap.aspx
// Version: 1.0.0
// Date: 26.07.2017
// Author: Plamen Kovandjiev <p.kovandiev@kmpelectronics.eu>

#include <FS.h> 
#include <KMPDinoWiFiESP.h>
#include <KMPCommon.h>
#include <DHT.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>

#include <DNSServer.h>            //Local DNS Server used for redirecting all requests to the configuration portal
#include <ESP8266WebServer.h>     //Local WebServer used to serve the configuration portal
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager WiFi Configuration Magic
#include <ArduinoJson.h>          //https://github.com/bblanchon/ArduinoJson

const uint8_t MQTT_SERVER_LEN = 40;
const uint8_t MQTT_PORT_LEN = 8;
const uint8_t MQTT_CLIENT_ID_LEN = 32;
const uint8_t MQTT_USER_LEN = 16;
const uint8_t MQTT_PASS_LEN = 16;

const char* MQTT_SERVER_KEY = "mqttServer";
const char* MQTT_PORT_KEY = "mqttPort";
const char* MQTT_CLIENT_ID_KEY = "mqttClientId";
const char* MQTT_USER_KEY = "mqttUser";
const char* MQTT_PASS_KEY = "mqttPass";
const char* CONFIG_FILE_NAME = "/config.json";

char _mqttServer[MQTT_SERVER_LEN] = "x.cloudmqtt.com";
char _mqttPort[MQTT_PORT_LEN] = "1883";
char _mqttClientId[MQTT_CLIENT_ID_LEN] = "ESP8266Client";
char _mqttUser[MQTT_USER_LEN];
char _mqttPass[MQTT_PASS_LEN];

const char TOPIC_SEPARATOR = '/';
const char* MAIN_TOPIC = "kmp/prodinowifi";
const char* HUMIDITY_SENSOR = "humidity";
const char* TEMPERATURE_SENSOR = "temperature";
const char* RELAY_OUTPUT = "relay";
const char* OPTO_INPUT = "optoin";
const char* SET_COMMAND = "set";

DHT _dhtSensor(EXT_GROVE_D0, DHT22, 11);
// Contains last measured humidity from sensor.
float _humidity;
// Contains last measured temperature from sensor.
float _temperature;

// Check sensor data, interval in milliseconds.
const long CHECK_HT_INTERVAL_MS = 10000;
// Store last measure time.
unsigned long _mesureTimeout;

// Declares a ESP8266WiFi client.
WiFiClient _wifiClient;
// Declare a MQTT client.
//PubSubClient _mqttClient(MQTT_SERVER, MQTT_PORT, _wifiClient);
PubSubClient _mqttClient;

// There arrays store last states by relay and optical isolated inputs.
bool _lastRelayStatus[4] = { false };
bool _lastOptoInStatus[4] = { false };

// Buffer by send output state.
char _payload[16];
bool _sendAllData;
bool _sendRelayData;

//flag for saving data
bool shouldSaveConfig = false;

/**
* @brief Execute first after start device. Initialize hardware.
*
* @return void
*/
void setup(void)
{
	// You can open the Arduino IDE Serial Monitor window to see what the code is doing
	// Serial connection from ESP-01 via 3.3v console cable
	Serial.begin(115200);
	// Init KMP ProDino WiFi-ESP board.
	KMPDinoWiFiESP.init();

	Serial.println("KMP Mqtt cloud client example.\r\n");

	//WiFiManager
	//Local initialization. Once its business is done, there is no need to keep it around
	WiFiManager wifiManager;

	// Is OptoIn 4 is On the board is resetting WiFi configuration.
	if (KMPDinoWiFiESP.GetOptoInState(OptoIn4))
	{
		Serial.println("Resetting WiFi configuration...\r\n");
		//reset saved settings
		wifiManager.resetSettings();
		Serial.println("WiFi configuration was reseted.\r\n");
	}

	//set config save notify callback
	wifiManager.setSaveConfigCallback(saveConfigCallback);

	if (!AddParametersAndAutoConnect(&wifiManager))
	{
		return;
	}

	// Initialize MQTT.
	_mqttClient.setClient(_wifiClient);
	uint16_t port = atoi(_mqttPort);
	_mqttClient.setServer(_mqttServer, port);
	_mqttClient.setCallback(callback);

	_sendAllData = true;
}

/**
* @brief Callback method. It is fire when has information in subscribed topic.
*
* @return void
*/
void callback(char* topic, byte* payload, unsigned int length) {

	Serial.print("Subscribed topic [");
	Serial.print(topic);
	Serial.print("]");
	
	Serial.print(" payload [");

	for (uint i = 0; i < length; i++)
	{
		Serial.print((char)payload[i]);
	}
	Serial.println("]");

	// kmp/prodinowifi - command send all data from device.
	if (strncmp(MAIN_TOPIC, topic, strlen(MAIN_TOPIC)) == 0)
	{
		// TODO: Print all data.
		return;
	}
	
	String topicName = String(topic);
	String topicStart = String(MAIN_TOPIC) + TOPIC_SEPARATOR + RELAY_OUTPUT + TOPIC_SEPARATOR;
	String topicEnd = String(TOPIC_SEPARATOR) + SET_COMMAND;

	// kmp/prodinowifi/relay/+/set - command send relay status.
	if (topicName.startsWith(topicStart) && topicName.endsWith(topicEnd))
	{
		// TODO: Set relay status.
		//if (strncmp(CMD_REL, (const char*)payload, cmdRelLen) == 0 && length >= cmdRelLen + 4)
		//{
		//	KMPDinoWiFiESP.SetRelayState(CharToInt(payload[4]), CharToInt(payload[6]) == 1);
		//	_sendRelayData = true;
		//}
	}

	// Command send all data.
	if ( ((const char*)payload, CMD_ALL, strlen(CMD_ALL)) == 0)
	{
		_sendAllData = true;
		return;
	}

	// Relay command.
	// Command structure: [command (rel):relay number (0..3):relay state (0 - Off, 1 - On)]. Example: rel:0:0 
	size_t cmdRelLen = strlen(CMD_REL);

	if (strncmp(CMD_REL, (const char*)payload, cmdRelLen) == 0 && length >= cmdRelLen + 4)
	{
		KMPDinoWiFiESP.SetRelayState(CharToInt(payload[4]), CharToInt(payload[6]) == 1);
		_sendRelayData = true;
	}
}

String createTopic(params char*[] prm)
{

	return NULL;
}

/**
* @brief Main method.
*
* @return void
*/
void loop(void)
{
	// By the normal device work need connected with WiFi and MQTT server.
	if (!ConnectWiFi() || !ConnectMqtt())
	{
		return;
	}

	_mqttClient.loop();

	// Publish information in MQTT.
	PublishInformation();
	GetDHTSensorData();

	_sendAllData = false;
	_sendRelayData = false;
}

/**
* @brief Publish information in the MQTT server.
*
* @return void
*/
void PublishInformation()
{
	char state[2];
	state[1] = '\0';
	// Get current Opto input and relay statuses.
	for (byte i = 0; i < RELAY_COUNT; i++)
	{
		bool rState = KMPDinoWiFiESP.GetRelayState(i);
		if (_lastRelayStatus[i] != rState || _sendAllData || _sendRelayData)
		{
			_lastRelayStatus[i] = rState;
			state[0] = rState ? '1' : '0';
			buildPayload(_payload, CMD_REL, CMD_SEP, i, state);
			Publish(MAIN_TOPIC, _payload);
		}
	}

	for (byte i = 0; i < OPTOIN_COUNT; i++)
	{
		bool oiState = KMPDinoWiFiESP.GetOptoInState(i);
		if (_lastOptoInStatus[i] != oiState || _sendAllData)
		{
			_lastOptoInStatus[i] = oiState;
			state[0] = oiState ? '1' : '0';
			buildPayload(_payload, CMD_OPTOIN, CMD_SEP, i, state);

			Publish(MAIN_TOPIC, _payload);
		}
	}
}

/**
* @brief Read data from sensors a specified time.
*
* @return void
*/
void GetDHTSensorData()
{
	if (millis() > _mesureTimeout || _sendAllData)
	{
		_dhtSensor.read(true);
		float humidity = _dhtSensor.readHumidity();
		float temperature = _dhtSensor.readTemperature();

		if (_humidity != humidity || _sendAllData)
		{
			FloatToChars(humidity, 1, _payload);
			_humidity = humidity;
			Publish(TOPIC_INFO_DHT_H, _payload);
		}

		if (_temperature != temperature || _sendAllData)
		{
			FloatToChars(temperature, 1, _payload);
			_temperature = temperature;
			Publish(DHT_SENSOR, _payload);
		}

		// Set next time to read data.
		_mesureTimeout = millis() + CHECK_HT_INTERVAL_MS;
	}
}

/**
* @brief Build publish payload.
* @param buffer where fill payload.
* @param command description
* @param number device number
* @param state device state
*
* @return void
*/
void buildPayload(char* buffer, const char* command, char separator, byte number, const char* state)
{
	int cmdLen = strlen(command);
	memcpy(buffer, command, cmdLen);
	buffer[cmdLen++] = separator;
	buffer[cmdLen++] = IntToChar(number);
	buffer[cmdLen++] = separator;
	buffer += cmdLen;
	int stLen = strlen(state);
	memcpy(buffer, state, stLen);
	buffer[stLen] = '\0';
}

/**
* @brief Publish topic.
* @param topic title.
* @param payload data to send
*
* @return void
*/
void Publish(const char* topic, char* payload)
{
	Serial.print("Publish topic [");
	Serial.print(topic);
	Serial.print("] payload [");
	Serial.print(_payload);
	Serial.println("]");

	_mqttClient.publish(topic, (const char*)_payload);
}

/**
* @brief Connect to WiFi access point.
*
* @return bool true - success.
*/
bool ConnectWiFi()
{
	if (WiFi.status() != WL_CONNECTED)
	{
		Serial.print("Reconnecting [");
		Serial.print(WiFi.SSID());
		Serial.println("]...");

		WiFi.begin();
		//WiFi.begin(SSID, SSID_PASSWORD);

		if (WiFi.waitForConnectResult() != WL_CONNECTED)
		{
			return false;
		}

		Serial.print("IP address: ");
		Serial.println(WiFi.localIP());
	}

	return true;
}

/**
* @brief Connect to MQTT server.
*
* @return bool true - success.
*/
bool ConnectMqtt()
{
	if (!_mqttClient.connected())
	{
		Serial.println("Attempting MQTT connection...");

		if (_mqttClient.connect(_mqttClientId, _mqttUser, _mqttPass))
		{
			Serial.println("Connected.");
			_mqttClient.subscribe(TOPIC_COMMAND);
		}
		else
		{
			Serial.print("failed, rc=");
			Serial.print(_mqttClient.state());
			Serial.println(" try again after 5 seconds");
			// Wait 5 seconds before retrying
			delay(5000);
		}
	}

	return _mqttClient.connected();
}

/**
* @brief Callback notifying us of the need to save configuration set from WiFiManager.
*
* @return void.
*/
void saveConfigCallback()
{
	Serial.println("Should save config");
	shouldSaveConfig = true;
}

/**
* @brief Collect information for connect WiFi and MQTT server. After successful connected and !!!!!
* @param wifiManager.
*
* @return bool if successful connected - true else false.
*/
bool AddParametersAndAutoConnect(WiFiManager* wifiManager)
{
	//read configuration from FS json
	Serial.println("mounting FS...");

	if (!SPIFFS.begin())
	{
		Serial.println("failed to mount FS");
	}
	else
	{
		Serial.println("Mounted file system");

		if (SPIFFS.exists(CONFIG_FILE_NAME))
		{
			//file exists, reading and loading
			Serial.println("reading config file");
			File configFile = SPIFFS.open(CONFIG_FILE_NAME, "r");
			if (configFile)
			{
				Serial.println("Opening config file");
				size_t size = configFile.size();
				// Allocate a buffer to store contents of the file.
				std::unique_ptr<char[]> buf(new char[size]);

				configFile.readBytes(buf.get(), size);
				DynamicJsonBuffer jsonBuffer;
				JsonObject& json = jsonBuffer.parseObject(buf.get());
				json.printTo(Serial);
				if (json.success())
				{
					Serial.println("\nparsed json");

					strcpy(_mqttServer, json[MQTT_SERVER_KEY]);
					strcpy(_mqttPort, json[MQTT_PORT_KEY]);
					strcpy(_mqttClientId, json[MQTT_CLIENT_ID_KEY]);
					strcpy(_mqttUser, json[MQTT_USER_KEY]);
					strcpy(_mqttPass, json[MQTT_PASS_KEY]);
				}
				else
				{
					Serial.println("failed to load json config");
				}
			}
		}
	}

	// The extra parameters to be configured (can be either global or just in the setup)
	// After connecting, parameter.getValue() will get you the configured value
	// id/name placeholder/prompt default length
	WiFiManagerParameter customMqttServer("server", "MQTT server", _mqttServer, MQTT_SERVER_LEN);
	WiFiManagerParameter customMqttPort("port", "MQTT port", String(_mqttPort).c_str(), MQTT_PORT_LEN);
	WiFiManagerParameter customClientName("clientName", "Client name", _mqttClientId, MQTT_CLIENT_ID_LEN);
	WiFiManagerParameter customMqttUser("user", "MQTT user", _mqttUser, MQTT_USER_LEN);
	WiFiManagerParameter customMqttPass("password", "MQTT pass", _mqttPass, MQTT_PASS_LEN);

	//add all your parameters here
	wifiManager->addParameter(&customMqttServer);
	wifiManager->addParameter(&customMqttPort);
	wifiManager->addParameter(&customClientName);
	wifiManager->addParameter(&customMqttUser);
	wifiManager->addParameter(&customMqttPass);

	//fetches ssid and pass from eeprom and tries to connect
	//if it does not connect it starts an access point with the specified name
	//auto generated name ESP + ChipID
	if (!wifiManager->autoConnect())
	{
		Serial.println("Doesn't connect");
		return false;
	}
		
	//if you get here you have connected to the WiFi
	Serial.println("Connected.");

	if (shouldSaveConfig)
	{
		Serial.println("Saving config");

		//read updated parameters
		strcpy(_mqttServer, customMqttServer.getValue());
		strcpy(_mqttPort, customMqttPort.getValue());
		strcpy(_mqttClientId, customClientName.getValue());
		strcpy(_mqttUser, customMqttUser.getValue());
		strcpy(_mqttPass, customMqttPass.getValue());

		DynamicJsonBuffer jsonBuffer;
		JsonObject& json = jsonBuffer.createObject();

		json[MQTT_SERVER_KEY] = _mqttServer;
		json[MQTT_PORT_KEY] = _mqttPort;
		json[MQTT_CLIENT_ID_KEY] = _mqttClientId;
		json[MQTT_USER_KEY] = _mqttUser;
		json[MQTT_PASS_KEY] = _mqttPass;

		File configFile = SPIFFS.open(CONFIG_FILE_NAME, "w");
		if (!configFile) {
			Serial.println("failed to open config file for writing");
		}

		json.prettyPrintTo(Serial);
		json.printTo(configFile);
		configFile.close();
	}

	return true;
}