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
        self.location_history = []
        self.origin_location = None
        self.show_marks = False
        self.mark = None
        self.path = None
        self.had_location = False
        self.stage = 0
        self.flashing_state = {}

        self.root.title("VOR Station Entries Table")
        self.root.attributes("-fullscreen", True)
        self.root.bind("<Escape>", lambda e: self.root.attributes("-fullscreen", not self.root.attributes("-fullscreen")))

        self.create_widgets()
        threading.Thread(target=self.listen_for_input, daemon=True).start()

        # Create a frame for map view and controls
        self.map_frame = ttk.Frame(self.root)


        map_and_label = ttk.Frame(self.map_frame)

        self.map_instruction_label = ttk.Label(
            map_and_label,
            font=(None, 18)
        )
        self.map_instruction_label.pack(side=tk.TOP)

        control_frame = ttk.Frame(self.map_frame)
        control_frame.pack(side=tk.RIGHT, pady=5)

        self.map_widget = TkinterMapView(
            map_and_label,
            corner_radius=0,
            database_path=database_path,
            use_database_only=True,
            max_zoom=7
        )
        self.map_widget.pack(side=tk.BOTTOM, fill="both", expand=True)
        self.map_widget.canvas.delete("button")
        map_and_label.pack(side=tk.LEFT, fill="both", expand=True)

        self.map_widget.canvas.unbind("<B1-Motion>")
        self.map_widget.canvas.unbind("<ButtonRelease-1>")

        def move(dx=0, dy=0):
            dx = dx * 10 / (self.map_widget.zoom - 1)
            dy = dy * 10 / (self.map_widget.zoom - 1)
            lat, lon = self.map_widget.get_position()
            self.map_widget.set_position(lat + dy, lon + dx)

        def set_zoom(zoom):
            self.map_widget.set_zoom(zoom)

        btn_font = tkFont.Font(size=27, weight="bold")  # bigger font
        style = ttk.Style()
        style.configure("Big.TButton", font=btn_font)
        style.map("Big.TButton", background=[("!disabled", "active", "pressed", "lightgrey")])
        style.configure("Flash.TButton", font=btn_font, background="white")
        style.map("Flash.TButton", background=[("!disabled", "active", "pressed", "white")])


        ttk.Button(control_frame, text="X", style="Big.TButton", width=3, command=lambda: self.exit_map()).pack(side=tk.TOP, pady=6, padx=5)
        ttk.Button(control_frame, text="←", style="Big.TButton", width=3, command=lambda: move(dx=-1)).pack(side=tk.TOP, pady=6, padx=5)
        ttk.Button(control_frame, text="→", style="Big.TButton", width=3, command=lambda: move(dx=1)).pack(side=tk.TOP, pady=6, padx=5)
        ttk.Button(control_frame, text="↑", style="Big.TButton", width=3, command=lambda: move(dy=1)).pack(side=tk.TOP, pady=6, padx=5)
        ttk.Button(control_frame, text="↓", style="Big.TButton", width=3, command=lambda: move(dy=-1)).pack(side=tk.TOP, pady=6, padx=5)

        ttk.Button(control_frame, text="+", style="Big.TButton", width=3, command=lambda: set_zoom(self.map_widget.zoom + 1)).pack(side=tk.TOP, pady=6, padx=5)
        ttk.Button(control_frame, text="-", style="Big.TButton", width=3, command=lambda: set_zoom(self.map_widget.zoom - 1)).pack(side=tk.TOP, pady=6, padx=5)

        self.open_map_picker()
        self.start_flashing(self.change_origin_button)


    def do_flash(self, btn):
        """Toggle button style while flashing is active."""
        state = self.flashing_state.get(btn)
        if state and state["flashing"]:
            current = btn.cget("style")
            new_style = "Flash.TButton" if current == "Big.TButton" else "Big.TButton"
            btn.config(style=new_style)
            state["job"] = root.after(800, self.do_flash, btn)  # schedule next flash

    def start_flashing(self, btn):
        """Begin flashing a specific button."""
        if btn not in self.flashing_state or not self.flashing_state[btn]["flashing"]:
            self.flashing_state[btn] = {"flashing": True, "job": None}
            self.do_flash(btn)

    def stop_flashing(self, btn):
        """Stop flashing a specific button and reset to normal style."""
        state = self.flashing_state.get(btn)
        if state:
            state["flashing"] = False
            if state["job"]:
                self.root.after_cancel(state["job"])
                state["job"] = None
            btn.config(style="Big.TButton")

    def create_widgets(self):
        self.main_frame = ttk.Frame(self.root)

        header = ttk.Frame(self.main_frame)
        self.location_label = ttk.Label(header, text="Pick origin location to start search", font=(None, 16))
        self.location_label.pack(side=tk.LEFT, fill=tk.BOTH, expand=True, padx=10)

        buttons = ttk.Frame(header)
        header.pack(fill=tk.X)

        self.current_location_button = ttk.Button(buttons, text="V", style="Big.TButton", width=3, command=self.open_map_view)
        self.current_location_button.pack(side=tk.RIGHT, pady=6, padx=5)
        self.change_origin_button = ttk.Button(buttons, text="P", style="Big.TButton", width=3, command=self.open_map_picker)
        self.change_origin_button.pack(side=tk.RIGHT, pady=6, padx=5)
        buttons.pack(side=tk.RIGHT)
        self.frame = ttk.Frame(self.main_frame)
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
        self.main_frame.pack(fill=tk.BOTH, expand=True)

    def populate_table(self):
        if self.current_location and "lat" in self.current_location and "lon" in self.current_location:
            lat = self.current_location["lat"]
            lon = self.current_location["lon"]
            self.location_label.config(text=f"Calculated Location - Lat: {lat:.4f} Lon: {lon:.4f}")
            if not self.stage == 2:
              self.start_flashing(self.current_location_button)
              self.stage = 2
        else:
            self.stop_flashing(self.current_location_button)
            if self.origin_location is not None:
                if self.stage == 2:
                    self.location_label.config(text="Lost location. Choose origin again")
                    self.start_flashing(self.change_origin_button)
                else:
                    lat = self.origin_location["lat"]
                    lon = self.origin_location["lon"]
                    self.location_label.config(text=f"Searching from - Lat: {lat:.4f} Lon: {lon:.4f}")
            
            if len(self.vor_data) <= 1:
                self.location_label.config(text="Not enough stations in range. Choose origin again")
                self.start_flashing(self.change_origin_button)


        self.tree.delete(*self.tree.get_children())
        for row in self.vor_data:
            self.tree.insert("", tk.END, values=row)

    def empty_data(self):
        lat = self.origin_location["lat"]
        lon = self.origin_location["lon"]
        self.location_label.config(text=f"Loading stations around - Lat: {lat:.4f} Lon: {lon:.4f}")
            
        self.tree.delete(*self.tree.get_children())

    def update_data(self, json_data):
        try:
            parsed = json.loads(json_data)
            self.current_location = parsed.get("location")
            if self.current_location is not None:
              self.location_history.append((self.current_location["lat"], self.current_location["lon"]))
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
            if self.show_marks:
              self.update_marks()
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

    def exit_map(self):

        # Hide map UI and show main UI
        self.map_frame.pack_forget()
        self.main_frame.pack(fill=tk.BOTH, expand=True)
        self.show_marks = False
        if self.mark is not None:
            self.mark.delete()
            self.mark = None
        if self.path is not None:
            self.path.delete()
            self.path = None

    def open_map(self):
        # Hide main UI widgets
        self.main_frame.pack_forget()

        self.map_frame.pack(fill=tk.BOTH, expand=True)


    def open_map_picker(self):
        self.open_map()
        self.map_instruction_label.config(text="Tap to pick location to search from:")

        self.map_widget.set_position(20, 0)
        self.map_widget.set_zoom(0)

        def release(event):
            coordinate_mouse_pos = self.map_widget.convert_canvas_coords_to_decimal_coords(*self.map_widget.mouse_click_position)
            self.map_widget.map_click_callback(coordinate_mouse_pos)

        self.map_widget.canvas.bind("<ButtonRelease-1>", release)

        def on_map_click(coord):
            lat, lon = coord
            self.location_label.config(text=f"Searching from - Lat: {lat:.6f} Lon: {lon:.6f}")
            self.origin_location = {"lat": lat, "lon": lon}

            print(f"{lat} {lon}", flush=True)

            self.empty_data()
            self.location_history = []
            self.stage = 1
            self.stop_flashing(self.current_location_button)
            self.stop_flashing(self.change_origin_button)

            self.exit_map()

        self.map_widget.add_left_click_map_command(on_map_click)

    def update_marks(self):
        if self.current_location is not None:
            lat = self.current_location["lat"]
            lon = self.current_location["lon"]
            if not self.had_location:
                self.map_widget.set_zoom(7)
                self.map_widget.set_position(lat, lon)
                self.had_location = True

            if self.mark is not None:
                self.mark.set_position(lat, lon)
            else:
                self.mark = self.map_widget.set_marker(lat, lon)
            if len(self.location_history) > 1:
                if self.path is not None:
                    self.path.set_position_list(self.location_history)
                else:
                    self.path = self.map_widget.set_path(self.location_history, width=3)
        elif self.had_location:
          self.had_location = False
          self.map_widget.set_zoom(0)
          self.map_widget.set_position(20, 0)

    def open_map_view(self):
        self.show_marks = True
        self.open_map()
        self.map_instruction_label.config(text="Current location and path:")

        self.map_widget.canvas.unbind("<ButtonRelease-1>")

        if self.current_location is None:
            self.map_widget.set_zoom(0)
            self.map_widget.set_position(20, 0)

        self.update_marks()




# Run the app
if __name__ == "__main__":
    root = tk.Tk()
    app = VORApp(root)
    root.mainloop()

