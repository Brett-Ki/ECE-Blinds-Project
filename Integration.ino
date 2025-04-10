#include <Wire.h>
#include <I2C_RTC.h>
#include <LiquidCrystal_I2C.h>
#include <DIYables_IRcontroller.h>
#include <AccelStepper.h>

/* Known Issues
  - For some reason debouncing works like compound interest, so for now its set to 0
  - Buttons on IR remote need to be properly mapped
*/

// Photoresistor Initialization
int light = analogRead(A0);

// Stepper Initialization
AccelStepper stepper(AccelStepper::DRIVER, 2, 3);

// Track current position
bool atPositionA = true;
const int positionA = 0;
const int positionB = 1000;

// LCD Initialization
LiquidCrystal_I2C lcd(0x27, 20, 4);

// RTC Initialization
static DS3231 rtc;

// IR Receiver pin
#define IR_RECEIVER_PIN 7
DIYables_IRcontroller_21 irController(IR_RECEIVER_PIN, 200);

// Alarm Storage
#define MAX_ALARMS 5
int openAlarmDay[MAX_ALARMS][7];
int openAlarmHour[MAX_ALARMS];
int openAlarmMinute[MAX_ALARMS];
int closeAlarmDay[MAX_ALARMS][7];
int closeAlarmHour[MAX_ALARMS];
int closeAlarmMinute[MAX_ALARMS];

// Previous Minutes - Used in Alarm Checks
int previousMinute1;
int previousMinute2;

// Menu items
const char *menuItems[] = {
    "Set Open Alarm", "Set Close Alarm", "Show Alarms", "Delete Open Alarm", "Delete Close Alarm", "Set Time", "Exit"};
const int menuLength = sizeof(menuItems) / sizeof(menuItems[0]);
int menuIndex = 0;

// Modes
int modeIndex = 0;
const char* modeItems[] = {"Manual Mode", "Light Mode", "Automatic Mode"};
const int modeCount = 3;

bool manualMode = false;
bool lightMode = false;
bool automaticMode = false;
bool menuMode = false;

// Manual Mode
bool manualOption = true;

// Debouncing variables
unsigned long lastCommandTime = 0;
const unsigned long debounceDelay = 0;

/* IR Remote Info 21 Keys

    CH-  CH   CH+   1   2   3
    <<   >>   >||   4   5   6
    -    +    EQ    7   8   9
    0    100+ 200+  10  11  12
    1    2    3     13  14  15
    4    5    6     16  17  18
    7    8    9     19  20  21

  
  Key21 command = irController.getKey();
  if (command != Key21::NONE) {
    switch (command) {
      case Key21::KEY_CH_MINUS:
        Serial.println("CH-");
        // TODO: YOUR CONTROL
        return 1;

      case Key21::KEY_CH:
        Serial.println("CH");
        // TODO: YOUR CONTROL
        return 2;

      case Key21::KEY_CH_PLUS:
        Serial.println("CH+");
        // TODO: YOUR CONTROL
        return 3;

      case Key21::KEY_PREV:
        Serial.println("<<");
        // TODO: YOUR CONTROL
        return 4;

      case Key21::KEY_NEXT:
        Serial.println(">>");
        // TODO: YOUR CONTROL
        return 5;

      case Key21::KEY_PLAY_PAUSE:
        Serial.println(">||");
        // TODO: YOUR CONTROL
        return 6;

      case Key21::KEY_VOL_MINUS:
        Serial.println("–");
        // TODO: YOUR CONTROL
        return 7;

      case Key21::KEY_VOL_PLUS:
        Serial.println("+");
        // TODO: YOUR CONTROL
        return 8;

      case Key21::KEY_EQ:
        Serial.println("EQ");
        // TODO: YOUR CONTROL
        return 9;

      case Key21::KEY_100_PLUS:
        Serial.println("100+");
        // TODO: YOUR CONTROL
        return 10;

      case Key21::KEY_200_PLUS:
        Serial.println("200+");
        // TODO: YOUR CONTROL
        return 11;

      case Key21::KEY_0:
        Serial.println("0");
        // TODO: YOUR CONTROL
        return 12;

      case Key21::KEY_1:
        Serial.println("1");
        // TODO: YOUR CONTROL
        return 13;

      case Key21::KEY_2:
        Serial.println("2");
        // TODO: YOUR CONTROL
        return 14;

      case Key21::KEY_3:
        Serial.println("3");
        // TODO: YOUR CONTROL
        return 15;

      case Key21::KEY_4:
        Serial.println("4");
        // TODO: YOUR CONTROL
        return 16;

      case Key21::KEY_5:
        Serial.println("5");
        // TODO: YOUR CONTROL
        return 17;

      case Key21::KEY_6:
        Serial.println("6");
        // TODO: YOUR CONTROL
        return 18;

      case Key21::KEY_7:
        Serial.println("7");
        // TODO: YOUR CONTROL
        return 19;

      case Key21::KEY_8:
        Serial.println("8");
        // TODO: YOUR CONTROL
        return 20;

      case Key21::KEY_9:
        Serial.println("9");
        // TODO: YOUR CONTROL
        return 21;

      default:
        Serial.println("WARNING: undefined command:");
        break;
    }
  }

*/


