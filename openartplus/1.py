import sensor
import time
from machine import UART


COLS = 16
ROWS = 12

VISION_HEADER_0 = 0xA5
VISION_HEADER_1 = 0x5A
VISION_CMD_MAP = 0x01
VISION_PAYLOAD_LEN = 195
VISION_SEND_PERIOD_MS = 500

UART_PORT = 12
UART_BAUD = 115200

GRID_CELL_UNKNOWN = 0
GRID_CELL_EMPTY = 1
GRID_CELL_WALL = 2
GRID_CELL_BOX = 3
GRID_CELL_GOAL = 4
GRID_CELL_BOMB = 5
GRID_CELL_CAR = 6

CAR_DIR_UP = 0

DEFAULT_CAR_X = COLS // 2
DEFAULT_CAR_Y = ROWS // 2
DEFAULT_CAR_DIR = CAR_DIR_UP

START_X = 43
END_X = 275
START_Y = 35
END_Y = 201

STEP_X = (END_X - START_X) / (COLS - 1)
STEP_Y = (END_Y - START_Y) / (ROWS - 1)
MID_X = (START_X + END_X) / 2
MID_Y = (START_Y + END_Y) / 2

GAIN_TOP_LEFT = 1.1
GAIN_TOP_RIGHT = 1.1
GAIN_BOTTOM_LEFT = 2.5
GAIN_BOTTOM_RIGHT = 1.8

CUSTOM_ZONES = [
    (93, 216, 125, 185, 1.5),
    (46, 77, 36, 66, 2.3),
]

COLOR_PROFILES = (
    ("-", GRID_CELL_EMPTY, (40, 50, 220)),
    ("#", GRID_CELL_WALL, (100, 100, 100)),
    ("B", GRID_CELL_BOX, (220, 210, 0)),
    ("T", GRID_CELL_GOAL, (255, 0, 255)),
    ("X", GRID_CELL_BOMB, (255, 60, 70)),
    ("@", GRID_CELL_CAR, (0, 200, 0)),
)

uart = UART(UART_PORT, baudrate=UART_BAUD)


def build_grid_points():
    grid_points = []
    for row in range(ROWS):
        row_points = []
        for col in range(COLS):
            x = int(START_X + col * STEP_X)
            y = int(START_Y + row * STEP_Y)
            row_points.append((x, y))
        grid_points.append(row_points)
    return grid_points


GRID_POINTS = build_grid_points()


def init_sensor():
    sensor.reset()
    sensor.set_pixformat(sensor.RGB565)
    sensor.set_framesize(sensor.QVGA)
    sensor.skip_frames(time=500)
    sensor.set_vflip(True)
    sensor.set_hmirror(False)
    sensor.set_auto_whitebal(False, rgb_gain_db=(60.0, 45.0, 60.0))
    sensor.set_auto_gain(False)
    sensor.set_auto_exposure(False, exposure_us=500)

def classify_symbol(r, g, b):
    best_symbol = "#"
    best_cell = GRID_CELL_UNKNOWN
    min_dist = 1 << 30

    for symbol, cell_value, (ref_r, ref_g, ref_b) in COLOR_PROFILES:
        dist = (r - ref_r) * (r - ref_r)
        dist += (g - ref_g) * (g - ref_g)
        dist += (b - ref_b) * (b - ref_b)
        if dist < min_dist:
            min_dist = dist
            best_symbol = symbol
            best_cell = cell_value

    return best_symbol, best_cell


def get_gain(x, y):
    for zone_xmin, zone_xmax, zone_ymin, zone_ymax, zone_gain in CUSTOM_ZONES:
        if zone_xmin <= x <= zone_xmax and zone_ymin <= y <= zone_ymax:
            return zone_gain

    if x < MID_X and y < MID_Y:
        return GAIN_TOP_LEFT
    if x >= MID_X and y < MID_Y:
        return GAIN_TOP_RIGHT
    if x < MID_X and y >= MID_Y:
        return GAIN_BOTTOM_LEFT
    return GAIN_BOTTOM_RIGHT


