#ifndef DUNGEON_RPG_H
#define DUNGEON_RPG_H

#include <Arduino.h>

// =====================
// MAP DIMENSIONS
// =====================
static const int MAP_W = 16;
static const int MAP_H = 16;
static const int MAX_ENEMIES = 8;
static const int MAX_ITEMS = 6;
static const int MAX_FLOORS = 99;

// =====================
// ENTITY TYPES
// =====================
enum EnemyType : uint8_t {
  ENEMY_NONE = 0,
  ENEMY_SKELETON,
  ENEMY_ORC,
  ENEMY_DEMON
};

enum ItemType : uint8_t {
  ITEM_NONE = 0,
  ITEM_HEALTH_POTION,
  ITEM_SWORD_UP,
  ITEM_SHIELD_UP,
  ITEM_STAIRS     // exit to next floor
};

struct Enemy {
  float x, y;
  EnemyType type;
  int hp;
  int maxHp;
  int atk;
  bool alive;
  float dist;  // computed each frame for sorting
};

struct Item {
  float x, y;
  ItemType type;
  bool collected;
};

struct PlayerStats {
  int hp;
  int maxHp;
  int atk;
  int def;
  int level;
  int xp;
  int xpToNext;
  int potions;
};

enum GameMode : uint8_t {
  MODE_EXPLORE,
  MODE_COMBAT,
  MODE_DEAD,
  MODE_VICTORY
};

struct DungeonRPG {
  uint8_t map[MAP_H][MAP_W];
  float playerX, playerY;
  float playerA;         // angle in radians
  float playerDirX, playerDirY;
  PlayerStats stats;
  Enemy enemies[MAX_ENEMIES];
  Item items[MAX_ITEMS];
  int floor;
  GameMode mode;
  int combatEnemyIdx;    // which enemy we're fighting
  bool needsRedraw;
};

extern DungeonRPG rpgGame;

void initDungeonRPG();
void runDungeonRPG();

#endif // DUNGEON_RPG_H