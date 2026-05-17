# Hardware Design

## 1. Overview

This document describes the hardware design of the wearable Hybrid IoT system for real-time health and safety monitoring of construction workers. The hardware platform integrates physiological sensing, motion sensing, environmental monitoring, local alert actuation, edge processing, wireless communication, and power-management mechanisms into a compact wearable architecture.

The proposed hardware design is centered on the **XIAO ESP32C6 microcontroller**, which coordinates sensor acquisition, local signal processing, decision logic, and communication control. The system uses the **MAX30102** for heart rate and SpO2 monitoring, the **MPU6050** for motion and fall detection, a **PM2.5 sensor** for air-quality monitoring, and a **SIM768x 4G LTE module** for emergency communication.

The design prioritizes the following objectives:

- Real-time physiological and environmental monitoring
- Low-latency emergency detection and alerting
- Wearable and non-intrusive deployment
- Hybrid communication through Wi-Fi and 4G LTE
- Local processing without full dependence on cloud infrastructure
- Energy-aware operation for prolonged field use
- Robustness under construction-site conditions

---

## 2. Hardware Design Requirements

The wearable device is intended for deployment in industrial and construction environments. Therefore, the hardware design must satisfy both functional and operational requirements.

### 2.1 Functional Requirements

The system shall:

1. Continuously acquire physiological data from the worker.
2. Measure heart rate and blood oxygen saturation.
3. Detect fall-related motion patterns using inertial sensing.
4. Monitor airborne fine particulate matter concentration.
5. Process sensor data locally on the embedded controller.
6. Trigger local warnings through buzzer and LED indicators.
7. Transmit routine monitoring data through Wi-Fi.
8. Send emergency alerts through 4G LTE using SMS or voice call.
9. Support low-power operation during idle periods.
10. Maintain emergency functionality even when Wi-Fi is unavailable.

### 2.2 Non-Functional Requirements

The hardware platform should also satisfy the following non-functional requirements:

- **Wearability:** The device should be compact and lightweight enough for wrist-worn or body-mounted use.
- **Reliability:** Critical alert functions should remain available even under unstable network conditions.
- **Responsiveness:** Fall detection and emergency alert triggering should occur with minimal latency.
- **Energy efficiency:** High-power modules should be duty-cycled or activated only when necessary.
- **Scalability:** The architecture should allow additional sensors or communication interfaces in future versions.
- **Maintainability:** Modular hardware organization should simplify testing, replacement, and debugging.

---

## 3. Hardware Architecture

The hardware architecture is organized into five main subsystems:

1. **Sensing subsystem**
2. **Processing subsystem**
3. **Communication subsystem**
4. **Actuation and alert subsystem**
5. **Power-management subsystem**

These subsystems are connected through digital communication interfaces and controlled by the central microcontroller.

```text
+--------------------------------------------------------------+
|                    Wearable IoT Device                       |
|                                                              |
|  +------------------+      +------------------------------+  |
|  | Sensing Layer    |      | Processing Layer             |  |
|  |                  |      |                              |  |
|  | - MAX30102       |----->| XIAO ESP32C6                 |  |
|  | - MPU6050        |----->| - Data acquisition           |  |
|  | - PM2.5 Sensor   |----->| - Filtering                 |  |
|  |                  |      | - Feature extraction         |  |
|  +------------------+      | - Threshold logic            |  |
|                            | - Communication control       |  |
|                            +------------------------------+  |
|                                      |                       |
|              +-----------------------+-------------------+   |
|              |                                           |   |
|  +-------------------------+          +-------------------+   |
|  | Local Alert Subsystem   |          | Communication     |   |
|  |                         |          | Subsystem         |   |
|  | - Buzzer                |          |                   |   |
|  | - LED indicator         |          | - Wi-Fi 6         |   |
|  | - Reset button          |          | - SIM768x 4G LTE  |   |
|  +-------------------------+          +-------------------+   |
|                                                              |
|  +--------------------------------------------------------+  |
|  | Power-Management Subsystem                            |  |
|  | - Battery supply                                      |  |
|  | - Voltage regulation                                  |  |
|  | - Low-power control                                   |  |
|  | - Event-driven activation                             |  |
|  +--------------------------------------------------------+  |
|                                                              |
+--------------------------------------------------------------+
```

