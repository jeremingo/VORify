import sqlite3
import os

# Create or open SQLite database
conn = sqlite3.connect("offline_tiles.db")
cursor = conn.cursor()

# Create table if not exists
cursor.execute("""
    CREATE TABLE IF NOT EXISTS tiles (
        zoom INTEGER,
        x INTEGER,
        y INTEGER,
        server TEXT,
        tile_image BLOB,
        PRIMARY KEY (zoom, x, y)
    )
""")

# OSM tile URL pattern for metadata
TILE_URL = "https://a.tile.openstreetmap.org/{z}/{x}/{y}.png"

def load_tile_from_file(z, x, y):
    filename = f"{z}_{x}_{y}.png"
    if os.path.exists(filename):
        with open(filename, "rb") as f:
            return f.read()
    else:
        print(f"Missing file: {filename}")
        return None

def store_tile(z, x, y, data):
    cursor.execute("""
        INSERT OR REPLACE INTO tiles (zoom, x, y, server, tile_image)
        VALUES (?, ?, ?, ?, ?)
    """, (z, x, y, TILE_URL, data))

# Read all tiles from local folder and store in DB
for z in range(0, 5):  # zoom 0 to 4
    max_index = 2 ** z
    for x in range(max_index):
        for y in range(max_index):
            print(f"Processing tile z={z}, x={x}, y={y}...")
            data = load_tile_from_file(z, x, y)
            if data:
                store_tile(z, x, y, data)

# Save and close
conn.commit()
conn.close()
print("Done storing tiles into the database.")

