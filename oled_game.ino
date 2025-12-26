#include <U8g2lib.h>
#include <Wire.h>

// ---------------- DISPLAY ----------------
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0);

// ---------------- BUTTONS ----------------
#define BTN_LEFT  32
#define BTN_RIGHT 33
#define BTN_JUMP  25

bool leftPressed, rightPressed, jumpPressed;

// ---------------- PLAYER ----------------
float playerX = 16;
float playerY = 16;
float playerVX = 0;
float playerVY = 0;

const float accel       = 0.4;   // horizontal acceleration
const float maxSpeed    = 2.2;   // max horizontal speed
const float friction    = 5;  // ground friction
const float airFriction = 0.1;  // air friction
const float gravity     = 1.1;
const float maxFall     = 4.5;
const float jumpStrength = -7.8;

bool onGround = false;

const int PLAYER_W = 8;
const int PLAYER_H = 8;

// ---------------- LEVEL / CHUNKS ----------------
const uint8_t CHUNK_WIDTH  = 16;
const uint8_t CHUNK_HEIGHT = 8;
const uint8_t TILE_SIZE    = 8;

const uint8_t NUM_CHUNKS = 5;

// 0 = empty, 1 = solid, 2 = spike
uint8_t level[NUM_CHUNKS][CHUNK_WIDTH][CHUNK_HEIGHT] = {0};

int currentChunk = 0;

// spawn positions per chunk
int spawnX[NUM_CHUNKS] = {16, 8, 8, 8, 8};
int spawnY[NUM_CHUNKS] = {16, 16, 16, 16, 16};

// ---------------- MOVING PLATFORMS ----------------
struct MovingPlatform {
  int chunk;      // which chunk it belongs to
  float x, y;     // top-left
  int w, h;
  float minX, maxX;
  float minY, maxY;
  float vx, vy;
  float prevX, prevY;
};

const int MAX_PLATFORMS = 8;
MovingPlatform plats[MAX_PLATFORMS];
int platCount = 0;

// ---------------- ENEMIES ----------------
struct Enemy {
  int chunk;
  float x, y;
  float minX, maxX;
  float vx;
};

const int MAX_ENEMIES = 8;
Enemy enemies[MAX_ENEMIES];
int enemyCount = 0;

// ==================================================
// HELPERS
// ==================================================
void resetPlayer() {
  playerX = spawnX[currentChunk];
  playerY = spawnY[currentChunk];
  playerVX = 0;
  playerVY = 0;
  onGround = false;
}

uint8_t getTile(int px, int py) {
  if (px < 0 || px >= 128 || py < 0 || py >= 64) return 0;
  int tx = px / TILE_SIZE;
  int ty = py / TILE_SIZE;
  if (tx < 0 || tx >= CHUNK_WIDTH || ty < 0 || ty >= CHUNK_HEIGHT) return 0;
  return level[currentChunk][tx][ty];
}

bool solidAt(int px, int py) {
  return getTile(px, py) == 1;
}

bool spikeAt(int px, int py) {
  return getTile(px, py) == 2;
}

void killPlayer() {
  delay(200);            // small pause
  currentChunk = 0;
  resetPlayer();
}

// AABB overlap
bool aabbOverlap(float x1, float y1, float w1, float h1,
                 float x2, float y2, float w2, float h2) {
  return !(x1 + w1 <= x2 || x2 + w2 <= x1 ||
           y1 + h1 <= y2 || y2 + h2 <= x1);
}

// ==================================================
// BUILD LEVELS
// ==================================================
void addPlatform(int chunk, float x, float y, int w, int h,
                 float minX, float maxX, float minY, float maxY,
                 float vx, float vy) {
  if (platCount >= MAX_PLATFORMS) return;
  plats[platCount].chunk = chunk;
  plats[platCount].x = x;
  plats[platCount].y = y;
  plats[platCount].w = w;
  plats[platCount].h = h;
  plats[platCount].minX = minX;
  plats[platCount].maxX = maxX;
  plats[platCount].minY = minY;
  plats[platCount].maxY = maxY;
  plats[platCount].vx = vx;
  plats[platCount].vy = vy;
  plats[platCount].prevX = x;
  plats[platCount].prevY = y;
  platCount++;
}

