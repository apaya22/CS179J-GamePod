#include "dungeon_rpg.h"
#include "../home_os/home_config.h"
#include "../include/controller/controller.h"
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <math.h>

extern Adafruit_ILI9341 tft;

#define max(a,b) (((a) > (b)) ? (a) : (b))

DungeonRPG rpgGame;

// =====================
// DISPLAY LAYOUT
// =====================
// Top: 3D viewport.  Bottom: HUD bar.
static const int VIEW_W    = SCREEN_W;       // 320
static const int VIEW_H    = 180;            // 3D viewport height
static const int HUD_Y     = VIEW_H;         // 180
static const int HUD_H     = SCREEN_H - VIEW_H; // 60
static const int RAY_COUNT = 160;            // one ray per 2-pixel column
static const float FOV     = 1.047f;         // 60 degrees
static const float MAX_DEPTH = 16.0f;

// =====================
// COLORS
// =====================
static const uint16_t COL_CEILING  = 0x1082;  // dark grey
static const uint16_t COL_FLOOR_C  = 0x3186;  // medium grey
static const uint16_t COL_HUD_BG   = 0x0000;
static const uint16_t COL_HP_BAR   = 0xF800;  // red
static const uint16_t COL_XP_BAR   = 0x07E0;  // green
static const uint16_t COL_WALL1    = 0x630C;  // stone grey
static const uint16_t COL_WALL2    = 0x4208;  // darker stone
static const uint16_t COL_WALL3    = 0x8410;  // lighter stone (door-like)
static const uint16_t COL_ENEMY_SK = 0xFFFF;  // skeleton - white
static const uint16_t COL_ENEMY_OR = 0x07E0;  // orc - green
static const uint16_t COL_ENEMY_DM = 0xF800;  // demon - red
static const uint16_t COL_ITEM     = 0xFFE0;  // yellow

// =====================
// MATH HELPERS
// =====================
static inline float clampf(float v, float lo, float hi) {
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}

// =====================
// RANDOM DUNGEON GENERATOR
// =====================
// Uses a simple room-and-corridor approach.

static void clearMap() {
  for (int y = 0; y < MAP_H; y++)
    for (int x = 0; x < MAP_W; x++)
      rpgGame.map[y][x] = 1;  // all walls
}

static void carveRoom(int rx, int ry, int rw, int rh) {
  for (int y = ry; y < ry + rh && y < MAP_H - 1; y++)
    for (int x = rx; x < rx + rw && x < MAP_W - 1; x++)
      rpgGame.map[y][x] = 0;
}

static void carveCorridor(int x1, int y1, int x2, int y2) {
  int cx = x1, cy = y1;
  while (cx != x2) {
    if (cx > 0 && cx < MAP_W - 1 && cy > 0 && cy < MAP_H - 1)
      rpgGame.map[cy][cx] = 0;
    cx += (x2 > cx) ? 1 : -1;
  }
  while (cy != y2) {
    if (cx > 0 && cx < MAP_W - 1 && cy > 0 && cy < MAP_H - 1)
      rpgGame.map[cy][cx] = 0;
    cy += (y2 > cy) ? 1 : -1;
  }
}

struct Room { int x, y, w, h; };

