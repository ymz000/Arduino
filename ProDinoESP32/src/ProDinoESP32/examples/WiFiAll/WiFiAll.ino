// WiFiEthAll.ino
// Company: KMP Electronics Ltd, Bulgaria
// Web: https://kmpelectronics.eu/
// Supported boards:
//		KMP ProDino ESP32 Ethernet V1 https://kmpelectronics.eu/products/prodino-esp32-ethernet-v1/
//		KMP ProDino ESP32 Ethernet GSM V1 https://kmpelectronics.eu/products/prodino-esp32-ethernet-gsm-v1/
//		KMP ProDino ESP32 Ethernet LoRa V1 https://kmpelectronics.eu/products/prodino-esp32-ethernet-lora-v1/
//		KMP ProDino ESP32 Ethernet LoRa RFM V1 https://kmpelectronics.eu/products/prodino-esp32-ethernet-lora-rfm-v1/
// Description:
//      Test all: WiFi, relays, inputs, RS485, GROVE connector.
//      Test all through Ethernet: Relays, inputs, RS485, GROVE connector.
// Example link: https://kmpelectronics.eu/tutorials-examples/prodino-esp32-versions-examples/
// Version: 1.0.0
// Date: 21.12.2018
// Author: Plamen Kovandjiev <p.kovandiev@kmpelectronics.eu>
// --------------------------------------------------------------------------------
// Prerequisites:
//	Before start this example you need to install:
//		Installing library: Sketch\Include library\Menage Libraries... find ... and click Install.
//         - SimpleDHT by Winlin
//		Connect DHT22 sensor(s) to GROVE connector. Only one we use in this example. Use pins: 
//			- sensor GROVE_D0, Vcc+, Gnd(-);
//		You have to fill fields in arduino_secrets.h file.

#include "KMPProDinoESP32.h"
#include "KMPCommon.h"
#include <SimpleDHT.h>
#include "arduino_secrets.h"

#define DEBUG

#include <WiFi.h>
#include <WiFiClient.h>

// Define sensors structure.
struct MeasureHT_t
{
	// Enable sensor - true, disable - false.
	bool IsEnable;
	// Name of sensor. Example: "First sensor".
	String Name;
	// DHT object with settings. Example: DHT(GROVE_D0 /* connected pin */, DHT22 /* sensor type */, 11 /* Constant */)
	SimpleDHT22 dht;
	// Store, read humidity from sensor.
	float Humidity;
	// Store, read temperature from sensor.
	float Temperature;
};

// Sensors count. 
#define SENSOR_COUNT 1

// Define array of 2 sensors.
MeasureHT_t _measureHT[SENSOR_COUNT] =
{
	{ true, "Sensor 1", SimpleDHT22(GROVE_D0), NAN, NAN }
};

// Check sensor data, interval in milliseconds.
const long CHECK_HT_INTERVAL_MS = 10000;
// Store last measure time.
unsigned long _mesureTimeout = 0;


// Define text colors.
const char GREEN[] = "#90EE90"; // LightGreen
const char RED[] = "#FF4500"; // OrangeRed 
const char GRAY[] = "#808080";

const uint8_t HTTP_PORT = 80;
// TCP server at port 80 will respond to HTTP requests
WiFiServer _wifiServer(HTTP_PORT);

const long LED_STATUS_INTERVAL_MS = 1000;
unsigned long _ledStatusTimeout = 0;
bool _ledState = false;

