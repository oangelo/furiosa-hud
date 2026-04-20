#!/usr/bin/env python3
"""
VESC Simulator via Bluetooth SPP.

Creates a BT SPP server on RFCOMM channel 1 that the ESP32 Furiosa HUD
can discover, connect to, and receive realistic telemetry data from.

Protocol: VESC UART (COMM_GET_VALUES, packet id 4).
Reference: VescUart.cpp from SolidGeek/VescUart 1.0.1

The ESP32 firmware must use connect(mac, 1) to bypass SDP lookup,
since this script does not register an SDP record.

Usage:
    python3 tools/vesc_sim.py

Controls:
    UP/DOWN  - adjust target speed (km/h)
    t        - temperature spike (test alerts)
    f        - simulate fault code
    r        - reset to defaults
    q/Ctrl+C - quit

Requirements:
    python-pybluez (Arch: sudo pacman -S python-pybluez)
    python-dbus    (Arch: sudo pacman -S dbus-python)
"""

import bluetooth
import struct
import time
import sys
import os
import threading
import select
import signal
import dbus
import dbus.service
import dbus.mainloop.glib

# ── VESC Protocol ──────────────────────────────────────────────

COMM_GET_VALUES = 4

CRC16_TAB = [
    0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50a5, 0x60c6, 0x70e7,
    0x8108, 0x9129, 0xa14a, 0xb16b, 0xc18c, 0xd1ad, 0xe1ce, 0xf1ef,
    0x1231, 0x0210, 0x3273, 0x2252, 0x52b5, 0x4294, 0x72f7, 0x62d6,
    0x9339, 0x8318, 0xb37b, 0xa35a, 0xd3bd, 0xc39c, 0xf3ff, 0xe3de,
    0x2462, 0x3443, 0x0420, 0x1401, 0x64e6, 0x74c7, 0x44a4, 0x5485,
    0xa56a, 0xb54b, 0x8528, 0x9509, 0xe5ee, 0xf5cf, 0xc5ac, 0xd58d,
    0x3653, 0x2672, 0x1611, 0x0630, 0x76d7, 0x66f6, 0x5695, 0x46b4,
    0xb75b, 0xa77a, 0x9719, 0x8738, 0xf7df, 0xe7fe, 0xd79d, 0xc7bc,
    0x48c4, 0x58e5, 0x6886, 0x78a7, 0x0840, 0x1861, 0x2802, 0x3823,
    0xc9cc, 0xd9ed, 0xe98e, 0xf9af, 0x8948, 0x9969, 0xa90a, 0xb92b,
    0x5af5, 0x4ad4, 0x7ab7, 0x6a96, 0x1a71, 0x0a50, 0x3a33, 0x2a12,
    0xdbfd, 0xcbdc, 0xfbbf, 0xeb9e, 0x9b79, 0x8b58, 0xbb3b, 0xab1a,
    0x6ca6, 0x7c87, 0x4ce4, 0x5cc5, 0x2c22, 0x3c03, 0x0c60, 0x1c41,
    0xedae, 0xfd8f, 0xcdec, 0xddcd, 0xad2a, 0xbd0b, 0x8d68, 0x9d49,
    0x7e97, 0x6eb6, 0x5ed5, 0x4ef4, 0x3e13, 0x2e32, 0x1e51, 0x0e70,
    0xff9f, 0xefbe, 0xdfdd, 0xcffc, 0xbf1b, 0xaf3a, 0x9f59, 0x8f78,
    0x9188, 0x81a9, 0xb1ca, 0xa1eb, 0xd10c, 0xc12d, 0xf14e, 0xe16f,
    0x1080, 0x00a1, 0x30c2, 0x20e3, 0x5004, 0x4025, 0x7046, 0x6067,
    0x83b9, 0x9398, 0xa3fb, 0xb3da, 0xc33d, 0xd31c, 0xe37f, 0xf35e,
    0x02b1, 0x1290, 0x22f3, 0x32d2, 0x4235, 0x5214, 0x6277, 0x7256,
    0xb5ea, 0xa5cb, 0x95a8, 0x8589, 0xf56e, 0xe54f, 0xd52c, 0xc50d,
    0x34e2, 0x24c3, 0x14a0, 0x0481, 0x7466, 0x6447, 0x5424, 0x4405,
    0xa7db, 0xb7fa, 0x8799, 0x97b8, 0xe75f, 0xf77e, 0xc71d, 0xd73c,
    0x26d3, 0x36f2, 0x0691, 0x16b0, 0x6657, 0x7676, 0x4615, 0x5634,
    0xd94c, 0xc96d, 0xf90e, 0xe92f, 0x99c8, 0x89e9, 0xb98a, 0xa9ab,
    0x5844, 0x4865, 0x7806, 0x6827, 0x18c0, 0x08e1, 0x3882, 0x28a3,
    0xcb7d, 0xdb5c, 0xeb3f, 0xfb1e, 0x8bf9, 0x9bd8, 0xabbb, 0xbb9a,
    0x4a75, 0x5a54, 0x6a37, 0x7a16, 0x0af1, 0x1ad0, 0x2ab3, 0x3a92,
    0xfd2e, 0xed0f, 0xdd6c, 0xcd4d, 0xbdaa, 0xad8b, 0x9de8, 0x8dc9,
    0x7c26, 0x6c07, 0x5c64, 0x4c45, 0x3ca2, 0x2c83, 0x1ce0, 0x0cc1,
    0xef1f, 0xff3e, 0xcf5d, 0xdf7c, 0xaf9b, 0xbfba, 0x8fd9, 0x9ff8,
    0x6e17, 0x7e36, 0x4e55, 0x5e74, 0x2e93, 0x3eb2, 0x0ed1, 0x1ef0,
]


