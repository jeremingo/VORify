import pyautogui
import time
import webbrowser

# Time delays (tweak these if needed)
LOAD_DELAY = 4      # time to wait for browser to load the tile
SAVE_DELAY = 1      # time between save steps
AFTER_SAVE_DELAY = 3  # wait after saving before continuing

def download_tile(z, x, y):
    url = f"https://tile.openstreetmap.org/{z}/{x}/{y}.png"
    filename = f"{z}_{x}_{y}.png"

    print(f"Downloading tile {filename}...")

    # Open the tile in the browser
    webbrowser.open(url)
    time.sleep(LOAD_DELAY)

    # Save using Ctrl+S and type filename
    pyautogui.hotkey('ctrl', 's')  # use 'command' on Mac
    time.sleep(SAVE_DELAY)

    pyautogui.typewrite(filename)
    time.sleep(SAVE_DELAY)

    pyautogui.press('enter')
    time.sleep(AFTER_SAVE_DELAY)

time.sleep(10)

# Loop through zoom levels 0 to 4
for z in range(0, 5):
    max_xy = 2 ** z
    for x in range(max_xy):
        for y in range(max_xy):
            download_tile(z, x, y)

print("Done downloading all tiles.")

