#include <SoftwareSerial.h>
#include <Adafruit_VC0706.h>
#include <SD.h>
#include <LiquidCrystal_I2C.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>

// SD slave pin
#define CS_PIN D8

bool debugOn = true;

// The camera may fail during reading (nodemcu only), try few times to get a picture.
bool fileOpen = false;
uint8_t maxErrors = 0;    // Max number of errors allowed
uint8_t retryShoots = 10; // Max number of chances to get a photo
SoftwareSerial camera(0, 2);  // RX(red on D3) TX(white on D4)
Adafruit_VC0706 cam = Adafruit_VC0706(&camera);

boolean takeShoot();

// bool doUpload = false;
// if true it uploads the picture, otherwise just list the files in the SD
bool doUpload = true;

// base filename to save
String devID = WiFi.macAddress() + "_000.jpg";
char *xfilename = const_cast<char*>(devID.c_str());
File root;

// LCD number of columns and rows
int lcdColumns = 16;
int lcdRows = 2;
// Set LCD address, number of columns and  rows
// Run an I2C scanner sketch to know your display address
LiquidCrystal_I2C lcd(0x27, lcdColumns, lcdRows);

// Router Configuration
const char* ssid = "ssid";
const char* wifipwd = "pass";

// FTP server details
char* xhost = "ftp_ip";
char* xusername = "ftp username";
char* xpassword = "tp pass";
char* xfolder = "ftp destination folder"; 

/* 20190113V7-FTP Client 
/*destination folder on FTP server
this is an optional parameter with the function prototype 
defined below. To leave the default folder on the server 
unchanged, either enter "" for xfolder or omit this param in 
the function call in doFTP() */

//Function prototype - required if folder is an optional argument
short doFTP(char* , char* , char* , char* , char* = "");
short FTPresult; //outcome of FTP upload

//mqtt callback
void callback(char* topic, byte* payload, unsigned int length);

String mac = WiFi.macAddress();
const char* topic = "device mac address";//const_cast<char*>(mac.c_str());
const char* mqtt_broker = "mqtt broker ip";
WiFiClient espClient;
PubSubClient client(mqtt_broker, 1883, callback, espClient);

// Button pin setting
int button = 16; //D0 (gpio16)
int buttonState = 0;

void setup(){
  Serial.begin(115200);

  pinMode(button, INPUT);
  // initialize LCD
  lcd.init();
  // turn on LCD backlight                      
  lcd.backlight();
  // set cursor to first column, first row
  lcd.setCursor(0, 0);
  lcd.print("Color Picker");

  delay(3000); // The camera does not accept commands before 2.5s at least

  pinMode(button, INPUT);
  Serial.println();
  Serial.println(mac);

  // init SD
  if (!SD.begin(CS_PIN)) {
    Serial.println("Fail to initialize SD, check the wires or even if the SD is present.");
    exit(0);
  }

  // Start camera connection
  Serial.println("Starting camera...");
  if(cam.begin()){
    // Print out the camera version information
    char *reply = cam.getVersion();
    if (reply == 0) {
      Serial.print("Failed to get version");
    } else {
      Serial.println("-----------------");
      Serial.print(reply);
      Serial.println("-----------------");
    }
    cam.reset();
    delay(3000);
    cam.setMotionDetect(false);
    // Set the picture size - you can choose one of 640x480, 320x240 or 160x120
    // Remember that bigger pictures take longer to transmit!
    // Reset is necessary only if resolution other than 640x480 is selected
    
//    cam.setImageSize(VC0706_640x480);cam.reset(); // too many errors on nodemcu
    cam.setImageSize(VC0706_320x240); cam.reset();delay(3000);
//     cam.setImageSize(VC0706_160x120); cam.reset(); delay(3000);
    Serial.print("Image size code = ");  // 00-Large 11-Medium 22-Small
    Serial.println(cam.getImageSize(), HEX);
    // cam.setCompression(80); cam.reset();
    delay(3000);
    // Serial.println("Compression = " + String((uint8_t) cam.getCompression()));
    lcd.clear();
    lcd.print("Camera ready.");
    delay(3000);
  }else{
    Serial.println("No camera found.");
    lcd.clear();
    lcd.print("Camera down.");
    delay(3000);
    }

  if(doUpload){
  //Wifi Setup
  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid,wifipwd);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print("."); }
  Serial.println();
  Serial.println("WiFi connected");  
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.println("----------------------------------------");
  lcd.clear();
  lcd.print("WiFi connected");
  }
  
  Serial.println("End setup.");
  