void setup() {
  Serial.begin(9600);
  Wire.begin();

  // LCD
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Starting...");
  delay(1000);
  lcd.clear();

  // IR Remote
  irController.begin();
  
  // RTC
  rtc.begin();
  rtc.setHourMode(CLOCK_H24);
  Wire.begin();
    if (!rtc.begin()) {
        lcd.setCursor(0, 0);
        lcd.print("RTC Not Found");
        while (1);
    }

  // Motor
  stepper.setMaxSpeed(1000);
  stepper.setAcceleration(500);
  stepper.setCurrentPosition(positionA);

  setMode(0); // Start in Manual Mode
}

// ===================== Modes =====================
void loop() {
  Key21 command = irController.getKey();
  unsigned long currentTime = millis();

  if (command != Key21::NONE && (currentTime - lastCommandTime > debounceDelay)) {
    lastCommandTime = currentTime;

    switch (command) {
      case Key21::KEY_6:
        setMode(3);
        displayMenu();
        break;
      case Key21::KEY_7:
        setMode(0); // Manual
        break;
      case Key21::KEY_8:
        setMode(1); // Light
        break;
      case Key21::KEY_9:
        setMode(2); // Automatic
        break;
      default:
        break;
    }
  }

  // Example: do something based on current mode
  if (manualMode) {
    manualModeFunct();
  } else if (lightMode) {
    lightModeFunct();
  } else if (automaticMode) {
    automaticModeFunct();
  } else if (menuMode) {
    mainMenu();
  }
}

void setMode(int newMode) {
  modeIndex = newMode;
  manualMode = (modeIndex == 0);
  lightMode = (modeIndex == 1);
  automaticMode = (modeIndex == 2);
  menuMode = (modeIndex == 3);

  // Print mode to Serial (for debug)
  Serial.print("Mode: ");
  Serial.println(modeItems[modeIndex]);

  // Update display
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Mode Selected:");
  lcd.setCursor(0, 1);
  lcd.print(modeItems[modeIndex]);
}
// =================================================

// ===================== Photoresistor Subsystem / Light Mode =====================
void lightModeFunct() {
  
  if(light > 400) { // When light level is low
    light = analogRead(A0);
    Serial.println(light);
    moveToPosition(false); // Closes Curtains
  }
  else { // When light level is high
      light = analogRead(A0);
      Serial.println(light);
      moveToPosition(true); // Opens Curtains
  }
}
// =================================================

// ===================== Manual Mode =====================
void manualModeFunct() {
  Key21 command = irController.getKey();
  unsigned long currentTime = millis();


  if (command != Key21::NONE && (currentTime - lastCommandTime > debounceDelay)) {
    lastCommandTime = currentTime;

    switch (command) {
      case Key21::KEY_5:
        moveToPosition(!manualOption);
        manualOption = !manualOption;
        break;
      default:
        break;
    }
  }
}
// =================================================