/**
* @brief Setup void. Ii is Arduino executed first. Initialize DiNo board.
*
*
* @return void
*/
void setup()
{
	delay(5000);
#ifdef DEBUG
	Serial.begin(115200);
	Serial.println("The example WiFi and Ethernet is starting...");
#endif

	// Init Dino board.
	KMPProDinoESP32.init(ProDino_ESP32);
	//KMPProDinoESP32.init(ProDino_ESP32_Ethernet);
	//KMPProDinoESP32.init(ProDino_ESP32_Ethernet_GSM);
	//KMPProDinoESP32.init(ProDino_ESP32_Ethernet_LoRa);
	//KMPProDinoESP32.init(ProDino_ESP32_Ethernet_LoRa_RFM);
	KMPProDinoESP32.SetStatusLed(blue);
	
	// Reset Relay status.
	KMPProDinoESP32.SetAllRelaysOff();

	// Start RS485 with baud 19200 and 8N1.
	KMPProDinoESP32.RS485Begin(19200);

	// Connect to WiFi network
	WiFi.begin(SSID, SSID_PASSWORD);
	Serial.print("\n\r \n\rWorking to connect");

	// Wait for connection
	while (WiFi.status() != WL_CONNECTED) {
		delay(500);
		Serial.print(".");
	}

#ifdef DEBUG
	Serial.println("");
	Serial.print("Connected to WiFi: ");
	Serial.print(SSID);
	Serial.print(" with IP address: ");
	Serial.println(WiFi.localIP());
#endif

	_wifiServer.begin();

	KMPProDinoESP32.OffStatusLed();
}

/**
* @brief Loop void. Arduino executed second.
*
*
* @return void
*/
void loop(void)
{
	ShowStatus();
	ProcessDHTSensors();

	String clientType;
	Client * client = NULL;
	// Check if a client has connected
	WiFiClient wifiClient = _wifiServer.available();

	if (wifiClient)
	{
		Serial.println("-- wifiClient --");
		// Wait for data from client to become available
		while (wifiClient.connected() && !wifiClient.available()) {
			delay(1);
		}

		clientType = "WiFi client";
		client = &wifiClient;
	}

	if (client == NULL)
	{
		return;
	}

#ifdef DEBUG
	Serial.println(">> Client connected.");
#endif

	// If client connected switch On status led.
	KMPProDinoESP32.SetStatusLed(yellow);

	// Read client request.
	ReadClientRequest(client);
	WriteClientResponse(client, &clientType);

	// Close the client connection.
	client->stop();

	// If client disconnected switch Off status led.
	KMPProDinoESP32.OffStatusLed();

#ifdef DEBUG
	Serial.println(">> Client disconnected.");
	Serial.println();
#endif
}

void ShowStatus()
{
	if (millis() > _ledStatusTimeout)
	{
		_ledState = !_ledState;

		if (_ledState)
		{
			// Here you can check statuses: is WiFi connected, is there Ethernet connection and other...
			KMPProDinoESP32.SetStatusLed(green);
		}
		else
		{
			KMPProDinoESP32.OffStatusLed();
		}

		// Set next time to read data.
		_ledStatusTimeout = millis() + LED_STATUS_INTERVAL_MS;
	}
}

bool ReadClientRequest(Stream *client)
{
#ifdef DEBUG
	Serial.println(">> Starts client request.");
#endif

	// Loop while read all request.
	// Read first and last row from request.
	String firstRow;
	String lastRow;
	if (ReadHttpRequestLine(client, &firstRow))
	{
		while (ReadHttpRequestLine(client, &lastRow));
	}

#ifdef DEBUG
	Serial.println("--firstRow--");
	Serial.println(firstRow);
	Serial.println("--lastRow--");
	Serial.println(lastRow);
#endif

	// If the request is GET we write only response.
	if (GetRequestType(firstRow.c_str()) == GET)
	{
		return true;
	}

	// Invalid request type.
	if (GetRequestType(firstRow.c_str()) != POST || lastRow.length() == 0)
	{
#ifdef DEBUG
		Serial.println(">> Invalid request type.");
#endif
		return false;
	}

	// Relay request.
	if (lastRow[0] == 'r')
	{
		// From POST parameters we get relay number and new status.
		uint8_t relayNumber = CharToInt(lastRow[1]) - 1;
		bool newState = lastRow.endsWith(W_ON);

		KMPProDinoESP32.SetRelayState(relayNumber, newState);
	}

	// RS485
	if (lastRow.startsWith("data"))
	{
		// From POST parameters we get data should be send.
		String dataToSend = GetValue(lastRow, "data");

#ifdef DEBUG
		Serial.print("RS485 data to send: ");
		Serial.println(dataToSend);
#endif

		// Transmit data.
		KMPProDinoESP32.RS485Write(dataToSend);
	}

#ifdef DEBUG
	Serial.println(">> End client request.");
#endif

	return true;
}

