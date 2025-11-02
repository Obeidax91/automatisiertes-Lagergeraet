import os
import threading
import queue
import tkinter as tk
from tkinter import ttk, messagebox
import serial
import serial.tools.list_ports
from PIL import Image, ImageTk, ImageSequence

DEFAULT_BAUD = 250000

#################################################################################################################################
############## WICHTIG: Diese Version ist nur mit XAxisRamp2DUE kompatibel!                                   ###################
############## Mit diesem Code wird der Artikel eingegeben --> wird gehomt --> dann wird PICK_X <mm> gesendet ###################
############## Arduino bewegt sich zu Position, holt den Artikel und fährt wieder zu Home Position zurück.     ###################
############## Wenn aber während des Abholens HOME_X gedrückt wird,                                            ###################
############## wird erneut gehomt und der Artikel erneut gesucht.                                             ###################
#################################################################################################################################

# Regalnummer → X-Position in mm
REGAL_X_MM = {
    1: 10.0,
    2: 60.0,
    3: 120.0,
    4: 180.0,
    5: 240.0,
    6: 500.0,
    7: 2000.0,
}

# Artikel → Regalnummer
ITEM_TO_REGAL = {
    "M6x30": 3,
    "M8x20": 2,
    "M4x10": 4,
    "M4x20": 6,
    "M4x50": 7,
    "Schraube M4x20": 1,
    "M10 Mutter": 5,
}


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