void addEnemy(int chunk, float x, float y, float minX, float maxX, float vx) {
  if (enemyCount >= MAX_ENEMIES) return;
  enemies[enemyCount].chunk = chunk;
  enemies[enemyCount].x = x;
  enemies[enemyCount].y = y;
  enemies[enemyCount].minX = minX;
  enemies[enemyCount].maxX = maxX;
  enemies[enemyCount].vx = vx;
  enemyCount++;
}

void buildLevels() {
  // Clear everything
  for (int c = 0; c < NUM_CHUNKS; c++)
    for (int x = 0; x < CHUNK_WIDTH; x++)
      for (int y = 0; y < CHUNK_HEIGHT; y++)
        level[c][x][y] = 0;

  platCount = 0;
  enemyCount = 0;

  // ---------------- CHUNK 0: basic intro ----------------
  // Small steps up to the right
  level[0][2][6] = 1; level[0][3][6] = 1;
  level[0][5][5] = 1; level[0][6][5] = 1;
  level[0][8][4] = 1; level[0][9][4] = 1;
  level[0][11][3] = 1; level[0][12][3] = 1;

  // Spikes at the bottom
  level[0][6][7] = 2;
  level[0][7][7] = 2;
  level[0][8][7] = 2;

  // One horizontal moving platform (rides)
  addPlatform(0, 20, 40, 16, 4, 20, 80, 40, 40, 0.6, 0.0);

  // ---------------- CHUNK 1: more vertical ----------------
  // Left side steps
  level[1][1][6] = 1; level[1][2][6] = 1;
  level[1][4][5] = 1; level[1][5][5] = 1;
  level[1][7][4] = 1; level[1][8][4] = 1;

  // Vertical wall
  level[1][10][3] = 1;
  level[1][10][4] = 1;
  level[1][10][5] = 1;

  // Small ledge past the wall
  level[1][12][4] = 1; level[1][13][4] = 1;

  // Spikes pit
  level[1][6][7] = 2; level[1][7][7] = 2; level[1][8][7] = 2;

  // Vertical moving platform (up/down)
  addPlatform(1, 64, 40, 16, 4, 64, 64, 24, 48, 0.0, -0.5);

  // Enemy patrolling on the ledge
  addEnemy(1, 90, 24, 80, 120, 0.5);

  // ---------------- CHUNK 2: tighter jumps ----------------
  // Narrow platforms
  level[2][2][5] = 1; level[2][3][5] = 1;
  level[2][6][4] = 1; level[2][7][4] = 1;
  level[2][10][3] = 1; level[2][11][3] = 1;
  level[2][13][2] = 1; level[2][14][2] = 1;

  // Double vertical walls with gap
  level[2][5][3] = 1; level[2][5][4] = 1;
  level[2][9][3] = 1; level[2][9][4] = 1;

  // Spikes below
  level[2][7][7] = 2; level[2][8][7] = 2; level[2][9][7] = 2;

  // Horizontal platform near top
  addPlatform(2, 40, 24, 16, 4, 24, 80, 24, 24, 0.5, 0.0);

  // Enemy on mid platform
  addEnemy(2, 56, 32, 40, 80, 0.4);

  // ---------------- CHUNK 3: vertical climb but fair ----------------
  // Staircase up left
  level[3][1][6] = 1;
  level[3][2][5] = 1;
  level[3][3][4] = 1;
  level[3][4][3] = 1;

  // Central wall
  level[3][7][2] = 1; level[3][7][3] = 1; level[3][7][4] = 1;

  // Ledge to the right
  level[3][10][3] = 1; level[3][11][3] = 1;

  // Top exit platform
  level[3][13][2] = 1; level[3][14][2] = 1;

  // Spikes bottom row
  for (int x = 3; x < 13; x++)
    level[3][x][7] = 2;

  // Vertical moving platform in center shaft
  addPlatform(3, 56, 40, 16, 4, 56, 56, 24, 48, 0.0, -0.6);

  // Enemy near the top
  addEnemy(3, 96, 16, 88, 120, 0.5);

  // ---------------- CHUNK 4: finale ----------------
  // Floating islands
  level[4][2][5] = 1; level[4][3][5] = 1;
  level[4][6][4] = 1; level[4][7][4] = 1;
  level[4][10][3] = 1; level[4][11][3] = 1;
  level[4][13][2] = 1; level[4][14][2] = 1;

  // Spikes in between
  level[4][4][7] = 2; level[4][5][7] = 2;
  level[4][8][7] = 2; level[4][9][7] = 2;
  level[4][12][7] = 2;

  // Combo moving platform: diagonal-ish (up then down)
  addPlatform(4, 32, 40, 16, 4, 24, 80, 24, 48, 0.4, -0.4);

  // Double enemy patrol
  addEnemy(4, 40, 32, 32, 80, 0.5);
  addEnemy(4, 96, 24, 88, 120, 0.4);
}

