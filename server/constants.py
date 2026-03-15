from enum import Enum

###########################################################################################################
#                                                CONSTANTS
###########################################################################################################

# based on our SPI display
GRID_COLS = 320
GRID_ROWS = 240

# color profiles
WHITE      = (255, 255, 255)
BLACK      = (0, 0, 0)
DARK_GRAY  = (30, 30, 30)
GREEN      = (0, 255, 0)
RED        = (255, 50, 50)
BLUE       = (50, 100, 255)
YELLOW     = (255, 255, 0)
CYAN       = (0, 200, 200)

PANEL_BG      = (15, 15, 15)
PANEL_GREEN   = (0, 200, 0)
PANEL_DIM     = (100, 100, 100)
PANEL_HEADER  = (0, 255, 100)

RED_TRAIL   = (150, 20, 20)
BLUE_TRAIL  = (20, 50, 150)
GREEN_TRAIL = (20, 150, 20)

# colors as arrays
PLAYER_COLORS = [RED, BLUE, GREEN]
TRAIL_COLORS  = [RED_TRAIL, BLUE_TRAIL, GREEN_TRAIL]

FPS        = 30
BIKE_SPEED = 1   # cells advanced per tick

TCP_HOST = "0.0.0.0"
TCP_PORT = 5005

MAX_LOG_LINES = 40

# ---------------------------------------------------------------------------
# Sprite sizes
# ---------------------------------------------------------------------------
SPRITE_HALF_W = 2
SPRITE_HALF_H = 4

WALL_LEFT   = SPRITE_HALF_W
WALL_RIGHT  = GRID_COLS - 1 - SPRITE_HALF_W
WALL_TOP    = SPRITE_HALF_H
WALL_BOTTOM = GRID_ROWS - 1 - SPRITE_HALF_H

UTURN_CLEARANCE = SPRITE_HALF_H + 1 # Don't allow perfect uturns

# ---------------------------------------------------------------------------
# Lobby
# ---------------------------------------------------------------------------
COUNTDOWN_SECONDS = 5
#MIN_PLAYERS       = 1 #testing
MIN_PLAYERS       = 2
MAX_PLAYERS       = 3

# ---------------------------------------------------------------------------
# Network — non-blocking send & keepalive
# ---------------------------------------------------------------------------
# BUG: Server locks when player leaves mid game waits for socket X to become available (fixed)

SEND_TIMEOUT = 0.05

READY_WINDOW_SECONDS = 10   # seconds players have to press A before being spectated

###########################################################################################################
#                                                ENUMS
###########################################################################################################

class Direction(Enum):
    UP    = 0
    DOWN  = 1
    LEFT  = 2
    RIGHT = 3

# TRON GAME - SM
class LobbyState(Enum):
    WAITING   = "waiting"    # LOBBY - Doesn't meet player count to start game
    COUNTDOWN = "countdown"  # LOBBY - min players to start game
    PLAYING   = "playing"    # GAME

DIR_DELTA = {
    Direction.UP:    ( 0,  1),
    Direction.DOWN:  ( 0, -1),
    Direction.LEFT:  ( 1,  0),
    Direction.RIGHT: (-1,  0),
}

# BUG: Moving in opposite direction forces collision into self (FIXED)
OPPOSITE = {
    Direction.UP:    Direction.DOWN,
    Direction.DOWN:  Direction.UP,
    Direction.LEFT:  Direction.RIGHT,
    Direction.RIGHT: Direction.LEFT,
}

DIR_MAP = {
    "UP":    Direction.UP,
    "DOWN":  Direction.DOWN,
    "LEFT":  Direction.LEFT,
    "RIGHT": Direction.RIGHT,
}

###########################################################################################################
#                                            BIKE SPRITES
###########################################################################################################

TRON_BIKE_UP = [
    0, 0, 1, 0, 0,
    0, 1, 1, 1, 0,
    1, 1, 1, 1, 1,
    0, 1, 1, 1, 0,
    1, 1, 1, 1, 1,
    1, 1, 1, 1, 1,
    0, 1, 0, 1, 0,
    0, 1, 0, 1, 0,
    0, 1, 0, 1, 0,
]

TRON_BIKE_DOWN = [
    0, 1, 0, 1, 0,
    0, 1, 0, 1, 0,
    0, 1, 0, 1, 0,
    1, 1, 1, 1, 1,
    1, 1, 1, 1, 1,
    0, 1, 1, 1, 0,
    1, 1, 1, 1, 1,
    0, 1, 1, 1, 0,
    0, 0, 1, 0, 0,
]

TRON_BIKE_LEFT = [
    0, 0, 1, 0, 1, 1, 0, 0, 0,
    0, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 0, 0, 0,
    0, 1, 1, 1, 1, 1, 1, 1, 1,
    0, 0, 1, 0, 1, 1, 0, 0, 0,
]

TRON_BIKE_RIGHT = [
    0, 0, 0, 1, 1, 0, 1, 0, 0,
    1, 1, 1, 1, 1, 1, 1, 1, 0,
    0, 0, 0, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 0,
    0, 0, 0, 1, 1, 0, 1, 0, 0,
]
