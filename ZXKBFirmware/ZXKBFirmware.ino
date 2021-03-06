//  Name:               ZXKBFirmware
//  Version:            2.3.1
//  Author:             JuananBow
//  Original author:    Mike Daley
//  Date:               19 April 2021
//-------------------------------------------------------------------------

#include <Keyboard.h>
#include "keys.h"

const int dataLines = 5;
const int addressLines = 8;
const int dataPin[dataLines] = {A3, A2, A1, A0, 15};          // The Pro-Micro does not have an A4 pin so using 15 instead
const int address[addressLines] = {9, 8, 7, 6, 5, 4, 3, 2};
const int keyboardModeButtonPin = 0;                          // The RGB Led needs 3 pins and we are out of pins, so using pin 0 (TX) for the keyboard mode button instead 
const int red_light_pin= 10;
const int green_light_pin = 16;
const int blue_light_pin = 14;

// Tracking when special keys that need us to send specific USB keyboard values have been pressed means that
// we can release them correctly without constantly sending Keyboard.release commands for those keys
bool symbolShiftPressed = false;
bool capsShiftPressed = false;
bool keyboardModeButtonPressed = false;
bool funcKeySub = false;
bool modKeySub[5] = {false, false, false, false, false}; // Modkey submode status, CTRL, SHIFT, ALT, GUI
bool debug = false;                                      // Serial Debug. Disabled by default
bool ledstatus = true;                                   // RGB Led. Enabled by default.

enum {
  MODE_SPECTRUM = 1,
  MODE_GAMEPAD = 2,
  MODE_PC = 3,
  MODE_LEDTEST = 4
};

int keyboardMode;
int outKey;
int addrLine; 
int dataLine;
int pressed;
int ledR;
int ledG;
int ledB;

// Spectrum 16k/48k keyboard matrix. This matrix layout matches the hardware layout
// Putting LCTRL & LALT in 7,1 fixes a problem with Fuse not being able to use SymbolShift due to the different locales
int spectrumKeyMap[addressLines][dataLines] = {
  // 0---------------1-----------------------------2------3------4-----
  {KEY_1,          KEY_2,                        KEY_3, KEY_4, KEY_5}, // 0
  {KEY_Q,          KEY_W,                        KEY_E, KEY_R, KEY_T}, // 1
  {KEY_A,          KEY_S,                        KEY_D, KEY_F, KEY_G}, // 2
  {KEY_0,          KEY_9,                        KEY_8, KEY_7, KEY_6}, // 3
  {KEY_P,          KEY_O,                        KEY_I, KEY_U, KEY_Y}, // 4
  {KEY_LEFT_SHIFT, KEY_Z,                        KEY_X, KEY_C, KEY_V}, // 5
  {KEY_RETURN,     KEY_L,                        KEY_K, KEY_J, KEY_H}, // 6
  {KEY_SPACE,      KEY_LEFT_CTRL & KEY_LEFT_ALT, KEY_M, KEY_N, KEY_B}, // 7
};

// Gamepad keyboard matrix.  When running FUSE-SDL on the Pi, to get to options you need to
// press F1 which is not on the Spectrum+ keyboard, so moving to Gamepad mode
// means that pressing 1 through 0 provides F1 - F10, plus a couple of other keys, such as enter.
// To navigate the menus: WASD
int gamepadKeyMap[addressLines][dataLines] = {
  // 0------------------1---------------------------------2-----------------3-----------------4-------
  {KEY_F1,            KEY_F2,                           KEY_F3,           KEY_F4,           KEY_F5},
  {KEY_ESC,           KEY_UP_ARROW,                     0,                0,                0},
  {KEY_LEFT_ARROW,    KEY_DOWN_ARROW,                   KEY_RIGHT_ARROW,  0,                0},
  {KEY_F10,           KEY_F9,                           KEY_F8,           KEY_F7,           KEY_F6},
  {KEY_P,             0,                                0,                0,                0},
  {KEY_LEFT_SHIFT,    KEY_Z,                            KEY_X,            0,                0},
  {KEY_RETURN,        0,                                0,                KEY_J,            0},
  {KEY_SPACE,         KEY_LEFT_CTRL & KEY_LEFT_ALT,     KEY_M,            KEY_N,            0}
};