static void generateDungeon() {
  clearMap();

  Room rooms[6];
  int numRooms = 0;
  int attempts = 0;

  while (numRooms < 6 && attempts < 50) {
    attempts++;
    int rw = random(3, 6);
    int rh = random(3, 6);
    int rx = random(1, MAP_W - rw - 1);
    int ry = random(1, MAP_H - rh - 1);

    // Check overlap with existing rooms (with 1-cell padding)
    bool overlap = false;
    for (int i = 0; i < numRooms; i++) {
      if (rx - 1 < rooms[i].x + rooms[i].w && rx + rw + 1 > rooms[i].x &&
          ry - 1 < rooms[i].y + rooms[i].h && ry + rh + 1 > rooms[i].y) {
        overlap = true;
        break;
      }
    }
    if (overlap) continue;

    carveRoom(rx, ry, rw, rh);
    rooms[numRooms] = {rx, ry, rw, rh};

    // Connect to previous room
    if (numRooms > 0) {
      int cx1 = rooms[numRooms - 1].x + rooms[numRooms - 1].w / 2;
      int cy1 = rooms[numRooms - 1].y + rooms[numRooms - 1].h / 2;
      int cx2 = rx + rw / 2;
      int cy2 = ry + rh / 2;
      carveCorridor(cx1, cy1, cx2, cy2);
    }
    numRooms++;
  }

  // Ensure at least 2 rooms
  if (numRooms < 2) {
    carveRoom(1, 1, 4, 4);
    carveRoom(MAP_W - 6, MAP_H - 6, 4, 4);
    carveCorridor(3, 3, MAP_W - 4, MAP_H - 4);
    rooms[0] = {1, 1, 4, 4};
    rooms[1] = {MAP_W - 6, MAP_H - 6, 4, 4};
    numRooms = 2;
  }

  // Place player in first room
  rpgGame.playerX = rooms[0].x + rooms[0].w / 2.0f + 0.5f;
  rpgGame.playerY = rooms[0].y + rooms[0].h / 2.0f + 0.5f;
  rpgGame.playerA = 0.0f;

  // Place stairs in last room
  for (int i = 0; i < MAX_ITEMS; i++) rpgGame.items[i].type = ITEM_NONE;
  rpgGame.items[0].x = rooms[numRooms - 1].x + rooms[numRooms - 1].w / 2.0f + 0.5f;
  rpgGame.items[0].y = rooms[numRooms - 1].y + rooms[numRooms - 1].h / 2.0f + 0.5f;
  rpgGame.items[0].type = ITEM_STAIRS;
  rpgGame.items[0].collected = false;

  // Scatter items in random open cells
  int itemIdx = 1;
  for (int tries = 0; tries < 40 && itemIdx < MAX_ITEMS; tries++) {
    int ix = random(1, MAP_W - 1);
    int iy = random(1, MAP_H - 1);
    if (rpgGame.map[iy][ix] == 0) {
      float dist = sqrtf((ix + 0.5f - rpgGame.playerX) * (ix + 0.5f - rpgGame.playerX) +
                         (iy + 0.5f - rpgGame.playerY) * (iy + 0.5f - rpgGame.playerY));
      if (dist < 2.0f) continue;
      rpgGame.items[itemIdx].x = ix + 0.5f;
      rpgGame.items[itemIdx].y = iy + 0.5f;
      rpgGame.items[itemIdx].collected = false;
      int r = random(0, 3);
      rpgGame.items[itemIdx].type = (r == 0) ? ITEM_HEALTH_POTION :
                                    (r == 1) ? ITEM_SWORD_UP : ITEM_SHIELD_UP;
      itemIdx++;
    }
  }

  // Place enemies in rooms (not the first room)
  for (int i = 0; i < MAX_ENEMIES; i++) rpgGame.enemies[i].alive = false;
  int enemyIdx = 0;
  for (int r = 1; r < numRooms && enemyIdx < MAX_ENEMIES; r++) {
    int numInRoom = random(1, 3);  // 1-2 enemies per room
    for (int e = 0; e < numInRoom && enemyIdx < MAX_ENEMIES; e++) {
      float ex = rooms[r].x + random(0, rooms[r].w) + 0.5f;
      float ey = rooms[r].y + random(0, rooms[r].h) + 0.5f;
      rpgGame.enemies[enemyIdx].x = ex;
      rpgGame.enemies[enemyIdx].y = ey;
      rpgGame.enemies[enemyIdx].alive = true;

      // Scale enemies to floor
      int f = rpgGame.floor;
      int roll = random(0, 10);
      if (roll < 5 || f < 3) {
        rpgGame.enemies[enemyIdx].type = ENEMY_SKELETON;
        rpgGame.enemies[enemyIdx].hp = 8 + f * 2;
        rpgGame.enemies[enemyIdx].maxHp = rpgGame.enemies[enemyIdx].hp;
        rpgGame.enemies[enemyIdx].atk = 3 + f;
      } else if (roll < 8 || f < 5) {
        rpgGame.enemies[enemyIdx].type = ENEMY_ORC;
        rpgGame.enemies[enemyIdx].hp = 15 + f * 3;
        rpgGame.enemies[enemyIdx].maxHp = rpgGame.enemies[enemyIdx].hp;
        rpgGame.enemies[enemyIdx].atk = 5 + f;
      } else {
        rpgGame.enemies[enemyIdx].type = ENEMY_DEMON;
        rpgGame.enemies[enemyIdx].hp = 25 + f * 4;
        rpgGame.enemies[enemyIdx].maxHp = rpgGame.enemies[enemyIdx].hp;
        rpgGame.enemies[enemyIdx].atk = 8 + f * 2;
      }
      enemyIdx++;
    }
  }
}