//  SD.end();
//  camera.end();
  Serial.println("---------------The End!---------------");
}
  
void loop(){

  // ask the mqtt broker for published messages and update the connection
  if (!client.connected()){
    reconnect();
    }
  client.loop();

  Serial.println("Hold button in 3s to snap!");
  lcd.clear();
  lcd.print("3 sec to snap!");
  delay(3000);
  // Button state reading
  buttonState=digitalRead(button);

  if (buttonState == 1) {
    uint8_t shoots = 10;    
    while(shoots > 0){
      bool ok = takeShoot();
      shoots-=1;
      cam.resumeVideo();
      delay(3000);
      if(ok)break;    // if the picture has no errors, send it.
      }
    if(shoots == retryShoots){
      Serial.println("Try changing configuration.");
      }
    Serial.print("loops needed to find one: ");
    Serial.println(retryShoots - shoots);
    
    //get and print contents of root folder
    root = SD.open("/");
    while(true){
      File entry = root.openNextFile();
      if(!entry)
        break;
  
      Serial.println();
      Serial.print(entry.name());
      if(!entry.isDirectory()){
        Serial.print(", size: ");
        Serial.println(entry.size());
        }
      }
  
    //Attempt FTP upload
    if (doUpload){
      lcd.clear();
      lcd.print("Load via FTP.");
      FTPresult = doFTP(xhost,xusername,xpassword,xfilename,xfolder);
      //What is the outcome?
      Serial.println("A return code of 226 is success");
      Serial.print("Return code = ");
      Serial.println(FTPresult);
      }
    }else{
      Serial.println("Button not pressed");
      lcd.clear();
      lcd.print("Not snapped");
      delay(2000);
      }
}

boolean takeShoot() {
  String debugMsg;

  xfilename[2] = '-';
  xfilename[5] = '-';
  xfilename[8] = '-';
  xfilename[11] = '-';
  xfilename[14] = '-';

  // Search the last name in the SD
  for (int i = 0; i < 1000; i++) {
    xfilename[18] = '0' + i/100;
    xfilename[19] = '0' + i/10;
    xfilename[20] = '0' + i%10;
    // create it if it does not exist
    if (! SD.exists(xfilename)) {
      break;
    }
  }
  
  // Prepare file to save image
  bool fileOpen = true;
  File imgFile = SD.open(xfilename, FILE_WRITE);
  if (imgFile == 0) {
    fileOpen = false;
    Serial.println("Failed to open file" );
    return false;
    }
  
  if (! cam.takePicture()){
    Serial.println("Failed to snap!");
    lcd.clear();
    lcd.print("Failed");
    cam.resumeVideo();
    SD.remove(xfilename);
    return false;
  }else{
    Serial.println("Picture shot...");
    lcd.clear();
    lcd.print("Loading image");
    delay(1200);
    }
  // Get the size of the image taken
  uint16_t jpglen = cam.frameLength(); delay(6);

  Serial.print(" ---------------- ");
  if (debugOn)  Serial.println("Read image size: " + String(jpglen));

  // Read all the data from the camera buffer
  uint8_t bytesToRead;
  uint8_t readFailures = 0;
  uint8_t* buf;
  uint8_t chunk = 64;   // read chunks of 64 bytes each
  while (jpglen > 0) {
    bytesToRead = jpglen<chunk? jpglen : chunk;
    buf = cam.readPicture(bytesToRead);  delay(15); // these delays are necessary for the nodemcu
    jpglen -= bytesToRead;
    if (buf == 0) {
      readFailures++;
    }else{
      imgFile.write(buf,bytesToRead); delay(15);  // ...here too...
      }
    }
    
  if (fileOpen) {
    imgFile.close();
    Serial.println("File closed.");
    }

  if(readFailures>maxErrors){
    Serial.println("Too many errors, retrying...");
    Serial.println(readFailures);
    SD.remove(xfilename);
    return false;
    }

  Serial.print("Image accepted. Number of errors: ");
  Serial.println(readFailures);
  return true;
}