// ===================== Automatic Mode =====================
void automaticModeFunct() {
  checkOpenAlarms();
  checkCloseAlarms();
}
// =================================================

// ===================== Motor Subsystem =====================
void moveToPosition(bool goToA) {
  if (goToA && !atPositionA) {
    Serial.println("Moving to A");
    stepper.moveTo(positionA);
    atPositionA = true;
  } else if (!goToA && atPositionA) {
    Serial.println("Moving to B");
    stepper.moveTo(positionB);
    atPositionA = false;
  } else {
    Serial.println("Already at target position — doing nothing.");
  }
}
// =================================================

// ===================== IR Remote & LCD Subsystems =====================

// ===================== Menu =====================
void mainMenu() {
    Key21 command = irController.getKey();
    unsigned long currentTime = millis();
    if (command != Key21::NONE && (currentTime - lastCommandTime > debounceDelay)) {
        lastCommandTime = currentTime;
        switch (command) {
            case Key21::KEY_CH_MINUS:
                if (menuIndex > 0) {
                    menuIndex--;
                    displayMenu();
                }
                break;
            case Key21::KEY_CH:
                if (menuIndex < menuLength - 1) {
                    menuIndex++;
                    displayMenu();
                }
                break;
            case Key21::KEY_CH_PLUS:
                selectOption();
                break;
            default:
                break;
        }
    }
}

void displayMenu() {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Main Menu:");
    for (int i = 0; i < 3; i++) {
        if (menuIndex + i < menuLength) {
            lcd.setCursor(0, i + 1);
            lcd.print((i == 0) ? ">" : " ");
            lcd.print(menuItems[menuIndex + i]);
        }
    }
}

void selectOption() {
    switch (menuIndex) {
        case 0: setAlarm(true); break;
        case 1: setAlarm(false); break;
        case 2: showAlarms(); break;
        case 3: deleteAlarm(true); break;
        case 4: deleteAlarm(false); break;
        case 5: setTime(); break;
        case 6: setMode(2); break;
        default: displayMenu(); break;
    }
}
// =================================================

// ===================== END of IR Remote & LCD Subsystems =====================

// ===================== RTC Subsystem =====================

