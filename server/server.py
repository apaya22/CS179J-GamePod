import json
import queue
import select
import socket
import struct
import threading
import time

from .constants import (
    TCP_HOST, TCP_PORT, MAX_LOG_LINES,
    PANEL_GREEN, PANEL_DIM, PLAYER_COLORS, WHITE, CYAN, RED,
    DIR_MAP, LobbyState, COUNTDOWN_SECONDS, MIN_PLAYERS, MAX_PLAYERS,
    SEND_TIMEOUT, READY_WINDOW_SECONDS,
)

###########################################################################################################
#                                             TCP SERVER
###########################################################################################################

class TronServer:
    """
    TCP server with an integrated lobby state machine.
    STATES:
    WAITING
    COUNTDOWN
    PLAYING
    """

    def __init__(self, host=TCP_HOST, port=TCP_PORT):
        self.host        = host
        self.port        = port
        self.input_queue = queue.Queue()   # shared with TronGame

        self._lock = threading.Lock()

        # pid {"addr", "socket", "connected_at", "last_input"}
        self._players: dict[int, dict] = {}

        # addr_str socket  (spectators: joined mid-game, lobby full, or timed-out)
        self._spectators: dict[str, socket.socket] = {}

        self._log: list[tuple] = []

        # Lobby state
        self._lobby_state     = LobbyState.WAITING
        self._countdown_end   = 0.0
        self._game_player_ids = frozenset()

        # Ready system
        self._ready: dict[int, bool] = {}   # pi True/False
        self._ready_deadline    = 0.0       # wall-clock time when ready window ends
        self._ready_deadline_id = 0         # version counter — cancels old workers

        # Socket setup
        self._server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self._server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self._server_socket.bind((self.host, self.port))
        self._server_socket.listen(5)

        self._add_log(f"TCP listening on {self.host}:{self.port}", PANEL_GREEN)

        threading.Thread(target=self._accept_loop,       daemon=True).start()
        threading.Thread(target=self._lobby_broadcaster, daemon=True).start()

    # ------------------------------------------------------------------ #
    #  Public interface used by TronGame / main                          #
    # ------------------------------------------------------------------ #

    @property
    def lobby_state(self) -> LobbyState:
        with self._lock:
            return self._lobby_state

    @property
    def game_player_ids(self) -> frozenset:
        with self._lock:
            return self._game_player_ids

    def on_game_reset(self):
        """
        Called by TronGame after a round ends. Keeps all players connected,
        clears ready states, and opens a fresh READY_WINDOW_SECONDS window
        so everyone has to re-ready before the next game.
        """
        with self._lock:
            self._game_player_ids = frozenset()
            self._ready.clear()
            self._lobby_state   = LobbyState.WAITING
            self._countdown_end = 0.0
            player_count = len(self._players)
            self._add_log(
                f"Round over — {player_count} player(s) still connected.", PANEL_DIM
            )
            if player_count >= MIN_PLAYERS:
                self._start_ready_window_locked()

    def send_to_all(self, data: str):
        """
        Non-blocking broadcast to every player and spectator socket.
        A dead or slow client can stall for at most SEND_TIMEOUT seconds.
        """
        encoded = (data + "\n").encode()

        with self._lock:
            socks = [info["socket"] for info in self._players.values()
                     if info.get("socket")]
            socks += list(self._spectators.values())

        # Filter out any sockets already closed (fd == -1 crashes select)
        socks = [s for s in socks if s.fileno() >= 0]

        if not socks:
            return

        try:
            _, writable, _ = select.select([], socks, [], SEND_TIMEOUT)
        except OSError:
            writable = []

        for sock in writable:
            try:
                sock.sendall(encoded)
            except OSError:
                pass    # reader thread will detect and clean up

    def get_snapshot(self):
        with self._lock:
            return dict(self._players), list(self._log), self._lobby_state

    def shutdown(self):
        try:
            self._server_socket.close()
        except OSError:
            pass

    # ------------------------------------------------------------------ #
    #  Connection management                                                #
    # ------------------------------------------------------------------ #

    def _accept_loop(self):
        while True:
            try:
                sock, addr = self._server_socket.accept()
                sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)

                sock.setsockopt(socket.SOL_SOCKET, socket.SO_KEEPALIVE, 1)
                try:
                    sock.ioctl(socket.SIO_KEEPALIVE_VALS,
                               struct.pack("III", 1, 3000, 1000))
                except AttributeError:            # Linux (THIS ONE WORKS)
                    try:
                        sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_KEEPIDLE,  3)
                        sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_KEEPINTVL, 1)
                        sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_KEEPCNT,   5)
                    except AttributeError:
                        pass
                self._add_log(f"Connection from {addr[0]}:{addr[1]}", CYAN)
                threading.Thread(
                    target=self._client_reader,
                    args=(sock, addr),
                    daemon=True,
                ).start()
            except OSError:
                break

    def _client_reader(self, sock: socket.socket, addr):
        addr_str  = f"{addr[0]}:{addr[1]}"
        buffer    = ""
        player_id = None
        try:
            while True:
                data = sock.recv(1024)
                if not data:          # recv() == b"" means client disconnected
                    break
                buffer += data.decode(errors="replace")
                while "\n" in buffer:
                    line, buffer = buffer.split("\n", 1)
                    line = line.strip()
                    if not line:
                        continue
                    pid = self._handle_message(line, addr_str, sock)
                    if pid is not None and player_id is None:
                        player_id = pid
        except (ConnectionResetError, ConnectionAbortedError, OSError):
            pass
        finally:
            sock.close()
            self._on_disconnect(player_id, addr_str)

    def _on_disconnect(self, player_id, addr_str: str):
        with self._lock:
            if player_id is not None and player_id in self._players:
                # Guard: if the player reconnected, their slot belongs to the
                # new connection — don't silently drop the new player.
                if self._players[player_id]["addr"] != addr_str:
                    self._add_log(
                        f"Stale disconnect ignored for P{player_id} ({addr_str})",
                        PANEL_DIM,
                    )
                    return

                del self._players[player_id]
                self._add_log(f"Player {player_id} disconnected ({addr_str})", RED)

                if self._lobby_state == LobbyState.PLAYING:
                    self._game_player_ids = self._game_player_ids - {player_id}
                    self.input_queue.put(("KILL", player_id))
                else:
                    self._ready.pop(player_id, None)
                    self._check_ready_locked()

            elif addr_str in self._spectators:
                del self._spectators[addr_str]
                self._add_log(f"Spectator disconnected ({addr_str})", PANEL_DIM)
            else:
                self._add_log(f"Disconnected (unregistered): {addr_str}", PANEL_DIM)

    # ------------------------------------------------------------------ #
    #  Message handling                                                     #
    # ------------------------------------------------------------------ #

    def _handle_message(self, line: str, addr_str: str, sock: socket.socket):
        """
        Parse one newline-delimited message.  Returns the player_id if a JOIN
        was successfully processed for a new player, otherwise None.
        """
        upper = line.upper()

        # ---- JOIN:<pid> ----
        if upper.startswith("JOIN:"):
            parts = line.split(":")
            if len(parts) < 2:
                return None
            try:
                pid = int(parts[1])
            except ValueError:
                self._add_log(f"Bad JOIN from {addr_str}: {line}", RED)
                return None

            if pid < 1 or pid > MAX_PLAYERS:
                self._add_log(f"Invalid player ID {pid} from {addr_str}", RED)
                return None

            with self._lock:
                state = self._lobby_state

                if state == LobbyState.PLAYING:
                    if pid in self._game_player_ids:
                        old_sock = self._players.get(pid, {}).get("socket")
                        if old_sock and old_sock is not sock:
                            try:
                                old_sock.close()
                            except OSError:
                                pass
                        self._players[pid] = {
                            "addr":         addr_str,
                            "socket":       sock,
                            "connected_at": _timestamp(),
                            "last_input":   None,
                        }
                        color = PLAYER_COLORS[pid - 1] if pid <= len(PLAYER_COLORS) else WHITE
                        self._add_log(f"P{pid} reconnected mid-game ({addr_str})", color)
                        return pid
                    else:
                        self._spectators[addr_str] = sock
                        color = PLAYER_COLORS[pid - 1] if pid <= len(PLAYER_COLORS) else WHITE
                        self._add_log(f"P{pid} joined as spectator ({addr_str})", color)
                        return None

                # WAITING / COUNTDOWN — lobby full spectator
                if (len(self._players) >= MAX_PLAYERS and pid not in self._players):
                    self._spectators[addr_str] = sock
                    self._add_log(f"P{pid}: lobby full — spectating ({addr_str})", PANEL_DIM)
                    return None

                # Register / re-register as player
                old_sock = self._players.get(pid, {}).get("socket")
                if old_sock and old_sock is not sock:
                    try:
                        old_sock.close()
                    except OSError:
                        pass
                self._players[pid] = {
                    "addr":         addr_str,
                    "socket":       sock,
                    "connected_at": _timestamp(),
                    "last_input":   None,
                }
                color = PLAYER_COLORS[pid - 1] if pid <= len(PLAYER_COLORS) else WHITE
                self._add_log(f"Player {pid} joined from {addr_str}", color)

                # Start the ready window once MIN_PLAYERS are in the lobby
                if len(self._players) >= MIN_PLAYERS and self._ready_deadline <= 0:
                    self._start_ready_window_locked()

            return pid

        # ---- READY:<pid>  (toggle; sent by client when A is pressed) ----
        if upper.startswith("READY:"):
            parts = line.split(":")
            if len(parts) < 2:
                return None
            try:
                pid = int(parts[1])
            except ValueError:
                return None
            with self._lock:
                if pid in self._players:
                    self._ready[pid] = not self._ready.get(pid, False)
                    status = "ready" if self._ready[pid] else "not ready"
                    color  = PLAYER_COLORS[pid - 1] if pid <= len(PLAYER_COLORS) else WHITE
                    self._add_log(f"P{pid} is {status}", color)
                    self._check_ready_locked()
            return None

        # ---- LEAVE:<pid>  (player pressed home in the lobby) ----
        if upper.startswith("LEAVE:"):
            parts = line.split(":")
            if len(parts) >= 2:
                try:
                    pid = int(parts[1])
                except ValueError:
                    return None
                with self._lock:
                    if (pid in self._players
                            and self._players[pid]["addr"] == addr_str
                            and self._lobby_state != LobbyState.PLAYING):
                        del self._players[pid]
                        self._ready.pop(pid, None)
                        self._add_log(f"P{pid} left the lobby ({addr_str})", PANEL_DIM)
                        self._check_ready_locked()
            return None

        # ---- TOGGLE_SPECTATE:<pid>  (spectator presses X to rejoin as player) ----
        if upper.startswith("TOGGLE_SPECTATE:"):
            parts = line.split(":")
            if len(parts) >= 2:
                try:
                    pid = int(parts[1])
                except ValueError:
                    return None
                with self._lock:
                    # If spectator wants to toggle back to player
                    if addr_str in self._spectators and self._lobby_state != LobbyState.PLAYING:
                        # Check if player slot is available
                        if len(self._players) < MAX_PLAYERS:
                            sock = self._spectators[addr_str]
                            del self._spectators[addr_str]
                            self._players[pid] = {
                                "addr":         addr_str,
                                "socket":       sock,
                                "connected_at": _timestamp(),
                                "last_input":   None,
                            }
                            self._ready[pid] = False
                            self._add_log(f"P{pid} rejoined as player from spectator", PLAYER_COLORS[pid - 1] if pid <= len(PLAYER_COLORS) else WHITE)
                            self._check_ready_locked()
                    # If player wants to toggle to spectator
                    elif pid in self._players and self._players[pid]["addr"] == addr_str:
                        info = self._players.pop(pid)
                        self._spectators[addr_str] = info["socket"]
                        self._ready.pop(pid, None)
                        self._add_log(f"P{pid} moved to spectator mode", PANEL_DIM)
                        self._check_ready_locked()
            return None

        # ---- DIR:<pid>:<direction> ----
        if upper.startswith("DIR:"):
            parts = line.split(":")
            if len(parts) == 3:
                try:
                    pid = int(parts[1])
                except ValueError:
                    return None
                dir_str = parts[2].upper()
                if dir_str in DIR_MAP:
                    self.input_queue.put(("DIR", pid, DIR_MAP[dir_str]))
                    with self._lock:
                        if pid in self._players:
                            self._players[pid]["last_input"] = dir_str
            return None

        return None

    # ------------------------------------------------------------------ #
    #  Ready system                                                         #
    # ------------------------------------------------------------------ #

    def _start_ready_window_locked(self):
        """Open a READY_WINDOW_SECONDS window for players to ready up.
        Must be called with _lock held."""
        self._ready_deadline_id += 1
        self._ready_deadline = time.time() + READY_WINDOW_SECONDS
        deadline_id = self._ready_deadline_id
        self._add_log(
            f"Ready window open — {READY_WINDOW_SECONDS}s to press A!", PANEL_GREEN
        )
        threading.Thread(
            target=self._ready_window_worker,
            args=(deadline_id,),
            daemon=True,
        ).start()

    def _ready_window_worker(self, deadline_id: int):
        """After READY_WINDOW_SECONDS, move non-ready connected players to spectators."""
        time.sleep(READY_WINDOW_SECONDS)
        with self._lock:
            if self._ready_deadline_id != deadline_id:
                return  # window was superseded (e.g. new round started)
            if self._lobby_state != LobbyState.WAITING:
                return  # already in countdown or playing
            self._ready_deadline = 0.0
            # Move every non-ready player to spectator when the window closes.
            # The client renders inactive slots as greyed-out (ghosted).
            to_spectate = [pid for pid in list(self._players.keys())
                           if not self._ready.get(pid, False)]
            for pid in to_spectate:
                info = self._players.pop(pid)
                self._spectators[info["addr"]] = info["socket"]
                self._ready.pop(pid, None)
                self._add_log(
                    f"P{pid} didn't ready in time — ghosted as spectator",
                    PANEL_DIM,
                )
            # Either way, re-evaluate state
            self._check_ready_locked()

    def _check_ready_locked(self):
        """
        Evaluate whether the lobby state should change based on ready status.
        Must be called with _lock held.
        """
        if self._lobby_state == LobbyState.PLAYING:
            return

        active_pids = set(self._players.keys())
        ready_pids  = {pid for pid in active_pids if self._ready.get(pid, False)}

        if self._lobby_state == LobbyState.COUNTDOWN:
            # Cancel if someone unreadied or not enough players left
            if ready_pids != active_pids or len(active_pids) < MIN_PLAYERS:
                self._lobby_state   = LobbyState.WAITING
                self._countdown_end = 0.0
                self._add_log("Countdown cancelled — player unreadied or left.", RED)
                self._start_ready_window_locked()

        else:  # WAITING
            # Start countdown only when ALL active players are ready and enough
            if len(active_pids) >= MIN_PLAYERS and ready_pids == active_pids:
                self._start_countdown_locked()

    # ------------------------------------------------------------------ #
    #  Lobby state machine                                                  #
    # ------------------------------------------------------------------ #

    def _start_countdown_locked(self):
        """Transition to COUNTDOWN. Must be called with _lock held."""
        self._lobby_state   = LobbyState.COUNTDOWN
        self._countdown_end = time.time() + COUNTDOWN_SECONDS
        self._add_log(
            f"All players ready — {COUNTDOWN_SECONDS}s countdown! "
            f"({len(self._players)}/{MAX_PLAYERS} players)",
            PANEL_GREEN,
        )
        threading.Thread(target=self._countdown_worker,
                         args=(self._countdown_end,), daemon=True).start()

    def _countdown_worker(self, expected_end: float):
        """Fires PLAYING transition when countdown expires, or exits if cancelled."""
        while True:
            time.sleep(0.1)
            with self._lock:
                if self._lobby_state != LobbyState.COUNTDOWN:
                    return
                if self._countdown_end != expected_end:
                    return
                if time.time() >= expected_end:
                    self._lobby_state     = LobbyState.PLAYING
                    self._game_player_ids = frozenset(self._players.keys())
                    self._add_log(
                        f"Game starting! Players: {sorted(self._game_player_ids)}",
                        PANEL_GREEN,
                    )
                    return

    # ------------------------------------------------------------------ #
    #  Lobby status broadcaster                                             #
    # ------------------------------------------------------------------ #

    def _lobby_broadcaster(self):
        """
        Sends a lobby-status JSON packet every 250 ms while in WAITING or
        COUNTDOWN.  Includes per-slot ready status for all MAX_PLAYERS slots.
        """
        while True:
            time.sleep(0.25)
            with self._lock:
                state = self._lobby_state
                if state == LobbyState.PLAYING:
                    continue

                # Build slot info for every possible player ID
                slots = [
                    {
                        "id":     pid,
                        "active": pid in self._players,
                        "ready":  self._ready.get(pid, False),
                    }
                    for pid in range(1, MAX_PLAYERS + 1)
                ]

                if state == LobbyState.WAITING:
                    ready_secs = (max(0.0, self._ready_deadline - time.time())
                                  if self._ready_deadline > 0 else 0.0)
                    msg = json.dumps({
                        "type":       "lobby",
                        "state":      "waiting",
                        "slots":      slots,
                        "ready_secs": round(ready_secs, 1),
                    })
                else:  # COUNTDOWN
                    cd_secs = max(0.0, self._countdown_end - time.time())
                    msg = json.dumps({
                        "type":          "lobby",
                        "state":         "countdown",
                        "slots":         slots,
                        "countdown_secs": round(cd_secs, 1),
                    })

            self.send_to_all(msg)

    # ------------------------------------------------------------------ #
    #  Logging                                                              #
    # ------------------------------------------------------------------ #

    def _add_log(self, message: str, color=PANEL_DIM):
        """Must be called with _lock held or before threads start."""
        entry = (_timestamp(), message, color)
        self._log.append(entry)
        if len(self._log) > MAX_LOG_LINES:
            self._log.pop(0)


def _timestamp() -> str:
    return time.strftime("%H:%M:%S")