// ==================================================
// INPUT
// ==================================================
void readInput() {
  leftPressed  = (digitalRead(BTN_LEFT)  == LOW);
  rightPressed = (digitalRead(BTN_RIGHT) == LOW);
  jumpPressed  = (digitalRead(BTN_JUMP)  == LOW);
}

// ==================================================
// MOVING PLATFORMS UPDATE
// ==================================================
void updatePlatforms() {
  for (int i = 0; i < platCount; i++) {
    if (plats[i].chunk != currentChunk) continue;

    plats[i].prevX = plats[i].x;
    plats[i].prevY = plats[i].y;

    plats[i].x += plats[i].vx;
    plats[i].y += plats[i].vy;

    if (plats[i].x < plats[i].minX || plats[i].x + plats[i].w > plats[i].maxX) {
      plats[i].vx = -plats[i].vx;
      plats[i].x += plats[i].vx;
    }
    if (plats[i].y < plats[i].minY || plats[i].y + plats[i].h > plats[i].maxY) {
      plats[i].vy = -plats[i].vy;
      plats[i].y += plats[i].vy;
    }
  }
}

// ==================================================
// ENEMIES UPDATE
// ==================================================
void updateEnemies() {
  for (int i = 0; i < enemyCount; i++) {
    if (enemies[i].chunk != currentChunk) continue;

    enemies[i].x += enemies[i].vx;
    if (enemies[i].x < enemies[i].minX || enemies[i].x > enemies[i].maxX) {
      enemies[i].vx = -enemies[i].vx;
      enemies[i].x += enemies[i].vx;
    }

    // Player collision
    if (aabbOverlap(playerX, playerY, PLAYER_W, PLAYER_H,
                    enemies[i].x, enemies[i].y, 8, 8)) {
      killPlayer();
    }
  }
}