// ===================== User Functions =====================
void showAlarms() {
    const char *daysOfWeek[] = {"Su", "Mo", "Tu", "We", "Th", "Fr", "Sa"};
    lcd.clear();
    int yPos = 0;

    bool showingOpenAlarms = true;
    int currentAlarm = 0;

    // Count total open and close alarms
    int totalOpenAlarms = 0;
    int totalCloseAlarms = 0;
    for (int i = 0; i < MAX_ALARMS; i++) {
        if (openAlarmHour[i] != NULL) totalOpenAlarms++;
        if (closeAlarmHour[i] != NULL) totalCloseAlarms++;
    }

    unsigned long lastCommandTime = 0;
    unsigned long debounceDelay = 200;

    while (true) {
        lcd.clear();

        int index = -1; // actual index in array
        int count = 0;

        // Find the actual index in the arrays based on visible alarm index
        for (int i = 0; i < MAX_ALARMS; i++) {
            if (showingOpenAlarms && openAlarmHour[i] != NULL) {
                if (count == currentAlarm) {
                    index = i;
                    break;
                }
                count++;
            } else if (!showingOpenAlarms && closeAlarmHour[i] != NULL) {
                if (count == currentAlarm) {
                    index = i;
                    break;
                }
                count++;
            }
        }

        if (index != -1) {
            lcd.setCursor(0, yPos);
            lcd.print("Alarm #");
            lcd.print(currentAlarm + 1);
            lcd.print(": ");

            if (showingOpenAlarms) {
                lcd.print(openAlarmHour[index]);
                lcd.print(":");
                if (openAlarmMinute[index] < 10) lcd.print("0");
                lcd.print(openAlarmMinute[index]);

                lcd.setCursor(0, yPos + 1);
                for (int j = 0; j < 7; j++) {
                    if (openAlarmDay[index][j] != 0) {
                        lcd.print(daysOfWeek[openAlarmDay[index][j] - 1]);
                        if (j < 6) lcd.print(" ");
                    }
                }

                lcd.setCursor(0, yPos + 2);
                lcd.print("Open Alarm");
            } else {
                lcd.print(closeAlarmHour[index]);
                lcd.print(":");
                if (closeAlarmMinute[index] < 10) lcd.print("0");
                lcd.print(closeAlarmMinute[index]);

                lcd.setCursor(0, yPos + 1);
                for (int j = 0; j < 7; j++) {
                    if (closeAlarmDay[index][j] != 0) {
                        lcd.print(daysOfWeek[closeAlarmDay[index][j] - 1]);
                        if (j < 6) lcd.print(" ");
                    }
                }

                lcd.setCursor(0, yPos + 2);
                lcd.print("Close Alarm");
            }
        } else {
            lcd.setCursor(0, yPos);
            lcd.print("No Alarms Found");
        }

        delay(1000);

        Key21 command = irController.getKey();
        unsigned long currentTime = millis();

        if (command != Key21::NONE && (currentTime - lastCommandTime > debounceDelay)) {
            lastCommandTime = currentTime;

            switch (command) {
                case Key21::KEY_CH_MINUS:
                    if (currentAlarm > 0) {
                        currentAlarm--;
                    } else if (!showingOpenAlarms && totalOpenAlarms > 0) {
                        showingOpenAlarms = true;
                        currentAlarm = totalOpenAlarms - 1;
                    }
                    break;

                case Key21::KEY_CH:
                    if (showingOpenAlarms && currentAlarm < totalOpenAlarms - 1) {
                        currentAlarm++;
                    } else if (showingOpenAlarms && currentAlarm == totalOpenAlarms - 1 && totalCloseAlarms > 0) {
                        showingOpenAlarms = false;
                        currentAlarm = 0;
                    } else if (!showingOpenAlarms && currentAlarm < totalCloseAlarms - 1) {
                        currentAlarm++;
                    }
                    break;

                case Key21::KEY_CH_PLUS:
                    lcd.clear();
                    displayMenu();
                    return;

                default:
                    break;
            }
        }
    }
}

void deleteAlarm(bool isOpen) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(isOpen ? "Delete Which" : "Delete Which");
    lcd.setCursor(0, 1);
    lcd.print(isOpen ? "Open Alarm?" : "Close Alarm?");
    int index = 0;
    while (true) {
        lcd.setCursor(0, 2);
        lcd.print("Alarm: ");
        lcd.print(index + 1);

        Key21 command = irController.getKey();
        if (command == Key21::KEY_CH_MINUS) index = (index > 0) ? index - 1 : MAX_ALARMS - 1;
        if (command == Key21::KEY_CH_PLUS) {
            if(isOpen){
                openAlarmHour[index] = NULL;
                openAlarmMinute[index] = NULL;
                for (int i = 0; i < 7; i++) openAlarmDay[index][i] = NULL;
            }
            else {
                closeAlarmHour[index] = NULL;
                closeAlarmMinute[index] = NULL;
                for (int i = 0; i < 7; i++) closeAlarmDay[index][i] = NULL;
            }
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("Alarm ");
            lcd.print(index + 1);
            lcd.print(" Deleted");
            delay(1000);
            displayMenu();
            return;
        }
    }
}

