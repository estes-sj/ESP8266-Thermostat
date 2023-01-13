/*********
  Rui Santos
  Complete project details at https://RandomNerdTutorials.com/esp32-esp8266-web-server-http-authentication/
  
  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.
*********/

// Import required libraries
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
// DHT Relay
#include <FS.h> // FOR SPIFFS
#include <ctype.h> // for isNumber check
#include <DHT.h>
#define DHTTYPE DHT22
#define DHTPIN D4 // D4 or 2
#define RELAYPIN D2 // D2 or 4
  
// Replace with your network credentials
const char* ssid = "I killed your cat; sorry";
const char* password = "35micesaved";

const char* http_username = "admin";
const char* http_password = "admin";

const char* PARAM_INPUT_1 = "state";

const int output = 2;

// Relay configurations
int heatOn = 69;
int heatOff = 73;
String relayState = "OFF";
const static String fName = "prop.txt";
const static String dfName = "data.txt";
int dataLines = 0;
int maxFileData = 20;

// This is for the ESP8266 processor on ESP-01
DHT dht(DHTPIN, DHTTYPE, 22); // 11 works fine for ESP8266

float humidity, temp_f;  // Values read from sensor
String webString = "";   // String to display
String webMessage = "";
// Generally, you should use "unsigned long" for variables that hold time to handle rollover
unsigned long previousMillis = 0;        // will store last temp was read
long interval = 20000;              // interval at which to read sensor

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);

// reads the temp and humidity from the DHT sensor and sets the global variables for both
void gettemperature() {

  // Reading temperature or humidity takes about 250 milliseconds!
  // Sensor readings may also be up to 2 seconds 'old' (it's a very slow sensor)
  humidity = dht.readHumidity();          // Read humidity (percent)
  temp_f = dht.readTemperature(true);     // Read temperature as Fahrenheit
  // Check if any reads failed and exit
  if (isnan(humidity) || isnan(temp_f)) {
    Serial.println("Failed to read from DHT sensor!");
    return;
  }

  // turn the relay switch Off or On depending on the temperature reading
  if (temp_f <= heatOn)
  {
    digitalWrite(RELAYPIN, HIGH);
    relayState = "ON";
  }
  else if (temp_f >= heatOff)
  {
    digitalWrite(RELAYPIN, LOW);
    relayState = "OFF";
  }
  Serial.println("RELAY STATE " + String(relayState));
}

/////////////////////////////////////////////////

void clearDataFile() // deletes all the stored data
{
  File f = SPIFFS.open(dfName, "w");
  if (!f) {
    Serial.println("data file open to clear failed");
  }
  else
  {
    Serial.println("-- Data file cleared =========");
    f.close();
  }
}

///////////////////////////////////////////////////////////////////////////////////
// removes the first line of a file by writing all data except the first line
// to a new file. The old file is deleted and new file is renamed.
///////////////////////////////////////////////////////////////////////////////////

void removeFileLine(String fi)
{
  File original = SPIFFS.open(fi, "r");
  if (!original) {

    Serial.println("original data file open failed");
  }

  File temp = SPIFFS.open("tempfile.txt", "w");
  if (!temp) {

    Serial.println("temp data file open failed");
  }

  Serial.println("------ Removing Data Lines ------");

  //Lets read line by line from the file
  for (int i = 0; i < maxFileData; i++) {

    String str = original.readStringUntil('\n'); // read a line
    if (i > 0) { // skip writing first line to the temp file

      temp.println(str);
      //  Serial.println(str); // uncomment to view the file data in the serial console
    }
  }

  int origSize = original.size();
  int tempSize = temp.size();
  temp.close();
  original.close();

  Serial.print("size orig: "); Serial.print(origSize); Serial.println(" bytes");
  Serial.print("size temp: "); Serial.print(tempSize); Serial.println(" bytes");

  if (! SPIFFS.remove(dfName))
    Serial.println("Remove file failed");


  if (! SPIFFS.rename("tempfile.txt", dfName))
    Serial.println("Rename file failed");
  // dataLines--;
}

//////////////////////////////////////////////////////////////////////
// writes the most recent variable values to the data file          //
//////////////////////////////////////////////////////////////////////

void updateDataFile()
{
  Serial.println("dataLines: ");
  Serial.println(dataLines);
  if (dataLines >= maxFileData)
  {
    removeFileLine(dfName);
  }
  ///////
  File f = SPIFFS.open(dfName, "a");
  if (!f) {

    Serial.println("data file open failed");
  }
  else
  {
    Serial.println("====== Writing to data file =========");

    f.print(relayState); f.print(":");
    f.print(temp_f); f.print( ","); f.println(humidity);

    Serial.println("Data file updated");
    f.close();
  }

  Serial.print("millis: ");
  Serial.println(millis());

}

