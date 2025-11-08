import os
import threading
import queue
import tkinter as tk
from tkinter import ttk, messagebox
import serial
import serial.tools.list_ports
from PIL import Image, ImageTk, ImageSequence

DEFAULT_BAUD = 250000

# ------------------- Regal- und Artikeldaten -------------------
REGAL_X_MM = {
    1: 10.0,
    2: 60.0,
    3: 120.0,
    4: 180.0,
    5: 240.0,
    6: 500.0,
    7: 2000.0,
}

ITEM_TO_REGAL = {
    "M6x30": 3,
    "M8x20": 2,
    "M4x10": 4,
    "M4x20": 6,
    "M4x50": 7,
    "Schraube M4x20": 1,
    "M10 Mutter": 5,
}

ALIASES = {
    "M4 20": "Schraube M4x20",
    "SCHRAUBE M4X20": "Schraube M4x20",
    "M10MUTTER": "M10 Mutter",
}

# ------------------- SerialReader Thread -------------------
class SerialReader(threading.Thread):
    def __init__(self, ser, out_queue, stop_event):
        super().__init__(daemon=True)
        self.ser = ser
        self.out_queue = out_queue
        self.stop_event = stop_event

    def run(self):
        buf = b""
        while not self.stop_event.is_set():
            try:
                b = self.ser.read(1)
                if not b:
                    continue
                if b in (b"\n", b"\r"):
                    line = buf.decode(errors="ignore").strip()
                    buf = b""
                    if line:
                        self.out_queue.put(line)
                else:
                    buf += b
            except Exception as e:
                self.out_queue.put(f"[SERIAL ERROR] {e}")
                break

# ------------------- AutoComplete Combobox -------------------
class AutoCompleteCombobox(ttk.Combobox):
    def set_completion_list(self, completion_list):
        self._completion_list = sorted(completion_list, key=str.lower)
        self.bind("<KeyRelease>", self._on_keyrelease)

    def _on_keyrelease(self, event):
        value = self.get().strip().lower()
        if not value:
            self["values"] = self._completion_list
            return
        matches = [item for item in self._completion_list if value in item.lower()]
        self["values"] = matches
        if matches:
            self.event_generate("<Down>")  # Vorschläge aufklappen