void setTime() {
    const char *daysOfWeek[] = {"Sunday   ", "Monday   ", "Tuesday  ", "Wednesday", "Thursday ", "Friday   ", "Saturday "};
    int hours = rtc.getHours(), minutes = rtc.getMinutes(), dayOfWeek = rtc.getWeek();
    bool settingHours = true, settingMinutes = false, settingDay = false;
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Set Time:");
    while (true) {
        lcd.setCursor(0, 1);
        lcd.print(settingHours ? ">" : " ");
        lcd.print("Hour:");
        lcd.print(hours);
        
        lcd.setCursor(10, 1);
        lcd.print(settingMinutes ? ">" : " ");
        lcd.print("Min:");
        lcd.print(minutes);
        
        lcd.setCursor(0, 2);
        lcd.print(settingDay ? ">" : " ");
        lcd.print("Day:");
        lcd.print(daysOfWeek[dayOfWeek - 1]);
        
        Key21 command = irController.getKey();
        unsigned long currentTime = millis();
        if (command != Key21::NONE && (currentTime - lastCommandTime > debounceDelay)) {
          lastCommandTime = currentTime;
            switch (command) {
                case Key21::KEY_CH_MINUS:
                    if (settingHours) {
                        hours = (hours > 0) ? hours - 1 : 23;
                    } else if (settingMinutes) {
                        minutes = (minutes > 0) ? minutes - 1 : 59;
                    } else if (settingDay) {
                        dayOfWeek = (dayOfWeek > 1) ? dayOfWeek - 1 : 7;
                    }
                    lcd.clear();
                    lcd.setCursor(0, 0);
                    lcd.print("Set Time:");
                    break;
                case Key21::KEY_CH:
                    if (settingHours) {
                        hours = (hours < 23) ? hours + 1 : 0;
                    } else if (settingMinutes) {
                        minutes = (minutes < 59) ? minutes + 1 : 0;
                    } else if (settingDay) {
                        dayOfWeek = (dayOfWeek < 7) ? dayOfWeek + 1 : 1;
                    }
                    lcd.clear();
                    lcd.setCursor(0, 0);
                    lcd.print("Set Time:");
                    break;
                case Key21::KEY_PREV:
                    if (settingHours) {
                        settingHours = false;
                        settingMinutes = true;
                    } else if (settingMinutes) {
                        settingMinutes = false;
                        settingDay = true;
                    } else {
                        settingDay = false;
                        settingHours = true;
                    }
                    lcd.clear();
                    lcd.setCursor(0, 0);
                    lcd.print("Set Time:");
                    break;
                case Key21::KEY_CH_PLUS:
                    rtc.setTime(hours, minutes, 0);
                    rtc.setWeek(dayOfWeek);
                    lcd.clear();
                    lcd.setCursor(0, 0);
                    lcd.print("Time Set!");
                    delay(1000);
                    displayMenu();
                    return;
            }
        }
    }
}