void WriteClientResponse(Client *client, String *clientType)
{
	if (!client->connected())
	{
		return;
	}

	// Add web page HTML.
	BuildPage(client, clientType);
}

/**
 * @brief Prepare sensor result.
 *
 * @return void
 */
String FormatMeasure(bool isEnable, float val)
{
	return isEnable ? String(val) : "-";
}

/**
 * @brief Reading temperature and humidity from DHT sensors every X seconds and if data is changed send it to Blynk.
 *
 * @return void
 */
void ProcessDHTSensors()
{
	// Checking if time to measure is occurred
	if (millis() > _mesureTimeout)
	{
		for (uint8_t i = 0; i < SENSOR_COUNT; i++)
		{
			// Get sensor structure.
			MeasureHT_t* measureHT = &_measureHT[i];
			// Is enable - read data from sensor.
			if (measureHT->IsEnable)
			{
				float humidity = NAN;
				float temperature = NAN;
				measureHT->dht.read2(&temperature, &humidity, NULL);

				if (measureHT->Humidity != humidity || measureHT->Temperature != temperature)
				{
					measureHT->Humidity = humidity;
					measureHT->Temperature = temperature;
				}
			}
		}

		// Set next time to read data.
		_mesureTimeout = millis() + CHECK_HT_INTERVAL_MS;
	}
}

String RelayTable()
{
	// Add table rows which includes relays information.
	String rows = "";
	for (uint8_t i = 0; i < RELAY_COUNT; i++)
	{
		// Row i, cell 1
		String relayNumber = String(i + 1);
		rows += "<tr><td>Relay " + relayNumber + "</td>";

		char* cellColor;
		char* cellStatus;
		char* nextRelayStatus;
		if (KMPProDinoESP32.GetRelayState(i))
		{
			cellColor = (char*)RED;
			cellStatus = (char*)W_ON;
			nextRelayStatus = (char*)W_OFF;
		}
		else
		{
			cellColor = (char*)GREEN;
			cellStatus = (char*)W_OFF;
			nextRelayStatus = (char*)W_ON;
		}

		// Cell i,2
		rows += "<td bgcolor='" + String(cellColor) + "'>" + String(cellStatus) + "</td>";

		// Cell i,3
		rows += "<td><input type='submit' name='r" + String(relayNumber) + "' value='" + String(nextRelayStatus) + "'/ ></td></tr>";
	}

	return "<h1 style = 'color: #0066FF;'>" + String(PRODINO_ESP32) + " - Web Relay example</h1>\
		<hr /><br><br>\
		<form method = 'post'>\
		<table border='1' width='300' cellpadding='5' cellspacing='0' align='center' style='text-align:center; font-size:large; font-family:Arial,Helvetica,sans-serif;'>"
		+ rows
		+ "</table>\
		</form><br><br><hr />";
}

String InputTable()
{
	// Add table rows which include input information.
	String tableBody = "";
	String tableHeader = "";
	for (uint8_t i = 0; i < OPTOIN_COUNT; i++)
	{
		tableHeader += "<th>In " + String(i + 1) + "</th>";

		char* cellColor;
		char* cellStatus;
		if (KMPProDinoESP32.GetOptoInState(i))
		{
			cellColor = (char*)RED;
			cellStatus = (char*)W_ON;
		}
		else
		{
			cellColor = (char*)GREEN;
			cellStatus = (char*)W_OFF;
		}

		tableBody += "<td bgcolor='" + String(cellColor) + "'>" + String(cellStatus) + "</td>";
	}

	return "<h1 style = 'color: #0066FF;'>" + String(PRODINO_ESP32) + " - Isolated inputs example</h1>\
		<hr /><br><br><table border='1' width='300' cellpadding='5' cellspacing='0' align='center' \
		 style='text-align:center; font-size:large; font-family:Arial,Helvetica,sans-serif;'><thead><tr>"
		+ tableHeader + "</tr></thead>"
		+ "<tbody><tr>" + tableBody + "</tr></tbody></table><br><br><hr />";
}

