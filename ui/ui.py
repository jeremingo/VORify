import tkinter as tk
from tkinter import ttk, simpledialog, messagebox
import csv
import os

def load_vor_data_from_csv(filename):
  data = []
  try:
    BASE_DIR = os.path.dirname(os.path.abspath(__file__))
    filename = os.path.join(BASE_DIR, filename)
    with open(filename, newline='', encoding='utf-8') as csvfile:
      reader = csv.DictReader(csvfile)
      for row in reader:
        # Extract required columns and handle missing values
        data.append((
          row.get("id", ""),
          row.get("name", ""),
          row.get("freq", ""),
          row.get("lat", ""),
          row.get("lon", "")
        ))
  except Exception as e:
    messagebox.showerror("Error", f"Failed to load VOR data: {e}")
  return data

class VORApp:
  def __init__(self, root, data):
    self.root = root
    self.vor_data = data
    self.origin_vor = None

    self.root.title("VOR Stations Table")
    self.root.geometry("700x350")

    self.create_widgets()

  def create_widgets(self):
    # Button to change origin
    self.change_button = ttk.Button(self.root, text="Change Origin VOR", command=self.change_origin)
    self.change_button.pack(pady=5)

    # Frame for the table and scrollbars
    frame = ttk.Frame(self.root)
    frame.pack(fill=tk.BOTH, expand=True)

    # Scrollbars
    vsb = ttk.Scrollbar(frame, orient="vertical")
    hsb = ttk.Scrollbar(frame, orient="horizontal")

    # Treeview (table)
    columns = ("Ident", "Name", "Frequency", "Latitude", "Longitude")
    self.tree = ttk.Treeview(frame, columns=columns, show="headings",
                 yscrollcommand=vsb.set, xscrollcommand=hsb.set)

    for col in columns:
      self.tree.heading(col, text=col)
      self.tree.column(col, anchor="center", width=100)

    vsb.config(command=self.tree.yview)
    hsb.config(command=self.tree.xview)
    vsb.pack(side="right", fill="y")
    hsb.pack(side="bottom", fill="x")
    self.tree.pack(fill=tk.BOTH, expand=True)

    # Insert initial data
    self.populate_table()

  def populate_table(self):
    self.tree.delete(*self.tree.get_children())
    for row in self.vor_data:
      tag = "origin" if self.origin_vor and row[0] == self.origin_vor else ""
      self.tree.insert("", tk.END, values=row, tags=(tag,))
    self.tree.tag_configure("origin", background="#d1e7dd")  # Light green

  def change_origin(self):
    # Prompt user for VOR Ident or Name
    search = simpledialog.askstring("Search VOR", "Enter VOR Ident or Name:")
    if search:
      found = None
      for vor in self.vor_data:
        if search.lower() in vor[0].lower() or search.lower() in vor[1].lower():
          found = vor
          break
      if found:
        self.origin_vor = found[0]
        self.populate_table()
      else:
        messagebox.showinfo("Not Found", f"No VOR station found for: {search}")

# Run the app
if __name__ == "__main__":
  vor_data = load_vor_data_from_csv("../VOR.CSV")
  root = tk.Tk()
  app = VORApp(root, vor_data)
  root.mainloop()