void setAlarm(bool isOpen) {
    const char *daysOfWeek[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
    int hours = 12, minutes = 0, dayIndex = 0;
    bool days[7] = {false};
    bool settingHours = true, settingMinutes = false, settingDays = false;
    
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(isOpen ? "Set Open Alarm" : "Set Close Alarm");
    
    while (true) {
        lcd.setCursor(0, 1);
        lcd.print(settingHours ? ">" : " ");
        lcd.print("H:");
        lcd.print(hours);
        
        lcd.setCursor(10, 1);
        lcd.print(settingMinutes ? ">" : " ");
        lcd.print("M:");
        lcd.print(minutes);
        
        lcd.setCursor(0, 2);
        lcd.print(settingDays ? ">" : " ");
        lcd.print("Day: ");
        lcd.print(daysOfWeek[dayIndex]);
        lcd.print(days[dayIndex] ? " [ON]" : " [OFF]");
        
        Key21 command = irController.getKey();
        unsigned long currentTime = millis();
        if (command != Key21::NONE && (currentTime - lastCommandTime > debounceDelay)) {
          lastCommandTime = currentTime;
            switch (command) {
                case Key21::KEY_CH_MINUS:
                    if (settingHours) {
                        hours = (hours > 0) ? hours - 1 : 23;
                    } else if (settingMinutes) {
                        minutes = (minutes > 0) ? minutes - 1 : 59;
                    } else if (settingDays) {
                        dayIndex = (dayIndex > 0) ? dayIndex - 1 : 6;
                    }
                    lcd.clear();
                    lcd.setCursor(0, 0);
                    lcd.print(isOpen ? "Set Open Alarm" : "Set Close Alarm");
                    break;
                case Key21::KEY_CH:
                    if (settingHours) {
                        hours = (hours < 23) ? hours + 1 : 0;
                    } else if (settingMinutes) {
                        minutes = (minutes < 59) ? minutes + 1 : 0;
                    } else if (settingDays) {
                        dayIndex = (dayIndex < 6) ? dayIndex + 1 : 0;
                    }
                    lcd.clear();
                    lcd.setCursor(0, 0);
                    lcd.print(isOpen ? "Set Open Alarm" : "Set Close Alarm");
                    break;
                case Key21::KEY_NEXT:
                    if (settingHours) {
                        settingHours = false;
                        settingMinutes = true;
                    } else if (settingMinutes) {
                        settingMinutes = false;
                        settingDays = true;
                    } else {
                        settingDays = false;
                        settingHours = true;
                    }
                    lcd.clear();
                    lcd.setCursor(0, 0);
                    lcd.print(isOpen ? "Set Open Alarm" : "Set Close Alarm");
                    break;
                case Key21::KEY_PREV:
                    if (settingDays) {
                        days[dayIndex] = !days[dayIndex]; // Toggle the selected day
                    }
                    lcd.clear();
                    lcd.setCursor(0, 0);
                    lcd.print(isOpen ? "Set Open Alarm" : "Set Close Alarm");
                    break;
                case Key21::KEY_PLAY_PAUSE:
                    // Find the first empty alarm slot (value of 255 means unfilled)
                    for (int i = 0; i < MAX_ALARMS; i++) {
                        if (isOpen && openAlarmHour[i] == NULL) {
                            openAlarmHour[i] = hours;
                            openAlarmMinute[i] = minutes;
                            for (int j = 0; j < 7; j++) openAlarmDay[i][j] = days[j] ? j + 1 : 0;
                            break;
                        } else if (!isOpen && closeAlarmHour[i] == NULL) {
                            closeAlarmHour[i] = hours;
                            closeAlarmMinute[i] = minutes;
                            for (int j = 0; j < 7; j++) closeAlarmDay[i][j] = days[j] ? j + 1 : 0;
                            break;
                        }
                    }
                    lcd.clear();
                    lcd.setCursor(0, 0);
                    lcd.print("Alarm Set!");
                    delay(1000);
                    displayMenu();
                    return;  // Exit after setting the alarm
            }
        }
    }
}
// =================================================

// ===================== Automatic Functions =====================
void checkOpenAlarms() {
  if (rtc.getMinutes() != previousMinute1) {
    for (int i = 0; i < MAX_ALARMS; i++)
    {
      for (int j = 0; j < 7; j++) {
        if((rtc.getWeek() == openAlarmDay[i][j]) && (rtc.getHours() == openAlarmHour[i]) && (rtc.getMinutes() == openAlarmMinute[i])) {
          Serial.println("Curtains Open"); 
          // open curtains function
          moveToPosition(true);
        }
      }
    }
    previousMinute1 = rtc.getMinutes();
  }
}

void checkCloseAlarms() {
  if (rtc.getMinutes() != previousMinute2) {
    for (int i = 0; i < MAX_ALARMS; i++)
    {
      for (int j = 0; j < 7; j++) {
        if((rtc.getWeek() == closeAlarmDay[i][j]) && (rtc.getHours() == closeAlarmHour[i]) && (rtc.getMinutes() == closeAlarmMinute[i])) {
          Serial.println("Curtains Close");  
          // close curtains function
          moveToPosition(false);
        }
      }
    }
    previousMinute2 = rtc.getMinutes();
  }
}
// =================================================


// ===================== END of RTC Subsystem =====================
