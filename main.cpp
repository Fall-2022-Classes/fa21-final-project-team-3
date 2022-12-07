/**
* @file main.cpp
*
* @brief A game that utilizes the on-board accelerometer to move a sprite to different locations
         You must avoid colliding walls to not lose points. Each second used is a loss in points.
         To earn extra points, you can target the red boxes. 
         The time and points are displayed on the right of the screen
*
**/

//#define _DEBUG
#include <math.h>
#include "chu_init.h"
#include "gpio_cores.h"
#include "vga_core.h"
#include "sseg_core.h"
#include "spi_core.h"
#include  "ps2_core.h"
#include "collision_map.h"

#define DIRECTION 1
#define REFRESHRATE 20
#define DISPLAYWIDTH 640
#define DISPLAYHEIGHT 480
#define SPRITEXINITIAL 131
#define SPRITEYINITIAL 14
#define MOVESENSITIVITY 10
#define WALLCLIPTHRESHOLD 5
#define POINTDEDUCTION 5
#define BONUSPOINTS 6
#define BONUSPOINTSIZE 16
#define BONUSPOINTVALUE 100
#define ENDX 90
#define ENDY 470

float initX, initY;
int moveX, moveY;
bool xActivation, yActivation;
int spriteX = SPRITEXINITIAL;
int spriteY = SPRITEYINITIAL;
int points = 1000;
bool gameResume = true;
bool gameDone = false;
bool gameReset = false;

uint16_t bonusPointCoords[6][2] = {
   {420, 267},
   {527, 124},
   {161, 268},
   {382, 49},
   {419, 341},
   {308, 414}
};

// external core instantiation
FrameCore frame(FRAME_BASE);
SpriteCore ghost(get_sprite_addr(BRIDGE_BASE, V3_GHOST), 1024);
OsdCore osd(get_sprite_addr(BRIDGE_BASE, V2_OSD));
SsegCore sseg(get_slot_addr(BRIDGE_BASE, S8_SSEG));
SpiCore spi(get_slot_addr(BRIDGE_BASE, S9_SPI));
TimerCore time(get_slot_addr(BRIDGE_BASE, S0_SYS_TIMER));
Ps2Core ps2(get_slot_addr(BRIDGE_BASE, S11_PS2));

unsigned char convert_coordinate_to_bytemap(unsigned int, unsigned int);

void get_accelerometer_data(SpiCore *spi_p)
{
   const uint8_t RD_CMD = 0x0b;
   const uint8_t DATA_REG = 0x08;
   const float raw_max = 127.0 / 2.0; // 128 max 8-bit reading for +/-2g
   float newX, newY;
   int8_t xraw, yraw, zraw;

   spi_p->set_freq(400000);
   spi_p->set_mode(0, 0);
   spi_p->assert_ss(0);       // activate
   spi_p->transfer(RD_CMD);   // for read operation
   spi_p->transfer(DATA_REG); //

   yraw = spi_p->transfer(0x00);
   xraw = spi_p->transfer(0x00);
   zraw = spi_p->transfer(0x00);
   spi_p->deassert_ss(0);

   newX = (float)xraw / raw_max;
   newY = (float)yraw / raw_max;

   newX *= DIRECTION;

   xActivation = fabs(newX - initX) * MOVESENSITIVITY >= 1;
   yActivation = fabs(newY - initY) * MOVESENSITIVITY >= 1;

   moveX = (int)(newX * 15);
   moveY = (int)(newY * 15);

   sleep_ms(REFRESHRATE);
}

