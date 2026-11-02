# SLT IoT Practicals: One Smart Home, One Smart Town

Code for the live builds in "Implementation of Digital Transformation Society Using IoT" (SLT-Mobitel, 2 November 2026).

Runs fully in the browser on [Wokwi](https://wokwi.com). No hardware needed.

**Live Smart Town wall (view only): https://isuru-rana.github.io/sltiot-wall/**

Open it, then run P3 or P5: your home appears on the wall within about 10 seconds. Nothing to set up.

## Open and run (no sign-in needed)

| Build | What it does | Wokwi link | Code |
|---|---|---|---|
| MQTT Basic | LDR + lamp, sends light level to the wall every 2 s (simplest MQTT example) | https://wokwi.com/projects/479572158508999681 | `code/p0_mqtt_basic` (circuit: `wokwi_basic/diagram.json`, library: PubSubClient) |
| P1 Sense | Reads tank level, light, motion, temperature | https://wokwi.com/projects/479509506434049025 | `code/p1_sense` |
| P2 Decide | Pump with two limits, lamp at dusk, alarms | https://wokwi.com/projects/479564845394356225 | `code/p2_edge` |
| P3 Connect | WiFi + MQTT, joins the town wall, obeys orders | https://wokwi.com/projects/479564910238319617 | `code/p3_connect` |
| P4 Control | Same code as P3, orders sent from outside | (use P3) | `code/p3_connect` |
| P5 Alert | P3 + push alerts to your phone with ntfy | https://wokwi.com/projects/479564965871595521 | `code/p5_alert` |

Open the link and press the green play button. Compiling can take 1 to 2 minutes on the free plan.

## Build it yourself in Wokwi

1. Open https://wokwi.com/projects/new/esp32
2. **sketch.ino**: paste the sketch from `code/`.
3. **diagram.json**: paste `wokwi/diagram.json` (the same circuit for every build).
4. **Library Manager**: add `DHT sensor library` and `Adafruit Unified Sensor`. For P3 and P5, also add `PubSubClient`.
5. Press play.

## Make it your home

- In P3 and P5, change `HOME_ID` (e.g. "KANDY-07") so your home has its own name on the town wall.
- In P5, change `NTFY_TOPIC` to a unique name. Then subscribe to the same name in the ntfy phone app.

## MQTT topics (broker.hivemq.com)

| Topic | Direction | Content |
|---|---|---|
| `sltiot/<HOME>/telemetry` | home to town | JSON: tank, pump, light, lamp, motion, temp, alarms |
| `sltiot/<HOME>/status` | home to town | `online` or `offline` (Last Will) |
| `sltiot/<HOME>/event` | home to town | Alarm raised or cleared |
| `sltiot/<HOME>/cmd` | town to home | `PUMP_ON`, `PUMP_OFF`, `AUTO` |
| `sltiot/town/cmd` | town to all homes | `RATION_ON`, `RATION_OFF` |

## Watch the messages and send orders (HiveMQ web client)

Open https://www.hivemq.com/demos/websocket-client/

**1. Connect**

| Field | Value |
|---|---|
| Host | `broker.hivemq.com` |
| Port | `8884` |
| SSL | ticked |
| Client ID | leave the random one |
| Username / Password | empty |

Press **Connect** (turns green). If 8884 fails, use port `8000` with SSL unticked.

**2. Watch:** Subscriptions, then Add New Topic Subscription.

| Topic | Shows |
|---|---|
| `sltiot/#` | everything (busy when the simulator runs) |
| `sltiot/+/telemetry` | JSON data from every home |
| `sltiot/+/event` | alarms raised or cleared |
| `sltiot/+/status` | online and offline |
| `sltiot/KANDY-07/#` | one home only (use its `HOME_ID`) |

**3. Send orders:** in Publish, enter topic and message, QoS 0, Retain unticked.

| Topic | Message | Effect |
|---|---|---|
| `sltiot/<HOME>/cmd` | `PUMP_ON`, `PUMP_OFF`, `AUTO` | one home's pump |
| `sltiot/town/cmd` | `RATION_ON`, `RATION_OFF` | every home: pump stops at 50 % |

Simulated homes send `"sim": true` in their JSON. Real Wokwi homes do not.

## The town (`town/`)

- `town_wall.html`: the City Operations Wall. Open it in a browser to see every home live.
  - View-only copy online: https://isuru-rana.github.io/sltiot-wall/
- `town_sim.py`: simulates about 20 homes so the town is busy. Run it with `uv run town_sim.py`.
  - Its homes are tagged as simulated. Press **Simulated: shown** on the wall to hide them and see only real homes.
  - Or open the wall with `?sim=0` at the end of the link.

The broker is public. Anyone can read these topics, so never send personal data.