// =====================
// HUD DRAWING
// =====================
static void drawHUD() {
  tft.fillRect(0, HUD_Y, SCREEN_W, HUD_H, COL_HUD_BG);

  // HP bar
  tft.setTextColor(GAMEPOD_WHITE);
  tft.setTextSize(1);
  tft.setCursor(4, HUD_Y + 4);
  tft.print("HP");
  int hpBarW = 80;
  int hpFill = (rpgGame.stats.hp * hpBarW) / max(1, rpgGame.stats.maxHp);
  tft.drawRect(20, HUD_Y + 2, hpBarW + 2, 10, GAMEPOD_WHITE);
  tft.fillRect(21, HUD_Y + 3, hpFill, 8, COL_HP_BAR);
  tft.setCursor(20 + hpBarW + 6, HUD_Y + 4);
  char hpBuf[16];
  snprintf(hpBuf, sizeof(hpBuf), "%d/%d", rpgGame.stats.hp, rpgGame.stats.maxHp);
  tft.print(hpBuf);

  // XP bar
  tft.setCursor(4, HUD_Y + 16);
  tft.print("XP");
  int xpBarW = 80;
  int xpFill = (rpgGame.stats.xp * xpBarW) / max(1, rpgGame.stats.xpToNext);
  tft.drawRect(20, HUD_Y + 14, xpBarW + 2, 10, GAMEPOD_WHITE);
  tft.fillRect(21, HUD_Y + 15, xpFill, 8, COL_XP_BAR);

  // Stats line
  tft.setCursor(4, HUD_Y + 28);
  char statBuf[48];
  snprintf(statBuf, sizeof(statBuf), "Lv:%d ATK:%d DEF:%d Pot:%d",
           rpgGame.stats.level, rpgGame.stats.atk, rpgGame.stats.def, rpgGame.stats.potions);
  tft.print(statBuf);

  // Floor
  tft.setCursor(4, HUD_Y + 40);
  char floorBuf[16];
  snprintf(floorBuf, sizeof(floorBuf), "Floor: %d", rpgGame.floor);
  tft.print(floorBuf);

  // Minimap (top-right of HUD)
  int mmX = SCREEN_W - MAP_W * 3 - 4;
  int mmY = HUD_Y + 4;
  for (int my = 0; my < MAP_H; my++) {
    for (int mx = 0; mx < MAP_W; mx++) {
      uint16_t c = rpgGame.map[my][mx] ? 0x4208 : 0x1082;
      tft.fillRect(mmX + mx * 3, mmY + my * 3, 3, 3, c);
    }
  }
  // Player dot
  int ppx = mmX + (int)(rpgGame.playerX) * 3;
  int ppy = mmY + (int)(rpgGame.playerY) * 3;
  tft.fillRect(ppx, ppy, 3, 3, COL_ITEM);
  // Stairs dot
  for (int i = 0; i < MAX_ITEMS; i++) {
    if (rpgGame.items[i].type == ITEM_STAIRS && !rpgGame.items[i].collected) {
      int sx = mmX + (int)(rpgGame.items[i].x) * 3;
      int sy = mmY + (int)(rpgGame.items[i].y) * 3;
      tft.fillRect(sx, sy, 3, 3, GAMEPOD_BLUE);
    }
  }
}

// =====================
// RAYCASTER RENDERING
// =====================
static float depthBuffer[RAY_COUNT];