void sprite_move_collision(SpriteCore *ghost_p)
{
   if(moveX >= WALLCLIPTHRESHOLD){
      moveX = WALLCLIPTHRESHOLD;
   }
   else if(moveX <= -WALLCLIPTHRESHOLD){
      moveX = -WALLCLIPTHRESHOLD;
   }
   
   if(moveY >= WALLCLIPTHRESHOLD){
      moveY = WALLCLIPTHRESHOLD;
   }
   else if(moveY <= -WALLCLIPTHRESHOLD){
      moveY = -WALLCLIPTHRESHOLD;
   }

   if (xActivation)
      spriteX += moveX;

   if (yActivation)
      spriteY += moveY;


   // Top Left Collision
   while(convert_coordinate_to_bytemap(spriteX, spriteY) == 0){
      // Top Collision
      while(convert_coordinate_to_bytemap(spriteX + 8, spriteY) == 0){
         spriteY += 1;
      }
      
      // Left Collision
      while(convert_coordinate_to_bytemap(spriteX, spriteY + 8) == 0){
         spriteX += 1;
      }

      // Corner
      while(convert_coordinate_to_bytemap(spriteX, spriteY) == 0){
         spriteX += 1;
         spriteY += 1;
      }
      points -= POINTDEDUCTION;
   }

   // Top Right Collision
   while(convert_coordinate_to_bytemap(spriteX+15, spriteY) == 0){
      // Top Collision
      while(convert_coordinate_to_bytemap(spriteX + 8, spriteY) == 0){
         spriteY += 1;
      }
      
      // Right Collision
      while(convert_coordinate_to_bytemap(spriteX + 15, spriteY + 8) == 0){
         spriteX -= 1;
      }

      // Corner
      while(convert_coordinate_to_bytemap(spriteX+15, spriteY) == 0){
         spriteX -= 1;
         spriteY += 1;
      }
      points -= POINTDEDUCTION;
   }

   // Bottom Left Collision
   while(convert_coordinate_to_bytemap(spriteX, spriteY+15) == 0){
      // Bottom Collision
      while(convert_coordinate_to_bytemap(spriteX + 8, spriteY + 15) == 0){
         spriteY -= 1;
      }
      
      // Left Collision
      while(convert_coordinate_to_bytemap(spriteX, spriteY + 8) == 0){
         spriteX += 1;
      }

      // Corner
      while(convert_coordinate_to_bytemap(spriteX, spriteY+15) == 0){
         spriteX += 1;
         spriteY -= 1;
      }
      points -= POINTDEDUCTION;
   }

   // Bottom Right Collision
   while(convert_coordinate_to_bytemap(spriteX + 15, spriteY+15) == 0){
      // Bottom Collision
      while(convert_coordinate_to_bytemap(spriteX + 8, spriteY + 15) == 0){
         spriteY -= 1;
      }
      
      // Right Collision
      while(convert_coordinate_to_bytemap(spriteX + 15, spriteY + 8) == 0){
         spriteX -= 1;
      }

      // Corner
      while(convert_coordinate_to_bytemap(spriteX + 15, spriteY+15) == 0){
         spriteX -= 1;
         spriteY -= 1;
      }
      points -= POINTDEDUCTION;
   }

   ghost_p->move_xy(spriteX, spriteY);
}

void accelerometer_init(SpiCore *spi_p)
{
   const uint8_t RD_CMD = 0x0b;
   const uint8_t DATA_REG = 0x08;
   const float raw_max = 127.0 / 2.0; // 128 max 8-bit reading for +/-2g
   int8_t xraw, yraw, zraw;

   spi_p->set_freq(400000);
   spi_p->set_mode(0, 0);
   spi_p->assert_ss(0);       // activate
   spi_p->transfer(RD_CMD);   // for read operation
   spi_p->transfer(DATA_REG); //

   xraw = spi_p->transfer(0x00);
   yraw = spi_p->transfer(0x00);
   zraw = spi_p->transfer(0x00);
   spi_p->deassert_ss(0);

   initX = (float)xraw / raw_max;
   initY = (float)yraw / raw_max;
}

void draw_maze(FrameCore *frame, OsdCore *osd_p)
{
   int yCount = 0;
   int xCount = 0;
   int color;

   for (int i = 0; i < sizeof(mazeMap) / sizeof(unsigned char); i++){
      for (int j = 7; j >= 0; j--){
         if (xCount % 640 == 0 && xCount != 0){
            xCount = 0;
            yCount++;
         }

         if ((mazeMap[i] >> j) & 1 == 1){
            color = 0xFFF;
         }
         else{
            color = 0x0;
         }

         frame->wr_pix(xCount, yCount, color);
         xCount++;
      }
   }

   osd_p->set_color(0x01, 0x0);
   
   // Show "Start"
   osd_p->wr_char(11, 1, 64 + 19, 1);
   osd_p->wr_char(12, 1, 64 + 20, 1);
   osd_p->wr_char(13, 1, 64 + 1, 1);
   osd_p->wr_char(14, 1, 64 + 18, 1);
   osd_p->wr_char(15, 1, 64 + 20, 1);

   // Show "End"
   osd_p->wr_char(7, 29, 64 + 5);
   osd_p->wr_char(8, 29, 64 + 14);
   osd_p->wr_char(9, 29, 64 + 4);

}

