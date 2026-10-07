"""
7-Segment Display Controller (UDP)

LISTENS on UDP port 5000  : heartbeats  "HB,<id>,<value>"  from the Arduinos
SENDS   to UDP port 5001  : commands    "<id>,<command>"    to the Arduino

command = 0..99 (show number) | A (all on) | S (all off) | color letter
ID 0 = all displays (broadcast)

Standard library only (tkinter).
"""
import socket
import threading
import time
import tkinter as tk
from tkinter import ttk

LISTEN_PORT = 5000          # PC listens here (heartbeats from Arduino)
SEND_PORT = 5001            # PC sends commands here (Arduino listens)
DEFAULT_IP = "192.168.1.177"
ONLINE_TIMEOUT = 3.0        # seconds without heartbeat -> OFFLINE

COLORS = [
    ("R", "Red", "#e53935"), ("G", "Green", "#43a047"), ("B", "Blue", "#1e88e5"),
    ("W", "White", "#eeeeee"), ("Y", "Yellow", "#fdd835"), ("C", "Cyan", "#00bcd4"),
    ("M", "Magenta", "#d81b60"), ("O", "Orange", "#fb8c00"), ("P", "Purple", "#8e24aa"),
    ("K", "Pink", "#ff4081"), ("L", "Lime", "#9ccc3c"), ("Q", "Gold", "#ffc107"),
]


