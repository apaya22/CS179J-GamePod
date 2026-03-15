import json
import queue
import time

from .constants import (
    GRID_COLS, GRID_ROWS, WALL_LEFT, WALL_RIGHT, WALL_TOP, WALL_BOTTOM,
    PLAYER_COLORS, TRAIL_COLORS, Direction, LobbyState, UTURN_CLEARANCE, FPS,
)
from .bike import Bike

###########################################################################################################
#                                                GAME
###########################################################################################################

def _make_bikes(player_ids):
    """
    Create bikes for the given player IDs.  Spawn positions are fixed per ID:
      1 (RED)   — left side,  facing right
      2 (BLUE)  — right side, facing left
      3 (GREEN) — top centre, facing down
    """
    spawn_margin = max(GRID_COLS // 5, WALL_LEFT + UTURN_CLEARANCE + 1)
    spawns = {
        1: (spawn_margin,             GRID_ROWS // 2, Direction.RIGHT),
        2: (GRID_COLS - spawn_margin, GRID_ROWS // 2, Direction.LEFT),
        3: (GRID_COLS // 2,           spawn_margin,   Direction.DOWN),
    }
    return [
        Bike(pid, spawns[pid][0], spawns[pid][1], spawns[pid][2],
             PLAYER_COLORS[pid - 1], TRAIL_COLORS[pid - 1])
        for pid in sorted(player_ids)
        if pid in spawns
    ]


class TronGame:

    def __init__(self, input_queue=None):
        self.input_queue     = input_queue if input_queue is not None else queue.Queue()
        self.grid            = [[0] * GRID_COLS for _ in range(GRID_ROWS)]
        self.bikes           = []
        self.running         = True
        self.game_over       = False
        self.game_over_time  = 0.0
        self.winner_text     = ""

    # ------------------------------------------------------------------
    #  Initialize
    # ------------------------------------------------------------------
    def init_for_players(self, player_ids):
        self.grid           = [[0] * GRID_COLS for _ in range(GRID_ROWS)]
        self.bikes          = _make_bikes(player_ids)
        self.game_over      = False
        self.game_over_time = 0.0
        self.winner_text    = ""
        for bike in self.bikes:
            self.grid[bike.row][bike.col] = bike.id
        # Drain stale input events from any previous game
        _drain(self.input_queue)

    # ------------------------------------------------------------------
    #  Input — two event kinds live on the queue:
    #    ("DIR",  pid, Direction)   — direction change from a client
    #    ("KILL", pid)              — player disconnected mid-game
    # ------------------------------------------------------------------
    def process_pending_inputs(self):
        while True:
            try:
                event = self.input_queue.get_nowait()
            except queue.Empty:
                break
            kind = event[0]
            if kind == "DIR":
                _, pid, direction = event
                for bike in self.bikes:
                    if bike.id == pid and bike.alive:
                        bike.set_direction(direction)
            elif kind == "KILL":
                _, pid = event
                for bike in self.bikes:
                    if bike.id == pid:
                        bike.alive = False

    # ------------------------------------------------------------------
    #  Update — one game tick
    # ------------------------------------------------------------------
    def update(self):
        if self.game_over:
            return

        for bike in self.bikes:
            if not bike.alive:
                continue

            col, row, should_stamp = bike.advance()

            if col < WALL_LEFT or col > WALL_RIGHT or row < WALL_TOP or row > WALL_BOTTOM:
                bike.alive = False
                continue

            if should_stamp:
                if self.grid[row][col] != 0:
                    bike.alive = False
                    continue
                self.grid[row][col] = bike.id
            else:
                if self.grid[row][col] not in (0, bike.id):
                    bike.alive = False
                    continue

        # Head-on collision
        positions = {}
        for bike in self.bikes:
            if bike.alive:
                pos = (bike.col, bike.row)
                if pos in positions:
                    bike.alive = False
                    positions[pos].alive = False
                else:
                    positions[pos] = bike

        alive = [b for b in self.bikes if b.alive]

        # Fixed: original used < 1 (never triggers with 1 winner). Must be <= 1.
        if len(alive) <= 1 and not self.game_over:
        #if len(alive) < 1 and not self.game_over: #testing for single player connection
            self.game_over      = True
            self.game_over_time = time.time()
            self.winner_text    = (
                f"Player {alive[0].id} Wins!" if len(alive) == 1 else "Draw!"
            )

    # ------------------------------------------------------------------
    #  State packet
    # ------------------------------------------------------------------
    def build_slim_state(self):
        return json.dumps({
            "type":        "game",
            "bikes":       [
                {
                    "id":        b.id,
                    "col":       b.col,
                    "row":       b.row,
                    "direction": b.direction.name,
                    "alive":     b.alive,
                }
                for b in self.bikes
            ],
            "game_over":   self.game_over,
            "winner_text": self.winner_text,
            "wall_left":   WALL_LEFT,
            "wall_right":  WALL_RIGHT,
            "wall_top":    WALL_TOP,
            "wall_bottom": WALL_BOTTOM,
        })

    # ------------------------------------------------------------------
    #  Main loop
    #
    #  When a lobby is provided the loop behaves as follows:
    #   - WAITING & COUNTDOWN  - sleep; do not tick or broadcast
    #   - PLAYING (first time) - call init_for_players, then tick each frame
    #   - game_over (5 s)      - reset, notify lobby, wait for next round
    #
    #  on_tick(game) is called once per active tick so the server can
    #  broadcast the current state.
    # ------------------------------------------------------------------
    def run(self, on_tick=None, lobby=None):
        initialized = False

        while self.running:

            # ---- Lobby gate ----
            if lobby is not None:
                state = lobby.lobby_state

                if state in (LobbyState.WAITING, LobbyState.COUNTDOWN):
                    initialized = False
                    time.sleep(0.05)
                    continue

                # First tick of a new PLAYING session - init bikes
                if state == LobbyState.PLAYING and not initialized:
                    self.init_for_players(lobby.game_player_ids)
                    initialized = True

            # ---- Tick -----------------
            self.process_pending_inputs()

            if not self.game_over:
                self.update()
            elif time.time() - self.game_over_time >= 5:
                # Keep the same player set; lobby decides next state
                self.init_for_players({b.id for b in self.bikes})
                initialized = False
                if lobby is not None:
                    lobby.on_game_reset()

            if on_tick:
                on_tick(self)

            time.sleep(1.0 / FPS)


# ------------------------------------------------------------------
#  Helpers
# ------------------------------------------------------------------
def _drain(q: queue.Queue):
    while True:
        try:
            q.get_nowait()
        except queue.Empty:
            break