# ------------------- Hauptklasse GUI -------------------
class PickerGUI:
    def __init__(self, root):
        self.root = root
        self.root.title("Regal-Picker – Artikel-Suchsystem")
        self.root.geometry("780x560")
        self._apply_dark_theme()

        self.ser = None
        self.reader_thread = None
        self.reader_stop = threading.Event()
        self.rx_queue = queue.Queue()
        self.motion_active = False

        EDGE_PAD = 16
        container = ttk.Frame(root, padding=16)
        container.pack(fill="both", expand=True)

        # Header
        header = ttk.Frame(container)
        header.pack(fill="x", pady=(0, 16), padx=(EDGE_PAD, EDGE_PAD))
        header.columnconfigure(0, weight=1)

        self.anim_label = ttk.Label(header, background="#1E1E1E")
        self.anim_label.grid(row=0, column=0, sticky="nw", padx=(0, 8), rowspan=2)
        self.anim_frames, self.anim_durations = self._load_gif_frames()
        self.anim_running = False
        if self.anim_frames:
            self.anim_label.configure(image=self.anim_frames[0])

        ttk.Label(
            header,
            text="Regal-Picker – Artikel-Suchsystem",
            font=("Segoe UI", 15, "bold"),
            foreground="#E9F3EE",
            background="#1E1E1E",
        ).grid(row=1, column=0, sticky="sw", pady=(2, 0))

        # Connection
        conn_frame = ttk.Frame(container)
        conn_frame.pack(fill="x", pady=4, padx=(EDGE_PAD, EDGE_PAD))
        ttk.Label(conn_frame, text="Port:", background="#1E1E1E").pack(side="left")
        self.port_var = tk.StringVar()
        self.port_combo = ttk.Combobox(conn_frame, textvariable=self.port_var, width=18, state="readonly")
        self.port_combo.pack(side="left", padx=(0, 12))
        ttk.Button(conn_frame, text="🔄 Neu", command=self.refresh_ports).pack(side="left")
        ttk.Button(conn_frame, text="🔌 Verbinden", command=self.connect).pack(side="left", padx=(6, 6))
        ttk.Button(conn_frame, text="❌ Trennen", command=self.disconnect).pack(side="left", padx=(6, 6))
        self.refresh_ports()

        # Search system
        search_frame = ttk.LabelFrame(container, text="Artikel-Suche")
        search_frame.pack(fill="x", pady=10, padx=(EDGE_PAD, EDGE_PAD))

        left_box = ttk.Frame(search_frame)
        left_box.pack(side="left", fill="x", expand=True)

        self.search_var = tk.StringVar()
        self.search_combo = AutoCompleteCombobox(
            left_box, textvariable=self.search_var, width=32, state="normal"
        )
        self.search_combo.set_completion_list(list(ITEM_TO_REGAL.keys()))
        self.search_combo.pack(side="left", padx=6, pady=8, ipady=3)
        self.search_combo.bind("<Return>", self._on_enter_search)

        self.pick_btn = ttk.Button(left_box, text="Hole Artikel", command=self.pick_item)
        self.pick_btn.pack(side="left", padx=8)
        self.home_btn = ttk.Button(left_box, text="🏠 HOME_X", command=self.home_x)
        self.home_btn.pack(side="left")

        stop_box = ttk.Frame(search_frame)
        stop_box.pack(side="right", fill="y")
        self.stop_btn = tk.Button(
            stop_box,
            text="■ STOP",
            font=("Segoe UI", 12, "bold"),
            fg="#FFFFFF",
            bg="#C4102D",
            activebackground="#FF5252",
            activeforeground="#000000",
            relief="flat",
            padx=18,
            pady=10,
            command=self.stop_now,
        )
        self.stop_btn.pack(side="right", padx=8, pady=6)

        # Status + Log
        status_frame = ttk.Frame(container)
        status_frame.pack(fill="x", pady=8, padx=(EDGE_PAD, EDGE_PAD))
        self.status = tk.StringVar(value="Getrennt")
        ttk.Label(status_frame, textvariable=self.status).pack(side="left")
        self.motion_state = tk.StringVar(value="Stillstand")
        ttk.Label(status_frame, textvariable=self.motion_state, foreground="#00FF88").pack(side="right")

        logf = ttk.LabelFrame(container, text="Serielles Log")
        logf.pack(fill="both", expand=True, pady=8, padx=(EDGE_PAD, EDGE_PAD))
        self.log = tk.Text(
            logf, height=16, wrap="word", bg="#121212", fg="#D0D0D0", insertbackground="white", relief="flat"
        )
        self.log.pack(fill="both", expand=True, padx=4, pady=4)

        self.root.after(50, self._process_rx)

    # Theme
    def _apply_dark_theme(self):
        self.root.configure(bg="#1E1E1E")
        style = ttk.Style(self.root)
        style.theme_use("clam")
        style.configure("TFrame", background="#1E1E1E")
        style.configure("TLabel", background="#1E1E1E", foreground="#D0D0D0")
        style.configure("TLabelFrame", background="#1E1E1E", foreground="#00FF88")
        style.configure("TButton", background="#2B2B2B", foreground="#FFFFFF", padding=6)
        style.map("TButton", background=[("active", "#00FF88")], foreground=[("active", "#000000")])

    # GIF handling
    def _load_gif_frames(self):
        script_dir = os.path.dirname(os.path.abspath(__file__))
        gif_path = os.path.join(script_dir, "assets", "gui_pick.gif")
        if not os.path.exists(gif_path):
            return [], []
        gif = Image.open(gif_path)
        frames, durations = [], []
        for frame in ImageSequence.Iterator(gif):
            fr = frame.convert("RGBA").resize((120, 70), Image.LANCZOS)
            frames.append(ImageTk.PhotoImage(fr))
            durations.append(frame.info.get("duration", 80))
        return frames, durations

    def _start_gif(self, index=0):
        if not self.anim_frames or not self.anim_label:
            return
        self.anim_label.configure(image=self.anim_frames[index])
        next_i = (index + 1) % len(self.anim_frames)
        delay = self.anim_durations[index] if index < len(self.anim_durations) else 80
        if self.anim_running:
            self.root.after(delay, self._start_gif, next_i)

    def _gif_on(self):
        if self.anim_frames and not self.anim_running:
            self.anim_running = True
            self._start_gif()
        self.motion_state.set("Bewegt sich...")

    def _gif_off(self):
        self.anim_running = False
        if self.anim_frames:
            self.anim_label.configure(image=self.anim_frames[0])
        self.motion_state.set("Stillstand")

    # Serial connection
    def refresh_ports(self):
        ports = [p.device for p in serial.tools.list_ports.comports()]
        self.port_combo["values"] = ports
        if ports and not self.port_var.get():
            self.port_var.set(ports[0])

    def connect(self):
        port = self.port_var.get()
        if not port:
            messagebox.showwarning("Hinweis", "Bitte COM-Port wählen.")
            return
        try:
            self.ser = serial.Serial(port, DEFAULT_BAUD, timeout=0.1, write_timeout=0.1)
            self.status.set(f"Verbunden: {port}")
            self._append_log(f"[INFO] Verbunden: {port}")
            self.reader_stop.clear()
            self.reader_thread = SerialReader(self.ser, self.rx_queue, self.reader_stop)
            self.reader_thread.start()
        except Exception as e:
            messagebox.showerror("Fehler", f"Port konnte nicht geöffnet werden:\n{e}")
            self.status.set("Getrennt")

    def disconnect(self):
        if self.reader_thread:
            self.reader_stop.set()
            self.reader_thread.join(timeout=1)
            self.reader_thread = None
        if self.ser:
            try:
                self.ser.close()
            except Exception:
                pass
            self.ser = None
        self.status.set("Getrennt")
        self.motion_active = False
        self._gif_off()
        self._append_log("[INFO] Verbindung getrennt")

    # Commands
    def stop_now(self):
        self._send_line("STOP_X")
        self.motion_active = False
        self._gif_off()

    def _on_enter_search(self, event):
        self.pick_item()

    def pick_item(self):
        if not self._is_connected():
            return
        raw = self.search_var.get().strip()
        if not raw:
            messagebox.showinfo("Hinweis", "Bitte einen Artikelnamen eingeben.")
            return
        key = raw.upper()
        if key in ALIASES:
            raw = ALIASES[key]
        else:
            for k in ITEM_TO_REGAL:
                if k.lower() == key.lower():
                    raw = k
                    break
            else:
                messagebox.showwarning("Nicht gefunden", f"Artikel '{raw}' nicht im System!")
                return
        regal = ITEM_TO_REGAL[raw]
        x_mm = REGAL_X_MM.get(regal)
        if x_mm is None:
            messagebox.showerror("Fehler", f"Keine X-Position für Regal {regal}!")
            return
        self._append_log(f"[INFO] '{raw}' → Regal {regal} ({x_mm} mm)")
        self._send_line(f"PICK_X {x_mm}")   # Start → GIF an via _send_line

    def home_x(self):
        if not self._is_connected():
            return
        self._send_line("HOME_X")           # Start → GIF an via _send_line

    def _is_connected(self):
        if not self.ser or not self.ser.is_open:
            messagebox.showwarning("Hinweis", "Nicht verbunden.")
            return False
        return True

    # Serial send / receive
    def _send_line(self, line):
        if not self._is_connected():
            return
        try:
            up = line.strip().upper()
            # Start GIF bei allen Bewegungsbefehlen
            if up.startswith(("PICK_X", "HOME_X", "GOTO_X")):
                self.motion_active = True
                self._gif_on()
            elif up.startswith("STOP_X"):
                self.motion_active = False
                self._gif_off()

            self.ser.write((line.strip() + "\n").encode("utf-8"))
            self.ser.flush()
            self._append_log(f">> {line}")
        except Exception as e:
            self._append_log(f"[ERROR] Senden fehlgeschlagen: {e}")

    def _process_rx(self):
        try:
            while True:
                line = self.rx_queue.get_nowait()
                self._append_log(f"<< {line}")
                low = line.lower()

                # *** Bewegung startet? → GIF an ***
                if ("homing: start" in low) or ("pick: start" in low) or ("goto_x ->" in low):
                    self.motion_active = True
                    self._gif_on()
                    continue

                # *** Bewegung beendet? → GIF sofort aus ***
                if (
                    "reached" in low
                    or "stopped" in low
                    or "stop cmd" in low
                    or "homing: done" in low
                    or "pick: done" in low
                ):
                    self.motion_active = False
                    self._gif_off()
                    continue
        except queue.Empty:
            pass
        self.root.after(80, self._process_rx)

    def _append_log(self, text):
        self.log.insert("end", text + "\n")
        self.log.see("end")

if __name__ == "__main__":
    root = tk.Tk()
    PickerGUI(root)
    root.mainloop()