def crc16(buf: bytes) -> int:
    cksum = 0
    for b in buf:
        cksum = CRC16_TAB[((cksum >> 8) ^ b) & 0xFF] ^ (cksum << 8)
        cksum &= 0xFFFF
    return cksum


def append_int16(buf: bytearray, val: int):
    buf += struct.pack(">h", val)


def append_int32(buf: bytearray, val: int):
    buf += struct.pack(">i", val)


def append_float16(buf: bytearray, val: float, scale: float):
    append_int16(buf, int(val * scale))


def append_float32(buf: bytearray, val: float, scale: float):
    append_int32(buf, int(val * scale))


def build_comm_get_values(
    temp_mosfet: float,
    temp_motor: float,
    avg_motor_current: float,
    avg_input_current: float,
    avg_id: float,
    avg_iq: float,
    duty_cycle_now: float,
    rpm: float,
    inp_voltage: float,
    amp_hours: float,
    amp_hours_charged: float,
    watt_hours: float,
    watt_hours_charged: float,
    tachometer: int,
    tachometer_abs: int,
    fault_code: int,
    pid_pos: float,
    controller_id: int,
) -> bytes:
    payload = bytearray()
    payload.append(COMM_GET_VALUES)

    append_float16(payload, temp_mosfet, 10.0)
    append_float16(payload, temp_motor, 10.0)
    append_float32(payload, avg_motor_current, 100.0)
    append_float32(payload, avg_input_current, 100.0)
    append_float32(payload, avg_id, 100.0)
    append_float32(payload, avg_iq, 100.0)
    append_float16(payload, duty_cycle_now, 1000.0)
    append_float32(payload, rpm, 1.0)
    append_float16(payload, inp_voltage, 10.0)
    append_float32(payload, amp_hours, 10000.0)
    append_float32(payload, amp_hours_charged, 10000.0)
    append_float32(payload, watt_hours, 10000.0)
    append_float32(payload, watt_hours_charged, 10000.0)
    append_int32(payload, tachometer)
    append_int32(payload, tachometer_abs)
    payload.append(fault_code & 0xFF)
    append_float32(payload, pid_pos, 1000000.0)
    payload.append(controller_id & 0xFF)

    return pack_packet(bytes(payload))


def pack_packet(payload: bytes) -> bytes:
    c = crc16(payload)
    if len(payload) <= 255:
        return bytes([2, len(payload)]) + payload + bytes([c >> 8, c & 0xFF, 3])
    else:
        return bytes([3, (len(payload) >> 8) & 0xFF, len(payload) & 0xFF]) + \
               payload + bytes([c >> 8, c & 0xFF, 3])


def try_read_request(sock) -> bytes | None:
    try:
        rlist, _, _ = select.select([sock], [], [], 0.05)
        if not rlist:
            return None
        data = sock.recv(64)
        return data if data else None
    except Exception:
        return None


