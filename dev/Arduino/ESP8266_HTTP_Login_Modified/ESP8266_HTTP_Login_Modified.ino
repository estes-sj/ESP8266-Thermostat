/*********
  Rui Santos
  Complete project details at https://RandomNerdTutorials.com/esp32-esp8266-web-server-http-authentication/
  
  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.
*********/

// Import required libraries
#include <ESP8266WiFi.h> // For WiFi
#include <ESPAsyncTCP.h> // For Web Server
#include <ESPAsyncWebServer.h> // For Web Server
#include <FS.h> // FOR SPIFFS
#include <ctype.h> // for isNumber check
#include <DHT.h> // For DHT Sensor

#define DHTTYPE DHT22
#define DHTPIN D4 // D4 or 2
#define RELAYPIN D2 // D2 or 4
  
// Replace with your network credentials
const char* ssid = "";
const char* password = "";

const char* http_username = "admin";
const char* http_password = "admin";

// Handle webpage form parameters
const char* PARAM_INPUT_1 = "heatOn";
const char* PARAM_INPUT_2 = "heatOff";
const char* PARAM_INPUT_3 = "sRate";
const char* PARAM_INPUT_4 = "maxData";

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

// Boolean to handle restarting the device
boolean restartNow = false;

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);

// reads the temp and humidity from the DHT sensor and sets the global variables for both
void gettemperature() {

  // Reading temperature or humidity takes about 250 milliseconds!
  // Sensor readings may also be up to 2 seconds 'old' (it's a very slow sensor)
  // Temporarily store the new reading before replacing the last reading with a failed reading
  float new_humidity = dht.readHumidity();          // Read humidity (percent)
  float new_temp_f = dht.readTemperature(true);     // Read temperature as Fahrenheit
  // Check if any reads failed and exit
  // If the graph continues to have the exact reading for a long period of time
  // then that may be an indication of the sensor not reading since the values will
  // not be updated in those cases
  if (isnan(humidity) || isnan(temp_f)) {
    Serial.println("Failed to read from DHT sensor! (Not a Number)");
    return;
  }
  // Max range DHT22 can support
  else if (temp_f <= -40 || temp_f >= 257) {
    Serial.println("Failed to read from DHT sensor! (Critical Temperature/Extreme Value)");
    return;
  }
  /*
  // Signs of inacurate reading
  else if (humidity <= 0) {
    Serial.println("Failed to read from DHT sensor! (Extreme Value)");
    return;
  }
  */
  else {
     humidity = new_humidity;
     temp_f = new_temp_f;
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
    Serial.println("========= Data file cleared =========");
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

// Remember to remove any instances of ");" which will prematurely close the literal
// This is common in any javascript code
// Percent signs need to be escaped with a percent sign if literal
const char home_html[] PROGMEM = R"rawliteral(
<html>
  <head>
    <meta http-equiv="refresh" content="60;url=http://%IP_ADDR%" />
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>Chicken Coop Thermostat Relay</title>
    <style>
      body {
        background-color: #1d0f4a;
        background-image: linear-gradient(45deg, #1d0f4a, #4b1989, #5d3d8f);
        background: -webkit-gradient(
          linear,
          left top,
          right top,
          color-stop(0%%, transparent),
          color-stop(50%%, rgb(110, 48, 48)),
          color-stop(100%%, transparent)
        ); /* Chrome, Safari4+ */
        background: -webkit-linear-gradient (-45deg, red, blue); /* Chrome10+, Safari5.1+ */
        background-image: -moz-linear-gradient(
          45deg,
          #1d0f4a,
          #5d3d8f
        ); /* FF3.6+ */
        background: linear-gradient(
          45deg,
          #1a162b,
          #4c1e83,
          #5d3d8f
        ); /* W3C This appeared on my chrome desktop*/
        color: #e8e0f0;
        font-family: helvetica, sans-serif;
        font-size: 1.2em;
      }

      h1 {
        color: #e8e0f0;
        text-align: center;
        padding-top: 0.4em;
        font-size: 2.2em;
        font-weight: 900;
      }

      .subheading {
        color: #b4adc8;
        text-align: center;
      }

      .dataMain,
      .navigation,
      .lastReadings,
      #curve_chart {
        background-color: #281b54;
        border-radius: 25px;
        padding: 20px;
        margin: 0 auto;
        margin-bottom: 1.2em;
        font-weight: 700;
      }
      
      .lastReadings {
        display: flex;
        width: 25em;
      }
      .dataMain tr td {
        padding-right: 1em;
      }      
      .heatOn {
        color: #30e3b5;
      }
      .heatOff {
        color: #c33769;
      }

      .alert {
        background-color: #281b54;
        border-radius: 25px;
        padding: 20px;
        width: 40%%;
        margin: 0 auto;
        margin-bottom: 1.2em;
        text-align: center;
        font-weight: bold;
        font-size: 1.1em;
        color: #c33769;
      }

      .inputBox,
      select,
      textarea {
        color: #e8e0f0;
        background-color: #281b54;
        border-radius: 0.5em;
        width: 35px;
        height: 30px;
        font-weight: bold;
        text-align: center;
        margin-bottom: 5px;
      }

      textarea:focus,
      input:focus {
        background-color: #e8e0f0;
        color: black;
        font-weight: normal;
      }

      .navigation {
        display: grid;
        width: fit-content;
      }
      .navigation button {
        width: 10em;
        margin-left: 10em;
        margin-right: 10em;
        margin-top: 1em;
        margin-bottom: 1em;
      }      
      .nav_left {
        text-align: center;
      }
      .nav_right {
        text-align: center;
      }      
      a {
        color: black;
        text-decoration: none;
      }

      .empty {
        width: 67%%;
      }

      .submit,
      .refresh,
      .clearData {
        transition: all 0.8s;
        background-color: #756d92;
        border: 0.3em solid #756d92;
        border-radius: 0.5em;
        color: black;
        font-weight: bold;
      }

      .refresh,
      .clearData {
        margin-left: 2em;
      }

      .submit:hover,
      .refresh:hover,
      .clearData:hover,
      a:hover {
        color: rgba(255, 255, 255, 1);
        box-shadow: 0 5px 15px rgba(145, 92, 182, 1);
      }

      #chart_divTemp,
      #chart_divTemp img,
      #chart_divHumid,
      #chart_divHumid img {
        width: 150px;
        height: 150px;
        border-radius: 50%%;
      }

      #curve_chart,
      #curve_chart img {
        width: 100%%;
        min-height: 400px;
        min-width: 300;
        max-width: 900px;
      }

      @media (max-width: 950px) {
        #curve_chart {
          padding-left: 0;
          padding-right: 0;
          margin: 0 auto;
          margin-bottom: 1em;
        }
        .navigation button {
          width: 10em;
          margin-left: 5em;
          margin-right: 5em;
          margin-top: 1em;
          margin-bottom: 1em;
        }
      }

      @media (max-width: 660px) {
        body {
          background-image: none;
          background-color: #32115a;
        }
        .alert {
          width: 80%%;
        }
        .alert p {
          font-size: 0.6em;
        }        
        .navigation button {
          width: 10em;
          margin-left: 1em;
          margin-right: 1em;
          margin-top: 1em;
          margin-bottom: 1em;
        }
      }
      @media (max-width: 455px) {
        .lastReadings {
          display: block;
          width: 80%%;
        }
        .lastReadings table {
          display: flex;
          justify-content: center;
        }
        .dataMain tr td div {
          padding: 1.5em 0em;
        }
      }
    </style>
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
          ["F", %TEMPERATURE%],
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
      $(window).resize(function () {
        drawChart()
        drawTempChart()
        drawHumidChart()
      })      
      function logoutButton() {
        var xhr = new XMLHttpRequest();
        xhr.open("GET", "/logout", true);
        xhr.send();
        setTimeout(function(){ window.open("/logged-out","_self"); }, 1000);
      }
    </script>
  </head>
  <body>
    <form action="/submit">
      <h1>Chicken Coop Thermostat Relay</h1>
      <p class="subheading">ESP8266-12E DHT Thermostat Controlled Relay IoT</p>
    <div class="alert">
        <br />
        %HEAT_STATUS%
        <p>%WEB_MSG%</p>
      </div>
      <div style="color: red"></div>
      <div class="lastReadings">
        <table>
          <tr>
            <td>
              <div><b>Last Readings</b></div>
              <div>Temperature: %TEMP_F2%&deg; F</div>
              <div>Humidity: %HUMIDITY%&percnt;</div>
              <div>Data Lines: %DATA_LINES%</div>
              <div>Sample Rate: %SAMPLE_RATE% seconds</div>
            </td>
          </tr>
        </table>
        <table>
          <td><div id="chart_divTemp"></div></td>
          <td><div id="chart_divHumid"></div></td>
        </table>
      </div>
    <table class="dataMain">
        <tr>
          <td>
            <div>Heat On Setting: &le; %HEAT_ON_TEMP%&deg; F</div>
            <div>Heat Off Setting: &ge; %HEAT_OFF_TEMP%&deg; F</div>
          </td>
          <td>
            <div>          
              Heat On: &le;
              <input
                type="text"
                class="inputBox"
                value="%HEAT_ON_TEMP%"
                name="heatOn"
                maxlength="3"
                size="2"
              /><br />
            </div>
            <div>            
              Heat Off: &ge;
              <input
                type="text"
                class="inputBox"
                value="%HEAT_OFF_TEMP%"
                name="heatOff"
                maxlength="3"
                size="2"
              /><br />
            </div>            
          </td>
          <td>
            <div>          
              Sample Rate (seconds):
              <input
                type="text"
                class="inputBox"
                value="%SAMPLE_RATE%"
                name="sRate"
                maxlength="3"
                size="2"
              /><br />
            </div>
            <div>        
              Maximum Chart Data:
              <input
                type="text"
                class="inputBox"
                value="%MAX_FILE_DATA%"
                name="maxData"
                maxlength="3"
                size="2"
              /><br />
            </div>            
          </td>
          <td><input class="submit" type="submit" value="Submit" /></td>
        </tr>
      </table>
      <div id="curve_chart"></div>
      <table class="navigation">
        <tr>
          <td class="nav_left">        
            <button class="refresh" type="button">
              <a href="http://%IP_ADDR%"
              >Refresh</a></button>
          </td>
          <td class="nav_right">          
            <button class="clearData" type="button">
              <a href="/clear">Clear Data</a>
            </button>
          </td>
        </tr>
        <tr>
          <td class="nav_left">        
            <button class="refresh" onclick="logoutButton()">Logout</button>
          </td>
          <td class="nav_right">          
            <button class="clearData" type="button">
            <a href="/restart">Restart ESP8266</a></button>
          </td>
        </tr>
      </table>
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
   // If the site reloads, reset the web message since it has been acknowledged
   if (webMessage.length() > 0){
    String webMessageToSend = webMessage;
    webMessage = ""; 
    return webMessageToSend;
   }
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
      return "<div class='heatOff'>Heat is OFF</div>";
    }
    else {
      return "<div class='heatOn'>Heat is ON</div>";
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
  //SPIFFS.format(); // uncomment to completely clear data


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
      
    // Handle redirect before restarting the device
    if (restartNow) {
      restartNow = false;
      ESP.restart();
      Serial.println("Restart Complete");
    }
      
    request->send_P(200, "text/html", home_html, processor);
  });

  // setup form submit
  server.on("/submit", HTTP_GET, [](AsyncWebServerRequest *request){
    String inputMessage_1;
    String inputMessage_2;
    String inputMessage_3;
    String inputMessage_4;
    String inputParam_1;
    String inputParam_2;
    String inputParam_3;
    String inputParam_4;

    int tempON = 0;
    int tempOFF = 0;
    long sRate = 0;
    int maxData = 0;

    webMessage = "";
    
    // Format = http://192.168.1.201/submit?heatOn=80&heatOff=81&sRate=20&maxData=60
    
    if (request->hasParam(PARAM_INPUT_1)) {
      // The name of the parameter
      inputParam_1 = PARAM_INPUT_1;
      // The value
      inputMessage_1 = request->getParam(PARAM_INPUT_1)->value();
      if (inputMessage_1 != "")
      {
        if (isValidNumber(inputMessage_1) )
          tempON = inputMessage_1.toInt();
        else
          webMessage += "Heat On must be a number<br>";
      }
      else
        webMessage += "Heat On is required<br>";
    }
  
    if (request->hasParam(PARAM_INPUT_2)) {
      // The name of the parameter
      inputParam_2 = PARAM_INPUT_2;
      // The value
      inputMessage_2 = request->getParam(PARAM_INPUT_2)->value();
      if (inputMessage_2 != "")
      {
        if (isValidNumber(inputMessage_2) )
          tempOFF = inputMessage_2.toInt();
        else
          webMessage += "Heat On must be a number<br>";
      }
      else
        webMessage += "Heat On is required<br>";
    }
  
    if (request->hasParam(PARAM_INPUT_3)) {
      // The name of the parameter
      inputParam_3 = PARAM_INPUT_3;
      // The value
      inputMessage_3 = request->getParam(PARAM_INPUT_3)->value();
      if (inputMessage_3 != "")
      {
        if (isValidNumber(inputMessage_3) )
          sRate = inputMessage_3.toInt();
        else
          webMessage += "Heat On must be a number<br>";
      }
      else
        webMessage += "Heat On is required<br>";
    }
  
    if (request->hasParam(PARAM_INPUT_4)) {
      // The name of the parameter
      inputParam_4 = PARAM_INPUT_4;
      // The value
      inputMessage_4 = request->getParam(PARAM_INPUT_4)->value();
      if (inputMessage_4 != "")
      {
        if (isValidNumber(inputMessage_4) )
          maxData = inputMessage_4.toInt();
        else
          webMessage += "Heat On must be a number<br>";
      }
      else
        webMessage += "Heat On is required<br>";
    }
  
  
    if (tempOFF <= tempON)
      webMessage += "Heat On must be lower than Heat Off<br>";

    if (sRate < 10)
      webMessage += "Sample Rate must be greater than or equal to 10<br>";

    if (maxData < 10 || maxData > 300)
      webMessage += "Max Chart Data must be between 10 and 300<br>";
  
  
    Serial.println("SUBMIT INFO");
    Serial.println(inputMessage_1);
    Serial.println(inputParam_1);
    Serial.println(tempON);
    Serial.println(PARAM_INPUT_1);
    
  if (webMessage == ""){
      heatOn = tempON;
      heatOff = tempOFF;
      interval = sRate * 1000;
      maxFileData = maxData;
      ///////
      File f = SPIFFS.open(fName, "w");
      if (!f) {

        Serial.println("file open for properties failed");
      }
      else
      {
        Serial.println("====== Writing to config file =========");

        f.print(heatOn); f.print( ","); f.print(heatOff);
        f.print("~"); f.print(sRate);
        f.print(":"); f.println(maxData);
        Serial.println("Properties file updated");
        f.close();
      }
    }

    if (webMessage == "") {
      webMessage = "Settings Updated";
    }
    Serial.println("WEB MESSAGE");
    Serial.println(webMessage);
    //gettemperature();
    // Send back to home page
    request->redirect("/");
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

  server.on("/clear", HTTP_ANY, [](AsyncWebServerRequest *request){
    // handler for http://iPaddress/clear

    // deletes all the stored data for temp and humidity
    clearDataFile();

    webMessage = "Data Cleared";

    // read the DHT and use new values to start new file data
    gettemperature();
    updateDataFile();
    request->redirect("/");
    delay(100);

  });

  server.on("/restart", HTTP_ANY, [](AsyncWebServerRequest *request){
    restartNow = true;
    request->redirect("/");
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
