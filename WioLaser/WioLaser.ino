/********************************************************************/
// First we include the libraries
#include <OneWire.h> 
#include <DallasTemperature.h>
#include"TFT_eSPI.h"
#include"Free_Fonts.h" //include the header file

#define TFT_GREY 0x5AEB
#define LOOP_PERIOD 35 // Display updates every 35 ms

/********************************************************************/
// Data wire is plugged into pin D0 
#define ONE_WIRE_BUS D0 
#define FLOW_PIN D1
#define RELAY_PIN D2
#define LID_PIN D3
/********************************************************************/
// Setup a oneWire instance to communicate with any OneWire devices  
// (not just Maxim/Dallas temperature ICs) 
OneWire oneWire(ONE_WIRE_BUS); 
TFT_eSPI tft = TFT_eSPI(); 
/********************************************************************/
// Pass our oneWire reference to Dallas Temperature. 
DallasTemperature sensors(&oneWire);
/********************************************************************/ 


#define M_SIZE 1.4
 
TFT_eSprite spr = TFT_eSprite(&tft);  // Sprite
 
float ltx = 0;    // Saved x coord of bottom of needle
uint16_t osx = M_SIZE * 120, osy = M_SIZE * 120; // Saved x & y coords
uint32_t updateTime = 0;       // time for next update
uint32_t updateTimeFlow = 0;       // time for next flow update
uint32_t updateTimeAlertLed = 0;       // time for next flow update
 
int old_analog =  -999; // Value last displayed
 
int value[6] = {0, 0, 0, 0, 0, 0};

int drawAlertLed = 0;

double flowRate; 
volatile int count;

int  lid_open = 1;
int  relay_on = 0;

int  alert_Condition_Lid = 1;
int  alert_Condition_Flow = 1;
int  alert_Condition_Temp = 1;

int temp_treshold = 55;  //Alert limit for temp
int flow_treshold = 4;   //Alert for flow

void setup(void) 
{ 
 digitalWrite(RELAY_PIN, LOW);
 
 // start serial port 
 Serial.begin(115200); 
 Serial.println("K40 Laser Controll"); 
 // Start up the library 
 sensors.begin(); 

  pinMode(FLOW_PIN, INPUT);
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(LID_PIN, INPUT_PULLUP);         
  attachInterrupt(FLOW_PIN, Flow, RISING);
  attachInterrupt(LID_PIN, Lid_Open, CHANGE );  

  Lid_Open();

  tft.begin();
  tft.init();
  tft.setRotation(3);
  tft.fillScreen(TFT_WHITE);
  spr.createSprite(TFT_HEIGHT,TFT_WIDTH);
  spr.setRotation(3);

  analogMeter(); // Draw analogue meter

  updateTime = millis(); // Next update time
  updateTimeFlow = millis();
  updateTimeAlertLed = millis();
} 

void loop(void) 
{ 
  float temperature = 0;

  if (updateTime <= millis()) {
        updateTime = millis() + 35; // Update emter every 35 milliseconds

         // call sensors.requestTemperatures() to issue a global temperature 
         // request to all devices on the bus 
        /********************************************************************/
         Serial.print(" Requesting temperatures..."); 
         sensors.requestTemperatures(); // Send the command to get temperature readings 
         Serial.println("DONE"); 
        /********************************************************************/
         
         temperature = sensors.getTempCByIndex(0);// Why "byIndex"? 

         // You can have more than one DS18B20 on the same bus.  
         // 0 refers to the first IC on the wire 

         if (temperature > temp_treshold) {
            alert_Condition_Temp = 1;
            digitalWrite(RELAY_PIN, LOW);           
         }
         else if (temperature < temp_treshold) {
            alert_Condition_Temp = 0;
         }
       
        plotNeedle(temperature, 0); // It takes between 2 and 12ms to replot the needle with zero delay
  }

  if (updateTimeFlow <= millis()) {
        updateTimeFlow = millis() + 1000;
        flowRate = (count / 2.25);
        count = 0;

        if (flowRate < flow_treshold) {
           alert_Condition_Flow = 1;
           digitalWrite(RELAY_PIN, LOW);           
        }
        else if (flowRate > flow_treshold) {
           alert_Condition_Flow = 0;
        }
  }

  if (updateTimeAlertLed <= millis()) {
        updateTimeAlertLed  = millis() + 500;
        if ( drawAlertLed == 1){drawAlertLed = 0;}
        else {
          if (alert_Condition_Flow == 0 & alert_Condition_Temp == 0 & alert_Condition_Lid == 0){
            drawAlertLed = 0;
          }
          else {
            drawAlertLed = 1;
          }
       }          
     
  }

  Reset_Relay();

  delay(100);
  spr.fillSprite(TFT_WHITE);
  spr.createSprite(250, 40);
  spr.fillSprite(TFT_WHITE);
  
  if (alert_Condition_Temp == 1){spr.setTextColor(TFT_RED, TFT_WHITE);}
  else {spr.setTextColor(TFT_BLACK, TFT_WHITE);}
  spr.setFreeFont(&FreeSansBoldOblique12pt7b);
  spr.drawNumber(temperature, 0, 0);
  spr.drawString("C", 30, 0);

  if (alert_Condition_Flow == 1){spr.setTextColor(TFT_RED, TFT_WHITE);}
  else {spr.setTextColor(TFT_BLACK, TFT_WHITE);}
  spr.drawNumber(flowRate, 110,0);
  spr.drawString("L/m", 140,0); 
  if (alert_Condition_Lid == 1){spr.fillRect(230, 0, 30, 30, TFT_RED);}
  else {spr.drawRect(230, 0, 30, 30, TFT_BLACK);}
  
  spr.pushSprite(30, 190); 
  spr.deleteSprite();

  if ( drawAlertLed == 1){
    tft.fillCircle(40, 40, 20, TFT_RED);
  }
  else
  {
    tft.fillCircle(40, 40, 20, TFT_WHITE);
  }
  
  
}