unsigned char convert_coordinate_to_bytemap(unsigned int x, unsigned int y){
   unsigned int pixelLength, byteLength, shiftAmount;
   
   pixelLength = (y * DISPLAYWIDTH + x);
   byteLength = int(pixelLength) / int(8);
   shiftAmount = pixelLength % 8; 
   shiftAmount = 7 - shiftAmount;

   return ((mazeMap[byteLength] >> shiftAmount) & 1);
}


void osd_display(OsdCore *osd_p, TimerCore *time_p){
   uint64_t timeSeconds;
   static uint64_t tempTime = 0;
   unsigned int minutes, seconds, secondsOnes, secondsTens, pointDeduction;

   if(gameReset)
      tempTime = 0;
   
   timeSeconds = time_p->read_time() / 1000000;
   pointDeduction = timeSeconds - tempTime;
   tempTime = timeSeconds;
   
   minutes = timeSeconds / 60;
   seconds = timeSeconds % 60;
   if(seconds < 10){
      secondsTens = 0;
      secondsOnes = seconds % 10;
   }
   else{
      secondsTens = seconds / 10;
      secondsOnes = seconds % 10;
   }

   osd_p->set_color(0xFFF, 0x001);
   // Time
   // Shows "TIME:"
   osd_p->wr_char(198, 0, 64 + 20);
   osd_p->wr_char(199, 0, 64 + 9);
   osd_p->wr_char(200, 0, 64 + 13);
   osd_p->wr_char(201, 0, 64 + 5);
   osd_p->wr_char(202, 0, 58);

   // Shows time in 0:00 format
   osd_p->wr_char(198, 1, 48 + minutes);
   osd_p->wr_char(199, 1, 58);
   osd_p->wr_char(200, 1, 48 + secondsTens);
   osd_p->wr_char(201, 1, 48 + secondsOnes); 

   // Points
   points -= pointDeduction;
   if(points <= 0){
      points = 0;
   }

   // Shows "POINTS"
   osd_p->wr_char(198, 3, 64 + 16);
   osd_p->wr_char(199, 3, 64 + 15);
   osd_p->wr_char(200, 3, 64 + 9);
   osd_p->wr_char(201, 3, 64 + 14);
   osd_p->wr_char(202, 3, 64 + 20);
   osd_p->wr_char(203, 3, 64 + 19);
   osd_p->wr_char(204, 3, 58);

   // Shows # of points
   int tempPoints = points;
   osd_p->wr_char(198, 4, 48 + (tempPoints / 1000));
   tempPoints %= 1000;
   osd_p->wr_char(199, 4, 48 + (tempPoints / 100));
   tempPoints %= 100;
   osd_p->wr_char(200, 4, 48 + (tempPoints / 10));
   tempPoints %= 10;
   osd_p->wr_char(201, 4, 48 + (tempPoints / 1));
}


void display_bonus_points(FrameCore *frame_p){
   for (int i = 0; i < BONUSPOINTS; i++)
   {
      uint16_t bonusX = bonusPointCoords[i][0];
      uint16_t bonusY = bonusPointCoords[i][1];
      for (int j = bonusX; j < bonusX +BONUSPOINTSIZE; j++)
      {
         for (int k = bonusY; k < bonusY + BONUSPOINTSIZE; k++)
         {
            frame_p->wr_pix(j, k, 0x700);
         }
      }
   }
}

void bonus_point_check(FrameCore *frame_p){
   for (int i = 0; i < BONUSPOINTS; i++)
   {
      uint16_t bonusX = bonusPointCoords[i][0];
      uint16_t bonusY = bonusPointCoords[i][1];
      
      // 8 added to sprite X and Y for middle of sprite
      // Check if sprite coords are within the box
      bool xBounds = bonusX <= spriteX + 8 && spriteX + 8 <= bonusX + BONUSPOINTSIZE;
      bool yBounds = bonusY <= spriteY + 8 && spriteY + 8 <= bonusY + BONUSPOINTSIZE;

      if(xBounds && yBounds){
         points += BONUSPOINTVALUE; 

         // Clear red from frame
         for (int j = bonusX; j < bonusX +BONUSPOINTSIZE; j++)
         {
            for (int k = bonusY; k < bonusY + BONUSPOINTSIZE; k++)
            {
               frame_p->wr_pix(j, k, 0xFFF);    // Overwrite with white
            }
         }

         // Remove ability to get more points from a cleared location
         bonusPointCoords[i][0] = 0;
         bonusPointCoords[i][1] = 0;
      } 
   }
}