static void renderView() {
  float pa = rpgGame.playerA;
  float px = rpgGame.playerX;
  float py = rpgGame.playerY;

  for (int i = 0; i < RAY_COUNT; i++) {
    float rayAngle = (pa - FOV / 2.0f) + ((float)i / (float)RAY_COUNT) * FOV;
    float rayDirX = cosf(rayAngle);
    float rayDirY = sinf(rayAngle);

    // DDA raycasting
    int mapX = (int)px;
    int mapY = (int)py;

    float deltaDistX = (rayDirX == 0) ? 1e10f : fabsf(1.0f / rayDirX);
    float deltaDistY = (rayDirY == 0) ? 1e10f : fabsf(1.0f / rayDirY);

    float sideDistX, sideDistY;
    int stepX, stepY;
    bool side;

    if (rayDirX < 0) { stepX = -1; sideDistX = (px - mapX) * deltaDistX; }
    else             { stepX =  1; sideDistX = (mapX + 1.0f - px) * deltaDistX; }
    if (rayDirY < 0) { stepY = -1; sideDistY = (py - mapY) * deltaDistY; }
    else             { stepY =  1; sideDistY = (mapY + 1.0f - py) * deltaDistY; }

    bool hit = false;
    float dist = MAX_DEPTH;

    for (int step = 0; step < 64; step++) {
      if (sideDistX < sideDistY) {
        sideDistX += deltaDistX;
        mapX += stepX;
        side = false;
      } else {
        sideDistY += deltaDistY;
        mapY += stepY;
        side = true;
      }
      if (mapX < 0 || mapX >= MAP_W || mapY < 0 || mapY >= MAP_H) break;
      if (rpgGame.map[mapY][mapX] > 0) {
        hit = true;
        if (!side)
          dist = sideDistX - deltaDistX;
        else
          dist = sideDistY - deltaDistY;
        break;
      }
    }

    // Fix fisheye
    dist *= cosf(rayAngle - pa);
    if (dist < 0.1f) dist = 0.1f;
    depthBuffer[i] = dist;

    // Calculate wall column height
    int lineHeight = (int)(VIEW_H / dist);
    if (lineHeight > VIEW_H) lineHeight = VIEW_H;

    int drawStart = (VIEW_H - lineHeight) / 2;
    int drawEnd = drawStart + lineHeight;

    // Color based on distance and side
    uint16_t wallCol;
    if (dist > 8.0f)      wallCol = 0x2104;
    else if (side)         wallCol = COL_WALL2;
    else                   wallCol = COL_WALL1;

    // Darken with distance
    if (dist > 4.0f) {
      uint8_t r = (wallCol >> 11) & 0x1F;
      uint8_t g = (wallCol >> 5) & 0x3F;
      uint8_t b = wallCol & 0x1F;
      float fade = clampf(1.0f - (dist - 4.0f) / 12.0f, 0.2f, 1.0f);
      r = (uint8_t)(r * fade);
      g = (uint8_t)(g * fade);
      b = (uint8_t)(b * fade);
      wallCol = ((uint16_t)r << 11) | ((uint16_t)g << 5) | b;
    }

    int sx = i * 2;
    // Draw ceiling
    if (drawStart > 0)
      tft.fillRect(sx, 0, 2, drawStart, COL_CEILING);
    // Draw wall
    if (hit)
      tft.fillRect(sx, drawStart, 2, lineHeight, wallCol);
    else
      tft.fillRect(sx, drawStart, 2, lineHeight, COL_CEILING);
    // Draw floor
    if (drawEnd < VIEW_H)
      tft.fillRect(sx, drawEnd, 2, VIEW_H - drawEnd, COL_FLOOR_C);
  }
}

