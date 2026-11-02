# /// script
# dependencies = ["paho-mqtt>=2"]
# ///
"""Smart Town simulator: keeps the City Operations Wall alive with ~20 homes.

Each simulated home behaves like the P3/P5 Wokwi sketch: same topics, same JSON,
same pump rules, and it obeys PUMP_ON / PUMP_OFF / AUTO / RATION_ON / RATION_OFF.
Real participant homes appear on the wall next to these.

Run (needs internet):
    uv run town_sim.py            # 20 homes, runs until Ctrl+C
    uv run town_sim.py --homes 12 --day 240

Scripted story (one "day" = --day seconds, default 300):
  - lamps switch on across town as night falls, off at dawn
  - one home slowly overheats (HEAT alarm), then cools down
  - one home has night motion (NIGHT MOTION alarm)
  - one home's tank runs dry (TANK EMPTY) until its pump catches up
  - one home drops offline for a minute (Last Will style), then returns
"""

import argparse
import json
import math
import random
import time

try:
    import paho.mqtt.client as mqtt
except ModuleNotFoundError:
    raise SystemExit("paho-mqtt missing. Run it with:  uv run town_sim.py")

BASE = "sltiot"
TOWNS = ["KANDY", "GALLE", "JAFFNA", "MATARA", "KURUNEGALA", "BADULLA", "RATNAPURA", "NEGOMBO",
         "TRINCO", "ANURADHAPURA", "BATTICALOA", "HAMBANTOTA", "NUWARAELIYA", "KALUTARA",
         "POLONNARUWA", "VAVUNIYA", "KEGALLE", "MANNAR", "AMPARA", "MATALE", "GAMPAHA", "PUTTALAM"]

PUMP_START, PUMP_STOP, RATION_STOP, DARK_BELOW, HOT_C = 25, 90, 50, 30, 35.0


class Home:
    def __init__(self, name, rng):
        self.name = name
        self.tank = rng.uniform(30, 95)
        self.use = rng.uniform(0.4, 1.2)          # % per tick
        self.base_temp = rng.uniform(27.0, 31.0)
        self.hum = rng.randint(60, 85)
        self.light_bias = rng.uniform(-8, 8)
        self.rssi = rng.randint(-85, -55)
        self.mode = "AUTO"
        self.pump = False
        self.extra_heat = 0.0
        self.motion = False
        self.offline_until = 0.0
        self.sent_offline = False
        self.last_alarms = ""

    def step(self, daylight, ration):
        self.tank = max(0.0, self.tank - self.use * random.uniform(0.5, 1.5))
        stop_at = RATION_STOP if ration else PUMP_STOP
        if self.mode == "ON":
            self.pump = True
        elif self.mode == "OFF":
            self.pump = False
        else:
            if self.tank < PUMP_START:
                self.pump = True
            if self.tank >= stop_at:
                self.pump = False
        if self.pump:
            self.tank = min(100.0, self.tank + 3.0)
        light = int(max(0, min(100, daylight * 100 + self.light_bias + random.uniform(-3, 3))))
        temp = round(self.base_temp + 3 * daylight + self.extra_heat + random.uniform(-0.3, 0.3), 1)
        dark = light < DARK_BELOW
        alarms = []
        if temp > HOT_C:
            alarms.append("HEAT")
        if dark and self.motion:
            alarms.append("NIGHT MOTION")
        if self.tank < 10:
            alarms.append("TANK EMPTY")
        return {
            "home": self.name, "tank": int(self.tank), "pump": "ON" if self.pump else "OFF",
            "mode": self.mode, "ration": ration, "light": light, "lamp": "ON" if dark else "OFF",
            "motion": self.motion, "temp": temp, "hum": self.hum, "alarms": " ".join(alarms),
            "rssi": self.rssi + random.randint(-4, 4),
            "sim": True,                          # lets the wall hide simulated homes
        }


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--homes", type=int, default=20)
    ap.add_argument("--day", type=float, default=300, help="seconds per simulated day")
    ap.add_argument("--broker", default="broker.hivemq.com")
    ap.add_argument("--seed", type=int, default=7)
    args = ap.parse_args()

    rng = random.Random(args.seed)
    names = [f"{t}-{rng.randint(1, 30):02d}" for t in TOWNS[: args.homes]]
    homes = {n: Home(n, rng) for n in names}
    state = {"ration": False}
    hot, prowl, dry, flaky = names[1 % len(names)], names[2 % len(names)], names[3 % len(names)], names[4 % len(names)]
    homes[dry].tank, homes[dry].use = 14.0, 1.6

    def on_message(client, userdata, msg):
        cmd = msg.payload.decode(errors="ignore").strip()
        parts = msg.topic.split("/")
        if parts[1] == "town":
            if cmd == "RATION_ON":
                state["ration"] = True
            elif cmd == "RATION_OFF":
                state["ration"] = False
            print("TOWN ORDER", cmd)
        elif parts[1] in homes:
            mode = {"PUMP_ON": "ON", "PUMP_OFF": "OFF", "AUTO": "AUTO"}.get(cmd)
            if mode:
                homes[parts[1]].mode = mode
                print("ORDER", parts[1], cmd)

    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id=f"town-sim-{random.randint(0, 0xffff):x}")
    client.on_message = on_message
    client.connect(args.broker, 1883, keepalive=30)
    client.subscribe(f"{BASE}/town/cmd")
    for n in names:
        client.subscribe(f"{BASE}/{n}/cmd")
    client.loop_start()
    for n in names:
        client.publish(f"{BASE}/{n}/status", "online", retain=False)

    t0 = time.time()
    print(f"Town running: {len(names)} homes on {args.broker}. Ctrl+C to stop.")
    try:
        while True:
            t = time.time() - t0
            phase = (t % args.day) / args.day                        # 0 = midday
            daylight = max(0.0, math.cos(2 * math.pi * phase)) ** 0.6
            # scripted events, repeating every day
            homes[hot].extra_heat = 8.0 * max(0.0, math.sin(math.pi * min(1, max(0, (phase - 0.05) / 0.35))))
            homes[prowl].motion = 0.30 < phase < 0.42
            for n in names:
                if n not in (prowl,):
                    homes[n].motion = random.random() < 0.05 and daylight > 0.3
            fl = homes[flaky]
            if 0.55 < phase < 0.57 and fl.offline_until < time.time():
                fl.offline_until = time.time() + 60
            for n, h in homes.items():
                if h.offline_until > time.time():
                    if not h.sent_offline:
                        client.publish(f"{BASE}/{n}/status", "offline")
                        h.sent_offline = True
                    continue
                if h.sent_offline:
                    client.publish(f"{BASE}/{n}/status", "online")
                    h.sent_offline = False
                d = h.step(daylight, state["ration"])
                client.publish(f"{BASE}/{n}/telemetry", json.dumps(d))
                before = h.last_alarms
                for a in ("HEAT", "NIGHT MOTION", "TANK EMPTY"):
                    if (a in d["alarms"]) != (a in before):
                        client.publish(f"{BASE}/{n}/event", f"{n}: {a} {'RAISED' if a in d['alarms'] else 'cleared'}")
                h.last_alarms = d["alarms"]
                time.sleep(0.05)
            time.sleep(max(0.5, 5 - 0.05 * len(homes)))
    except KeyboardInterrupt:
        pass
    finally:
        import signal; signal.signal(signal.SIGINT, signal.SIG_IGN)
        for n in names:
            client.publish(f"{BASE}/{n}/status", "offline")
        time.sleep(1)
        client.loop_stop()


if __name__ == "__main__":
    main()
