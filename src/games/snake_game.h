#ifndef SNAKE_GAME_H
#define SNAKE_GAME_H

#include <Arduino.h>

struct SnakeGame {
  struct Segment { int x, y; } snake[100];  // Max 100 segments
  int snakeLength = 3;
  int dirX = 1, dirY = 0;  // Current direction
  int nextDirX = 1, nextDirY = 0;  // Buffered direction for next move
  int appleX, appleY;
  int score = 0;
  int gridSize = 8;  // 8x8 pixel grid
  int updateCounter = 0;
  int updateRate = 5;  // Update every 5 frames
};

// Global instance
extern SnakeGame snakeGame;
extern int lastScore;
extern bool gameOverScreenRendered;
extern bool snakeGameNeedsFullRedraw;

// Function declarations
void initSnakeGame();
void updateSnakeGame();
bool checkSnakeCollision();
void runSnakeGame();

#endif // SNAKE_GAME_H