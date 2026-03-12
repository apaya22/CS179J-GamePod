from .constants import DIR_DELTA, OPPOSITE, UTURN_CLEARANCE, BIKE_SPEED, Direction

###########################################################################################################
#                                                PLAYER
###########################################################################################################

class Bike:
    def __init__(self, player_id, start_col, start_row, direction, color, trail_color):
        self.id          = player_id
        self.col         = start_col
        self.row         = start_row
        self.direction   = direction
        self.color       = color
        self.trail_color = trail_color
        self.alive       = True

        self.trail = [(start_col, start_row)]

        self._pending_turn  = False
        self._turn_axis_col = None
        self._turn_axis_row = None
        self._cells_in_direction = 0

    def set_direction(self, new_dir):
        if new_dir == self.direction:
            return

        if new_dir == OPPOSITE[self.direction]:
            if self._cells_in_direction < UTURN_CLEARANCE:
                return
            self._start_turn()
            self.direction = new_dir
            self._cells_in_direction = 0
            return

        self._start_turn()
        self.direction = new_dir
        self._cells_in_direction = 0

    def _start_turn(self):
        self._pending_turn  = True
        self._turn_axis_col = self.col
        self._turn_axis_row = self.row

    def advance(self):
        dc, dr = DIR_DELTA[self.direction]
        self.col += dc * BIKE_SPEED
        self.row += dr * BIKE_SPEED
        self._cells_in_direction += BIKE_SPEED

        if self._pending_turn:
            if self.direction in (Direction.LEFT, Direction.RIGHT):
                cleared = (self.col != self._turn_axis_col)
            else:
                cleared = (self.row != self._turn_axis_row)

            if cleared:
                self._pending_turn  = False
                self._turn_axis_col = None
                self._turn_axis_row = None
                should_stamp = True
            else:
                should_stamp = False
        else:
            should_stamp = True

        if should_stamp:
            self.trail.append((self.col, self.row))

        return self.col, self.row, should_stamp