// =====================
// SPRITE RENDERING (billboarded enemies & items)
// =====================
static void renderSprites() {
  float pa = rpgGame.playerA;
  float px = rpgGame.playerX;
  float py = rpgGame.playerY;
  float dirX = cosf(pa);
  float dirY = sinf(pa);
  // Camera plane (perpendicular to direction)
  float planeX = -sinf(pa) * tanf(FOV / 2.0f);
  float planeY =  cosf(pa) * tanf(FOV / 2.0f);

  // Collect visible sprites
  struct Sprite { float x, y, dist; uint16_t color; int size; bool isEnemy; int idx; };
  Sprite sprites[MAX_ENEMIES + MAX_ITEMS];
  int numSprites = 0;

  for (int i = 0; i < MAX_ENEMIES; i++) {
    if (!rpgGame.enemies[i].alive) continue;
    float dx = rpgGame.enemies[i].x - px;
    float dy = rpgGame.enemies[i].y - py;
    float d = sqrtf(dx * dx + dy * dy);
    rpgGame.enemies[i].dist = d;
    uint16_t col;
    switch (rpgGame.enemies[i].type) {
      case ENEMY_SKELETON: col = COL_ENEMY_SK; break;
      case ENEMY_ORC:      col = COL_ENEMY_OR; break;
      case ENEMY_DEMON:    col = COL_ENEMY_DM; break;
      default:             col = GAMEPOD_WHITE; break;
    }
    sprites[numSprites++] = {rpgGame.enemies[i].x, rpgGame.enemies[i].y, d, col, 0, true, i};
  }

  for (int i = 0; i < MAX_ITEMS; i++) {
    if (rpgGame.items[i].type == ITEM_NONE || rpgGame.items[i].collected) continue;
    float dx = rpgGame.items[i].x - px;
    float dy = rpgGame.items[i].y - py;
    float d = sqrtf(dx * dx + dy * dy);
    uint16_t col = (rpgGame.items[i].type == ITEM_STAIRS) ? GAMEPOD_BLUE : COL_ITEM;
    sprites[numSprites++] = {rpgGame.items[i].x, rpgGame.items[i].y, d, col, 0, false, i};
  }

  // Sort by distance (farthest first)
  for (int i = 0; i < numSprites - 1; i++)
    for (int j = i + 1; j < numSprites; j++)
      if (sprites[j].dist > sprites[i].dist) {
        Sprite tmp = sprites[i]; sprites[i] = sprites[j]; sprites[j] = tmp;
      }

  // Project and draw
  for (int s = 0; s < numSprites; s++) {
    float sx = sprites[s].x - px;
    float sy = sprites[s].y - py;

    // Transform to camera space
    float invDet = 1.0f / (planeX * dirY - dirX * planeY);
    float txf = invDet * (dirY * sx - dirX * sy);
    float tyf = invDet * (-planeY * sx + planeX * sy);

    if (tyf <= 0.1f) continue;  // behind camera

    int spriteScreenX = (int)((VIEW_W / 2) * (1.0f + txf / tyf));
    int spriteH = (int)(VIEW_H / tyf);
    if (spriteH > VIEW_H * 2) spriteH = VIEW_H * 2;
    int spriteW = spriteH / 2;

    int drawStartY = (VIEW_H - spriteH) / 2;
    int drawStartX = spriteScreenX - spriteW / 2;

    // Clamp and draw only columns not occluded by walls
    for (int stripe = max(0, drawStartX); stripe < min((int)VIEW_W, drawStartX + spriteW); stripe++) {
      int rayIdx = stripe / 2;
      if (rayIdx < 0 || rayIdx >= RAY_COUNT) continue;
      if (tyf >= depthBuffer[rayIdx]) continue;  // behind wall

      int colH = min(spriteH, VIEW_H);
      int colY = max(0, drawStartY);
      tft.fillRect(stripe, colY, 1, colH, sprites[s].color);
    }
  }
}