String RS485Table()
{
	String receivedData = "";
	int i;
	// Reading data from the RS485 port.
	while ((i = KMPProDinoESP32.RS485Read()) != -1)
	{
		// Adding received data in a buffer.
		receivedData += (char)i;
#ifdef DEBUG
		Serial.write((char)i);
#endif
	}

	return
		"<h1 style = 'color: #0066FF;'>" + String(PRODINO_ESP32) + " - RS485</h1>\
		<hr /><br><br>\
		<form method='post'>\
		<table border='1' width='300' cellpadding='5' cellspacing='0' align='center' style='text-align:center; font-size:large; font-family:Arial,Helvetica,sans-serif;'> \
		<thead><tr><th width='80%'>Data</th><th>Action</th></tr></thead> \
		<tbody><tr><td><input type='text' name='data' style='width: 100%'></td> \
		<td><input type='submit' name='btn' value='Transmit'/></td></tr> \
		<tr><td>"
		+ receivedData
		+ "</td><td>Received</td></tr></tbody>\
		</table>\
		</form><br><br><hr />";
}

String DHTTable()
{
	// Add table rows, relay information.
	String rows = "";
	for (uint8_t i = 0; i < SENSOR_COUNT; i++)
	{
		// Row i, cell 1
		MeasureHT_t* measureHT = &_measureHT[i];
		rows += "<tr><td" + (measureHT->IsEnable ? "" : " bgcolor='" + String(GRAY) + "'") + ">" + measureHT->Name + "</td>";

		// Cell i,2
		rows += "<td>" + FormatMeasure(measureHT->IsEnable, measureHT->Temperature) + "</td>";

		// Cell i,3
		rows += "<td>" + FormatMeasure(measureHT->IsEnable, measureHT->Humidity) + "</td></tr>";
	}

	return "<h1 style = 'color: #0066FF;'>" + String(PRODINO_ESP32) + " - Web DHT example</h1>\
		<hr /><br><br>\
		<table border='1' width='300' cellpadding='5' cellspacing='0' align='center' style='text-align:center; font-size:large; font-family:Arial,Helvetica,sans-serif;'>\
		<thead><tr><th style='width:30%'></th><th style='width:35%'>Temperature C&deg;</th><th>Humidity</th></tr></thead><tbody>"
		+ rows
		+ "</tbody></table><br><br><hr />";

}

/**
* @brief Build HTML page.
*
* @return void
*/
void BuildPage(Stream *client, String *clientType)
{
	// Response write.
	// Send a standard http header.
	client->write(HEADER_200_TEXT_HTML);

	client->write(
		("<html><head><title>" + String(KMP_ELECTRONICS_LTD) + " " + String(PRODINO_ESP32) + " - Web All </title></head>\
		<body><div style='text-align: center'>\
		<hr /><h1 style = 'color: #0066FF;'>" + *clientType + "</h1><hr />").c_str());

	// *clientType
	// RS485
	client->write(RS485Table().c_str());
	// Relays
	client->write(RelayTable().c_str());
	// Inputs
	client->write(InputTable().c_str());
	// DHT sensors data
	client->write(DHTTable().c_str());

	client->write(
		("<h1><a href='" + String(URL_KMPELECTRONICS_EU) + "' target='_blank'>Visit " + String(KMP_ELECTRONICS_LTD) + "</a></h1>\
		<h3><a href='" + String(URL_KMPELECTRONICS_EU_PRODINO_ESP32) + "' target='_blank'>Information about " + String(PRODINO_ESP32) + " board</a></h3>\
		<hr /></div></body></html>").c_str());
}