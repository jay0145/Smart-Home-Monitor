#include <Wire.h>
#include <Adafruit_RGBLCDShield.h>
#include <utility/Adafruit_MCP23017.h>

Adafruit_RGBLCDShield lcd = Adafruit_RGBLCDShield();

#define RED 0x1
#define GREEN 0x2
#define YELLOW 0x3
#define BLUE 0x4
#define PURPLE 0x5
#define TEAL 0x6
#define WHITE 0x7

//enum defined in macros
#define IDLE 0
#define SCROLL_UP 1
#define SCROLL_DOWN 2
#define IDSCREEN 3

#define SYNC_PHASE 0
#define MAIN_PHASE 1

#define ADD_DEVICE 'A'
#define REMOVE_DEVICE 'R'
#define SET_POWER 'P'
#define SET_STATE 'S'



//Free SRAM code
#ifdef __arm__
// should use uinstd.h to define sbrk but Due causes a conflict
extern "C" char* sbrk(int incr);
#else  // __ARM__
extern char *__brkval;
#endif  // __arm__


byte upArrow[8] = {
  0b00100,
  0b01110,
  0b10101,
  0b00100,
  0b00100,
  0b00100,
  0b00100,
  0b00000
};

byte downArrow[8] = {
  0b00000,
  0b00100,
  0b00100,
  0b00100,
  0b00100,
  0b10101,
  0b01110,
  0b00100
};

//special character for degree symbol
byte celsius[8] = {
  0b01110,
  0b01010,
  0b01110,
  0b00000,
  0b00000,
  0b00000,
  0b00000,
  0b00000
};

struct device {
  String deviceId;
  char type;
  String deviceLoc;
  String state = "OFF";
  String power = "    ";
};

device deviceArray[10];
int operationPointer = 0;
int displayPointer = 0;

String deviceID; 
char deviceType;
String deviceLocation;
String deviceState;
String devicePower;

const int hyphenPositions[] = {1,5,7};
const int interval = 1000;
int phaseState = SYNC_PHASE; //for the 'FSM'
bool displayingID = false;
bool firstPress = true;
unsigned long previousTime = 0;
unsigned long currentTime;
unsigned long syncDelayPrev = 0;

//utility functions
bool containsChar(char searchChar){
  char validChars[] = {'S','O','C','T','L'};
  for (int i = 0; i < 5; i++){
    if (validChars[i]==searchChar){
      return true;
    }
  }
  return false;
}

bool isAnInteger(String checkStr){
  for (int i=0; i<checkStr.length(); i++){
    if (!isDigit(checkStr[i])){
      return false;
    }
  }
  return true;
}

//operational functions
int freeMemory() {
  char top;
#ifdef __arm__
  return &top - reinterpret_cast<char*>(sbrk(0));
#elif defined(CORE_TEENSY) || (ARDUINO > 103 && ARDUINO != 151)
  return &top - __brkval;
#else  // __arm__
  return __brkval ? &top - __brkval : &top - __malloc_heap_start;
#endif  // __arm__
};

void syncDevice() {
  lcd.setBacklight(PURPLE);
  char receivedChar;
  bool synced = false;
  
  while (!synced) {
    unsigned long syncCurrent = millis();
    if (syncCurrent-syncDelayPrev >= interval){
      Serial.print("Q");
      syncDelayPrev = syncCurrent;
    }
    
    receivedChar = Serial.read();
    
    if (receivedChar=='X') {
      synced = true;
      Serial.print("\nUDCHARS,FREERAM\n");
      lcd.setBacklight(WHITE);
    }
  }
}