class PickerGUI:
    def __init__(self, root):
        self.root = root
        self.root.title("Regal-Picker – Artikel-Suchsystem (Dark Mode)")
        self.root.geometry("780x560")
        self._apply_dark_theme()

        EDGE_PAD = 16

        container = ttk.Frame(root, padding=16)
        container.pack(fill="both", expand=True)

        # ===== HEADER =====
        header = ttk.Frame(container)
        header.pack(fill="x", pady=(0, 16), padx=(EDGE_PAD, EDGE_PAD))
        header.columnconfigure(0, weight=1)
        header.columnconfigure(1, weight=0)

        # GIF links
        self.anim_label = ttk.Label(header, background="#1E1E1E")
        self.anim_label.grid(row=0, column=0, sticky="nw", padx=(0, 8), rowspan=2)
        self.anim_frames, self.anim_durations = self._load_gif_frames()
        self.anim_running = False
        if self.anim_frames:
            self.anim_label.configure(image=self.anim_frames[0])

        # Titel
        self.title_label = ttk.Label(
            header,
            text="Regal-Picker – Artikel-Suchsystem",
            font=("Segoe UI", 15, "bold"),
            foreground="#E9F3EE",
            background="#1E1E1E"
        )
        self.title_label.grid(row=1, column=0, sticky="sw", pady=(2, 0))

        # Logo rechts
        self.logo_img = None
        script_dir = os.path.dirname(os.path.abspath(__file__))
        logo_path = os.path.join(script_dir, "assets", "logo.png")
        if os.path.exists(logo_path):
            img = Image.open(logo_path).resize((150, 70), Image.LANCZOS)
            self.logo_img = ImageTk.PhotoImage(img)
            self.logo_label = ttk.Label(header, image=self.logo_img, background="#1E1E1E")
            self.logo_label.grid(row=0, column=1, sticky="ne", padx=(8, 16), rowspan=2)

        # ===== CONNECTION =====
        conn_frame = ttk.Frame(container)
        conn_frame.pack(fill="x", pady=4, padx=(EDGE_PAD, EDGE_PAD))

        ttk.Label(conn_frame, text="Port:", background="#1E1E1E").pack(side="left", padx=(0, 6))
        self.port_var = tk.StringVar()
        self.port_combo = ttk.Combobox(conn_frame, textvariable=self.port_var, width=18, state="readonly")
        self.port_combo.pack(side="left", padx=(0, 12))

        ttk.Button(conn_frame, text="🔄 Neu", command=self.refresh_ports).pack(side="left", padx=(0, 6))
        ttk.Button(conn_frame, text="🔌 Verbinden", command=self.connect).pack(side="left", padx=(0, 6))
        ttk.Button(conn_frame, text="❌ Trennen", command=self.disconnect).pack(side="left", padx=(0, 6))

        self.refresh_ports()

        # ===== SEARCH + HOME + STOP (STOP ganz rechts) =====
        search_frame = ttk.LabelFrame(container, text="Artikel-Suche")
        search_frame.pack(fill="x", pady=10, padx=(EDGE_PAD, EDGE_PAD))

        # linke Seite: Suche + Hole Artikel + Home
        left_box = ttk.Frame(search_frame)
        left_box.pack(side="left", fill="x", expand=True)

        self.search_var = tk.StringVar()
        self.search_entry = tk.Entry(
            left_box,
            textvariable=self.search_var,
            width=32,
            bg="#2B2B2B",
            fg="#FFFFFF",
            insertbackground="#00FF88",
            relief="flat",
            highlightthickness=2,
            highlightbackground="#555555",
            highlightcolor="#00FF88",
            insertwidth=2
        )
        self.search_entry.pack(side="left", padx=6, pady=8, ipady=3)
        self.search_entry.bind("<Return>", self._on_enter_search)

        ttk.Button(left_box, text="Hole Artikel", command=self.pick_item).pack(side="left", padx=8)
        ttk.Button(left_box, text="🏠 HOME_X", command=self.home_x).pack(side="left")

        # Füller und STOP rechts alleine
        stop_box = ttk.Frame(search_frame)
        stop_box.pack(side="right", fill="y")
        self.stop_btn = tk.Button(
            stop_box,
            text="■  STOP",
            font=("Segoe UI", 12, "bold"),
            fg="#FFFFFF",
            bg="#C4102D",
            activebackground="#FF5252",
            activeforeground="#000000",
            relief="flat",
            padx=18, pady=10,
            command=self.stop_now
        )
        self.stop_btn.pack(side="right", padx=(8, 8), pady=(6, 6))

        # ===== STATUS =====
        status_frame = ttk.Frame(container)
        status_frame.pack(fill="x", pady=8, padx=(EDGE_PAD, EDGE_PAD))

        self.status = tk.StringVar(value="Getrennt")
        ttk.Label(status_frame, textvariable=self.status, foreground="#AAAAAA").pack(side="left")

        self.motion_state = tk.StringVar(value="Stillstand")
        self.motion_label = ttk.Label(status_frame, textvariable=self.motion_state, foreground="#00FF88")
        self.motion_label.pack(side="right")

        # ===== LOG =====
        logf = ttk.LabelFrame(container, text="Serielles Log")
        logf.pack(fill="both", expand=True, pady=8, padx=(EDGE_PAD, EDGE_PAD))

        self.log = tk.Text(
            logf, height=16, wrap="word", bg="#121212", fg="#D0D0D0",
            insertbackground="white", borderwidth=0, relief="flat"
        )
        self.log.pack(fill="both", expand=True, padx=4, pady=4)

        # ===== SERIAL HANDLING =====
        self.ser = None
        self.reader_thread = None
        self.reader_stop = threading.Event()
        self.rx_queue = queue.Queue()
        self.root.after(50, self._process_rx)

        self.pick_in_progress = False

    # === THEME ===
    def _apply_dark_theme(self):
        self.root.configure(bg="#1E1E1E")
        style = ttk.Style(self.root)
        style.theme_use("clam")
        style.configure("TFrame", background="#1E1E1E")
        style.configure("TLabel", background="#1E1E1E", foreground="#D0D0D0")
        style.configure("TLabelFrame", background="#1E1E1E", foreground="#00FF88")
        style.configure("TButton", background="#2B2B2B", foreground="#FFFFFF",
                        borderwidth=0, focusthickness=3, focuscolor="#00FF88", padding=6)
        style.map("TButton", background=[("active", "#00FF88")], foreground=[("active", "#000000")])

    # === GIF HANDLING ===
    def _load_gif_frames(self):
        script_dir = os.path.dirname(os.path.abspath(__file__))
        gif_path = os.path.join(script_dir, "assets", "gui_pick.gif")
        if not os.path.exists(gif_path):
            return [], []
        gif = Image.open(gif_path)
        frames = []
        durations = []
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

    def _show_gif(self):
        if self.anim_frames and not self.anim_running:
            self.anim_running = True
            self._start_gif()

    def _hide_gif(self):
        self.anim_running = False
        if self.anim_frames:
            self.anim_label.configure(image=self.anim_frames[0])

    # === COM PORT ===
    def refresh_ports(self):
        ports = [p.device for p in serial.tools.list_ports.comports()]
        self.port_combo["values"] = ports
        if ports and not self.port_var.get():
            self.port_var.set(ports[0] if ports else "")

    def connect(self):
        if self.ser and self.ser.is_open:
            self.disconnect()
        port = self.port_var.get()
        if not port:
            messagebox.showwarning("Hinweis", "Bitte COM-Port wählen.")
            return
        try:
            self.ser = serial.Serial(port, DEFAULT_BAUD, timeout=0.1, write_timeout=0.1)
            self.ser.reset_input_buffer()
            self.status.set(f"Verbunden: {port}")
            self._append_log(f"[INFO] Verbunden: {port}")

            self.reader_stop.clear()
            self.reader_thread = SerialReader(self.ser, self.rx_queue, self.reader_stop)
            self.reader_thread.start()
        except Exception as e:
            messagebox.showerror("Fehler", f"Konnte {port} nicht öffnen:\n{e}")
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
        self.motion_state.set("Stillstand")
        self._append_log("[INFO] Verbindung getrennt")
        self._hide_gif()
        self.pick_in_progress = False

    # === STOP ===
    def stop_now(self):
        self._send_line("STOP_X")
        self._hide_gif()
        self.pick_in_progress = False
        self.motion_state.set("Stillstand")

    # === SUCHE / STEUERUNG ===
    def _on_enter_search(self, event):
        self.pick_item()

    def pick_item(self):
        raw = self.search_var.get().strip()
        if not raw:
            messagebox.showinfo("Hinweis", "Bitte einen Artikelnamen eingeben.")
            return

        if raw not in ITEM_TO_REGAL:
            warn_txt = f"Artikel '{raw}' ist im Regalsystem nicht vorhanden!"
            messagebox.showwarning("Nicht gefunden", warn_txt)
            self._append_log(f"[WARN] {warn_txt}")
            self.motion_state.set("Stillstand")
            return

        regal = ITEM_TO_REGAL[raw]
        x_mm = REGAL_X_MM.get(regal)
        if x_mm is None:
            err_txt = f"Keine X-Position für Regal {regal} definiert!"
            messagebox.showerror("Fehler", err_txt)
            self._append_log(f"[ERROR] {err_txt}")
            return

        info_txt = f"[INFO] '{raw}' → Regal {regal} ({x_mm} mm)"
        self._append_log(info_txt)

        self.pick_in_progress = True
        self._show_gif()
        self._send_line(f"PICK_X {x_mm}")
        self.motion_state.set("Bewegt sich...")

    def home_x(self):
        self.pick_in_progress = False
        self._show_gif()
        self._send_line("HOME_X")
        self.motion_state.set("Bewegt sich...")

    # === SERIAL HANDLING ===
    def _send_line(self, line):
        if not self.ser or not self.ser.is_open:
            messagebox.showwarning("Hinweis", "Nicht verbunden.")
            return
        try:
            up = line.strip().upper()
            if up.startswith("PICK_X"):
                self.pick_in_progress = True
                self._show_gif()
            elif up.startswith("HOME_X"):
                self.pick_in_progress = False
                self._show_gif()
            elif up.startswith("STOP_X"):
                self.pick_in_progress = False

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

                if "stop cmd" in low or "stopped" in low:
                    self.pick_in_progress = False
                    self._hide_gif()
                    self.motion_state.set("Stillstand")
                    continue

                if "pick: start" in low:
                    self.pick_in_progress = True
                    self._show_gif()
                    continue

                if "pick: done" in low:
                    self.pick_in_progress = False
                    # ggf. Homing folgt → GIF noch nicht zwingend stoppen
                    continue

                if "homing: done" in low:
                    if not self.pick_in_progress:
                        self._hide_gif()
                        self.motion_state.set("Stillstand")
                    continue

                if "reached" in low:
                    if not self.pick_in_progress:
                        self.motion_state.set("Stillstand")
                        self._hide_gif()
                    continue

        except queue.Empty:
            pass
        self.root.after(80, self._process_rx)

    def _append_log(self, text):
        self.log.configure(state="normal")
        self.log.insert("end", text + "\n")
        self.log.see("end")
        self.log.configure(state="disabled")


if __name__ == "__main__":
    root = tk.Tk()
    PickerGUI(root)
    root.mainloop()