// PC Normal keyboard matrix
int pcKeyMapNormal[addressLines][dataLines] = {
  // 0---------------1-------------2------3------4
  {KEY_1,          KEY_2,        KEY_3, KEY_4, KEY_5}, // 0
  {KEY_Q,          KEY_W,        KEY_E, KEY_R, KEY_T}, // 1
  {KEY_A,          KEY_S,        KEY_D, KEY_F, KEY_G}, // 2
  {KEY_0,          KEY_9,        KEY_8, KEY_7, KEY_6}, // 3
  {KEY_P,          KEY_O,        KEY_I, KEY_U, KEY_Y}, // 4
  {KEY_LEFT_SHIFT, KEY_Z,        KEY_X, KEY_C, KEY_V}, // 5
  {KEY_RETURN,     KEY_L,        KEY_K, KEY_J, KEY_H}, // 6
  {KEY_SPACE,      0,            KEY_M, KEY_N, KEY_B}, // 7
};

// PC Shifted keyboard matrix.
// 0xFFF = CTRL+ALT+DEL - This key will send an CTRL+ALT+DEL combination, simulating the Shift+Space=Break
int pcKeyMapShifted[addressLines][dataLines] = {
  // 0---------------1-------------2----------------3-------------4---------------
  {0x00,           0x00,         0x00,            0x00,         KEY_LEFT_ARROW}, // 0
  {KEY_Q,          KEY_W,        KEY_E,           KEY_R,        KEY_T},          // 1
  {KEY_A,          KEY_S,        KEY_D,           KEY_F,        KEY_G},          // 2
  {KEY_BACKSPACE,  0,            KEY_RIGHT_ARROW, KEY_UP_ARROW, KEY_DOWN_ARROW}, // 3
  {KEY_P,          KEY_O,        KEY_I,           KEY_U,        KEY_Y},          // 4
  {KEY_LEFT_SHIFT, KEY_Z,        KEY_X,           KEY_C,        KEY_V},          // 5
  {KEY_RETURN,     KEY_L,        KEY_K,           KEY_J,        KEY_H},          // 6
  {0xFFF,          KEY_LEFT_GUI, KEY_M,           KEY_N,        KEY_B},          // 7
};

// PC Symbol-Shifted keyboard matrix.
int pcKeyMapSymbolShift[addressLines][dataLines] = {
  // 0------------------1------------------2------------------3---------------4----------------
  {KEY_EXCLAM,        KEY_AT,            KEY_HASH,          KEY_DOLLAR,     KEY_PERCENT},    // 0
  {KEY_ESC,           KEY_CAROT,         KEY_TAB,           KEY_LESSTHAN,   KEY_GTRTHAN},    // 1
  {KEY_TILDA,         KEY_BAR,           KEY_BACKSLASH,     KEY_LEFT_BRACE, KEY_RIGHT_BRACE},// 2
  {KEY_UNDER,         KEY_RIGHT_BRACKET, KEY_LEFT_BRACKET,  KEY_SGL_QUOTE,  KEY_APERSAND},   // 3
  {KEY_DBL_QUOTE,     KEY_SEMICOLON,     0,                 KEY_SQR_RIGHT,  KEY_SQR_LEFT},   // 4
  {0,                 KEY_COLON,         KEY_POUND,         KEY_QUESTION,   KEY_FRWDSLASH},  // 5
  {KEY_PRINTSCREEN,   KEY_EQUALS,        KEY_PLUS,          KEY_MINUS,      0},              // 6
  {KEY_DELETE,        0,                 KEY_PERIOD,        KEY_COMMA,      KEY_ASTER},      // 7
};

// Array used to store the state of indiviaul keys that have been pressed
bool keyPressed[addressLines][dataLines] = {
  {false, false, false, false, false},
  {false, false, false, false, false},
  {false, false, false, false, false},
  {false, false, false, false, false},
  {false, false, false, false, false},
  {false, false, false, false, false},
  {false, false, false, false, false},
  {false, false, false, false, false}
};

// *** SETUP ***
void setup() {

  // Setup the keyboard default mode and button pins
  pinMode(keyboardModeButtonPin, INPUT);
  keyboardMode = MODE_PC;

  // Setup the RGB pins and the default value
  pinMode(red_light_pin, OUTPUT);
  pinMode(green_light_pin, OUTPUT);
  pinMode(blue_light_pin, OUTPUT);
  RGB_color(128,128,128); // Spectrum mode = Red

  // Set the address line pins to output and set HIGH
  for (int a = 0; a < addressLines; a++) {
    pinMode(address[a], OUTPUT);
    digitalWrite(address[a], HIGH);
  }

  // Set the data line pins to INPUT
  for (int d = 0; d < dataLines; d++) {
    pinMode(dataPin[d], INPUT);
  }

  // Start the keyboard
  Keyboard.begin();

  // Setup the serial port for debug info if debugging has been enabled
  if (debug) Serial.begin(9600);

}

