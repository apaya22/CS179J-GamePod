#ifndef TETRIS_H
#define TETRIS_H

#include <Arduino.h>

// =====================
// TETRIS GRID
// =====================
static const int TETRIS_COLS = 10;
static const int TETRIS_ROWS = 20;

// =====================
// PIECE DEFINITIONS
// =====================
// Each piece has 4 rotations, each rotation is 4 (col,row) offsets
// Pieces: I, O, T, S, Z, L, J

struct TetrisPiece {
  int8_t blocks[4][4][2];  // [rotation][block_index][col/row]
  uint16_t color;
};

struct TetrisGame {
  uint16_t board[TETRIS_ROWS][TETRIS_COLS]; // 0 = empty, else color
  int currentPiece;     // index into pieces array
  int rotation;         // 0-3
  int pieceCol;         // column of piece origin
  int pieceRow;         // row of piece origin
  int nextPiece;        // preview
  int score;
  int level;
  int linesCleared;
  bool gameOver;
};

extern TetrisGame tetrisGame;

void initTetrisGame();
void runTetrisGame();

#endif // TETRIS_H