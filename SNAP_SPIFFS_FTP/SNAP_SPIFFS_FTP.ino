#include <SoftwareSerial.h>
#include <Adafruit_VC0706.h>
#include <LiquidCrystal_I2C.h>
#include <ESP8266WiFi.h>
#include "FS.h"

// Device identifier
char* deviceID = "NM0001";

const char* ssid = "your_ssid";
const char* wifipwd = "your_wifipwd";

// FTP server details
char* xhost = "your_IP_Address";
char* xusername = "your_ftp_username";
char* xpassword = "your_ftp_password";
// Destination folder on FTP server
char* xfolder = "/files";
/* This is an optional parameter with the function prototype 
defined below. To leave the default folder on the server 
unchanged, either enter "" for xfolder or omit this param in 
the function call in doFTP() */

// SPIFFS file path to upload
char* xfilepath = "/last.jpg";
// Name of the uploaded file
char* xfilename = "last.jpg";
 // FTP response for the upload operation
short FTPresult;


bool debugOn = false;
bool fileOpen = false;
// Maximum number of acceptable errors in camera reading
uint8_t maxErrors = 0;
// Maximum number of attempts taking shots
uint8_t retryShoots = 10;
// Camera connection to NodeMCU: TX -> D3 and RX -> D4)
SoftwareSerial camera(0, 2);

Adafruit_VC0706 cam = Adafruit_VC0706(&camera);

// File path used to save the photo on the file system SPIFFS of NodeMCU
char* filePath = "/last.jpg";

// LCD number of columns and rows
int lcdColumns = 16;
int lcdRows = 2;
// Set LCD address, number of columns and  rows
// Run an I2C scanner sketch to know your display address
LiquidCrystal_I2C lcd(0x27, lcdColumns, lcdRows);

// Function prototype - perform the shoot
boolean takeShoot();
//Function prototype - required if folder is an optional argument
short doFTP(char* , char* , char* , char* , char* , char* = "");

// Button pin setting
int button = 16; //D0 (gpio16)
int buttonState = 0;

bool doUpload = true;

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

  	delay(3000); 	

	Serial.println("---------------Setup complete---------------");
	lcd.clear();
	lcd.print("Press the button");
	lcd.setCursor(0,1);  
  	lcd.print("to take a photo");
}
  
void loop() {

	// Button state reading
	buttonState=digitalRead(button);

	if (buttonState == 1) {
		Serial.println("Button pressed");
		lcd.clear();
		lcd.setCursor(0,0); 
		lcd.print("Button pressed");

		// Start camera connection
		// Try to get the baud rate of the serial connection
		Serial.println("Starting camera...");
		lcd.clear();
		lcd.print("Starting camera");

		if(cam.begin()) {
			// Print out the camera version information
			char *reply = cam.getVersion();
			if (reply == 0) {
			  Serial.print("Failed to get version");
			  lcd.clear();
			  lcd.print("Failed to get version");
			}
			else {
			  Serial.println("-----------------");
			  Serial.print(reply);
			  Serial.println("-----------------");
			}
			cam.reset();
			delay(3000);
			cam.setMotionDetect(false);

			// Set the picture size: 640x480, 320x240 or 160x120
			// Bigger pictures take longer to transmit
			// Reset is necessary only if resolution other than 640x480 is selected
			//cam.setImageSize(VC0706_640x480); // The biggest format does not work
			// cam.setImageSize(VC0706_320x240); cam.reset();delay(3000); // Medium, perform in 4 or 5 attemps
			cam.setImageSize(VC0706_160x120); cam.reset(); delay(3000); // Small, perform in 3 or 4 attemps	
			Serial.print("Image size selected");
			lcd.clear();
			lcd.print("Shooting...");
			uint8_t shoots = 0;
			bool success = false;
			while(shoots < retryShoots && success == false) {
				delay(3000);
				success = takeShoot();
				shoots++;
				cam.resumeVideo();			
			}
			if(shoots == retryShoots) {
				Serial.println("No shoots in 10 attempts without errors. Try changing configuration.");
				lcd.clear();
				lcd.print("Shoots failure");
			}
			else {
				Serial.print("Loops needed to find one: ");
				Serial.println(shoots);
				lcd.clear();
				lcd.print("Server upload...");
				bool ftpResult = sendToFTP();
				if (ftpResult) {
					lcd.clear();
					lcd.print("File loaded");
				}
				else{
					lcd.clear();
					lcd.print("Upload failure");	
				}
			}
		}
		else{
			Serial.println("No camera found");
			lcd.clear();
			lcd.print("No camera found");
		}
		camera.end();
		Serial.println("--------------- Process complete ---------------");
		lcd.clear();
		lcd.print("Process complete");
		delay(1000);
		lcd.clear();
		lcd.print("Press the button");
		lcd.setCursor(0,1);  
  		lcd.print("to take a photo");
	}
	if (buttonState==0){
		//Serial.println("Button not pressed");
		delay(200);
	}
}
// Attention! - Consider to retry the FTP upload later if it does not succeed at the first time