---

## 4. Main Hardware Components

| Component | Function | Interface | Role in System |
|---|---|---|---|
| MAX30102 | Heart rate and SpO2 sensing | I2C | Captures optical PPG signals for physiological monitoring |
| MPU6050 | Motion and fall detection | I2C | Provides acceleration and angular velocity data |
| PM2.5 Sensor | Air-quality monitoring | UART / digital interface | Measures fine particulate concentration |
| XIAO ESP32C6 | Central controller | GPIO, I2C, UART, Wi-Fi | Performs local processing and system coordination |
| SIM768x 4G LTE Module | Emergency communication | UART | Sends SMS and voice-call alerts |
| Buzzer | Local audible alert | GPIO | Provides immediate emergency warning |
| LED Indicator | Visual status indication | GPIO | Displays normal, warning, and emergency states |
| Reset Button | User interaction | GPIO interrupt | Allows false-alarm cancellation or system reset |
| Battery and Regulators | Power supply | Power rails | Provides stable power to all modules |

---

## 5. Processing Subsystem

### 5.1 XIAO ESP32C6 Microcontroller

The **XIAO ESP32C6** is the central controller of the wearable hardware platform. It manages sensor communication, executes local processing algorithms, evaluates safety thresholds, controls local alerts, and coordinates wireless communication.

The ESP32C6 is selected because it provides:

- Embedded processing capability for edge computing
- Integrated Wi-Fi connectivity
- Low-power operating modes
- GPIO support for alert and control signals
- I2C and UART interfaces for sensor and module integration
- Sufficient flexibility for wearable IoT applications

### 5.2 Main Responsibilities

The microcontroller performs the following tasks:

1. Initializes all sensors and communication modules.
2. Reads physiological, motion, and environmental data.
3. Applies signal filtering and noise reduction.
4. Extracts relevant features such as BPM, SpO2, acceleration magnitude, and PM2.5 concentration.
5. Evaluates risk thresholds for health, fall, and environmental hazards.
6. Controls local buzzer and LED indicators.
7. Sends routine data through Wi-Fi.
8. Activates the SIM768x module during emergency events.
9. Manages sleep and wake-up states to reduce energy consumption.

---

## 6. Physiological Sensing Subsystem

### 6.1 MAX30102 Sensor

The **MAX30102** is used for heart rate and SpO2 monitoring. It is an optical sensor module that integrates red and infrared LEDs, a photodetector, and analog front-end circuitry.

The sensor operates based on reflective photoplethysmography. Light emitted by the LEDs enters the skin tissue, and the reflected signal is captured by the photodetector. Variations in the reflected light intensity are associated with blood volume changes, allowing the system to estimate heart rate and oxygen saturation.

### 6.2 Hardware Interface

The MAX30102 communicates with the ESP32C6 through the **I2C bus**.

Typical signal connections include:

| MAX30102 Pin | Connected To | Description |
|---|---|---|
| VIN / VCC | Regulated supply | Sensor power input |
| GND | System ground | Common reference |
| SDA | ESP32C6 I2C SDA | I2C data line |
| SCL | ESP32C6 I2C SCL | I2C clock line |
| INT | ESP32C6 GPIO | Optional interrupt signal |

### 6.3 Design Considerations

The MAX30102 should be positioned to maintain stable optical contact with the skin. Poor mechanical contact may introduce motion artifacts and reduce measurement accuracy.

Important design considerations include:

- Stable contact pressure between sensor and skin
- Reduction of ambient light interference
- Mechanical isolation from excessive vibration
- Proper routing of I2C lines
- Short signal paths where possible
- Use of pull-up resistors on I2C lines when required
- Avoidance of strong electrical noise near the optical sensor

---

## 7. Motion Sensing Subsystem

### 7.1 MPU6050 Sensor

The **MPU6050** is a six-degree-of-freedom inertial measurement unit that integrates:

- A three-axis accelerometer
- A three-axis gyroscope