// =====================
// COMBAT SYSTEM
// =====================
static void drawCombatScreen() {
  int idx = rpgGame.combatEnemyIdx;
  Enemy &e = rpgGame.enemies[idx];

  tft.fillRect(0, 0, SCREEN_W, VIEW_H, 0x0000);

  // Enemy portrait (big colored block)
  uint16_t col;
  const char* name;
  switch (e.type) {
    case ENEMY_SKELETON: col = COL_ENEMY_SK; name = "SKELETON"; break;
    case ENEMY_ORC:      col = COL_ENEMY_OR; name = "ORC";      break;
    case ENEMY_DEMON:    col = COL_ENEMY_DM; name = "DEMON";    break;
    default:             col = GAMEPOD_WHITE; name = "???";      break;
  }

  // Draw a simple enemy figure
  int cx = SCREEN_W / 2;
  int cy = VIEW_H / 2 - 10;
  tft.fillCircle(cx, cy - 20, 18, col);                        // head
  tft.fillRect(cx - 14, cy, 28, 40, col);                      // body
  if (e.type == ENEMY_DEMON) {
    tft.fillTriangle(cx - 18, cy - 30, cx - 10, cy - 40, cx - 10, cy - 20, col); // horn
    tft.fillTriangle(cx + 18, cy - 30, cx + 10, cy - 40, cx + 10, cy - 20, col); // horn
  }
  // Eyes
  tft.fillRect(cx - 8, cy - 24, 4, 4, 0x0000);
  tft.fillRect(cx + 4, cy - 24, 4, 4, 0x0000);

  // Enemy name and HP
  tft.setTextColor(GAMEPOD_WHITE);
  tft.setTextSize(2);
  int16_t x1, y1; uint16_t w, h;
  tft.getTextBounds(name, 0, 0, &x1, &y1, &w, &h);
  tft.setCursor(cx - (int)w / 2, cy + 50);
  tft.print(name);

  tft.setTextSize(1);
  char hpStr[24];
  snprintf(hpStr, sizeof(hpStr), "HP: %d / %d", e.hp, e.maxHp);
  tft.getTextBounds(hpStr, 0, 0, &x1, &y1, &w, &h);
  tft.setCursor(cx - (int)w / 2, cy + 70);
  tft.print(hpStr);

  // Action prompt
  tft.setCursor(4, VIEW_H - 14);
  tft.setTextColor(GAMEPOD_WHITE);
  tft.print("A:Attack  B:Potion  D:Flee");
}

static void showCombatMessage(const char* msg, uint16_t color) {
  tft.fillRect(0, VIEW_H - 28, SCREEN_W, 14, 0x0000);
  tft.setTextColor(color);
  tft.setTextSize(1);
  tft.setCursor(4, VIEW_H - 26);
  tft.print(msg);
  delay(600);
}

static void runCombat() {
  rpgGame.mode = MODE_COMBAT;
  int idx = rpgGame.combatEnemyIdx;
  Enemy &e = rpgGame.enemies[idx];

  drawCombatScreen();
  drawHUD();

  bool lastA = buttonPressed(BTN_A);
  bool lastB = buttonPressed(BTN_B);
  bool lastD = buttonPressed(BTN_D);

  while (e.alive && rpgGame.stats.hp > 0) {
    bool btnA = buttonPressed(BTN_A);
    bool btnB = buttonPressed(BTN_B);
    bool btnD = buttonPressed(BTN_D);

    bool acted = false;

    // Attack
    if (btnA && !lastA) {
      int dmg = max(1, rpgGame.stats.atk - random(0, 3));
      e.hp -= dmg;
      char buf[32];
      snprintf(buf, sizeof(buf), "You deal %d damage!", dmg);
      showCombatMessage(buf, COL_ITEM);
      acted = true;
    }

    // Potion
    if (btnB && !lastB) {
      if (rpgGame.stats.potions > 0) {
        rpgGame.stats.potions--;
        int heal = rpgGame.stats.maxHp / 3;
        rpgGame.stats.hp = min(rpgGame.stats.maxHp, rpgGame.stats.hp + heal);
        char buf[32];
        snprintf(buf, sizeof(buf), "Healed %d HP!", heal);
        showCombatMessage(buf, COL_XP_BAR);
        acted = true;
      } else {
        showCombatMessage("No potions!", GAMEPOD_RED);
      }
    }

    // Flee
    if (btnD && !lastD) {
      if (random(0, 3) > 0) {
        showCombatMessage("You fled!", GAMEPOD_WHITE);
        rpgGame.mode = MODE_EXPLORE;
        return;
      } else {
        showCombatMessage("Can't escape!", GAMEPOD_RED);
        acted = true;
      }
    }

    lastA = btnA; lastB = btnB; lastD = btnD;

    // Enemy turn after player acts
    if (acted) {
      if (e.hp <= 0) {
        e.alive = false;
        // XP reward
        int xpGain = 5 + e.maxHp / 2;
        rpgGame.stats.xp += xpGain;
        char buf[32];
        snprintf(buf, sizeof(buf), "Victory! +%d XP", xpGain);
        showCombatMessage(buf, COL_ITEM);

        // Level up check
        while (rpgGame.stats.xp >= rpgGame.stats.xpToNext) {
          rpgGame.stats.xp -= rpgGame.stats.xpToNext;
          rpgGame.stats.level++;
          rpgGame.stats.maxHp += 5;
          rpgGame.stats.hp = rpgGame.stats.maxHp;
          rpgGame.stats.atk += 2;
          rpgGame.stats.def += 1;
          rpgGame.stats.xpToNext = rpgGame.stats.level * 20;
          showCombatMessage("** LEVEL UP! **", COL_ITEM);
        }
        delay(400);
        break;
      }

      // Enemy attacks
      int eDmg = max(1, e.atk - rpgGame.stats.def + random(-2, 3));
      rpgGame.stats.hp -= eDmg;
      char buf[32];
      snprintf(buf, sizeof(buf), "Enemy deals %d damage!", eDmg);
      showCombatMessage(buf, GAMEPOD_RED);

      drawCombatScreen();
      drawHUD();
    }

    delay(16);
  }

  if (rpgGame.stats.hp <= 0) {
    rpgGame.mode = MODE_DEAD;
    return;
  }

  rpgGame.mode = MODE_EXPLORE;
}