// *** LOOP ***
void loop() {

  // Check of the keyboard mode button has been pressed, if so then register its press, otherwise
  // check to see if it has registered a press and update the keyboard mode as necessary
  if (digitalRead(keyboardModeButtonPin) == LOW) {

    // If the button has not already be registered as pressed then register the press now
    if (!keyboardModeButtonPressed) {
      keyboardModeButtonPressed = true;
    }

  } else if (keyboardModeButtonPressed) {

    // The keyboard mode button has been pressed so reset the pressed state and update the
    // keyboard mode and beep to signify which mode you are in.
    keyboardModeButtonPressed = false;

    // Cycle through the different keyboard modes
    keyboardMode = (keyboardMode + 1 > MODE_PC) ? MODE_SPECTRUM : keyboardMode + 1;
    delay (100);

    // Changes the color of the RGB Led according to the operation mode active
    switch (keyboardMode) {
      case MODE_SPECTRUM: // Red
        ledR = 64;
        ledG = 0;
        ledB = 0;
        break;
      case MODE_GAMEPAD:  // Yellow
        ledR = 128;
        ledG = 128;
        ledB = 0;   
        break;
      case MODE_PC:       // White
        ledR = 128;
        ledG = 128;
        ledB = 128;      
        break;
      default:            // Off
        ledR = 0;
        ledG = 0;
        ledB = 0;
    }
    if (ledstatus) {
      RGB_color(ledR,ledG,ledB);
    } else {
      RGB_color(0, 0, 0);
    }
    // Resets all submodes and releases all modifier keys
    clearmodkeys();
    funcKeySub = false;

    if (debug) printDebug(keyboardModeButtonPin,digitalRead(keyboardModeButtonPin));
  }

  // Loop through all the address lines
  for (addrLine = 0; addrLine < addressLines; addrLine++) {

    // Set the current address line to LOW so that we can check for a LOW on the data line
    digitalWrite(address[addrLine], LOW);

    // Loop through all the data lines
    for (dataLine = 0; dataLine < dataLines; dataLine++) {

      // Get the value of the current data line...
      pressed = digitalRead(dataPin[dataLine]);

      // ...and if it's LOW then a key has been pressed
      if (pressed == LOW) {
        
        outKey = 0;

        // Deal with special keys by setting flags so we know what has been pressed
        if (addrLine == 7 && dataLine == 1 && !symbolShiftPressed) {
            symbolShiftPressed = true;
        } else if (addrLine == 5 && dataLine == 0 && !capsShiftPressed) {
            capsShiftPressed = true;
        }

        // Manage what is sent from the keyboard based on the keyboard mode
        switch (keyboardMode) {

          case MODE_SPECTRUM:
            outKey = spectrumKeyMap[addrLine][dataLine];
            break;

          case MODE_GAMEPAD:
            outKey = gamepadKeyMap[addrLine][dataLine];
            break;

          case MODE_PC:
            // In PC mode we use the shift key being pressed to read keys from a different matrix making it
            // easier to assign different keys to Spectrum keyboard keys e.g. CAPS SHIFT + 0 = BACKSPACE
            if (capsShiftPressed) {
              // Pressing a dedicated arrow key on the Spectrum+ keyboard actually regiters a CAPS SHIFT and then the
              // appropriate number for the arrow e.g. 5, 6, 7, 8. In PC mode this CAPS SHIFT means than as the cursor
              // moves it highlights as well. To stop this happening we release the CAPS SHIFT before sending the arrow
              // key. You now can't highlight with arrow keys but correct movement seemed more important :O)
                 if ((addrLine == 3 && dataLine == 2) || (addrLine == 3 && dataLine == 3) ||
                    (addrLine == 3 && dataLine == 4) || (addrLine == 0 && dataLine == 4) || (addrLine == 7 && dataLine == 1)) {
                    Keyboard.release(KEY_LEFT_SHIFT);
                 }
              // Pressing numbers 1 to 4 will put the keyboard into the special "Modifier Keys Submode".
              // In this mode pressing CAPS SHIFT + 1 to 4 will activate CTRL, SHIFT, ALT and GUI keys in that order.
              // These modifier keys will stack. So they will stay pressed until another key out of those four gets pressed.
              if ((addrLine == 0 && dataLine == 0) || (addrLine == 0 && dataLine == 1) ||
                  (addrLine == 0 && dataLine == 2) || (addrLine == 0 && dataLine == 3)) {
                  modKeySub[0] = true;
                  if (addrLine == 0 && dataLine == 0) {
                    modKeySub[1]= true;
                    Keyboard.press(KEY_LEFT_CTRL);  
                  }
                  if (addrLine == 0 && dataLine == 1) {
                    modKeySub[2]= true; 
                    Keyboard.press(KEY_LEFT_SHIFT);
                  }
                  if (addrLine == 0 && dataLine == 2) {
                    modKeySub[3]= true;
                    Keyboard.press(KEY_LEFT_ALT);  
                  }
                  if (addrLine == 0 && dataLine == 3) {
                    modKeySub[4]= true;
                    Keyboard.press(KEY_LEFT_GUI);
                  }
              }
              outKey = pcKeyMapShifted[addrLine][dataLine];
            } else if (symbolShiftPressed) {
              // Pressing Symbol Shift + H(Up arrow) will put the keyboard into the special "Function Key Submode".
              // This will remap the number keys (0-9) to the function keys (F1-F10) temporary.
              if (funcKeySub) {
                outKey = gamepadKeyMap[addrLine][dataLine];
              } else {
                outKey = pcKeyMapSymbolShift[addrLine][dataLine];
              }
              if (addrLine == 6 && dataLine == 4) funcKeySub = true;
            } else {
                outKey = pcKeyMapNormal[addrLine][dataLine];
            }
            break;

          case MODE_LEDTEST:
            outKey = 0;
            if (addrLine == 0 && dataLine == 0) RGB_color(0,0,255);     // 1 - Blue
            if (addrLine == 0 && dataLine == 1) RGB_color(255,0,0);     // 2 - Red
            if (addrLine == 0 && dataLine == 2) RGB_color(255,0,255);   // 3 - Magenta
            if (addrLine == 0 && dataLine == 3) RGB_color(0,255,0);     // 4 - Green
            if (addrLine == 0 && dataLine == 4) RGB_color(0,255,255);   // 5 - Cyan
            if (addrLine == 3 && dataLine == 4) RGB_color(255,255,0);   // 6 - Yellow
            if (addrLine == 3 && dataLine == 3) RGB_color(255,255,255); // 7 - White
            if (addrLine == 3 && dataLine == 0) RGB_color(0,0,0);       // 0 - Off (Black)
            break;
            
          default:
            if (debug) Serial.print("ERROR: Unknown keyboard mode");
        }

        // "Cheat codes"
        // Check now the position of the Keyboard Mode Button, so we can add extra functions to the keyboard in form of "Cheat codes"
        // Debug: D + Mode to enable,  X + Mode to disable
        // LED:   1 + Mode to enable,  0 + Mode to disable, 
        //        L + Mode to enable LED Test Mode. Keys 0, and 1 to 7 change the color of the LED
        while (digitalRead(keyboardModeButtonPin) == LOW) {

          if ( (addrLine == 2 && dataLine == 2) && (digitalRead(keyboardModeButtonPin) == LOW) && (!debug) ) {
            debug = true;
            Serial.println("Debug enabled!");
            printDebug(keyboardModeButtonPin,digitalRead(keyboardModeButtonPin));
          }
          if ( (addrLine == 5 && dataLine == 2) && (digitalRead(keyboardModeButtonPin) == LOW) && (debug) ) {
            debug = false;
            Serial.println("Debug disabled!");
          }
          if ( (addrLine == 0 && dataLine == 0) && (digitalRead(keyboardModeButtonPin) == LOW) && (!ledstatus) && (keyboardMode != MODE_LEDTEST) ) {
            ledstatus = true;
            RGB_color(ledR,ledG,ledB);
            if (debug) Serial.println("LED enabled!");
          }
          if ( (addrLine == 3 && dataLine == 0) && (digitalRead(keyboardModeButtonPin) == LOW) && (ledstatus) && (keyboardMode != MODE_LEDTEST) ) {
            ledstatus = false;
            RGB_color(0,0,0);
            if (debug) Serial.println("LED disabled!");
          }
          if ( (addrLine == 6 && dataLine == 1) && (digitalRead(keyboardModeButtonPin) == LOW) && (keyboardMode != MODE_LEDTEST) ) {
            if (debug) Serial.println("LED Test Mode enabled!");
            keyboardMode = MODE_LEDTEST;
            RGB_color(0,0,0);
          }
          outKey = 0;
          keyboardModeButtonPressed = false;
        }

        // We know what has been pressed and taken the key stroke to be send from the appropriate
        // matrix array, so now send it and marked it as pressed in the keyPressed matrix array
        if (!keyPressed[addrLine][dataLine]) {
          switch (outKey) {
            case 0xFFF: // CTRL+ALT+DEL
              Keyboard.releaseAll();
              delay(100);
              Keyboard.press(KEY_LEFT_CTRL);
              Keyboard.press(KEY_LEFT_ALT);
              Keyboard.press(KEY_DELETE);
              delay(100);
              Keyboard.releaseAll();
              break;

            case 0:
              break;
            
            default:
              Keyboard.press(outKey);
              if (modKeySub[0]) clearmodkeys();
              funcKeySub = false;
          }
          if (debug) printDebug(addrLine, dataLine);
          keyPressed[addrLine][dataLine] = true;
        }
        
      } else {

        outKey = 0;

        // Deal with special keys that may have been pressed previously set
        if (addrLine == 7 && dataLine == 1 && symbolShiftPressed) {
          symbolShiftPressed = false;
        } else if (addrLine == 5 && dataLine == 0 && capsShiftPressed) {
          capsShiftPressed = false;
        }

        // Based on the keyboard mode deal with releasing previously pressed keys
        switch (keyboardMode) {

          case MODE_SPECTRUM:
            outKey = spectrumKeyMap[addrLine][dataLine];
            break;

          case MODE_GAMEPAD:
            outKey = gamepadKeyMap[addrLine][dataLine];
            break;

          case MODE_PC:
            if (capsShiftPressed) {
              outKey = pcKeyMapShifted[addrLine][dataLine];
            } else if (symbolShiftPressed) {
              outKey = pcKeyMapSymbolShift[addrLine][dataLine];
            } else {
              outKey = pcKeyMapNormal[addrLine][dataLine];
            }
            break;

          case MODE_LEDTEST:
            outKey = 0;
            break;

          default:
            if (debug) Serial.println("ERROR: Unknown keyboard mode");
        }

        // Check to see if the current matrix poition has a key press registered. If so then release
        // that key and mark it as released
        if (keyPressed[addrLine][dataLine]) {
          Keyboard.release(outKey);
          keyPressed[addrLine][dataLine] = false;
        }

      }
    }

    // Set the address line back to HIGH
    digitalWrite(address[addrLine], HIGH);
  }
  delay(50); // Small delay to avoid duplicated presses
}