//////////////////////////////////////////////////////////////////////////////
// reads the data and formats it so that it can be used by google charts    //
//////////////////////////////////////////////////////////////////////////////

String readDataFile()
{
  String returnStr = "";
  File f = SPIFFS.open(dfName, "r");

  if (!f)
  {
    Serial.println("Data File Open for read failed.");

  }
  else
  {
    Serial.println("----Reading Data File-----");
    dataLines = 0;

    while (f.available()) {

      //Lets read line by line from the file
      dataLines++;
      String str = f.readStringUntil('\n');
      String switchState =  str.substring(0, str.indexOf(":")  );
      /*
          String tempF = str.substring(str.indexOf(":") + 1, str.indexOf(",")  );
        //  String humid = str.substring(str.indexOf(",") + 1 );
        //    String milliSecs = str.substring(str.indexOf("~") + 1 , str.indexOf("~"));
        //   Serial.println(tempF);
        //   Serial.println(humid);
        //  Serial.println(str);
      */

      returnStr += ",['" + switchState + "'," + str.substring(str.indexOf(":") + 1) + "]";
    }
    f.close();
  }

  return returnStr;

}

//////////////////////////////////
///  used for error checking   ///
//////////////////////////////////

boolean isValidNumber(String str) {
  for (byte i = 0; i < str.length(); i++)
  {
    if (isDigit(str.charAt(i))) return true;
  }
  return false;
}