// #########################################################################
//  Draw the analogue meter on the screen
// #########################################################################
void analogMeter() {
 
    // Meter outline
    tft.fillRect(0, 0, M_SIZE * 239, M_SIZE * 126, TFT_GREY);
    tft.fillRect(5, 3, M_SIZE * 230, M_SIZE * 119, TFT_WHITE);
 
    tft.setTextColor(TFT_BLACK);  // Text colour
 
    // Draw ticks every 5 degrees from -50 to +50 degrees (100 deg. FSD swing)
    for (int i = -50; i < 51; i += 5) {
        // Long scale tick length
        int tl = 15;
 
        // Coodinates of tick to draw
        float sx = cos((i - 90) * 0.0174532925);
        float sy = sin((i - 90) * 0.0174532925);
        uint16_t x0 = sx * (M_SIZE * 100 + tl) + M_SIZE * 120;
        uint16_t y0 = sy * (M_SIZE * 100 + tl) + M_SIZE * 140;
        uint16_t x1 = sx * M_SIZE * 100 + M_SIZE * 120;
        uint16_t y1 = sy * M_SIZE * 100 + M_SIZE * 140;
 
        // Coordinates of next tick for zone fill
        float sx2 = cos((i + 5 - 90) * 0.0174532925);
        float sy2 = sin((i + 5 - 90) * 0.0174532925);
        int x2 = sx2 * (M_SIZE * 100 + tl) + M_SIZE * 120;
        int y2 = sy2 * (M_SIZE * 100 + tl) + M_SIZE * 140;
        int x3 = sx2 * M_SIZE * 100 + M_SIZE * 120;
        int y3 = sy2 * M_SIZE * 100 + M_SIZE * 140;
 
        // Yellow zone limits
        if (i >= -50 && i < -25) {
          tft.fillTriangle(x0, y0, x1, y1, x2, y2, TFT_GREEN);
          tft.fillTriangle(x1, y1, x2, y2, x3, y3, TFT_GREEN);
        }
 
        if (i >= -25 && i < 0) {
          tft.fillTriangle(x0, y0, x1, y1, x2, y2, TFT_GREEN);
          tft.fillTriangle(x1, y1, x2, y2, x3, y3, TFT_GREEN);
        }
 
        // Green zone limits
        if (i >= 0 && i < 25) {
            tft.fillTriangle(x0, y0, x1, y1, x2, y2, TFT_YELLOW);
            tft.fillTriangle(x1, y1, x2, y2, x3, y3, TFT_YELLOW);
        }
 
        // Orange zone limits
        if (i >= 25 && i < 50) {
            tft.fillTriangle(x0, y0, x1, y1, x2, y2, TFT_RED);
            tft.fillTriangle(x1, y1, x2, y2, x3, y3, TFT_RED);
        }
 
        // Short scale tick length
        if (i % 25 != 0) {
            tl = 8;
        }
 
        // Recalculate coords incase tick lenght changed
        x0 = sx * (M_SIZE * 100 + tl) + M_SIZE * 120;
        y0 = sy * (M_SIZE * 100 + tl) + M_SIZE * 140;
        x1 = sx * M_SIZE * 100 + M_SIZE * 120;
        y1 = sy * M_SIZE * 100 + M_SIZE * 140;
 
        // Draw tick
        tft.drawLine(x0, y0, x1, y1, TFT_BLACK);
 
        // Check if labels should be drawn, with position tweaks
        if (i % 25 == 0) {
            // Calculate label positions
            x0 = sx * (M_SIZE * 100 + tl + 10) + M_SIZE * 120;
            y0 = sy * (M_SIZE * 100 + tl + 10) + M_SIZE * 140;
 
            ////////////////////////////////////////////////////////////
            //  In here, you can change the dial plate 
            ////////////////////////////////////////////////////////////
            switch (i / 25) {
                case -2: tft.drawCentreString("20", x0, y0 - 12, 2); break;
                case -1: tft.drawCentreString("30", x0, y0 - 9, 2); break;
                case 0: tft.drawCentreString("40", x0, y0 - 7, 2); break;
                case 1: tft.drawCentreString("50", x0, y0 - 9, 2); break;
                case 2: tft.drawCentreString("60", x0, y0 - 12, 2); break;
            }
        }
 
        // Now draw the arc of the scale
        sx = cos((i + 5 - 90) * 0.0174532925);
        sy = sin((i + 5 - 90) * 0.0174532925);
        x0 = sx * M_SIZE * 100 + M_SIZE * 120;
        y0 = sy * M_SIZE * 100 + M_SIZE * 140;
        // Draw scale arc, don't draw the last part
        if (i < 50) {
            tft.drawLine(x0, y0, x1, y1, TFT_BLACK);
        }
    }
 
    //tft.drawString("%RH", M_SIZE * (5 + 230 - 40), M_SIZE * (119 - 20), 2); // Units at bottom right
    tft.drawCentreString("0 C", M_SIZE * 120, M_SIZE * 80, 4); // Comment out to avoid font 4
    tft.drawRect(5, 3, M_SIZE * 230, M_SIZE * 119, TFT_BLACK); // Draw bezel line
 
    plotNeedle(0, 0); // Put meter needle at 0
}
 