/*------------------------------------------------------
 * FUNCTION - doFTP
 * Connects to a FTP server and transfers a file. Connection
 * is established using the FTP commands/responses defined at
 * https://en.wikipedia.org/wiki/List_of_FTP_commands or in
 * more detail at http://www.nsftools.com/tips/RawFTP.htm
 * 
 * Parameters passed:
 *   host - the IP address of the FTP server
 *   uname - username for the account on the server
 *   pwd - user password
 *   filename - the file to be transferred
 *   folder (optional) - folder on the server where 
 *   the file will be copied. This is may be omitted if 
 *   the file is to be copied into the default folder on
 *   the server.
 *   
 * Return codes:  
 *    226 - a successful transfer
 *    400+ - any return code greater than 400 indicates
 *    an error. These codes are defined at
 *    https://en.wikipedia.org/wiki/List_of_FTP_server_return_codes 
 *    Exceptions to this are:
 *    900 - failed to open file on SPIFFS
 *    910 - failed to connect to server
 *    
 * Dependencies:
 *   Libraries - <ESP8266WiFi.h> wifi library
 *               <FS.h> SPIFFS library
 *   Functions - eRcv
 --------------------------------------------------------*/
short doFTP(char* host, char* uname, char* pwd, char* fileName, char* folder)
{
  WiFiClient ftpclient;
  WiFiClient ftpdclient;

  const short FTPerrcode = 400; //error codes are > 400
  const byte Bufsize = 128;
  char outBuf[Bufsize];
  short FTPretcode = 0;
  const byte port = 21; //21 is the standard connection port
  
  File ftx = SD.open(fileName, FILE_READ); //file to be transmitted
  if (!ftx) {
    Serial.println(F("file open failed"));
    return 900;}
  if (ftpclient.connect(host,port)) {
    Serial.println(F("Connected to FTP server"));} 
  else {
    ftx.close();
    Serial.println(F("Failed to connect to FTP server"));
    return 910;}
  FTPretcode = eRcv(ftpclient,outBuf,Bufsize);
  if(FTPretcode >= 400) return FTPretcode;
  
  /* User - Authentication username 
   * Send this command to begin the login process. username should be a 
   * valid username on the system, or "anonymous" to initiate an anonymous login.
   */
  ftpclient.print("USER ");
  ftpclient.println(uname);
  FTPretcode = eRcv(ftpclient,outBuf,Bufsize);
  if(FTPretcode >= 400) return FTPretcode;
  
  /* PASS - Authentication password
   * After sending the USER command, send this command to complete 
   * the login process. (Note, however, that an ACCT command may have to be 
   * used on some systems, not needed with synology diskstation)
   */
  ftpclient.print("PASS ");
  ftpclient.println(pwd);  
  FTPretcode = eRcv(ftpclient,outBuf,Bufsize);
  if(FTPretcode >= 400) return FTPretcode;

  //CWD - Change the working folder on the FTP server
  if(!(folder == "")) {
    ftpclient.print("CWD ");
    ftpclient.println(folder);
    FTPretcode = eRcv(ftpclient,outBuf,Bufsize);
    if(FTPretcode >= 400) {return FTPretcode;} }
  
  /* SYST - Returns a word identifying the system, the word "Type:", 
   * and the default transfer type (as would be set by the 
   * TYPE command). For example: UNIX Type: L8 - this is what
   * the diskstation returns
   */
  ftpclient.println("SYST");
  FTPretcode = eRcv(ftpclient,outBuf,Bufsize);
  if(FTPretcode >= 400) return FTPretcode;
  
  /* TYPE - sets the transfer mode
   * A - ASCII text
   * E - EBCDIC text
   * I - image (binary data)
   * L - local format
   * for A & E, second char is:
   * N - Non-print (not destined for printing). This is the default if 
   * second-type-character is omitted
   * Telnet format control (<CR>, <FF>, etc.)
   * C - ASA Carriage Control
   */
  ftpclient.println("Type I");
  FTPretcode = eRcv(ftpclient,outBuf,Bufsize);
  if(FTPretcode >= 400) return FTPretcode;
  
  /* PASV - Enter passive mode
   * Tells the server to enter "passive mode". In passive mode, the server 
   * will wait for the client to establish a connection with it rather than 
   * attempting to connect to a client-specified port. The server will 
   * respond with the address of the port it is listening on, with a message like:
   * 227 Entering Passive Mode (a1,a2,a3,a4,p1,p2), e.g. from diskstation
   * Entering Passive Mode (192,168,0,5,217,101)
   */
  ftpclient.println("PASV");
  FTPretcode = eRcv(ftpclient,outBuf,Bufsize);
  if(FTPretcode >= 400) return FTPretcode;
  /* This is parsing the return from the server
   * where a1.a2.a3.a4 is the IP address and p1*256+p2 is the port number. 
   */
  char *tStr = strtok(outBuf,"(,"); //chop the output buffer into tokens based on the delimiters
  int array_pasv[6];
  for ( int i = 0; i < 6; i++) { //there are 6 elements in the address to decode
    tStr = strtok(NULL,"(,"); //1st time in loop 1st token, 2nd time 2nd token, etc.
    array_pasv[i] = atoi(tStr); //convert to int, why atoi - because it ignores any non-numeric chars
                                //after the number
    if(tStr == NULL) {Serial.println(F("Bad PASV Answer"));}
  }
  //extract data port number
  unsigned int hiPort,loPort;
  hiPort=array_pasv[4]<<8; //bit shift left by 8
  loPort=array_pasv[5]&255;//bitwise AND
  Serial.print(F("Data port: "));
  hiPort = hiPort|loPort; //bitwise OR
  Serial.println(hiPort);
  //first instance of dftp
  if(ftpdclient.connect(host, hiPort)){Serial.println(F("Data port connected"));}
  else {
    Serial.println(F("Data connection failed"));
    ftpclient.stop();
    ftx.close(); }

  /* STOR - Begin transmission of a file to the remote site. Must be preceded 
   * by either a PORT command or a PASV command so the server knows where 
   * to accept data from
   */
  ftpclient.print("STOR ");
  ftpclient.println(fileName);
  FTPretcode = eRcv(ftpclient,outBuf,Bufsize);
  if(FTPretcode >= 400) {
    ftpdclient.stop();
    return FTPretcode; } 
  Serial.println(F("Writing..."));
  
  byte clientBuf[64];
  int clientCount = 0;
  
  while(ftx.available()) {
    clientBuf[clientCount] = ftx.read();
    clientCount++;
    if(clientCount > 63) {
      ftpdclient.write((const uint8_t *)clientBuf, 64);
      clientCount = 0; }
  }
  if(clientCount > 0) ftpdclient.write((const uint8_t *)clientBuf, clientCount);
  ftpdclient.stop();
  Serial.println(F("Data disconnected"));
  FTPretcode = eRcv(ftpclient,outBuf,Bufsize);
  if(FTPretcode >= 400) {return FTPretcode; } 
  
  //End the connection
  ftpclient.println("QUIT");
  ftpclient.stop();
  Serial.println(F("Disconnected from FTP server"));

  ftx.close();
  Serial.println(F("File closed"));
  return FTPretcode;
} // end function doFTP