void end_check(OsdCore *osd_p){
   // 8 added to sprite X and Y for middle of sprite
   bool xBounds = ENDX <= spriteX + 8 && spriteX + 8 <= ENDX + BONUSPOINTSIZE;
   bool yBounds = ENDY <= spriteY + 8 && spriteY + 8 <= ENDY + BONUSPOINTSIZE;

   if(xBounds && yBounds){
      gameDone = true;        // Indicate the game is over
      
      // Right Side
      //Shows "Game"
      osd_p->wr_char(200, 13, 64 + 7);
      osd_p->wr_char(201, 13, 64 + 1);
      osd_p->wr_char(202, 13, 64 + 13);
      osd_p->wr_char(203, 13, 64 + 5);
      
      //Shows "OVER"
      osd_p->wr_char(200, 14, 64 + 15);
      osd_p->wr_char(201, 14, 64 + 22);
      osd_p->wr_char(202, 14, 64 + 5);
      osd_p->wr_char(203, 14, 64 + 18);

      // Left Side
      //Shows "GAME"
      osd_p->wr_char(4, 13, 64 + 7);
      osd_p->wr_char(5, 13, 64 + 1);
      osd_p->wr_char(6, 13, 64 + 13);
      osd_p->wr_char(7, 13, 64 + 5);
      
      //Shows "OVER"
      osd_p->wr_char(4, 14, 64 + 15);
      osd_p->wr_char(5, 14, 64 + 22);
      osd_p->wr_char(6, 14, 64 + 5);
      osd_p->wr_char(7, 14, 64 + 18);
   } 
}


void get_ps2_data(Ps2Core *ps2_p) {
   int lbtn, rbtn, xmov, ymov;

   static bool leftButtonPress = false;
   static bool rightButtonPress = false;
   static bool leftToggle = false; 
   static bool rightToggle = false; 

   if (ps2_p->deviceID == 2) {  // mouse
      while (ps2_p->get_mouse_activity(&lbtn, &rbtn, &xmov, &ymov)) {
         
         // Left Button
         if(lbtn == 1 && leftButtonPress == false){
            leftButtonPress = true;
            leftToggle = true;
         }
         if(lbtn == 0 && leftButtonPress == true){
            leftButtonPress = false;
         }
         if(leftToggle){
            gameResume = !gameResume;
            leftToggle = false;
         }

         // Right Button
         if(rbtn == 1 && rightButtonPress == false){
            rightButtonPress = true;
            rightToggle = true;
         }
         if(rbtn == 0 && rightButtonPress == true){
            rightButtonPress = false;
         }
         if(rightToggle){
            gameReset = true;
            rightToggle = false;
         }
      }     
   }
}

void game_reset(){
   spriteX = SPRITEXINITIAL;
   spriteY = SPRITEYINITIAL;
   points = 1000;
   gameResume = true;
   gameDone = false;

   osd.clr_screen();
   draw_maze(&frame, &osd);
   display_bonus_points(&frame);
   osd_display(&osd, &time);
   gameReset = false;
   time.clear();
}

int main()
{
   bool toggleCtrl = true;
   ps2.init();
   osd.bypass(0);
   frame.bypass(0);
   ghost.bypass(0);
   ghost.wr_ctrl(0x1c); // animation; blue ghost

   accelerometer_init(&spi);     // Get the initial measurements (what is flat)
   game_reset();

   while (1)
   {
      get_ps2_data(&ps2);

      // Run only when game is not paused and not done
      if(gameResume && gameDone == false){
         
         // Resume the timer core if game is unpaused
         if(gameResume == true && toggleCtrl == false){
            time.go();
            toggleCtrl = true;
         }         

         get_accelerometer_data(&spi);          // Get the movement data
         sprite_move_collision(&ghost);         // Move the sprite, account for collisions
         osd_display(&osd, &time);              // Update the OSD
         
         bonus_point_check(&frame);             // Check if any points were earned
         end_check(&osd);                       // Check if the end of the game is reached
      }
      
      // If game paused, pause the timer core
      if(gameResume == false && toggleCtrl){
         time.pause();
         toggleCtrl = false;
      }

      if(gameReset){
         game_reset();
      }
   }
}