// #########################################################################
// Update needle position
// This function is blocking while needle moves, time depends on ms_delay
// 10ms minimises needle flicker if text is drawn within needle sweep area
// Smaller values OK if text not in sweep area, zero for instant movement but
// does not look realistic... (note: 100 increments for full scale deflection)
// #########################################################################
void plotNeedle(int value, byte ms_delay) {
 
    String strUnit =  " C";
    String label =  value + strUnit;
    
    if (value < 20) {
        value = 20;    // Limit value to emulate needle end stops
    }
    if (value > 60) {
        value = 60;
    }
 
    // Move the needle until new value reached
    while (!(value == old_analog)) {
        if (old_analog < value) {
            old_analog++;
        } else {
            old_analog--;
        }
 
        if (ms_delay == 0) {
            old_analog = value;    // Update immediately if delay is 0
        }
 
        //float sdeg = map(old_analog, -10, 110, -150, -30); // Map value to angle
        float sdeg = map(old_analog, 20, 60, -140, -40); // Map value to angle
        // Calcualte tip of needle coords
        float sx = cos(sdeg * 0.0174532925);
        float sy = sin(sdeg * 0.0174532925);
 
        // Calculate x delta of needle start (does not start at pivot point)
        float tx = tan((sdeg + 90) * 0.0174532925);
 
        // Erase old needle image
        tft.drawLine(M_SIZE * (120 + 20 * ltx - 1), M_SIZE * (140 - 20), osx - 1, osy, TFT_WHITE);
        tft.drawLine(M_SIZE * (120 + 20 * ltx), M_SIZE * (140 - 20), osx, osy, TFT_WHITE);
        tft.drawLine(M_SIZE * (120 + 20 * ltx + 1), M_SIZE * (140 - 20), osx + 1, osy, TFT_WHITE);
 
        // Re-plot text under needle

        tft.fillRect(130, 100, 80, 40, TFT_WHITE); // Draw bezel line
        
        tft.setTextColor(TFT_BLACK);
        tft.drawCentreString(label, M_SIZE * 120, M_SIZE * 80, 4); // // Comment out to avoid font 4
 
        // Store new needle end coords for next erase
        ltx = tx;
        osx = M_SIZE * (sx * 98 + 120);
        osy = M_SIZE * (sy * 98 + 140);
 
        // Draw the needle in the new postion, magenta makes needle a bit bolder
        // draws 3 lines to thicken needle
        tft.drawLine(M_SIZE * (120 + 20 * ltx - 1), M_SIZE * (140 - 20), osx - 1, osy, TFT_RED);
        tft.drawLine(M_SIZE * (120 + 20 * ltx), M_SIZE * (140 - 20), osx, osy, TFT_MAGENTA);
        tft.drawLine(M_SIZE * (120 + 20 * ltx + 1), M_SIZE * (140 - 20), osx + 1, osy, TFT_RED);
 
        // Slow needle down slightly as it approaches new postion
        if (abs(old_analog - value) < 10) {
            ms_delay += ms_delay / 5;
        }
 
        // Wait before next update
        delay(ms_delay);
    }
}

void Flow()
{
   count++; 
}


void Lid_Open()
{
   if (digitalRead(LID_PIN) == 1){
      digitalWrite(RELAY_PIN, LOW);
      alert_Condition_Lid = 1;   
   }
   else {
      alert_Condition_Lid = 0;
   }
}

void Reset_Relay()
{
  if ( alert_Condition_Flow == 0 & alert_Condition_Temp == 0 & alert_Condition_Lid == 0){
    digitalWrite(RELAY_PIN, HIGH);
  }
};