String outputState(){
  if(digitalRead(output)){
    return "checked";
  }
  else {
    return "";
  }
  return "";
}

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <title>ESP Web Server</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    html {font-family: Arial; display: inline-block; text-align: center;}
    h2 {font-size: 2.6rem;}
    body {max-width: 600px; margin:0px auto; padding-bottom: 10px;}
    .switch {position: relative; display: inline-block; width: 120px; height: 68px} 
    .switch input {display: none}
    .slider {position: absolute; top: 0; left: 0; right: 0; bottom: 0; background-color: #ccc; border-radius: 34px}
    .slider:before {position: absolute; content: ""; height: 52px; width: 52px; left: 8px; bottom: 8px; background-color: #fff; -webkit-transition: .4s; transition: .4s; border-radius: 68px}
    input:checked+.slider {background-color: #2196F3}
    input:checked+.slider:before {-webkit-transform: translateX(52px); -ms-transform: translateX(52px); transform: translateX(52px)}
  </style>
</head>
<body>
  <h2>ESP Web Server</h2>
  <button onclick="logoutButton()">Logout</button>
  <p>Ouput - GPIO 2 - State <span id="state">%STATE%</span></p>
  %BUTTONPLACEHOLDER%
<script>function toggleCheckbox(element) {
  var xhr = new XMLHttpRequest();
  if(element.checked){ 
    xhr.open("GET", "/update?state=1", true); 
    document.getElementById("state").innerHTML = "ON";  
  }
  else { 
    xhr.open("GET", "/update?state=0", true); 
    document.getElementById("state").innerHTML = "OFF";      
  }
  xhr.send();
}
function logoutButton() {
  var xhr = new XMLHttpRequest();
  xhr.open("GET", "/logout", true);
  xhr.send();
  setTimeout(function(){ window.open("/logged-out","_self"); }, 1000);
}
</script>
</body>
</html>
)rawliteral";

const char logout_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
</head>
<body>
  <p>Logged out or <a href="/">return to homepage</a>.</p>
  <p><strong>Note:</strong> close all web browser tabs to complete the logout process.</p>
</body>
</html>
)rawliteral";

const char home_html[] PROGMEM = R"rawliteral(
<html>
  <head>
    <meta http-equiv="refresh" content="60;url=http://%IP_ADDR%" />
      <meta name="viewport" content="width=device-width, initial-scale=1">
    <script
      type="text/javascript"
      src="https://www.gstatic.com/charts/loader.js"
    ></script>
    <script type="text/javascript">
      google.charts.load("current", { packages: ["corechart", "gauge"] })
      google.charts.setOnLoadCallback(drawTempChart)
      google.charts.setOnLoadCallback(drawHumidChart)
      google.charts.setOnLoadCallback(drawChart)
      function drawTempChart() {
        var data = google.visualization.arrayToDataTable([
          ["Label", "Value"],
          ["Temp°", %TEMPERATURE%],
        ])
        var options = {
          width: 250,
          height: 150,
          min: -10,
          max: 120,
          greenFrom: -10,
          greenTo: %HEAT_ON_TEMP%,
          yellowFrom: %HEAT_ON_TEMP%,
          yellowTo: %HEAT_OFF_TEMP%,
          redFrom: %HEAT_OFF_TEMP%,
          redTo: 120,
          minorTicks: 5,
        }
        var chart = new google.visualization.Gauge(
          document.getElementById("chart_divTemp")
        )
        chart.draw(data, options)
      }
      function drawHumidChart() {
        var data = google.visualization.arrayToDataTable([
          ["Label", "Value"],
          ["Humidity", %HUMIDITY%],
        ])
        var options = {
          width: 250,
          height: 150,
          min: 0,
          max: 100,
          greenFrom: 0,
          greenTo: 25,
          yellowFrom: 25,
          yellowTo: 75,
          redFrom: 75,
          redTo: 100,
          minorTicks: 5,
        }
        var chart = new google.visualization.Gauge(
          document.getElementById("chart_divHumid")
        )
        chart.draw(data, options)
      }
      function drawChart() {
        var data = google.visualization.arrayToDataTable([
          ["Hit", "Temp F", "Humidity"]
          %DATA_FILE%
        ])
        var options = {
          title: "Temp/Humidity Activity",
          curveType: "function",
          series: {
            0: { targetAxisIndex: 0 },
            1: { targetAxisIndex: 1 },
          },
          vAxes: {
            // Adds titles to each axis.
            0: { title: "Temp Fahrenheit" },
            1: { title: "Humidity " },
          },
          hAxes: {
            // Adds titles to each axis.
            0: { title: "Heat On/Off" },
            1: { title: "" },
          },
          legend: { position: "bottom" },
        }
        var chart = new google.visualization.LineChart(
          document.getElementById("curve_chart")
        )
        chart.draw(data, options);
      }
    </script>
  </head>
  <body>
    <form action="http://%IP_ADDR%/submit" method="POST">
      <h1>ESP8266-12E DHT Thermostat IoT</h1>
      <div style="color: red">Data Cleared</div>
      <table style="width: 800px">
        <tr>
          <td>
            <div>Heat On Setting: &le; %HEAT_ON_TEMP%&deg; F</div>
            <div>Heat Off Setting: &ge; %HEAT_OFF_TEMP%&deg; F</div>
          </td>
          <td style="text-align: right">
            Heat On: &le;
            <input
              type="text"
              value="%HEAT_ON_TEMP%"
              name="heatOn"
              maxlength="3"
              size="2"
            /><br />
            Heat Off: &ge;
            <input
              type="text"
              value="81"
              name="%HEAT_OFF_TEMP%"
              maxlength="3"
              size="2"
            /><br />
          </td>
          <td style="text-align: right">
            Sample Rate (seconds):
            <input
              type="text"
              value="%SAMPLE_RATE%"
              name="sRate"
              maxlength="3"
              size="2"
            /><br />
            Maximum Chart Data:
            <input
              type="text"
              value="%MAX_FILE_DATA%"
              name="maxData"
              maxlength="3"
              size="2"
            /><br />
          </td>
          <td><input type="submit" value="Submit" /></td>
        </tr>
      </table>
      <table style="width: 800px">
        <tr>
          <td>
            <div style="width: 300px"><b>Last Readings</b></div>
            <div>Temperature: %TEMP_F2%&deg; F</div>
            <div>Humidity: %HUMIDITY%</div>
            %HEAT_STATUS%
            <div>Data Lines: %DATA_LINES%</div>
            <div>Sample Rate: %SAMPLE_RATE% seconds</div>
            <div><a href="http://%IP_ADDR%">Refresh</a></div>
          </td>
          <td><div id="chart_divTemp" style="width: 250px"></div></td>
          <td><div id="chart_divHumid" style="width: 250px"></div></td>
        </tr>
      </table>
      <div id="curve_chart" style="width: 1000px; height: 500px"></div>
      <div><a href="/clear">Clear Data</a></div>
    </form>
  </body>
</html>

)rawliteral";

// Replaces placeholder with button section in your web page
String processor(const String& var){
  Serial.println(var);
  if(var == "BUTTONPLACEHOLDER"){
    String buttons ="";
    String outputStateValue = outputState();
    buttons+= "<p><label class=\"switch\"><input type=\"checkbox\" onchange=\"toggleCheckbox(this)\" id=\"output\" " + outputStateValue + "><span class=\"slider\"></span></label></p>";
    return buttons;
  }
  if (var == "STATE"){
    if(digitalRead(output)){
      return "ON";
    }
    else {
      return "OFF";
    }
  }
  if (var == "IP_ADDR"){
   return WiFi.localIP().toString();
  }
  if (var == "TEMPERATURE"){
   return String((int)temp_f);
  }
  if (var == "HUMIDITY"){
   return String((int)humidity);
  }
  if (var == "WEB_MSG"){
   return webMessage;
  }
  if (var == "HEAT_ON_TEMP"){
   return String((int)heatOn);
  }
  if (var == "HEAT_OFF_TEMP"){
   return String((int)heatOff);
  }
  if (var == "SAMPLE_RATE"){
   return String((long)interval / 1000);
  }
  if (var == "MAX_FILE_DATA"){
   return String((long)maxFileData);
  }
  if (var == "DATA_LINES"){
   return String((int)dataLines);
  }
  if (var == "TEMP_F2"){
   return String(temp_f, 2);
  }
  if (var == "DATA_FILE"){
   return readDataFile();
  }
  if (var == "HEAT_STATUS"){
    if (digitalRead(RELAYPIN) == LOW) {
      return "<div>Heat is OFF</div>";
    }
    else {
      return "<div>Heat is ON</div>";
    }
  }
  return String();
   
}

