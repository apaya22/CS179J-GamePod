#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>

#include "controller.h"

static const int Width_Sreen = 320;
static const int Height_screen = 240;
static const uint8_t Screen_ROT = 1; //1 is for horzontal depending on 0/1/2/3 u will need to switch the w and h

  //currently bike is a 9x5 or 5x9 depeding on direction and there is a trail guard that will hold the trail until a turn
  //the trail is working and bike is working
  //have yet to figure out turning and bike over lapping with a trail and how i will recolor the trail after the bike is not on it anymore
  //need to figure out how to color certain pixels on the bike difference colors
  //need to figure out how to keep track of which pixles are apart of the trail in order for deletion later, this will also be used for over lap checks
  //need to make death animation

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

//will hold value for a 9x5 character sprite(UP)
const uint16_t characterSpriteUp[9] = {
  0b00100,
  0b01110,
  0b11111,
  0b01110,
  0b11111,
  0b11011,
  0b01010,
  0b01010,
  0b01010
};

//will hold value for a 9x5 character sprite(Down)
const uint16_t characterSpriteDown[9] = {
  0b01010,
  0b01010,
  0b01010,
  0b11011,
  0b11111,
  0b01110,
  0b11111,
  0b01110,
  0b00100
};

//will hold value for a 9x5 character sprite(Left)
const uint16_t characterSpriteLeft[5] = {
  0b001011000,
  0b011111111,
  0b111110000,
  0b011111111,
  0b001011000
};

//will hold value for a 9x5 character sprite(Right)
const uint16_t characterSpriteRight[5] = {
  0b000110100,
  0b111111110,
  0b000011111,
  0b111111110,
  0b000110100
};

//will take an x and y location and direction from the server and then based off that draw a character 9x9 on the screen
//this will also need to intake a color for the draw pixels
void drawCharacter(int character_x_cord, int character_y_cord, uint8_t character_direction){
    
  //we need to draw form the top left x,y cordinate of the sprite
  int character_top_left_x = 0;
  int character_top_left_y = 0;

  //pinter for character sprite direction, may make it a global
  const uint16_t* sprite = 0;

  //Slect character sprite based on current direction 0-3 up,down,left,right
  if (character_direction == 0){

    //up
    sprite = characterSpriteUp;
    // 0bxxxxx
    // 0bxxxxx
    // 0bxxxxx
    // 0bxxxxx
    // 0bxx1xx
    // 0bxxxxx
    // 0bxxxxx
    // 0bxxxxx
    // 0bxxxxx
    character_top_left_x = character_x_cord - 2;
    character_top_left_y = character_y_cord - 4;

  } else if (character_direction == 1){

    //down
    sprite = characterSpriteDown;
    // 0bxxxxx
    // 0bxxxxx
    // 0bxxxxx
    // 0bxxxxx
    // 0bxx1xx
    // 0bxxxxx
    // 0bxxxxx
    // 0bxxxxx
    // 0bxxxxx
    character_top_left_x = character_x_cord - 2;
    character_top_left_y = character_y_cord - 4;

  } else if (character_direction == 2){

    //left
    sprite = characterSpriteLeft;
    // 0bxxxxxxxxx,
    // 0bxxxxxxxxx,
    // 0bxxxx1xxxx,
    // 0bxxxxxxxxx,
    // 0bxxxxxxxxx,
    character_top_left_x = character_x_cord - 4;
    character_top_left_y = character_y_cord - 2;

  } else {

    //right
    sprite = characterSpriteRight;
    // 0bxxxxxxxxx,
    // 0bxxxxxxxxx,
    // 0bxxxx1xxxx,
    // 0bxxxxxxxxx,
    // 0bxxxxxxxxx,
    character_top_left_x = character_x_cord - 4;
    character_top_left_y = character_y_cord - 2;

  }

  if (character_direction == 0 || character_direction == 1){
    //if the character if facing up or down it is a 9x5 so here we go through a 9x5 and draws each pixle
    //needs to find a way to draw certain pixles a certain color 
    for (int row = 0; row < 9; row++) {
        uint16_t row_data = sprite[row];

        for (int col = 0; col < 5; col++) {

            //checks to see if the bit in the row x column is 1 of not here since 5 bits we do highest bit index 4
            bool draw_pixel = (row_data >> (4 - col)) & 1;

            if (draw_pixel) {
                tft.drawPixel(character_top_left_x + col, character_top_left_y + row, ILI9341_CYAN);
            }
        }
    }
  }else{
    //if the character if facing up or down it is a 5x9 so here we go through a 5x9 and draws each pixle
    //needs to find a way to draw certain pixles a certain color 
      for (int row = 0; row < 5; row++) {
        uint16_t row_data = sprite[row];

        for (int col = 0; col < 9; col++) {

            //checks to see if the bit in the row x column is 1 of not here we do highest bit index 8
            bool draw_pixel = (row_data >> (8 - col)) & 1;

            if (draw_pixel) {
                tft.drawPixel(character_top_left_x + col, character_top_left_y + row, ILI9341_CYAN);
            }
        }
    }
  }
}