// ==================================================
// PLAYER PHYSICS
// ==================================================
void updatePlayer() {
  // Horizontal input accel
  if (leftPressed) {
    playerVX -= accel;
  } else if (rightPressed) {
    playerVX += accel;
  } else {
    // friction
    float f = onGround ? friction : airFriction;
    if (playerVX > 0) {
      playerVX -= f;
      if (playerVX < 0) playerVX = 0;
    } else if (playerVX < 0) {
      playerVX += f;
      if (playerVX > 0) playerVX = 0;
    }
  }

  // Clamp speed
  if (playerVX > maxSpeed)  playerVX = maxSpeed;
  if (playerVX < -maxSpeed) playerVX = -maxSpeed;

  // Jump
  if (jumpPressed && onGround) {
    playerVY = jumpStrength;
    onGround = false;
  }

  // Gravity
  playerVY += gravity;
  if (playerVY > maxFall) playerVY = maxFall;

  // Horizontal movement with tile collision
  float newX = playerX + playerVX;
  if (!solidAt((int)newX, (int)playerY) &&
      !solidAt((int)newX + PLAYER_W - 1, (int)playerY) &&
      !solidAt((int)newX, (int)playerY + PLAYER_H - 1) &&
      !solidAt((int)newX + PLAYER_W - 1, (int)playerY + PLAYER_H - 1)) {
    playerX = newX;
  } else {
    playerVX = 0;
  }

  // Vertical movement with tile collision
  float newY = playerY + playerVY;
  if (playerVY > 0) {
    // falling
    if (!solidAt((int)playerX, (int)newY + PLAYER_H - 1) &&
        !solidAt((int)playerX + PLAYER_W - 1, (int)newY + PLAYER_H - 1)) {
      playerY = newY;
      onGround = false;
    } else {
      // snap to tile
      int ty = ((int)newY + PLAYER_H - 1) / TILE_SIZE;
      playerY = ty * TILE_SIZE - PLAYER_H;
      playerVY = 0;
      onGround = true;
    }
  } else {
    // jumping up
    if (!solidAt((int)playerX, (int)newY) &&
        !solidAt((int)playerX + PLAYER_W - 1, (int)newY)) {
      playerY = newY;
    } else {
      playerVY = 0;
    }
  }

  // Moving platforms interaction (ride + push)
  onGround = onGround; // keep flag but refine with platforms
  for (int i = 0; i < platCount; i++) {
    if (plats[i].chunk != currentChunk) continue;

    // platform AABB
    float px = plats[i].x;
    float py = plats[i].y;
    float pw = plats[i].w;
    float ph = plats[i].h;

    float dx = plats[i].x - plats[i].prevX;
    float dy = plats[i].y - plats[i].prevY;

    // Standing on platform: check player feet just above platform and falling
    bool wasAbove = (playerY + PLAYER_H <= py + 2);
    bool feetOnPlatform =
      playerY + PLAYER_H >= py - 1 &&
      playerY + PLAYER_H <= py + ph &&
      playerX + PLAYER_W > px &&
      playerX < px + pw;

    if (feetOnPlatform && playerVY >= 0 && wasAbove) {
      playerY = py - PLAYER_H;
      playerVY = 0;
      onGround = true;
      // ride horizontal/vertical movement
      playerX += dx;
      playerY += dy;
    } else {
      // Side push (rough, simple)
      if (aabbOverlap(playerX, playerY, PLAYER_W, PLAYER_H,
                      px, py, pw, ph)) {
        if (dx > 0) playerX += dx;
        if (dx < 0) playerX += dx;
        if (dy > 0) playerY += dy;
        if (dy < 0) playerY += dy;
      }
    }
  }

  // Spikes collision
  if (spikeAt((int)playerX, (int)playerY) ||
      spikeAt((int)playerX + PLAYER_W - 1, (int)playerY) ||
      spikeAt((int)playerX, (int)playerY + PLAYER_H - 1) ||
      spikeAt((int)playerX + PLAYER_W - 1, (int)playerY + PLAYER_H - 1)) {
    killPlayer();
  }

  // Fall off screen
  if (playerY > 64) {
    killPlayer();
  }

  // Chunk transitions
  if (playerX > 128) {
    if (currentChunk < NUM_CHUNKS - 1) {
      currentChunk++;
      resetPlayer();
    } else {
      playerX = 120;
    }
  }
  if (playerX + PLAYER_W < 0) {
    if (currentChunk > 0) {
      currentChunk--;
      resetPlayer();
    } else {
      playerX = 0;
    }
  }
}

// ==================================================
// RENDER
// ==================================================
void drawLevel() {
  for (int x = 0; x < CHUNK_WIDTH; x++) {
    for (int y = 0; y < CHUNK_HEIGHT; y++) {
      uint8_t t = level[currentChunk][x][y];
      int sx = x * TILE_SIZE;
      int sy = y * TILE_SIZE;
      if (t == 1) {
        u8g2.drawBox(sx, sy, TILE_SIZE, TILE_SIZE);
      } else if (t == 2) {
        // spike triangle
        u8g2.drawTriangle(sx, sy + TILE_SIZE,
                          sx + TILE_SIZE / 2, sy,
                          sx + TILE_SIZE, sy + TILE_SIZE);
      }
    }
  }
}

void drawPlatforms() {
  for (int i = 0; i < platCount; i++) {
    if (plats[i].chunk != currentChunk) continue;
    u8g2.drawBox((int)plats[i].x, (int)plats[i].y, plats[i].w, plats[i].h);
  }
}

void drawEnemies() {
  for (int i = 0; i < enemyCount; i++) {
    if (enemies[i].chunk != currentChunk) continue;
    u8g2.drawFrame((int)enemies[i].x, (int)enemies[i].y, 8, 8);
  }
}

void drawPlayer() {
  u8g2.drawBox((int)playerX, (int)playerY, PLAYER_W, PLAYER_H);
}

// ==================================================
// SETUP / LOOP
// ==================================================
void setup() {
  u8g2.begin();

  pinMode(BTN_LEFT,  INPUT_PULLUP);
  pinMode(BTN_RIGHT, INPUT_PULLUP);
  pinMode(BTN_JUMP,  INPUT_PULLUP);

  buildLevels();
  resetPlayer();
}

void loop() {
  readInput();
  updatePlatforms();
  updateEnemies();
  updatePlayer();

  u8g2.clearBuffer();
  drawLevel();
  drawPlatforms();
  drawEnemies();
  drawPlayer();
  u8g2.sendBuffer();

  delay(16); // ~60 FPS
}