The accelerometer measures linear acceleration, while the gyroscope measures angular velocity. These measurements allow the system to detect motion patterns associated with worker movement, sudden impacts, and possible falls.

### 7.2 Hardware Interface

The MPU6050 communicates with the ESP32C6 through the **I2C bus**.

Typical signal connections include:

| MPU6050 Pin | Connected To | Description |
|---|---|---|
| VCC | Regulated supply | Sensor power input |
| GND | System ground | Common reference |
| SDA | ESP32C6 I2C SDA | I2C data line |
| SCL | ESP32C6 I2C SCL | I2C clock line |
| INT | ESP32C6 GPIO | Motion interrupt or wake-up signal |

### 7.3 Role in Fall Detection

The MPU6050 provides raw acceleration data used to compute the Signal Vector Magnitude:

```text
SVM = sqrt(ax^2 + ay^2 + az^2)
```

The fall-detection algorithm evaluates three major stages:

1. **Free-fall phase**
   - Acceleration magnitude drops below a predefined threshold.

2. **Impact phase**
   - Acceleration magnitude increases sharply due to sudden deceleration.

3. **Post-impact inactivity phase**
   - The worker remains motionless or near-motionless after impact.

### 7.4 Design Considerations

The MPU6050 should be mechanically secured to the wearable enclosure so that its readings accurately represent body movement.

Important design considerations include:

- Firm mechanical mounting to reduce sensor displacement
- Consistent orientation during calibration
- Interrupt line connection for low-power wake-up
- Proper I2C bus sharing with other sensors
- Software calibration to reduce bias and drift
- Mechanical protection against shock and vibration

---

## 8. Environmental Sensing Subsystem

### 8.1 PM2.5 Sensor

The PM2.5 sensor is used to monitor airborne fine particulate matter. It operates using an optical scattering principle, where a laser light source interacts with suspended particles and the scattered light is measured by a photodetector.

The sensor provides an estimated particulate concentration that can be processed by the ESP32C6 and mapped to air-quality risk categories.

### 8.2 Hardware Interface

Depending on the sensor model, the PM2.5 sensor may communicate through UART, PWM, or another digital interface. In this hardware design, it is treated as a digital environmental sensor connected to the ESP32C6.

Typical signal connections include:

| PM2.5 Sensor Pin | Connected To | Description |
|---|---|---|
| VCC | Regulated supply | Sensor power input |
| GND | System ground | Common reference |
| TX | ESP32C6 RX | Sensor data output |
| RX | ESP32C6 TX | Optional command input |
| EN / SET | ESP32C6 GPIO | Optional enable or sleep control |

### 8.3 Design Considerations

The PM2.5 sensor requires exposure to ambient air. Therefore, the enclosure must allow airflow while protecting the sensor from physical damage.

Important design considerations include:

- Placement near air vents or sampling openings
- Avoidance of obstruction by straps or clothing
- Protection from direct liquid ingress
- Duty-cycled operation to reduce power consumption
- Proper warm-up or sampling interval depending on the sensor module
- Isolation from heat sources that may affect airflow or readings

---

## 9. Communication Subsystem

The system uses a hybrid communication design consisting of Wi-Fi and 4G LTE.

### 9.1 Wi-Fi Communication

Wi-Fi is provided by the ESP32C6 and is used for routine monitoring data transmission.

Wi-Fi communication supports:

- Sensor telemetry upload
- Dashboard synchronization
- Local gateway communication
- Cloud database update
- Non-critical monitoring tasks

Wi-Fi is treated as the primary communication channel under normal operating conditions because it is suitable for periodic data synchronization.

### 9.2 SIM768x 4G LTE Module

The **SIM768x 4G LTE module** provides the emergency communication backbone. It is activated when the system detects a critical condition or when local Wi-Fi communication is unavailable.

The module supports:

- SMS emergency notification
- Voice-call emergency alert
- Supervisor contact
- Cloud-bypassing communication
- Fail-safe operation outside local Wi-Fi coverage

### 9.3 Hardware Interface

The SIM768x module communicates with the ESP32C6 through a **UART interface**.

Typical signal connections include:

| SIM768x Pin | Connected To | Description |
|---|---|---|
| VCC / VBAT | Dedicated power rail | Module power input |
| GND | System ground | Common reference |
| TXD | ESP32C6 RX | UART data from module |
| RXD | ESP32C6 TX | UART data to module |
| PWRKEY | ESP32C6 GPIO | Module power control |
| RST | ESP32C6 GPIO | Optional module reset |
| STATUS | ESP32C6 GPIO | Optional module status monitoring |

### 9.4 Design Considerations

Cellular modules may draw high peak current during network registration and transmission. Therefore, the SIM768x should be connected to a stable power rail with sufficient current capacity.

Important design considerations include:

- Dedicated power regulation for the 4G module
- Adequate decoupling capacitors near the module
- Short and wide power traces for high-current paths
- Antenna placement away from noisy digital circuits
- GPIO-based power control for duty-cycled operation
- UART voltage-level compatibility with the ESP32C6
- Proper grounding to reduce communication instability

---

## 10. Local Alert and User Interaction Subsystem

The system includes local alert components to provide immediate feedback during abnormal or emergency conditions.

### 10.1 Buzzer

The buzzer provides an audible warning when the system detects a critical event. It is controlled by an ESP32C6 GPIO pin.

The buzzer may be activated during:

- Confirmed fall detection
- Severe SpO2 drop
- Sustained abnormal heart rate
- Hazardous PM2.5 exposure
- Emergency communication activation

If the buzzer requires more current than the microcontroller GPIO can provide directly, a transistor driver circuit should be used.

### 10.2 LED Indicator

LED indicators provide visual feedback about system status.

A simple status-mapping strategy may include:

| LED State | System State |
|---|---|
| Off | Device inactive or sleeping |
| Slow blinking | Normal monitoring |
| Fast blinking | Warning condition |
| Continuous blinking with buzzer | Emergency condition |
| Short pulse | Data transmission or sensor read |

### 10.3 Reset or Cancel Button

A physical button allows the user to cancel false alarms or reset the device.

Possible button functions include:

- False-alarm cancellation
- Manual emergency acknowledgement
- Device reset
- Wake-up from low-power mode
- Diagnostic mode entry during development

The button should be connected to a GPIO pin with interrupt capability and configured with appropriate debounce handling.

---

## 11. Power-Management Subsystem

### 11.1 Power Architecture

The wearable system requires stable power delivery to low-power sensors, the ESP32C6 controller, and the high-current SIM768x communication module.

A typical power architecture includes:

```text
Battery
  |
  +--> Main power switch
        |
        +--> Voltage regulator for ESP32C6
        |
        +--> Sensor power rail
        |
        +--> Dedicated 4G module power rail
        |
        +--> Optional charging / protection circuit
```

### 11.2 Power Profiles

The main system components have different current requirements.

| Component | Function | Typical Power Behavior |
|---|---|---|
| MAX30102 | Physiological sensing | Low-power periodic optical sensing |
| MPU6050 | Motion tracking | Low-power inertial sensing with interrupt support |
| PM2.5 Sensor | Environmental sensing | Higher current due to laser and fan or optical chamber |
| ESP32C6 | Processing and Wi-Fi | Moderate active current, very low deep-sleep current |
| SIM768x | 4G communication | High peak current during cellular transmission |
| Buzzer | Local alert | Event-driven current consumption |

### 11.3 Low-Power Strategy

The system applies several energy-saving techniques:

1. **Duty cycling**
   - PM2.5 sensing and 4G communication are activated only when needed.

2. **Deep sleep**
   - The ESP32C6 enters low-power mode during idle periods.

3. **Interrupt-driven wake-up**
   - The MPU6050 can wake the controller when abnormal motion is detected.

4. **Tiered communication**
   - Wi-Fi is used for routine telemetry, while 4G is reserved for emergencies.

5. **Event-driven activation**
   - The SIM768x module is powered only during critical alerts or network failover.

### 11.4 Power Design Considerations

Important power-design considerations include:

- Separate regulation for high-current communication modules
- Adequate decoupling for all sensor and controller power pins
- Battery protection against over-discharge and over-current
- Low-noise supply for optical and inertial sensors
- Power gating for high-consumption peripherals
- Thermal considerations for enclosed wearable operation
- Charging interface accessibility
- Mechanical safety of the battery enclosure