void addDevice(String id, char type, String location) {
  device newDevice;
  device tempDevice;
  bool changed = false;

  if (id.length()==3) {
    id.toUpperCase();
    newDevice.deviceId = id;
    if (containsChar(type)) {
      newDevice.type = type;
      if (location.length()>0) { 
        if (location.length()>15) {
          location = location.substring(0,16);
        }
        newDevice.deviceLoc = location; 
        if (newDevice.type == 'T') {
          newDevice.power = " 9 C";
          newDevice.power[2] = char(3);
        }else if (newDevice.type == 'L' || newDevice.type == 'S') {
          newDevice.power = "  0%";
        }
        //checks if device being added already exists
        if (operationPointer > 0) {
          for (int i=0; i<operationPointer; i++){
            if (deviceArray[i].deviceId==newDevice.deviceId){
              deviceArray[i] = newDevice;
              displayPointer = i;
              displayOnLcd(deviceArray[displayPointer]);
              changed = true;
            }
          }
        }
        //if it is unique, adds new device and displays it
        if (changed==false) {
          if (operationPointer==0){
            deviceArray[operationPointer] = newDevice;
            operationPointer++;
            displayPointer = operationPointer - 1;
            displayOnLcd(deviceArray[displayPointer]);
          } else if(operationPointer > 0){
              //adds device to the end
              int insertIndex = operationPointer;
              deviceArray[operationPointer] = newDevice;
              operationPointer++;

              //pushes device "forwards" if it is alphabetically smaller than existing devices
              while (deviceArray[insertIndex].deviceId < deviceArray[insertIndex-1].deviceId){
                tempDevice = deviceArray[insertIndex];
                deviceArray[insertIndex] = deviceArray[insertIndex-1];
                deviceArray[insertIndex-1] = tempDevice;
                insertIndex--;
              }
              displayPointer = insertIndex;
              displayOnLcd(deviceArray[displayPointer]);
          }
        }
      } else {
        Serial.println(F("ERROR: location string must be entered."));
      }
    } else {
      Serial.println(F("ERROR: incorrect device type inputted."));
      }
  } else {
    Serial.println(F("ERROR: enter an id length of 3 characters."));
    }

}

void setState(String id, String state) {
  bool foundDevice = false;
  if (id.length()==3) {
    if (state=="OFF") {
      for (int i=0; i<operationPointer; i++) {
        if (deviceArray[i].deviceId == id){
          foundDevice = true;
          deviceArray[i].state = "OFF";
          displayPointer = i;
          displayOnLcd(deviceArray[displayPointer]);
        }
      }
      if (!foundDevice) {
        Serial.println(F("ERROR: device with inputted id not found."));
      }

    } else if (state=="ON") {
      for (int i=0; i < operationPointer; i++) {
        if (deviceArray[i].deviceId == id) {
          foundDevice = true;
          deviceArray[i].state = " ON";
          
          displayPointer = i;
          displayOnLcd(deviceArray[displayPointer]);
        }
      }
      if (!foundDevice) {
        Serial.println(F("ERROR: device with inputted id not found."));
      }
    } else{
      Serial.println(F("ERROR: state not recognised (ON or OFF)."));
    }
  } else {
    Serial.println(F("ERROR: enter an id length of 3 characters."));
  }
}

void setPower(String id, String power){
  bool foundDevice = false;
  String whitespace;
  String degreeString;
  if (id.length()==3){
    if (isAnInteger(power)){
      for (int i=0; i<operationPointer; i++){
        if (deviceArray[i].deviceId==id){

          foundDevice = true;

          if (deviceArray[i].type=='T'){
            if (power.toInt()>8 && power.toInt()<33){

              if (power.length()==1){
                whitespace = " ";
              } else if (power.length()==2){
                whitespace = "";
              }

              degreeString = " C";
              degreeString[0] = char(3);
              deviceArray[i].power = whitespace+power+degreeString;
              displayPointer = i;
              displayOnLcd(deviceArray[displayPointer]);
              
            }else{
              Serial.println(F("ERROR: power for Thermostat must be in range 9-32."));
            }

          } else if (deviceArray[i].type=='S' || deviceArray[i].type=='L'){
              if (power.toInt()>=0 && power.toInt()<101){

                if (power.length()==1){
                  whitespace = "  ";
                } else if (power.length()==2){
                  whitespace = " ";
                } else if (power.length()==3){
                  whitespace = "";
                }

                deviceArray[i].power = whitespace+power+"%";
                displayOnLcd(deviceArray[i]);
                
              } else{
                Serial.println(F("ERROR: power for Speaker/Light must be in range 0-100."));
              }
          } else{
            Serial.println(F("ERROR: cannot set power for this device type."));
          }
        }
      }
      if (foundDevice==false){
          Serial.println(F("ERROR: device with inputted id not found."));
        }
    } else {
      Serial.println(F("ERROR: enter a numeric value as a power."));
    }
  } else {
    Serial.println(F("ERROR: enter an id length of 3 characters."));
  }
}