boolean takeShoot() {
	String debugMsg;

	// Initialize file system.
	bool formatOk = false;
	bool beginOk = SPIFFS.begin();
	if (beginOk) {

		// Formatting file system
		formatOk = SPIFFS.format();
		if (formatOk) {
			Serial.println("SPIFFS ready to use");
			lcd.clear();
			lcd.print("Memory ready");

			// File creation with write option to save image			
			File imgFile = SPIFFS.open(filePath, "w");
			if (!imgFile) {			
				Serial.println("Failed to open the created file" );
				lcd.clear();
				lcd.print("File failure");
				return false;
			}
			
			Serial.println("Success on open the created file" );
			lcd.print("File created");

			if (! cam.takePicture()){
				Serial.println("Failed to snap");
				lcd.clear();
				lcd.print("Failed to snap");
				cam.resumeVideo();
				return false;
			}
			
			Serial.println("Picture shoots with success");
			lcd.clear();
			lcd.print("Shot success!");
			delay(1200);
			
			// Get the size of the image (frame) taken
			uint16_t jpgSize = cam.frameLength(); delay(6);

			Serial.println(" ---------------- ");
			Serial.println("Image size: " + String(jpgSize));
			Serial.println(" ---------------- ");

			// Print image size in the LCD display
			lcd.clear();
			lcd.print("Image size: " + String(jpgSize));
			lcd.clear();
			lcd.print("Storing image...");

			uint32_t startTime = millis();
			// Timout counter
			uint32_t timeOut = millis();
			// Number of bytes written in the filesystem
			int bytesWritten = 0;
			uint8_t bytesToRead;
			uint8_t readFailures = 0;
			uint8_t* buf;
			// Bytes chunk dimension for the camera buffer reading operation
			uint8_t chunk = 8;			
			// Read all the data up to jpgSize bytes
			while (jpgSize > 0) {
				bytesToRead = jpgSize < chunk ? jpgSize : chunk;
				buf = cam.readPicture(bytesToRead);
				delay(15);				
				jpgSize -= bytesToRead;

				if (buf == 0) readFailures++;				
				else {
					if (debugOn) Serial.println("Bytes to read: " + String(bytesToRead));
					bytesWritten += imgFile.write(buf,bytesToRead);
					if (debugOn) Serial.println("Bytes writed: " + String(bytesWritten));
					delay(15);									  	
				}
			}
			cam.printBuff();

			imgFile.close();
			Serial.println("File closed");			

			if(readFailures > maxErrors) {
				Serial.println("Too many errors during camera reading, the process has been terminated");
				lcd.clear();
				lcd.print("Too many errors");
				Serial.println(readFailures);
				return false;
			}

			uint32_t endTime = millis();
			if (debugOn) {
				float transRate = ((float)bytesWritten/1024)/((float)(endTime-startTime)/1000.0);
				debugMsg = "Read from camera finished: " + String(bytesWritten) + " bytes in " + String(endTime-startTime) + " ms -> " + String(transRate) + " kB/s";
				Serial.println(debugMsg);
			}

			Serial.print("Image accepted. Number of errors: ");
			lcd.clear();
			lcd.print("Image stored");
			Serial.println(readFailures);
			return true;

		}
		else {
			Serial.println("SPIFFS not formatted, can not use it");
			lcd.clear();
			lcd.print("Memory failure");
			return false;
		}
		SPIFFS.end();
	}
	else {
		Serial.println("SPIFFS currupted, can not use it");
		lcd.clear();
		lcd.print("Memory failure");
		return false;
	}
}

boolean sendToFTP() {

	if(doUpload){
	  	// Wifi Setup
	    Serial.println();
	    Serial.print("Connecting to ");
	    Serial.println(ssid);
	    WiFi.begin(ssid,wifipwd);
	    while (WiFi.status() != WL_CONNECTED) {
			delay(500);
			Serial.print("."); 
	  	}
	    Serial.println();
	    Serial.println("WiFi connected");  
	    Serial.println("IP address: ");
	    Serial.println(WiFi.localIP());
	    Serial.println("----------------------------------------");
	}

	// Get and print contents of SPIFFS root folder
	SPIFFS.begin();
	Serial.println("SPIFFS content:");
	String str = "";
	Dir dir = SPIFFS.openDir("");
	while (dir.next()) {
		str += dir.fileName();
		str += " / ";
		str += dir.fileSize();
		str += "\r\n";
	}
	Serial.print(str);
	Serial.println("--------------------------------");

	if (doUpload){
	    FTPresult = doFTP(xhost,xusername,xpassword,xfilepath,xfilename,xfolder);	    
	    Serial.println("A return code of 226 is success");
	    Serial.print("Return code = ");
	    Serial.println(FTPresult);
	    if (FTPresult == 226) {
	    	Serial.println("FTP success");
	    }
	    else {
	    	Serial.println("FTP failure");
	    }

	}
	SPIFFS.end();
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
 *    400+ - any return code greater than 400 indicates an error.
 *    These codes are defined at
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
short doFTP(char* host, char* uname, char* pwd, char* filePath, char* fileName, char* folder)
{
  WiFiClient ftpclient;
  WiFiClient ftpdclient;

  const short FTPerrcode = 400; // error codes are > 400
  const byte Bufsize = 100;
  char outBuf[Bufsize];
  short FTPretcode = 0;
  const byte port = 21; // 21 is the standard connection port
  
  Serial.print("Trying to open the file path: ");
  Serial.println(filePath);
  File ftx = SPIFFS.open(filePath, "r"); // file to be transmitted
  if (!ftx) {
    Serial.println(F("File open failed"));
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
    if(index < (size - 2)) { // Less 2 to leave room for null at end
      outBuf[index] = thisByte;
      index++;}
  } // Note: the return from server will be truncated if it is greater than size.
  outBuf[index] = 0; // Putting a null because later strtok requires a null-delimited string
  //The first three bytes of outBuf contain the FTP server return code - convert to int.
  for(index = 0; index < 3; index++) {respStr += (char)outBuf[index];}
  return respStr.toInt();
} // end function eRcv