---

## 12. Electrical Interface Design

### 12.1 I2C Bus Design

The MAX30102 and MPU6050 are connected to the ESP32C6 through the I2C bus.

The I2C bus includes:

- SDA data line
- SCL clock line
- Pull-up resistors
- Common ground
- Shared voltage reference

Design recommendations:

- Keep I2C traces short where possible.
- Use appropriate pull-up resistor values.
- Avoid routing I2C lines near high-current 4G power traces.
- Verify that all I2C devices operate at compatible logic levels.
- Assign unique I2C addresses or resolve address conflicts.

### 12.2 UART Design

UART is used for communication between the ESP32C6 and the SIM768x module. It may also be used by the PM2.5 sensor depending on the selected model.

Design recommendations:

- Cross-connect TX and RX lines correctly.
- Verify baud-rate compatibility.
- Ensure voltage-level compatibility.
- Use short signal traces for UART lines.
- Avoid noisy routing near antenna and switching regulators.
- Reserve debugging access during development.

### 12.3 GPIO Design

GPIO pins are used for alert actuation, module control, and user interaction.

Typical GPIO functions include:

| Function | Direction | Description |
|---|---|---|
| Buzzer control | Output | Activates audible alert |
| LED control | Output | Displays system state |
| Reset button | Input | Receives user command |
| MAX30102 interrupt | Input | Optional physiological sensor interrupt |
| MPU6050 interrupt | Input | Motion wake-up or fall-event signal |
| SIM768x PWRKEY | Output | Controls cellular module power state |
| SIM768x RESET | Output | Optional cellular module reset |
| PM2.5 enable | Output | Optional sensor power or sleep control |

---

## 13. Suggested Logical Pin Allocation

The following table provides a logical pin-allocation model. The exact physical pin numbers should be verified against the final ESP32C6 development board layout and schematic.

| Signal Group | Signal Name | Connected Module | Function |
|---|---|---|---|
| I2C | SDA | MAX30102, MPU6050 | Shared I2C data |
| I2C | SCL | MAX30102, MPU6050 | Shared I2C clock |
| UART 1 | TX | SIM768x RXD | Command transmission |
| UART 1 | RX | SIM768x TXD | Response reception |
| UART 2 | RX | PM2.5 TX | PM2.5 data reception |
| UART 2 | TX | PM2.5 RX | Optional PM2.5 configuration |
| GPIO | BUZZER | Buzzer driver | Audible alert |
| GPIO | LED_STATUS | LED indicator | Visual status |
| GPIO | BTN_RESET | Push button | Reset or cancel alarm |
| GPIO | MPU_INT | MPU6050 | Motion interrupt |
| GPIO | PPG_INT | MAX30102 | Optional data-ready interrupt |
| GPIO | LTE_PWRKEY | SIM768x | 4G module power control |
| GPIO | LTE_RST | SIM768x | Optional reset control |
| GPIO | PM_EN | PM2.5 sensor | Optional power control |

---

## 14. Mechanical and Wearable Integration

The device is intended to operate as a wearable safety-monitoring platform. Hardware placement and enclosure design are therefore critical to performance.

### 14.1 Sensor Placement

The MAX30102 should be positioned where stable skin contact can be maintained. In a wrist-worn design, the optical sensor should face the skin and be shielded from ambient light.

The MPU6050 should be fixed firmly to the enclosure to ensure that measured acceleration reflects body movement. The PM2.5 sensor should be exposed to ambient air through a protected opening.

### 14.2 Enclosure Design

The enclosure should provide:

- Protection against dust and impact
- Openings for PM2.5 air sampling
- Optical contact area for the MAX30102
- Mechanical stability for the MPU6050
- Access to the reset button
- Visibility of LED indicators
- Acoustic opening for the buzzer
- Safe battery containment
- Antenna clearance for the 4G module

### 14.3 Wearability Considerations

The wearable form factor should avoid interfering with worker movement, gloves, tools, or protective clothing.

Important ergonomic considerations include:

- Low-profile housing
- Rounded edges
- Sweat-resistant surface
- Impact-absorbing material
- Secure but comfortable strap
- Easy charging access
- Minimal obstruction to hand movement

---

## 15. Hardware Reliability Considerations

The system is designed for operation in harsh construction-site environments. Hardware reliability is therefore essential.

Key reliability considerations include:

1. **Electrical reliability**
   - Stable voltage regulation
   - Proper grounding
   - Noise reduction
   - Protection from voltage spikes

2. **Mechanical reliability**
   - Secure sensor mounting
   - Shock-resistant enclosure
   - Strain relief for wires and connectors
   - Durable strap mechanism

3. **Communication reliability**
   - Hybrid Wi-Fi and 4G architecture
   - Cellular failover for emergency alerts
   - Antenna placement optimization
   - Cloud-bypassing SMS and voice-call capability

4. **Sensing reliability**
   - Proper optical contact for PPG sensing
   - Motion-artifact reduction
   - Sensor calibration
   - Airflow access for PM2.5 monitoring

5. **Power reliability**
   - Battery protection
   - High-current support for 4G transmission
   - Low-power idle state
   - Event-driven activation of power-hungry modules

---

## 16. Hardware Testing Plan

A structured hardware testing process is required to validate the system before field deployment.

### 16.1 Sensor Interface Testing

Sensor interface testing verifies that each sensor communicates correctly with the ESP32C6.

Test cases include:

- I2C detection of MAX30102
- I2C detection of MPU6050
- PM2.5 sensor data reception
- Sensor initialization after reset
- Sensor response under normal operation
- Sensor recovery after communication error

### 16.2 Physiological Sensor Testing

Physiological sensor testing evaluates whether the MAX30102 produces stable and usable PPG data.

Test cases include:

- Raw red and infrared signal acquisition
- Heart-rate estimation under rest conditions
- SpO2 estimation under stable contact
- Signal stability under slight movement
- Ambient light rejection
- Sensor contact-loss detection

### 16.3 Motion Sensor Testing

Motion sensor testing validates the MPU6050 and fall-detection input.

Test cases include:

- Static acceleration reading
- Orientation change detection
- Sudden impact detection
- Free-fall simulation
- Post-impact inactivity detection
- Interrupt-based wake-up

### 16.4 Environmental Sensor Testing

Environmental sensor testing verifies PM2.5 measurement behavior.

Test cases include:

- Data acquisition in clean air
- Response to increased particulate concentration
- Recovery after dust exposure
- Measurement smoothing
- Duty-cycled sampling behavior
- Sensor behavior under airflow variation

### 16.5 Communication Testing

Communication testing verifies both routine and emergency communication paths.

Test cases include:

- Wi-Fi connection establishment
- Telemetry upload through Wi-Fi
- Wi-Fi disconnection handling
- SIM768x initialization
- SMS alert transmission
- Voice-call triggering
- Emergency alert during Wi-Fi failure

### 16.6 Power Testing

Power testing evaluates battery operation and energy-management behavior.

Test cases include:

- Active-mode current measurement
- Deep-sleep current measurement
- PM2.5 duty-cycle current impact
- SIM768x peak-current behavior
- Battery runtime estimation
- Wake-up from interrupt
- Recovery after low-power mode

---

## 17. Summary

The hardware design of the wearable Hybrid IoT safety-monitoring system integrates sensing, processing, communication, actuation, and power-management components into a unified embedded platform.

The **MAX30102** enables physiological monitoring through heart rate and SpO2 sensing. The **MPU6050** supports motion analysis and fall detection. The **PM2.5 sensor** provides environmental hazard monitoring. The **XIAO ESP32C6** acts as the central edge-processing controller, while the **SIM768x 4G LTE module** provides a reliable emergency communication channel independent of local Wi-Fi availability.

The hardware architecture is designed to support:

- Real-time data acquisition
- Local edge processing
- Fall and health-risk detection
- Air-quality risk monitoring
- Local audible and visual alerting
- Wi-Fi-based routine telemetry
- 4G-based emergency notification
- Energy-aware wearable operation

Overall, the hardware platform provides a practical and scalable foundation for proactive occupational health and safety monitoring in construction environments.