class App:
    def __init__(self, root):
        self.root = root
        root.title("7-Segment Display Controller (UDP)")
        root.geometry("780x680")

        self.devices = {}                 # id -> {"ip", "value", "last"}
        self.lock = threading.Lock()
        self.running = True
        self.rx_sock = None

        # Separate socket for sending (works even if listening fails)
        self.tx_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.tx_sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)

        self.build_ui()
        self.start_listening()
        self.refresh()
        root.protocol("WM_DELETE_WINDOW", self.on_close)

    # ---------------- Listening ----------------
    def start_listening(self):
        """Bind UDP 5000. Shows a clear message instead of crashing if it fails."""
        if self.rx_sock:
            try:
                self.rx_sock.close()
            except OSError:
                pass
            self.rx_sock = None
        try:
            s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            s.bind(("0.0.0.0", LISTEN_PORT))      # no SO_REUSEADDR (it causes error 10013 on Windows)
            s.settimeout(0.5)
            self.rx_sock = s
            threading.Thread(target=self.rx_loop, args=(s,), daemon=True).start()
            self.status_var.set(f"Listening on UDP {LISTEN_PORT}  |  Sending to UDP {SEND_PORT}")
            self.status_lbl.configure(foreground="#2e7d32")
            self.write_log(f"Listening on UDP port {LISTEN_PORT}")
        except OSError as e:
            self.status_var.set(f"CANNOT listen on UDP {LISTEN_PORT}: {e}")
            self.status_lbl.configure(foreground="#c62828")
            self.write_log(f"Bind failed on port {LISTEN_PORT}: {e}")
            self.write_log("Close other programs using the port, or run:  netstat -ano | findstr :5000")
            self.write_log("Sending still works. Press 'Retry listen' after fixing.")

    def rx_loop(self, sock):
        while self.running and sock is self.rx_sock:
            try:
                data, addr = sock.recvfrom(256)
            except socket.timeout:
                continue
            except OSError:
                break
            try:
                parts = data.decode("ascii", "ignore").strip().split(",")
                if parts[0] == "HB" and len(parts) >= 3:
                    dev_id = int(parts[1])
                    value = int(parts[2])
                    with self.lock:
                        self.devices[dev_id] = {"ip": addr[0], "value": value, "last": time.time()}
            except ValueError:
                pass

    # ---------------- UI ----------------
    def build_ui(self):
        pad = {"padx": 8, "pady": 4}

        # Status bar
        top = ttk.Frame(self.root)
        top.pack(fill="x", **pad)
        self.status_var = tk.StringVar()
        self.status_lbl = ttk.Label(top, textvariable=self.status_var)
        self.status_lbl.pack(side="left")
        ttk.Button(top, text="Retry listen", command=self.start_listening).pack(side="right")

        # Devices
        frm = ttk.LabelFrame(self.root, text="Displays (from heartbeats)")
        frm.pack(fill="x", **pad)
        cols = ("id", "ip", "value", "status", "seen")
        self.tree = ttk.Treeview(frm, columns=cols, show="headings", height=5)
        for c, t, w in (("id", "ID", 60), ("ip", "IP address", 150), ("value", "Showing", 80),
                        ("status", "Status", 90), ("seen", "Last seen", 120)):
            self.tree.heading(c, text=t)
            self.tree.column(c, width=w, anchor="center")
        self.tree.pack(fill="x", padx=6, pady=6)
        self.tree.tag_configure("online", foreground="#2e7d32")
        self.tree.tag_configure("offline", foreground="#c62828")
        self.tree.bind("<<TreeviewSelect>>", self.on_select)

        # Target
        tgt = ttk.LabelFrame(self.root, text="Target")
        tgt.pack(fill="x", **pad)
        ttk.Label(tgt, text="Display ID:").grid(row=0, column=0, **pad)
        self.id_var = tk.StringVar(value="1")
        ttk.Spinbox(tgt, from_=0, to=255, width=6, textvariable=self.id_var).grid(row=0, column=1, **pad)
        ttk.Label(tgt, text="(0 = all displays)").grid(row=0, column=2, **pad)
        ttk.Label(tgt, text="Arduino IP:").grid(row=0, column=3, **pad)
        self.ip_var = tk.StringVar(value=DEFAULT_IP)
        ttk.Entry(tgt, width=16, textvariable=self.ip_var).grid(row=0, column=4, **pad)
        ttk.Label(tgt, text="IP is taken from heartbeats when the display is online; otherwise this field is used.",
                  foreground="#666").grid(row=1, column=0, columnspan=5, sticky="w", padx=8)

        # Value
        val = ttk.LabelFrame(self.root, text="Show a number (0 - 99)")
        val.pack(fill="x", **pad)
        self.value_var = tk.StringVar(value="0")
        entry = ttk.Entry(val, width=6, textvariable=self.value_var, font=("Segoe UI", 18), justify="center")
        entry.grid(row=0, column=0, **pad)
        entry.bind("<Return>", lambda e: self.send_value())
        ttk.Button(val, text="Send", command=self.send_value).grid(row=0, column=1, **pad)
        ttk.Button(val, text="-", width=3, command=lambda: self.step(-1)).grid(row=0, column=2, **pad)
        ttk.Button(val, text="+", width=3, command=lambda: self.step(1)).grid(row=0, column=3, **pad)
        self.scale = tk.Scale(val, from_=0, to=99, orient="horizontal", length=240, command=self.on_scale)
        self.scale.grid(row=0, column=4, **pad)
        self.live = tk.BooleanVar(value=False)
        ttk.Checkbutton(val, text="Send while dragging", variable=self.live).grid(row=0, column=5, **pad)

        # Colors
        col = ttk.LabelFrame(self.root, text="Color")
        col.pack(fill="x", **pad)
        for i, (code, name, hexcol) in enumerate(COLORS):
            tk.Button(col, text=name, bg=hexcol, fg="black", width=9,
                      command=lambda c=code: self.send_cmd(c)).grid(row=i // 6, column=i % 6, padx=4, pady=4)

        # Actions
        act = ttk.LabelFrame(self.root, text="Actions")
        act.pack(fill="x", **pad)
        ttk.Button(act, text="All segments ON", command=lambda: self.send_cmd("A")).grid(row=0, column=0, **pad)
        ttk.Button(act, text="All OFF", command=lambda: self.send_cmd("S")).grid(row=0, column=1, **pad)
        ttk.Label(act, text="Raw command:").grid(row=0, column=2, **pad)
        self.raw_var = tk.StringVar()
        raw = ttk.Entry(act, width=12, textvariable=self.raw_var)
        raw.grid(row=0, column=3, **pad)
        raw.bind("<Return>", lambda e: self.send_cmd(self.raw_var.get()))
        ttk.Button(act, text="Send raw", command=lambda: self.send_cmd(self.raw_var.get())).grid(row=0, column=4, **pad)

        # Log
        log = ttk.LabelFrame(self.root, text="Log")
        log.pack(fill="both", expand=True, **pad)
        self.log = tk.Text(log, height=8, state="disabled", font=("Consolas", 9))
        self.log.pack(fill="both", expand=True, padx=6, pady=6)

    # ---------------- Sending ----------------
    def target(self):
        try:
            dev_id = int(self.id_var.get())
            if not 0 <= dev_id <= 255:
                raise ValueError
        except ValueError:
            self.write_log("Invalid ID (0-255)")
            return None
        if dev_id == 0:
            return dev_id, "255.255.255.255"
        ip = self.ip_var.get().strip()
        with self.lock:
            dev = self.devices.get(dev_id)
        if dev and time.time() - dev["last"] <= ONLINE_TIMEOUT:
            ip = dev["ip"]
        if not ip:
            self.write_log("No IP for this ID")
            return None
        return dev_id, ip

    def send_cmd(self, cmd):
        cmd = str(cmd).strip()
        if not cmd:
            return
        t = self.target()
        if not t:
            return
        dev_id, ip = t
        msg = f"{dev_id},{cmd}"
        try:
            self.tx_sock.sendto(msg.encode("ascii"), (ip, SEND_PORT))
            self.write_log(f"TX -> {ip}:{SEND_PORT}   {msg}")
        except OSError as e:
            self.write_log(f"Send error: {e}")

    def send_value(self):
        try:
            v = int(self.value_var.get())
        except ValueError:
            self.write_log("Value must be a number 0-99")
            return
        v = max(0, min(99, v))
        self.value_var.set(str(v))
        self.scale.set(v)
        self.send_cmd(str(v))

    def step(self, d):
        try:
            v = int(self.value_var.get())
        except ValueError:
            v = 0
        self.value_var.set(str(max(0, min(99, v + d))))
        self.send_value()

    def on_scale(self, val):
        self.value_var.set(str(int(float(val))))
        if self.live.get():
            self.send_cmd(self.value_var.get())

    # ---------------- Helpers ----------------
    def on_select(self, _):
        sel = self.tree.selection()
        if not sel:
            return
        dev_id = int(self.tree.item(sel[0], "values")[0])
        with self.lock:
            dev = self.devices.get(dev_id)
        self.id_var.set(str(dev_id))
        if dev:
            self.ip_var.set(dev["ip"])

    def refresh(self):
        now = time.time()
        with self.lock:
            snapshot = dict(self.devices)
        selected = self.tree.selection()
        sel_id = self.tree.item(selected[0], "values")[0] if selected else None
        self.tree.delete(*self.tree.get_children())
        for dev_id in sorted(snapshot):
            d = snapshot[dev_id]
            age = now - d["last"]
            online = age <= ONLINE_TIMEOUT
            shown = "-" if d["value"] < 0 else str(d["value"])
            iid = self.tree.insert("", "end",
                                   values=(dev_id, d["ip"], shown,
                                           "ONLINE" if online else "OFFLINE", f"{age:.1f}s ago"),
                                   tags=("online" if online else "offline",))
            if sel_id is not None and str(dev_id) == str(sel_id):
                self.tree.selection_set(iid)
        self.root.after(500, self.refresh)

    def write_log(self, text):
        self.log.configure(state="normal")
        self.log.insert("end", time.strftime("%H:%M:%S  ") + text + "\n")
        self.log.see("end")
        self.log.configure(state="disabled")

    def on_close(self):
        self.running = False
        for s in (self.rx_sock, self.tx_sock):
            try:
                if s:
                    s.close()
            except OSError:
                pass
        self.root.destroy()


if __name__ == "__main__":
    root = tk.Tk()
    App(root)
    root.mainloop()