void setup(){
  // Serial port for debugging purposes
  Serial.begin(115200);

  pinMode(output, OUTPUT);
  digitalWrite(output, LOW);
  
  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  WiFi.config(IPAddress(192, 168, 1, 201), IPAddress(192, 168, 1, 1), IPAddress(255, 255, 255, 0));
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi..");
  }

  // Print ESP Local IP Address
  Serial.println(WiFi.localIP());

//////////////////////////////////////////////////////////////
  pinMode(RELAYPIN, OUTPUT);
  digitalWrite(RELAYPIN, LOW);
  
  dht.begin();           // initialize temperature sensor
  delay(10);


  SPIFFS.begin();
  delay(10);
  ///////////////////
  // SPIFFS.format(); // uncomment to completely clear data


  File f = SPIFFS.open(fName, "r");

  if (!f) {
    // no file exists so lets format and create a properties file
    Serial.println("Please wait 30 secs for SPIFFS to be formatted");

    SPIFFS.format();

    Serial.println("Spiffs formatted");

    f = SPIFFS.open(fName, "w");
    if (!f) {

      Serial.println("properties file open failed");
    }
    else
    {
      // write the defaults to the properties file
      Serial.println("====== Writing to properties file =========");

      f.print(heatOn); f.print( ","); f.print(heatOff);
      f.print("~"); f.print(interval / 1000);
      f.print(":"); f.println(maxFileData);
      Serial.println("Properties file created");
      dataLines = 1;
      f.close();
    }

  }
  else
  {
    // if the properties file exists on startup,  read it and set the defaults
    Serial.println("Properties file exists. Reading.");

    while (f.available()) {

      //Lets read line by line from the file
      String str = f.readStringUntil('\n');

      String loSet = str.substring(0, str.indexOf(",")  );
      String hiSet = str.substring(str.indexOf(",") + 1, str.indexOf("~") );
      String sampleRate = str.substring(str.indexOf("~") + 1, str.indexOf(":") );
      String maxData = str.substring(str.indexOf(":") + 1 );

      Serial.println(loSet);
      Serial.println(hiSet);
      Serial.println(sampleRate);
      Serial.println(maxData);

      heatOn = loSet.toInt();
      heatOff = hiSet.toInt();
      interval = sampleRate.toInt() * 1000;
      maxFileData = maxData.toInt();
    }

    f.close();
  }

  // now lets read the data file mainly to set the number of lines
  readDataFile();
  // now read the DHT and set the temp and humidity variables
  gettemperature();
  // update the datafile to start a new session
  updateDataFile();

////////////////////////////////////////////////////////////////////////////////////
  // Route for root / web page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    if(!request->authenticate(http_username, http_password))
      return request->requestAuthentication();
    request->send_P(200, "text/html", home_html, processor);
  });
    
  server.on("/logout", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(401);
  });

  server.on("/logged-out", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", logout_html, processor);
  });

  // Send a GET request to <ESP_IP>/update?state=<inputMessage>
  server.on("/update", HTTP_GET, [] (AsyncWebServerRequest *request) {
    if(!request->authenticate(http_username, http_password))
      return request->requestAuthentication();
    String inputMessage;
    String inputParam;
    // GET input1 value on <ESP_IP>/update?state=<inputMessage>
    if (request->hasParam(PARAM_INPUT_1)) {
      inputMessage = request->getParam(PARAM_INPUT_1)->value();
      inputParam = PARAM_INPUT_1;
      digitalWrite(output, inputMessage.toInt());
    }
    else {
      inputMessage = "No message sent";
      inputParam = "none";
    }
    Serial.println(inputMessage);
    request->send(200, "text/plain", "OK");
  });
  
  // Start server
  server.begin();
}
  
void loop() {
  
  // check timer to see if it's time to update the data file with another DHT reading
  unsigned long currentMillis = millis();

  // cast to unsigned long to handle rollover
  if ((unsigned long)(currentMillis - previousMillis) >= interval )
  {
    // save the last time you read the sensor
    previousMillis = currentMillis;

    gettemperature();

    Serial.print("Temp: ");
    Serial.println(temp_f);
    Serial.print("Humidity: ");
    Serial.println(humidity);

    updateDataFile();
    readDataFile();
  }
}