// =====================
// ITEM PICKUP
// =====================
static void checkItemPickup() {
  for (int i = 0; i < MAX_ITEMS; i++) {
    if (rpgGame.items[i].type == ITEM_NONE || rpgGame.items[i].collected) continue;
    float dx = rpgGame.items[i].x - rpgGame.playerX;
    float dy = rpgGame.items[i].y - rpgGame.playerY;
    if (dx * dx + dy * dy < 0.5f) {
      switch (rpgGame.items[i].type) {
        case ITEM_HEALTH_POTION:
          rpgGame.stats.potions++;
          rpgGame.items[i].collected = true;
          break;
        case ITEM_SWORD_UP:
          rpgGame.stats.atk += 2;
          rpgGame.items[i].collected = true;
          break;
        case ITEM_SHIELD_UP:
          rpgGame.stats.def += 1;
          rpgGame.items[i].collected = true;
          break;
        case ITEM_STAIRS:
          rpgGame.floor++;
          generateDungeon();
          rpgGame.needsRedraw = true;
          return;
        default: break;
      }
    }
  }
}

// =====================
// ENEMY PROXIMITY CHECK
// =====================
static void checkEnemyContact() {
  for (int i = 0; i < MAX_ENEMIES; i++) {
    if (!rpgGame.enemies[i].alive) continue;
    float dx = rpgGame.enemies[i].x - rpgGame.playerX;
    float dy = rpgGame.enemies[i].y - rpgGame.playerY;
    if (dx * dx + dy * dy < 0.6f) {
      rpgGame.combatEnemyIdx = i;
      runCombat();
      rpgGame.needsRedraw = true;
      return;
    }
  }
}

// =====================
// PUBLIC API
// =====================
void initDungeonRPG() {
  randomSeed(esp_random());

  rpgGame.floor = 1;
  rpgGame.mode = MODE_EXPLORE;
  rpgGame.needsRedraw = true;

  rpgGame.stats.hp = 30;
  rpgGame.stats.maxHp = 30;
  rpgGame.stats.atk = 5;
  rpgGame.stats.def = 2;
  rpgGame.stats.level = 1;
  rpgGame.stats.xp = 0;
  rpgGame.stats.xpToNext = 20;
  rpgGame.stats.potions = 1;

  generateDungeon();
}

// =====================
// MAIN GAME LOOP
// =====================
// Controls:
//   Joystick UP/DOWN  = move forward/back
//   Joystick LEFT/RIGHT = turn
//   Button A = attack (in combat)
//   Button B = use potion (in combat)
//   Button D = pause / flee (in combat)

