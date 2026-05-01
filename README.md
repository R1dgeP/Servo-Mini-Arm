# Servo-Mini-Arm
4-DOF 3D printed robot arm controlled via dual joysticks on Arduino Uno. Built from LucaDilo's Servo Mini Arm (MakerWorld). First arm build — v2 planned with higher-torque servos and redesigned mounts

---

## Overview

This was my third robotics project, built as a learning exercise in
multi-servo coordination, analog joystick input, and electronics wiring.
The arm was assembled, wired, and tested with custom joystick control code.
A mechanical torque limitation was identified on the base servo — documented
below as a learning outcome and addressed in the v2 plan.

---

## Hardware

| Component | Quantity | Notes |
|---|---|---|
| Arduino Uno (Elegoo R3) | 1 | Main controller |
| SG90 micro servo | 2 | Base + Gripper |
| MG90S servo (metal gear) | 2 | Shoulder + Elbow |
| Joystick module | 2 | Dual-axis analog input |
| Breadboard (full size) | 1 | Prototyping |
| External 5V power supply | 1 | Powers servos separately from Arduino |
| Jumper wires (M-M, M-F) | many | Signal + power routing |
| M3/M4 screws | various | Assembly hardware |

---

## Wiring

| Component | Arduino Pin |
|---|---|
| Base servo signal | Pin 3 |
| Shoulder servo signal | Pin 2 |
| Elbow servo signal | Pin 4 |
| Gripper servo signal | Pin 5 |
| Joystick 1 VRx (base) | A0 |
| Joystick 1 VRy (shoulder) | A1 |
| Joystick 2 VRx (gripper) | A2 |
| Joystick 2 VRy (elbow) | A3 |
| Joystick 1 button | Pin 7 |
| Joystick 2 button | Pin 8 |

All servo power (red) and ground (black) wires connect to the external 5V
supply rails on the breadboard. Arduino GND is tied to the breadboard ground
rail to establish a shared reference.

---

## Control Scheme

Joysticks are held upside down, so all axes are
inverted in code.

| Joystick | Axis | Controls |
|---|---|---|
| Joystick 1 | Left/Right | Base rotation |
| Joystick 1 | Up/Down | Shoulder |
| Joystick 2 | Left/Right | Gripper open/close |
| Joystick 2 | Up/Down | Elbow |
| Button 1 | Press | Return to rest position |
| Button 2 | Press | Play wave emote |

---

## Code Features

- Direction-based joystick control (not target-based) eliminates drift
- Wide deadzone (±150 around center) prevents movement from analog noise
- Joystick averaging (4 samples per read) smooths noisy readings
- Safe-write system only sends PWM signal when position actually changes,
  eliminating servo buzz from redundant writes
- Staggered servo startup prevents brownout from simultaneous current spike
- Smooth single-step movement (1-2 degrees per update at 20ms intervals)
- Button debouncing for clean single-press detection
- Animation lock prevents joystick input during emote/rest sequences
- Wave emote: raises arm, opens gripper, waves base left/right 3x with
  gripper snap, elbow pump, returns to rest

---

## What Worked

- Full mechanical assembly from STL files with no modifications
- Wiring 4 servos and 2 joysticks with shared external 5V power
- Direction-based joystick control with deadzone — rock solid at rest
- Safe-write system eliminated servo buzzing
- Smooth incremental movement
- Staggered homing prevented brownout on startup
- Wave emote with multi-joint coordination

---

## What I Learned

- How to drive multiple servos from one Arduino with external power
- Why Arduino and external supply must share a common ground
- SG90 vs MG90 differences — torque rating, gear material, use cases
- How analog joystick noise causes servo jitter and how to fix it
- Why target-based servo control jitters and why direction-based is better
- Safe-write pattern: only write to servo when position changes
- How to debounce buttons in code
- How to diagnose servo problems — LED check, manual assist test,
  isolation testing
- How to distinguish electrical problems from mechanical ones

---

## What Went Wrong

**Base and gripper were glitchy and barely responsive under load.**

Initial hypotheses:
- Wiring issue (ruled out — joystick diagnostic sketch confirmed clean readings)
- Code issue (ruled out — direction-based rewrite didn't change symptoms)
- Power brownout (ruled out — Arduino LED stayed steady throughout)

**Final diagnosis: torque limitation on SG90 base servo.**

Confirmed by manually supporting the arm weight while operating joysticks —
when the load was removed, the base responded correctly. The SG90 (~1.8 kg-cm
torque) cannot reliably rotate the full arm assembly (shoulder + elbow +
gripper + their servos) against gravity and inertia.

The gripper exhibited similar symptoms likely due to mechanical disadvantage
in the linkage design amplifying the load beyond SG90 capability.

---

## V2 Plan

- Redesign base mount in Fusion 360 for MG996R servo (~10 kg-cm torque)
- Redesign gripper with better mechanical advantage or upgrade to MG90S
- Add cable management channels into printed parts
- Add ball bearing to base joint to reduce friction load on servo
- Reduce infill on long arm sections to lower overall weight
- Consider counterweight geometry to balance arm around base pivot

---

## Print Settings

- Material: PLA
- Layer height: 0.2mm
- Infill: 15%
- Walls: 3 perimeters
- Supports: as needed per part orientation
- Printer: Bambu P2S

---

## Credits

- Arm design: [LucaDilo — Servo Mini Arm on MakerWorld](https://makerworld.com)
- Arduino Servo library (built-in)
