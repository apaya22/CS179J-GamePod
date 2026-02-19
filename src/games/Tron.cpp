#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>

#include "controller.h"

static const int Width_Sreen = 320;
static const int Height_screen = 240;
static const uint8_t Screen_ROT = 1; //1 is for horzontal depending on 0/1/2/3 u will need to switch the w and h

// =====================
// DISPLAY PIN CONFIGURATION (your new wiring)
// =====================
#define TFT_CS    4
#define TFT_DC    15
#define TFT_RST   9
#define SCLK_PIN  12
#define MOSI_PIN  11
#define MISO_PIN  13

Adafruit_ILI9341 tft(TFT_CS, TFT_DC, TFT_RST);

struct Player {
  int x, y;
  JoyDir dir;
  uint16_t color;
  bool alive;
};

Player player1;

//resets screen and fills it with black, will need more later like reseting/redrawing the characters
void resetScreenBeforeStart() {
  tft.fillScreen(ILI9341_BLACK);
}

//will hold value for a 9x9 character sprite(UP)
const uint16_t characterSpriteUp[9] = {
  0b000010000,
  0b000111000,
  0b001111100,
  0b001111100,
  0b000111000,
  0b000111000,
  0b111111111,
  0b001111100,
  0b000111000
};

//will hold value for a 9x9 character sprite(Down)
const uint16_t characterSpriteDown[9] = {
  0b000111000,
  0b001111100,
  0b111111111,
  0b000111000,
  0b000111000,
  0b001111100,
  0b001111100,
  0b000111000,
  0b000010000
};

//will hold value for a 9x9 character sprite(Left)
const uint16_t characterSpriteLeft[9] = {
  0b000000100,
  0b000000100,
  0b001100110,
  0b011111111,
  0b111111111,
  0b011111111,
  0b001100110,
  0b000000100,
  0b000000100
};

//will hold value for a 9x9 character sprite(Right)
const uint16_t characterSpriteRight[9] = {
  0b001000000,
  0b001000000,
  0b011001100,
  0b111111110,
  0b111111111,
  0b111111110,
  0b011001100,
  0b001000000,
  0b001000000
};

  //for future new 9x5 bike spirte with trail guards(going right), trail needs to come off middle of the bike too and somehow need to figure our overlap logic 
  // of bike wheel on turns
  //would also be nice to figure out how to color the bike tail just white for all bikes along with wheels maybe
  //need to figure out bike overlap on turns with trail
  //need to track trail locally for deletions
  //need to make death animation

  // 0b000000000,
  // 0b000000000,
  // 0b000110100,
  // 0b111111110,
  // 0b000111111,
  // 0b111111110,
  // 0b000110100,
  // 0b000000000,
  // 0b000000000

//will take an x and y location and direction from the server and then based off that draw a character 9x9 on the screen
//this will also need to intake a color for the draw pixels
void drawCharacter(int character_x_cord, int character_y_cord, uint8_t character_direction){
    
  //here we need to draw from the top left X and Y cords meaning the center point if the fifth
  //bit of our 9x9 sprite 0bxxxx1xxxx
  int character_top_left_x = character_x_cord - 4;
  int character_top_left_y = character_y_cord - 4;

  //pinter for character sprite direction, may make it a global
  const uint16_t* sprite = 0;

  //Slect character sprite based on current direction 0-3 up,down,left,right
  if (character_direction == 0){

    //up
    sprite = characterSpriteUp;

  } else if (character_direction == 1){

    //down
    sprite = characterSpriteDown;

  } else if (character_direction == 2){

    //left
    sprite = characterSpriteLeft;

  } else {

    //right
    sprite = characterSpriteRight;

  }

  //logic for drawing character(made it into a for loop, can do this statically but I dont think its worth)
  //essentailly this goes through all 9 rows and collumns of the sprite and colors the pixles on the screen
  for (int row = 0; row < 9; row++) {
      uint16_t row_data = sprite[row];

      for (int col = 0; col < 9; col++) {

          //checks to see if the bit in the row x column is 1 of not
          bool draw_pixel = (row_data >> (8 - col)) & 1;

          if (draw_pixel) {
              tft.drawPixel(character_top_left_x + col, character_top_left_y + row, ILI9341_CYAN);
          }
      }
  }

}

//this should clear a 9x9 grid of the character
void clearCharacter(int character_x_cord, int character_y_cord){

  int character_top_left_x = character_x_cord - 4;
  int character_top_left_y = character_y_cord - 4;

    for (int row = 0; row < 9; row++) {

      for (int col = 0; col < 9; col++) {

        tft.drawPixel(character_top_left_x + col, character_top_left_y + row, ILI9341_BLACK);
      }
  }
}

//will just leave behind a 1x1 trail based on the players directions (will also need a character color)
void drawTrail(int character_x_cord, int character_y_cord, uint8_t character_direction){

  int trailX, trailY = 0;

  //if the character is facing up
  if(character_direction == 0){
    trailX = character_x_cord;
    trailY = character_y_cord - 5;
    tft.drawPixel(trailX,trailY, ILI9341_CYAN);
  }else if(character_direction ==1){
    trailX = character_x_cord;
    trailY = character_y_cord + 5;
    tft.drawPixel(trailX,trailY, ILI9341_CYAN);
  }else if(character_direction ==2){
    trailX = character_x_cord + 5;
    trailY = character_y_cord;
    tft.drawPixel(trailX,trailY, ILI9341_CYAN);
  }else{
    trailX = character_x_cord - 5;
    trailY = character_y_cord;
    tft.drawPixel(trailX,trailY, ILI9341_CYAN);
  }

}

void setup() {
  Serial.begin(115200);
  randomSeed(esp_random());

  //starts screen
  SPI.begin(SCLK_PIN, MISO_PIN, MOSI_PIN, TFT_CS);
  tft.begin();
  tft.setRotation(Screen_ROT);

  //starts controller
  initController();

  //restarts the screen
  resetScreenBeforeStart();
}


  int dummyx = 100;
  int dummyy= 100;
  int dummyd= 3;
void loop() {

  //testing for onscreen and how it looks
  // int dummyx = 125;
  // int dummyy= 125;
  // int dummyd= 0;
  // drawCharacter(dummyx,dummyy,dummyd);

  // dummyx = 50;
  // dummyy= 50;
  // dummyd= 1;
  // drawCharacter(dummyx,dummyy,dummyd);

  // dummyx = 200;
  // dummyy= 200;
  // dummyd= 2;
  // drawCharacter(dummyx,dummyy,dummyd);

  // dummyx = 300;
  // dummyy= 200;
  // dummyd= 3;
  // drawCharacter(dummyx,dummyy,dummyd);

  // // clearCharacter(dummyx,dummyy);

  // dummyx = 177;
  // dummyy= 177;
  // dummyd= 3;
  // drawTrail(dummyx,dummyy,dummyd);




  delay(50);
  drawCharacter(dummyx,dummyy,dummyd);
  drawTrail(dummyx,dummyy,dummyd);
  clearCharacter(dummyx,dummyy);
  dummyx++;
  drawCharacter(dummyx,dummyy,dummyd);

  // dummyx = 177;
  // dummyy= 177;
  // dummyd= 3;
  // drawTrail(dummyx,dummyy,dummyd);

}