void runDungeonRPG() {
  initDungeonRPG();

  tft.fillScreen(0x0000);
  renderView();
  renderSprites();
  drawHUD();

  float moveSpeed = 0.08f;
  float turnSpeed = 0.06f;
  bool lastBtnD = buttonPressed(BTN_D);
  bool running = true;

  while (running) {
    if (rpgGame.mode == MODE_DEAD) break;

    // --- INPUT ---
    JoyDir joy = joystickDirection();
    float px = rpgGame.playerX;
    float py = rpgGame.playerY;
    float pa = rpgGame.playerA;
    float dx = cosf(pa);
    float dy = sinf(pa);
    bool moved = false;

    if (joy == DOWN) {
      float nx = px + dx * moveSpeed;
      float ny = py + dy * moveSpeed;
      if (rpgGame.map[(int)py][(int)nx] == 0) rpgGame.playerX = nx;
      if (rpgGame.map[(int)ny][(int)px] == 0) rpgGame.playerY = ny;
      moved = true;
    }
    if (joy == UP) {
      float nx = px - dx * moveSpeed * 0.6f;
      float ny = py - dy * moveSpeed * 0.6f;
      if (rpgGame.map[(int)py][(int)nx] == 0) rpgGame.playerX = nx;
      if (rpgGame.map[(int)ny][(int)px] == 0) rpgGame.playerY = ny;
      moved = true;
    }
    if (joy == LEFT) {
      rpgGame.playerA -= turnSpeed;
      moved = true;
    }
    if (joy == RIGHT) {
      rpgGame.playerA += turnSpeed;
      moved = true;
    }

    // Check pickups and enemies
    if (moved || rpgGame.needsRedraw) {
      checkItemPickup();
      if (rpgGame.mode == MODE_DEAD) break;
      checkEnemyContact();
      if (rpgGame.mode == MODE_DEAD) break;
    }

    // Render
    renderView();
    renderSprites();
    drawHUD();
    rpgGame.needsRedraw = false;

    // --- PAUSE ---
    bool btnD = buttonPressed(BTN_D);
    if (btnD && !lastBtnD && rpgGame.mode == MODE_EXPLORE) {
      tft.setTextColor(GAMEPOD_WHITE);
      tft.setTextSize(2);
      int16_t x1, y1; uint16_t w, h;
      tft.getTextBounds("PAUSED", 0, 0, &x1, &y1, &w, &h);
      int cx = SCREEN_W / 2;
      int cy = VIEW_H / 2;
      tft.fillRect(cx - (int)w / 2 - 4, cy - (int)h / 2 - 4, w + 8, h + 8, GAMEPOD_DARK);
      tft.setCursor(cx - (int)w / 2, cy - (int)h / 2);
      tft.print("PAUSED");
      while (buttonPressed(BTN_D)) delay(10);
      delay(200);
      while (!buttonPressed(BTN_D)) delay(10);
      while (buttonPressed(BTN_D)) delay(10);
    }
    lastBtnD = btnD;

    delay(16);  // ~60fps target
  }

  // =====================
  // DEATH SCREEN
  // =====================
  int cx = SCREEN_W / 2;
  int cy = SCREEN_H / 2;
  tft.fillScreen(0x0000);
  tft.fillRect(cx - 90, cy - 36, 180, 72, GAMEPOD_DARK);
  tft.setTextColor(GAMEPOD_RED);
  tft.setTextSize(2);
  int16_t x1, y1; uint16_t w, h;
  tft.getTextBounds("YOU DIED", 0, 0, &x1, &y1, &w, &h);
  tft.setCursor(cx - (int)w / 2, cy - 24);
  tft.print("YOU DIED");
  tft.setTextSize(1);
  tft.setTextColor(GAMEPOD_WHITE);
  char buf[40];
  snprintf(buf, sizeof(buf), "Floor %d  Level %d  Score %d",
           rpgGame.floor, rpgGame.stats.level, rpgGame.floor * 100 + rpgGame.stats.level * 50);
  tft.getTextBounds(buf, 0, 0, &x1, &y1, &w, &h);
  tft.setCursor(cx - (int)w / 2, cy + 4);
  tft.print(buf);
  tft.setCursor(cx - 40, cy + 20);
  tft.print("PERMADEATH");
  delay(4000);
}