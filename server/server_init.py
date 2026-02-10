import pygame
import socket
import json
import os
from enum import Enum

###########################################################################################################
#                                                CONSTANTS
###########################################################################################################

WINDOW_WIDTH =  800
WINDOW_HEIGHT = 600

WHITE =      (255, 255, 255)
BLACK =      (0, 0, 0)
GRAY =       (128, 128, 128)
LIGHT_GRAY = (100, 100, 100)
GREEN =      (0, 255, 0)
RED =        (255, 0, 0)
BLUE =       (0, 0, 255)

CONFIG_FILE = "tron_config.json"  # Save just so I don't have to keep typing in the same ips

###########################################################################################################
#                                                CLASSES
###########################################################################################################

"""State Machine"""
class GameState(Enum):
    CONNECTION = 1
    PLAYING = 2

class Player:
    def __init__(self, player_id, color):
        self.id = player_id
        self.ip = ""
        self.port = 0
        self.connected = False
        self.color = color
        self.socket = None

###########################################################################################################
#                                                APP
###########################################################################################################

class TronHost:

    def __init__(self):
        pygame.init()
        self.screen = pygame.display.set_mode((WINDOW_WIDTH, WINDOW_HEIGHT))
        pygame.display.set_caption("Tron Host - Connection Setup")
        self.clock = pygame.time.Clock()
        self.font = pygame.font.Font(None, 32)
        self.small_font = pygame.font.Font(None, 24)

        # Game state
        self.state = GameState.CONNECTION
        self.running = True

        # Players
        self.players = [
            Player(1, RED),
            Player(2, BLUE),
            Player(3, GREEN)
        ]

        # UDP setup
        self.server_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.server_socket.setblocking(False)
        self.server_port = 5005

        # UI elements
        self.input_boxes = []
        self.port_box = None
        self.active_box = None
        self.setup_input_boxes()

        # Buttons
        self.connect_button =   pygame.Rect(50, 520, 200, 50)
        self.save_button =      pygame.Rect(270, 520, 150, 50)
        self.load_button =      pygame.Rect(440, 520, 150, 50)
        self.start_button =     pygame.Rect(610, 520, 140, 50)

    def setup_input_boxes(self):
        """Create input boxes for IPs and port"""
        y_start = 150
        y_spacing = 80

        for i in range(3):
            box = {
                'rect': pygame.Rect(250, y_start + i * y_spacing, 400, 40),
                'text': '',
                'player_id': i,
                'active': False
            }
            self.input_boxes.append(box)

        # Port input box
        self.port_box = {
            'rect': pygame.Rect(250, 430, 150, 40),
            'text': str(self.server_port),
            'active': False
        }

    def save_config(self):
        """Save IP addresses and port to config file"""
        config = {
            'port': int(self.port_box['text']) if self.port_box['text'].isdigit() else self.server_port,
            'players': []
        }

        for i, box in enumerate(self.input_boxes):
            config['players'].append({
                'id': i + 1,
                'ip': box['text']
            })

        with open(CONFIG_FILE, 'w') as f:
            json.dump(config, f, indent=2)

        print("Configuration saved!")

    def load_config(self):
        """Load IP addresses and port from config file"""
        if not os.path.exists(CONFIG_FILE):
            print("No config file found")
            return

        try:
            with open(CONFIG_FILE, 'r') as f:
                config = json.load(f)

            self.port_box['text'] = str(config.get('port', self.server_port))
            self.server_port = int(self.port_box['text'])

            for player_data in config.get('players', []):
                player_id = player_data['id'] - 1
                if 0 <= player_id < len(self.input_boxes):
                    self.input_boxes[player_id]['text'] = player_data['ip']

            print("Configuration loaded!")
        except Exception as e:
            print(f"Error loading config: {e}")

    def test_connection(self, player_id):
        """Send handshake packet to ESP32 and wait for response"""
        if player_id >= len(self.players):
            return

        player = self.players[player_id]
        ip = self.input_boxes[player_id]['text']

        if not ip:
            print(f"Player {player_id + 1}: No IP entered")
            return

        try:
            # Send handshake
            message = f"HANDSHAKE:{player_id + 1}".encode()
            self.server_socket.sendto(message, (ip, self.server_port))

            # Try to receive response (non-blocking)
            try:
                self.server_socket.settimeout(1.0)
                data, addr = self.server_socket.recvfrom(1024)
                response = data.decode()

                if response.startswith("ACK"):
                    player.connected = True
                    player.ip = ip
                    player.port = self.server_port
                    print(f"Player {player_id + 1} connected: {ip}")
                else:
                    player.connected = False
                    print(f"Player {player_id + 1}: Invalid response")
            except socket.timeout:
                player.connected = False
                print(f"Player {player_id + 1}: Connection timeout")
            finally:
                self.server_socket.setblocking(False)

        except Exception as e:
            player.connected = False
            print(f"Player {player_id + 1}: Connection error - {e}")

    def connect_all(self):
        """Test connections to all players"""
        for i in range(len(self.players)):
            self.test_connection(i)

    def all_connected(self):
        """Check if all players are connected"""
        return all(player.connected for player in self.players)

    def handle_events(self):
        """Handle pygame events"""
        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                self.running = False

            if event.type == pygame.MOUSEBUTTONDOWN:
                # Check input boxes
                for box in self.input_boxes:
                    if box['rect'].collidepoint(event.pos):
                        box['active'] = True
                        self.active_box = box
                    else:
                        box['active'] = False

                # Check port box
                if self.port_box['rect'].collidepoint(event.pos):
                    self.port_box['active'] = True
                    self.active_box = self.port_box
                    for box in self.input_boxes:
                        box['active'] = False
                else:
                    if self.active_box == self.port_box:
                        self.port_box['active'] = False
                        self.active_box = None

                # Check buttons
                if self.connect_button.collidepoint(event.pos):
                    self.connect_all()

                if self.save_button.collidepoint(event.pos):
                    self.save_config()

                if self.load_button.collidepoint(event.pos):
                    self.load_config()

                if self.start_button.collidepoint(event.pos) and self.all_connected():
                    self.state = GameState.PLAYING
                    print("Starting game...")

            if event.type == pygame.KEYDOWN:
                if self.active_box:
                    if event.key == pygame.K_BACKSPACE:
                        self.active_box['text'] = self.active_box['text'][:-1]
                    elif event.key == pygame.K_RETURN:
                        self.active_box['active'] = False
                        self.active_box = None
                    else:
                        # Only allow IP-valid characters
                        if event.unicode in '0123456789.':
                            self.active_box['text'] += event.unicode

    def draw_connection_screen(self):
        """Draw the connection setup screen"""
        self.screen.fill(WHITE)

        # Title
        title = self.font.render("Tron Host - Connection Setup", True, BLACK)
        self.screen.blit(title, (WINDOW_WIDTH // 2 - title.get_width() // 2, 30))

        # Player labels and input boxes
        y_start = 150
        y_spacing = 80

        for i, box in enumerate(self.input_boxes):
            # Player label
            label = self.small_font.render(f"Player {i + 1} IP:", True, BLACK)
            self.screen.blit(label, (50, box['rect'].y + 8))

            # Input box
            color = BLUE if box['active'] else GRAY
            pygame.draw.rect(self.screen, color, box['rect'], 2)

            # Text
            text_surface = self.small_font.render(box['text'], True, BLACK)
            self.screen.blit(text_surface, (box['rect'].x + 5, box['rect'].y + 8))

            # Connection status indicator
            status_color = GREEN if self.players[i].connected else RED
            pygame.draw.circle(self.screen, status_color, (680, box['rect'].y + 20), 10)

        # Port label and input
        port_label = self.small_font.render("UDP Port:", True, BLACK)
        self.screen.blit(port_label, (50, self.port_box['rect'].y + 8))

        color = BLUE if self.port_box['active'] else GRAY
        pygame.draw.rect(self.screen, color, self.port_box['rect'], 2)
        text_surface = self.small_font.render(self.port_box['text'], True, BLACK)
        self.screen.blit(text_surface, (self.port_box['rect'].x + 5, self.port_box['rect'].y + 8))




        ################################################
        # Buttons
        ################################################

        # Connect button
        pygame.draw.rect(self.screen, LIGHT_GRAY, self.connect_button)
        pygame.draw.rect(self.screen, BLACK, self.connect_button, 2)
        connect_text = self.small_font.render("Connect All", True, BLACK)
        self.screen.blit(connect_text, (self.connect_button.x + 20, self.connect_button.y + 15))

        # Save button
        pygame.draw.rect(self.screen, LIGHT_GRAY, self.save_button)
        pygame.draw.rect(self.screen, BLACK, self.save_button, 2)
        save_text = self.small_font.render("Save Config", True, BLACK)
        self.screen.blit(save_text, (self.save_button.x + 20, self.save_button.y + 15))

        # Load button
        pygame.draw.rect(self.screen, LIGHT_GRAY, self.load_button)
        pygame.draw.rect(self.screen, BLACK, self.load_button, 2)
        load_text = self.small_font.render("Load Config", True, BLACK)
        self.screen.blit(load_text, (self.load_button.x + 20, self.load_button.y + 15))

        # Start button (only enabled if all connected)
        button_color = GREEN if self.all_connected() else GRAY
        pygame.draw.rect(self.screen, button_color, self.start_button)
        pygame.draw.rect(self.screen, BLACK, self.start_button, 2)
        start_text = self.small_font.render("Start Game", True, BLACK)
        self.screen.blit(start_text, (self.start_button.x + 20, self.start_button.y + 15))

    ###########################################################################################################
    ##                                                RUN
    ###########################################################################################################

    def run(self):
        """Main game loop"""
        while self.running:
            self.handle_events()

            if self.state == GameState.CONNECTION:
                self.draw_connection_screen()
            elif self.state == GameState.PLAYING:
                # TODO: Draw game screen
                self.screen.fill(BLACK)
                text = self.font.render("Game Starting...", True, WHITE)
                self.screen.blit(text, (WINDOW_WIDTH // 2 - 150, WINDOW_HEIGHT // 2))

            pygame.display.flip()
            self.clock.tick(60)

        pygame.quit()

###########################################################################################################
#                                                MAIN
###########################################################################################################

if __name__ == "__main__":
    host = TronHost()
    host.run()