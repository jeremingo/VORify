import tkinter as tk
from tkinter import ttk, simpledialog, messagebox
import sys
import json
import threading
import tkinter.font as tkFont

class VORApp:
    def __init__(self, root):
        self.root = root
        self.vor_data = []
        self.origin_vor = None
        self.current_location = None

        self.root.title("VOR Station Entries Table")
        self.root.attributes("-fullscreen", True)
        self.root.bind("<Escape>", lambda e: self.root.attributes("-fullscreen", False))

        self.create_widgets()
        threading.Thread(target=self.listen_for_input, daemon=True).start()

    def create_widgets(self):

        self.location_label = ttk.Label(self.root, text="No known location", font=(None, 16))
        self.location_label.pack(pady=5)

        self.change_button = ttk.Button(self.root, text="Highlight by ID", command=self.change_origin)
        self.change_button.pack(pady=5)

        frame = ttk.Frame(self.root)
        frame.pack(fill=tk.BOTH, expand=True)

        vsb = ttk.Scrollbar(frame, orient="vertical")
        hsb = ttk.Scrollbar(frame, orient="horizontal")

        columns = ("Name", "Ident", "Frequency", "Bearing", "Distance")
        self.tree = ttk.Treeview(frame, columns=columns, show="headings",
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
        # Update location label
        if self.current_location and "lat" in self.current_location and "lon" in self.current_location:
            lat = self.current_location["lat"]
            lon = self.current_location["lon"]
            self.location_label.config(text=f"Lat: {lat:.6f} Lon: {lon:.6f}")
        else:
            self.location_label.config(text="No known location")

        # Update VOR table
        self.tree.delete(*self.tree.get_children())
        for row in self.vor_data:
            tag = "origin" if self.origin_vor and row[1] == self.origin_vor else ""
            self.tree.insert("", tk.END, values=row, tags=(tag,))
        self.tree.tag_configure("origin", background="#d1e7dd")  # Light green

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
                break  # EOF
            buffer += line
            try:
                json.loads(buffer)
                self.update_data(buffer)
                buffer = ""
            except json.JSONDecodeError:
                continue

    def change_origin(self):
        search = simpledialog.askstring("Highlight Entry", "Enter Station ID to highlight:")
        if search:
            found = next((vor for vor in self.vor_data if search.lower() in vor[1].lower()), None)
            if found:
                self.origin_vor = found[1]
                self.populate_table()
            else:
                messagebox.showinfo("Not Found", f"No station found for: {search}")

# Run the app
if __name__ == "__main__":
    root = tk.Tk()
    app = VORApp(root)
    root.mainloop()