void displayOnLcd(device inputD) {
  if (inputD.state == "OFF") {
    lcd.setBacklight(YELLOW);
  } else{
    lcd.setBacklight(GREEN);
  }
  lcd.clear();
  if (displayPointer == 0 && operationPointer == 0) { 
      lcd.clear();
      LCD_RETURNHOME;
      lcd.setBacklight(WHITE);
      lcd.print("NO DEVICES");
  } else if (operationPointer == 1) {
    lcd.print(" ");
    lcd.print(inputD.deviceId + " " + inputD.deviceLoc);
    lcd.setCursor(0, 1);
    lcd.print(" ");
    lcd.print(inputD.type);
    lcd.print(" " + inputD.state + " " + inputD.power);
  } else if (displayPointer==0 && operationPointer > 1){
    lcd.print(" ");
    lcd.print(inputD.deviceId + " " + inputD.deviceLoc);
    lcd.setCursor(0, 1);
    lcd.write(1);
    lcd.print(inputD.type);
    lcd.print(" " + inputD.state + " " + inputD.power);
  } else if (displayPointer == operationPointer -1 && operationPointer > 1){
    lcd.write(0);
    lcd.print(inputD.deviceId + " " + inputD.deviceLoc);
    lcd.setCursor(0, 1);
    lcd.print(" ");
    lcd.print(inputD.type);
    lcd.print(" " + inputD.state + " " + inputD.power);
  } else {
    lcd.write(0);
    lcd.print(inputD.deviceId + " " + inputD.deviceLoc);
    lcd.setCursor(0, 1);
    lcd.write(1);
    lcd.print(inputD.type);
    lcd.print(" " + inputD.state + " " + inputD.power);
  }
}

void checkButtonPress() {
  int button_state = lcd.readButtons();
  static int interval = 1000;
  switch (button_state) {
    case BUTTON_UP:{
        displayPointer --;
        if (displayPointer < 0) {
          displayPointer = 0;
        }
      displayOnLcd(deviceArray[displayPointer]);
      break;
    }
    case BUTTON_DOWN:{
      displayPointer ++;
      if (operationPointer>0){
        if (displayPointer >= operationPointer) {
          displayPointer = operationPointer - 1;
        }
      } else if(operationPointer==0){
        displayPointer = 0;
      }
      displayOnLcd(deviceArray[displayPointer]);
      break;
    }
    case BUTTON_SELECT:{ 
      currentTime = millis();
      if (firstPress){
        previousTime = currentTime; //clocks time
        firstPress = false;
      } else{
        if (!displayingID){ 
          if (currentTime - previousTime >= interval){
            int freeRam = freeMemory();
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.setBacklight(PURPLE);
            lcd.print("F225730 ");
            lcd.setCursor(0,1);
            lcd.print("Free SRAM: ");
            lcd.print(freeRam); //free ram
            displayingID = true;
          }
        }
      }
     break;
    }
    
    default:{
      firstPress = true;
      previousTime = currentTime;
      if (displayingID){
        displayOnLcd(deviceArray[displayPointer]);
        displayingID = false;
      }
    }
  }
}