def is_comm_get_values(data: bytes) -> bool:
    if not data or len(data) < 6:
        return False
    if data[0] == 2 and data[-1] == 3:
        payload_len = data[1]
        if payload_len >= 1:
            return data[2] == COMM_GET_VALUES
    return False


# ── D-Bus Agent (auto-accept pairing) ──────────────────────────

AGENT_PATH = "/vesc_sim/agent"


class PairAgent(dbus.service.Object):
    def __init__(self, bus, loop):
        dbus.service.Object.__init__(self, bus, AGENT_PATH)
        self._loop = loop

    @dbus.service.method("org.bluez.Agent1",
                         in_signature="", out_signature="")
    def Release(self):
        pass

    @dbus.service.method("org.bluez.Agent1",
                         in_signature="os", out_signature="")
    def RequestConfirmation(self, device, passkey):
        print(f"[PAIR] Auto-accepting pairing from {device}")

    @dbus.service.method("org.bluez.Agent1",
                         in_signature="o", out_signature="s")
    def RequestPinCode(self, device):
        print(f"[PAIR] Auto-accepting PIN from {device}")
        return "0000"

    @dbus.service.method("org.bluez.Agent1",
                         in_signature="ou", out_signature="")
    def DisplayPasskey(self, device, passkey):
        pass

    @dbus.service.method("org.bluez.Agent1",
                         in_signature="os", out_signature="")
    def DisplayPinCode(self, device, pincode):
        pass

    @dbus.service.method("org.bluez.Agent1",
                         in_signature="os", out_signature="")
    def AuthorizeService(self, device, uuid):
        print(f"[PAIR] Authorizing {uuid} from {device}")

    @dbus.service.method("org.bluez.Agent1",
                         in_signature="", out_signature="")
    def Cancel(self):
        pass


def register_agent(loop):
    try:
        bus = dbus.SystemBus()
        agent = PairAgent(bus, loop)
        manager = dbus.Interface(
            bus.get_object("org.bluez", "/org/bluez"),
            "org.bluez.AgentManager1",
        )
        manager.RegisterAgent(AGENT_PATH, "NoInputNoOutput")
        manager.RequestDefaultAgent(AGENT_PATH)
        print("[PAIR] D-Bus agent registered (NoInputNoOutput)")
        return agent
    except Exception as e:
        print(f"[PAIR] Warning: Could not register D-Bus agent: {e}")
        print("[PAIR] You may need to accept pairing dialogs manually")
        return None


# ── Simulation State ───────────────────────────────────────────

MOTOR_POLE_PAIRS = 14
WHEEL_DIAMETER_MM = 254.0
MAX_SPEED_KMH = 35.0