def apply_gain(r, g, b, gain):
    if gain == 1.0:
        return r, g, b

    r = min(255, int(r * gain))
    g = min(255, int(g * gain))
    b = min(255, int(b * gain))
    return r, g, b


def draw_idle_overlay(img):
    for row in range(ROWS):
        for col in range(COLS):
            x, y = GRID_POINTS[row][col]
            if row == 0 or row == (ROWS - 1) or col == 0 or col == (COLS - 1):
                img.draw_circle(x, y, 2, color=(100, 100, 100), fill=True)
            else:
                img.draw_circle(x, y, 2, color=(255, 255, 255), fill=True)


def capture_map(img, last_car_state):
    map_values = bytearray(COLS * ROWS)
    map_rows = []
    car_x, car_y, car_dir = last_car_state
    car_found = False

    for row in range(ROWS):
        row_chars = []
        for col in range(COLS):
            x, y = GRID_POINTS[row][col]

            if row == 0 or row == (ROWS - 1) or col == 0 or col == (COLS - 1):
                symbol = "#"
                cell_value = GRID_CELL_WALL
                img.draw_circle(x, y, 2, color=(100, 100, 100), fill=True)
            else:
                r, g, b = img.get_pixel(x, y)
                r, g, b = apply_gain(r, g, b, get_gain(x, y))
                symbol, cell_value = classify_symbol(r, g, b)
                img.draw_circle(x, y, 2, color=(255, 255, 255), fill=True)

            if cell_value == GRID_CELL_CAR and not car_found:
                car_x = col
                car_y = row
                car_found = True

            map_values[row * COLS + col] = cell_value
            row_chars.append(symbol)

        map_rows.append(" ".join(row_chars))

    return map_values, map_rows, (car_x, car_y, car_dir), car_found


def build_frame(car_state, map_values):
    if len(map_values) != (COLS * ROWS):
        raise ValueError("map_values length must be 192")

    car_x, car_y, car_dir = car_state

    payload = bytearray(VISION_PAYLOAD_LEN)
    payload[0] = car_x
    payload[1] = car_y
    payload[2] = car_dir

    for index, value in enumerate(map_values):
        payload[index + 3] = value

    checksum = VISION_CMD_MAP ^ VISION_PAYLOAD_LEN
    for value in payload:
        checksum ^= value

    frame = bytearray(4 + VISION_PAYLOAD_LEN + 1)
    frame[0] = VISION_HEADER_0
    frame[1] = VISION_HEADER_1
    frame[2] = VISION_CMD_MAP
    frame[3] = VISION_PAYLOAD_LEN

    for index, value in enumerate(payload):
        frame[index + 4] = value

    frame[-1] = checksum
    return frame


def print_map(map_rows, car_state, car_found):
    print("\n====== vision_map frame ======")
    for row_text in map_rows:
        print(row_text)

    if car_found:
        print("car:", car_state[0], car_state[1], car_state[2])
    else:
        print("car: fallback", car_state[0], car_state[1], car_state[2])


def main():
    init_sensor()
    clock = time.clock()
    last_capture_time = time.ticks_ms()
    last_car_state = (DEFAULT_CAR_X, DEFAULT_CAR_Y, DEFAULT_CAR_DIR)

    while True:
        clock.tick()
        img = sensor.snapshot()
        current_time = time.ticks_ms()

        if time.ticks_diff(current_time, last_capture_time) >= VISION_SEND_PERIOD_MS:
            last_capture_time = current_time
            map_values, map_rows, last_car_state, car_found = capture_map(img, last_car_state)
            frame = build_frame(last_car_state, map_values)
            uart.write(frame)
            print_map(map_rows, last_car_state, car_found)
        else:
            draw_idle_overlay(img)

        time.sleep_ms(20)


if __name__ == "__main__":
    main()
