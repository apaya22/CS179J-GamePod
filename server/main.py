import threading
import time

from .constants import TCP_PORT, WALL_LEFT, WALL_RIGHT, WALL_TOP, WALL_BOTTOM, LobbyState
from .server import TronServer
from .game import TronGame

###########################################################################################################
#                                                MAIN
###########################################################################################################

def main():
    server = TronServer()
    game   = TronGame(input_queue=server.input_queue)

    def on_tick(g):

        if server.lobby_state == LobbyState.PLAYING:
            server.send_to_all(g.build_slim_state())

    game_thread = threading.Thread(
        target=game.run,
        kwargs={"on_tick": on_tick, "lobby": server},
        daemon=True,
    )
    game_thread.start()

    print("Tron Proto server running.")
    print(f"TCP (ESP32):   port {TCP_PORT}")
    print(f"Play-field:    cols {WALL_LEFT}–{WALL_RIGHT}, rows {WALL_TOP}–{WALL_BOTTOM}")
    print(f"Lobby:         game starts after {2} players connect (+5 s countdown)")
    print(f"               a 3rd player may join during the countdown V1 - LOBBY ADDED")

    try:
        while True:
            time.sleep(1.0)
    except KeyboardInterrupt:
        pass
    finally:
        game.running = False
        game_thread.join(timeout=2.0)
        server.shutdown()


if __name__ == "__main__":
    main()