class SimState:
    def __init__(self):
        self.target_speed_kmh = 0.0
        self.current_speed_kmh = 0.0
        self.voltage = 39.2
        self.temp_esc = 32.0
        self.temp_motor = 28.0
        self.fault_code = 0
        self.tachometer_abs = 0
        self.amp_hours = 0.0
        self.temp_spike = False
        self.temp_spike_end = 0.0
        self.running = True
        self.connected = False

    def update(self, dt: float):
        speed_diff = self.target_speed_kmh - self.current_speed_kmh
        accel = 8.0 * dt
        if abs(speed_diff) < accel:
            self.current_speed_kmh = self.target_speed_kmh
        else:
            self.current_speed_kmh += accel if speed_diff > 0 else -accel
        self.current_speed_kmh = max(0.0, min(MAX_SPEED_KMH, self.current_speed_kmh))

        circumference_m = (WHEEL_DIAMETER_MM / 1000.0) * 3.14159265
        mech_rpm = (self.current_speed_kmh / 3.6) / circumference_m * 60.0
        erpm = mech_rpm * MOTOR_POLE_PAIRS

        duty = (self.current_speed_kmh / MAX_SPEED_KMH) * 0.95 if self.current_speed_kmh > 0 else 0.0
        motor_current = self.current_speed_kmh * 0.8
        input_current = self.current_speed_kmh * 0.5

        self.voltage -= 0.0001 * dt * self.current_speed_kmh
        self.voltage = max(33.0, min(42.0, self.voltage))

        now = time.time()
        base_temp_esc = 30.0 + self.current_speed_kmh * 1.2
        base_temp_motor = 28.0 + self.current_speed_kmh * 1.5

        if self.temp_spike and now > self.temp_spike_end:
            self.temp_spike = False

        if self.temp_spike:
            self.temp_esc = base_temp_esc + 55.0
            self.temp_motor = base_temp_motor + 72.0
        else:
            self.temp_esc += (base_temp_esc - self.temp_esc) * 0.05
            self.temp_motor += (base_temp_motor - self.temp_motor) * 0.05

        distance_m = (self.current_speed_kmh / 3.6) * dt
        ticks_per_m = MOTOR_POLE_PAIRS * 3 * 1000.0 / (circumference_m * 1000.0)
        self.tachometer_abs += int(distance_m * ticks_per_m)

        self.amp_hours += input_current * dt / 3600.0

        return {
            'temp_mosfet': self.temp_esc,
            'temp_motor': self.temp_motor,
            'avg_motor_current': motor_current,
            'avg_input_current': input_current,
            'avg_id': 0.0,
            'avg_iq': 0.0,
            'duty_cycle_now': duty,
            'rpm': erpm,
            'inp_voltage': self.voltage,
            'amp_hours': self.amp_hours,
            'amp_hours_charged': 0.0,
            'watt_hours': self.amp_hours * self.voltage,
            'watt_hours_charged': 0.0,
            'tachometer': self.tachometer_abs,
            'tachometer_abs': self.tachometer_abs,
            'fault_code': self.fault_code,
            'pid_pos': 0.0,
            'controller_id': 42,
        }


def keyboard_input(sim: SimState):
    import tty, termios
    old_settings = termios.tcgetattr(sys.stdin)
    try:
        tty.setcbreak(sys.stdin.fileno())
        while sim.running:
            rlist, _, _ = select.select([sys.stdin], [], [], 0.1)
            if rlist:
                ch = sys.stdin.read(1)
                if ch == 'q' or ch == '\x03':
                    sim.running = False
                    break
                elif ch == 'w':
                    sim.target_speed_kmh = min(MAX_SPEED_KMH, sim.target_speed_kmh + 2.0)
                    print(f"  [SIM] Target speed: {sim.target_speed_kmh:.0f} km/h")
                elif ch == 's':
                    sim.target_speed_kmh = max(0.0, sim.target_speed_kmh - 2.0)
                    print(f"  [SIM] Target speed: {sim.target_speed_kmh:.0f} km/h")
                elif ch == 't':
                    sim.temp_spike = True
                    sim.temp_spike_end = time.time() + 5.0
                    print("  [SIM] Temperature spike for 5s!")
                elif ch == 'f':
                    sim.fault_code = (sim.fault_code + 1) % 5
                    fault_names = ['NONE', 'OVER_VOLTAGE', 'UNDER_VOLTAGE', 'DRV', 'ABS_OVER_CURRENT']
                    print(f"  [SIM] Fault code: {sim.fault_code} ({fault_names[sim.fault_code]})")
                elif ch == 'r':
                    sim.target_speed_kmh = 0.0
                    sim.voltage = 39.2
                    sim.temp_esc = 32.0
                    sim.temp_motor = 28.0
                    sim.fault_code = 0
                    sim.tachometer_abs = 0
                    sim.amp_hours = 0.0
                    sim.temp_spike = False
                    print("  [SIM] Reset to defaults")
                elif ch == '\x1b':
                    r2, _, _ = select.select([sys.stdin], [], [], 0.02)
                    if r2:
                        ch2 = sys.stdin.read(1)
                        if ch2 == '[':
                            r3, _, _ = select.select([sys.stdin], [], [], 0.02)
                            if r3:
                                ch3 = sys.stdin.read(1)
                                if ch3 == 'A':
                                    sim.target_speed_kmh = min(MAX_SPEED_KMH, sim.target_speed_kmh + 2.0)
                                    print(f"  [SIM] Target speed: {sim.target_speed_kmh:.0f} km/h")
                                elif ch3 == 'B':
                                    sim.target_speed_kmh = max(0.0, sim.target_speed_kmh - 2.0)
                                    print(f"  [SIM] Target speed: {sim.target_speed_kmh:.0f} km/h")
    finally:
        termios.tcsetattr(sys.stdin, termios.TCSADRAIN, old_settings)


