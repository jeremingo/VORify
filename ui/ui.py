import tkinter as tk
from tkinter import ttk, simpledialog, messagebox
import sys
import json
import threading
import tkinter.font as tkFont
from tkintermapview import TkinterMapView
import os

# path for the database to use
script_directory = os.path.dirname(os.path.abspath(__file__))
database_path = os.path.join(script_directory, "offline_tiles.db")


class VORApp:
    def __init__(self, root):
        self.root = root
        self.vor_data = []
        self.current_location = None
        self.origin_location = None

        self.root.title("VOR Station Entries Table")
        self.root.attributes("-fullscreen", True)
        self.root.bind("<Escape>", lambda e: self.root.attributes("-fullscreen", False))

        self.create_widgets()
        threading.Thread(target=self.listen_for_input, daemon=True).start()

    def create_widgets(self):
        self.location_label = ttk.Label(self.root, text="No known location", font=(None, 16))
        self.location_label.pack(pady=5)

        self.origin_location_label = ttk.Label(self.root, text="No origin location", font=(None, 16))
        self.origin_location_label.pack(pady=5)

        self.map_button = ttk.Button(self.root, text="Pick Origin from Map", command=self.open_map_picker)
        self.map_button.pack(pady=5)

        self.frame = ttk.Frame(self.root)
        self.frame.pack(fill=tk.BOTH, expand=True)

        vsb = ttk.Scrollbar(self.frame, orient="vertical")
        hsb = ttk.Scrollbar(self.frame, orient="horizontal")

        columns = ("Name", "Ident", "Frequency", "Bearing", "Distance")
        self.tree = ttk.Treeview(self.frame, columns=columns, show="headings",
                                 yscrollcommand=vsb.set, xscrollcommand=hsb.set)

        style = ttk.Style()
        style.configure("Treeview", font=(None, 30), rowheight=50)
        style.configure("Treeview.Heading", font=(None, 10, 'bold'))

        for col in columns:
            self.tree.heading(col, text=col)
            self.tree.column(col, anchor="center", width=120)

        vsb.config(command=self.tree.yview)
        hsb.config(command=self.tree.xview)
        vsb.pack(side="right", fill="y")
        hsb.pack(side="bottom", fill="x")
        self.tree.pack(fill=tk.BOTH, expand=True)

    def populate_table(self):
        if self.current_location and "lat" in self.current_location and "lon" in self.current_location:
            lat = self.current_location["lat"]
            lon = self.current_location["lon"]
            self.location_label.config(text=f"Lat: {lat:.6f} Lon: {lon:.6f}")
        else:
            self.location_label.config(text="No known location")

        self.tree.delete(*self.tree.get_children())
        for row in self.vor_data:
            self.tree.insert("", tk.END, values=row)

    def update_data(self, json_data):
        try:
            parsed = json.loads(json_data)
            self.current_location = parsed.get("location")
            stations = parsed.get("stations", [])

            self.vor_data = [
                (
                    item.get("name", ""),
                    item.get("id", ""),
                    item.get("frequency", ""),
                    item.get("bearing", {}).get("value", "") if item.get("bearing") else "",
                    item.get("distance", "") if item.get("distance") else ""
                )
                for item in stations
            ]
            self.root.after(0, self.populate_table)
        except Exception as e:
            print(f"[Error parsing JSON]: {e}", file=sys.stderr)

    def listen_for_input(self):
        buffer = ""
        while True:
            line = sys.stdin.readline()
            if not line:
                break
            buffer += line
            try:
                json.loads(buffer)
                self.update_data(buffer)
                buffer = ""
            except json.JSONDecodeError:
                continue

    def open_map_picker(self):
        # Hide main UI widgets
        self.location_label.pack_forget()
        self.origin_location_label.pack_forget()
        self.map_button.pack_forget()
        self.frame.pack_forget()
        self.tree.pack_forget()

        # Create a frame for map view and controls
        self.map_frame = ttk.Frame(self.root)
        self.map_frame.pack(fill=tk.BOTH, expand=True)

        control_frame = ttk.Frame(self.map_frame)
        control_frame.pack(pady=5)

        map_widget = TkinterMapView(
            self.map_frame,
            corner_radius=0,
            database_path=database_path,
            use_database_only=True,
            max_zoom=6
        )
        map_widget.pack(fill="both", expand=True)
        map_widget.canvas.delete("button")

        map_widget.set_position(20, 0)
        map_widget.set_zoom(0)

        def release(event):
            coordinate_mouse_pos = map_widget.convert_canvas_coords_to_decimal_coords(*map_widget.mouse_click_position)
            map_widget.map_click_callback(coordinate_mouse_pos)

        map_widget.canvas.unbind("<B1-Motion>")
        map_widget.canvas.bind("<ButtonRelease-1>", release)

        def move(dx=0, dy=0):
            dx = dx * 10 / (map_widget.zoom - 1)
            dy = dy * 10 / (map_widget.zoom - 1)
            lat, lon = map_widget.get_position()
            map_widget.set_position(lat + dy, lon + dx)

        def set_zoom(zoom):
            map_widget.set_zoom(zoom)

        ttk.Button(control_frame, text="←", width=3, command=lambda: move(dx=-1)).pack(side=tk.LEFT, padx=2)
        ttk.Button(control_frame, text="→", width=3, command=lambda: move(dx=1)).pack(side=tk.LEFT, padx=2)
        ttk.Button(control_frame, text="↑", width=3, command=lambda: move(dy=1)).pack(side=tk.LEFT, padx=2)
        ttk.Button(control_frame, text="↓", width=3, command=lambda: move(dy=-1)).pack(side=tk.LEFT, padx=2)

        ttk.Button(control_frame, text="+", width=3, command=lambda: set_zoom(map_widget.zoom + 1)).pack(side=tk.LEFT, padx=10)
        ttk.Button(control_frame, text="-", width=3, command=lambda: set_zoom(map_widget.zoom - 1)).pack(side=tk.LEFT)

        def on_map_click(coord):
            lat, lon = coord
            self.origin_location_label.config(text=f"Origin Lat: {lat:.6f} Lon: {lon:.6f}")
            self.origin_location = {"lat": lat, "lon": lon}

            print(f"{lat} {lon}", flush=True)

            self.update_data("{\"location\": null, \"stations\": []}")

            # Hide map UI and show main UI
            self.map_frame.destroy()
            self.location_label.pack(pady=5)
            self.origin_location_label.pack(pady=5)
            self.map_button.pack(pady=5)
            self.frame.pack(fill=tk.BOTH, expand=True)
            self.tree.pack(fill=tk.BOTH, expand=True)

        map_widget.add_left_click_map_command(on_map_click)


# Run the app
if __name__ == "__main__":
    root = tk.Tk()
    app = VORApp(root)
    root.mainloop()