void RGB_color(int red_light_value, int green_light_value, int blue_light_value) {
  analogWrite(red_light_pin, red_light_value);
  analogWrite(green_light_pin, green_light_value);
  analogWrite(blue_light_pin, blue_light_value);
}

void clearmodkeys() {
  delay(100);
  Keyboard.releaseAll();
  memset(modKeySub,0,5);
}

void printDebug(int addrLine, int dataLine) {
  Serial.print("Addr Line: "); Serial.print(addrLine);
  Serial.print(" - ");
  Serial.print("Data Line: "); Serial.print(dataLine);
  Serial.print(" - ");
  Serial.print("Key: 0x"); Serial.print(outKey,HEX); Serial.print(" \["); Serial.print(char(outKey));Serial.print("\]");
  Serial.print(" - ");
  Serial.print("CAPS:");Serial.print(capsShiftPressed);
  Serial.print(" SYM:");Serial.println(symbolShiftPressed);
  Serial.print("Keyboard Mode: "); Serial.print(keyboardMode);
  Serial.print(" - Submodes: "); Serial.print((funcKeySub) ? "Func: ON" : "Func: OFF" ); Serial.print(" "); Serial.print((modKeySub[0]) ? "Mod: ON" : "Mod: OFF" );
  Serial.print(" - ");
  Serial.print("CTRL:"); Serial.print(modKeySub[1]); Serial.print(" SHIFT:"); Serial.print(modKeySub[2]); Serial.print(" ALT:"); Serial.print(modKeySub[3]); Serial.print(" GUI:"); Serial.print(modKeySub[4]);
  Serial.println("");
}