# ── Client Handler ─────────────────────────────────────────────

def handle_client(sock, sim: SimState):
    sock.setblocking(False)
    last_update = time.time()
    pkt_count = 0

    while sim.running:
        now = time.time()
        dt = now - last_update
        last_update = now

        data = try_read_request(sock)
        if data is not None:
            if is_comm_get_values(data):
                vals = sim.update(dt)
                pkt = build_comm_get_values(**vals)
                try:
                    sock.send(pkt)
                    pkt_count += 1
                    if pkt_count % 20 == 0:
                        print(
                            f"  [{pkt_count:4d}] "
                            f"Speed:{sim.current_speed_kmh:5.1f} km/h  "
                            f"V:{sim.voltage:5.1f}V  "
                            f"ESC:{sim.temp_esc:5.1f}C  "
                            f"Motor:{sim.temp_motor:5.1f}C  "
                            f"Fault:{sim.fault_code}",
                            end="\r",
                        )
                        sys.stdout.flush()
                except Exception:
                    break
        else:
            sim.update(dt)

        time.sleep(0.02)


# ── Main ───────────────────────────────────────────────────────

def main():
    sim = SimState()

    print("=" * 50)
    print("  Furiosa HUD — VESC Simulator")
    print("=" * 50)
    print()
    print("Controls:")
    print("  UP/w    Increase speed (+2 km/h)")
    print("  DOWN/s  Decrease speed (-2 km/h)")
    print("  t       Temperature spike (5s)")
    print("  f       Cycle fault codes")
    print("  r       Reset all values")
    print("  q       Quit")
    print()

    dbus.mainloop.glib.DBusGMainLoop(set_as_default=True)
    import gi
    gi.require_version('GLib', '2.0')
    from gi.repository import GLib
    loop = GLib.MainLoop()

    register_agent(loop)

    os.popen("bluetoothctl discoverable on").read()
    os.popen("bluetoothctl pairable on").read()

    server_sock = bluetooth.BluetoothSocket(bluetooth.RFCOMM)
    port = 1

    try:
        server_sock.bind(("", port))
    except Exception as e:
        print(f"ERROR: Failed to bind BT socket: {e}")
        print("Make sure bluetooth service is running:")
        print("  sudo systemctl start bluetooth")
        sys.exit(1)

    server_sock.listen(1)

    bt_name = "VESC_BT"

    print("[SDP] Registering Serial Port service...")
    sdp_result = os.popen("sdptool add SP 2>&1").read().strip()
    print(f"[SDP] {sdp_result}")

    try:
        bluetooth.advertise_service(
            server_sock,
            bt_name,
            service_id="00001101-0000-1000-8000-00805F9B34FB",
            service_classes=[bluetooth.SERIAL_PORT_CLASS],
            profiles=[bluetooth.SERIAL_PORT_PROFILE],
        )
        print(f"[SDP] Advertising '{bt_name}' on channel {port}")
    except Exception as e:
        print(f"[SDP] advertise_service failed: {e}")
        print(f"[SDP] Relying on sdptool registration only")

    print(f"Listening on RFCOMM channel {port}")
    print("Waiting for ESP32 connection...")
    print("(Pairing dialogs will be auto-accepted)")
    print()

    kb_thread = threading.Thread(target=keyboard_input, args=(sim,), daemon=True)
    kb_thread.start()

    dbus_thread = threading.Thread(target=loop.run, daemon=True)
    dbus_thread.start()

    def accept_loop():
        while sim.running:
            server_sock.settimeout(1.0)
            try:
                client_sock, client_info = server_sock.accept()
            except bluetooth.BluetoothError:
                continue
            except OSError:
                break

            print(f"\n[BT] Connected from {client_info}")
            sim.connected = True

            try:
                handle_client(client_sock, sim)
            except Exception as e:
                print(f"\n[BT] Client error: {e}")
            finally:
                sim.connected = False
                try:
                    client_sock.close()
                except Exception:
                    pass
                print("\n[BT] Client disconnected")
                print("Waiting for ESP32 connection...")

    accept_loop()

    loop.quit()
    server_sock.close()
    sim.running = False
    print("\nSimulator stopped.")


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print("\nInterrupted.")
        sys.exit(0)