void removeDevice(String id){
  bool foundDevice = false;
  if (operationPointer == 0){
    Serial.println(F("ERROR: no devices to remove."));
  } else{
    if (id.length()==3){
      for (int i=0; i<operationPointer; i++){
        if (deviceArray[i].deviceId==id){
          foundDevice = true;
          device emptyDevice;
          int currentIndex = i;
          int lastDevice = operationPointer-1;
          if (currentIndex==lastDevice){
            deviceArray[currentIndex] = emptyDevice;
            operationPointer--;
          } else if (currentIndex < lastDevice){
            while (currentIndex < lastDevice){
              deviceArray[currentIndex] = deviceArray[currentIndex+1];
              currentIndex++;
            }
            deviceArray[lastDevice] = emptyDevice;
            operationPointer--;
          }
          displayPointer = 0;
          displayOnLcd(deviceArray[displayPointer]);// in testing
        }
      }
      if (foundDevice==false){
        Serial.println(F("ERROR: device with inputted id not found."));
      }
    } else{
      Serial.println(F("ERROR: enter an id length of 3 characters."));
    }
  }
}
void setup() {
  // put your setup code here, to run once:

  Serial.begin(9600);
  Serial.setTimeout(50);
  lcd.begin(16,2);
  lcd.setCursor(0,0);

  //enumerating each character
  lcd.createChar(0,upArrow);
  lcd.createChar(1,downArrow);
  lcd.createChar(3, celsius);
  
}

void loop() {
  // put your main code here, to run repeatedly:
  switch (phaseState){
    case SYNC_PHASE:{
      syncDevice();
      lcd.print("NO DEVICES");
      phaseState = MAIN_PHASE;
    }
    case MAIN_PHASE:{
      if (Serial.available() > 0) {
      String input = Serial.readStringUntil('\n');
      input.trim(); 
      char command = input.charAt(0);
      
      switch (command) {
        case ADD_DEVICE:{
          deviceID = input.substring(2,5);
          deviceType = input.charAt(6);
          deviceLocation = input.substring(8);
          bool validInput = false;
          for (int i=0; i<3; i++) {
            if (input.charAt(hyphenPositions[i])=='-'){
              validInput = true;
            } else {
              validInput = false;
            }
          }

          if (validInput==true){
            addDevice(deviceID, deviceType, deviceLocation);
          } else {
            Serial.println(F("ERROR: input was not correctly formatted."));
          }
          break;
        }
        case SET_STATE:{
        
          deviceID = input.substring(2,5);
          deviceState = input.substring(6);
          bool validInput = false;
          for (int i=0; i<2; i++) {
            if (input.charAt(hyphenPositions[i])=='-'){
              validInput = true;
            } else{
              validInput = false;
            }
          }
          if (validInput==true) {
            setState(deviceID, deviceState);
          } else {
            Serial.println(F("ERROR: input was not correctly formatted."));
          }
        break;
        }
        case SET_POWER:{
          deviceID = input.substring(2,5);
          devicePower = input.substring(6);
          bool validInput = false;
          for (int i=0; i<2; i++){
            if (input.charAt(hyphenPositions[i])=='-'){
              validInput = true;
            } else{
              validInput = false;
            }
          }
          if (validInput==true){
            setPower(deviceID, devicePower);
          } else{
            Serial.println(F("ERROR: input was not correctly formatted."));
          }
          break;
        }
        case REMOVE_DEVICE:{
          deviceID = input.substring(2,5);
          bool validInput = false;
          for (int i=0; i<1; i++){
            if(input.charAt(hyphenPositions[i])=='-'){
              validInput = true;
            } else{
              validInput = false;
            }
          }
          if (validInput==true){
            removeDevice(deviceID);
          } else{
            Serial.println(F("ERROR: input was not correctly formatted."));
          }
          break;
        }
      }
    }
    checkButtonPress();

    }
  };
  
  

}