/*------------------------------------------------------
 * FUNCTION - eRcv
 * Reads the response from an FTP server and stores the 
 * output in a buffer.Extracts the server return code from
 * the buffer.
 * 
 * Parameters passed:
 *   aclient - a wifi client connected to FTP server and
 *   delivering the server response
 *   outBuf - a buffer to store the server response on
 *   size - size of the buffer in bytes
 *   
 * Return codes:  
 *    These are the first three chars in the buffer and are 
 *    defined in 
 *    https://en.wikipedia.org/wiki/List_of_FTP_server_return_codes 
 *    
 * Dependencies:
 *   Libraries - <ESP8266WiFi.h> wifi library
 *   Functions - none
 --------------------------------------------------------*/
short eRcv(WiFiClient aclient, char outBuf[], int size)
{
  byte thisByte;
  char index;
  String respStr = "";
  while(!aclient.available()) delay(1);
  index = 0;
  while(aclient.available()) {  
    thisByte = aclient.read();    
    Serial.write(thisByte);
    if(index < (size - 2)) { //less 2 to leave room for null at end
      outBuf[index] = thisByte;
      index++;}
  } //note if return from server is > size it is truncated.
  outBuf[index] = 0; //putting a null because later strtok requires a null-delimited string
  //The first three bytes of outBuf contain the FTP server return code - convert to int.
  for(index = 0; index < 3; index++) {respStr += (char)outBuf[index];}
  return respStr.toInt();
} // end function eRcv


void callback(char* topic, byte* payload, unsigned int length) {
  LiquidCrystal_I2C lcd(0x27, lcdColumns, lcdRows);
  // initialize LCD
  lcd.init();
  // turn on LCD backlight                      
  lcd.backlight();
  // set cursor to first column, first row

  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
    }
  Serial.println();

  String red;
  String green;
  String blue;

  red += (char)payload[0];
  red += (char)payload[1];

  green += (char)payload[3];
  green += (char)payload[4];

  blue += (char)payload[6];
  blue += (char)payload[7];

  Serial.println(red);
  Serial.println(green);
  Serial.println(blue);

  lcd.setCursor(0, 0);
  lcd.clear();
  lcd.print(red);
  lcd.print(green);
  lcd.print(blue);
  delay(6000);

}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if(client.connect ("ESP8266Client")){
      Serial.println("connected");
      client.subscribe(topic);
    }else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
      }
    }
}