//will hold value for a 9x5 character sprite(UP) clearing and leaving the pixles for the
//trail guards and trail if 1 clear bit else leave it
const uint16_t clearCharacterSpriteUp[9] = {
  0b11111,
  0b11111,
  0b11111,
  0b11111,
  0b11011,
  0b11011,
  0b11011,
  0b11011,
  0b11011
};

//will hold value for a 9x5 character sprite(DOWN) clearing and leaving the pixles for the
//trail guards and trail if 1 clear bit else leave it
const uint16_t clearCharacterSpriteDown[9] = {
  0b11011,
  0b11011,
  0b11011,
  0b11011,
  0b11011,
  0b11111,
  0b11111,
  0b11111,
  0b11111
};

//will hold value for a 9x5 character sprite(Left) clearing and leaving the pixles for the
//trail guards and trail if 1 clear bit else leave it
const uint16_t clearCharacterSpriteLeft[5] = {
  0b111111111,
  0b011111111,
  0b111100000,
  0b011111111,
  0b111111111
};

//will hold value for a 9x5 character sprite(Right) clearing and leaving the pixles for the
//trail guards and trail if 1 clear bit else leave it
const uint16_t clearCharacterSpriteRight[5] = {
  0b111111111,
  0b111111111,
  0b000001111,
  0b111111111,
  0b111111111
};

//this should clear a 9x5 or 5x9 grid of the character
void clearCharacter(int character_x_cord, int character_y_cord, uint8_t character_direction){

  //we need to draw form the top left x,y cordinate of the sprite
  int character_top_left_x = 0;
  int character_top_left_y = 0;

  //pinter for character sprite direction, may make it a global
  const uint16_t* clearSprite = 0;

  //Slect character sprite based on current direction 0-3 up,down,left,right
  if (character_direction == 0){

    //up
    clearSprite = clearCharacterSpriteUp;
    character_top_left_x = character_x_cord - 2;
    character_top_left_y = character_y_cord - 4;

  } else if (character_direction == 1){

    //down
    clearSprite = clearCharacterSpriteDown;
    character_top_left_x = character_x_cord - 2;
    character_top_left_y = character_y_cord - 4;

  } else if (character_direction == 2){

    //left
    clearSprite = clearCharacterSpriteLeft;
    character_top_left_x = character_x_cord - 4;
    character_top_left_y = character_y_cord - 2;

  } else {

    //right
    clearSprite = clearCharacterSpriteRight;
    character_top_left_x = character_x_cord - 4;
    character_top_left_y = character_y_cord - 2;

  }

  if (character_direction == 0 || character_direction == 1){
    //if the character if facing up or down it is a 9x5 so here we go through a 9x5 and draws each pixle
    //needs to find a way to draw certain pixles a certain color 
    for (int row = 0; row < 9; row++) {
        uint16_t row_data = clearSprite[row];

        for (int col = 0; col < 5; col++) {

            //checks to see if the bit in the row x column is 1 of not here since 5 bits we do highest bit index 4
            bool draw_pixel = (row_data >> (4 - col)) & 1;

            if (draw_pixel) {
                tft.drawPixel(character_top_left_x + col, character_top_left_y + row, ILI9341_BLACK);
            }
        }
    }
  }else{
    //if the character if facing up or down it is a 5x9 so here we go through a 5x9 and draws each pixle
    //needs to find a way to draw certain pixles a certain color 
      for (int row = 0; row < 5; row++) {
        uint16_t row_data = clearSprite[row];

        for (int col = 0; col < 9; col++) {

            //checks to see if the bit in the row x column is 1 of not here we do highest bit index 8
            bool draw_pixel = (row_data >> (8 - col)) & 1;

            if (draw_pixel) {
                tft.drawPixel(character_top_left_x + col, character_top_left_y + row, ILI9341_BLACK);
            }
        }
    }
  }
}

//will just leave behind a 1x1 trail based on the players directions (will also need a character color)
// void drawTrail(int character_x_cord, int character_y_cord, uint8_t character_direction){

//   int trailX, trailY = 0;
//   tft.drawPixel(character_x_cord,character_y_cord, ILI9341_WHITE);

// }

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


  int dummyx = 30;
  int dummyy= 200;
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
  //drawCharacter(dummyx,dummyy,dummyd);
  clearCharacter(dummyx,dummyy,dummyd);
  dummyx++;
  //dummyy--;
  drawCharacter(dummyx,dummyy,dummyd);
  //drawTrail(dummyx,dummyy,dummyd);

  // dummyx = 177;
  // dummyy= 177;
  // dummyd= 3;
  // drawTrail(dummyx,dummyy,dummyd);

}