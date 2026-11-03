# Central Weather Monitoring & Telemetry Hub

A robust, low-overhead weather telemetry system designed for Raspberry Pi. It features a C++ edge-polling daemon running on a central RPi 5 hub (`RainbowPi`), automated atomic SQLite backups via `VACUUM INTO`, a FastAPI backend, and a single-file Tailwind CSS / Chart.js dashboard supporting time-proportional historical trends.

---

## 🏗️ Architecture & Stack

- **Edge Sensor Node:** Raspberry Pi Zero W running a C++ BME280 sensor reader, serving telemetry JSON over port `8080`.
- **Central Hub (`RainbowPi`):** Raspberry Pi 5 running:
  - **C++ Polling Daemon (`weather_poller`):** Periodically polls the edge sensor, writes telemetry to a local SQLite database, and executes automated daily database snapshots.
  - **FastAPI Backend (`weather-dashboard.service`):** Serves current and historical weather endpoints via Uvicorn.
- **Storage:** Local SQLite database (`weather_history.db`) with rotating daily atomic backups stored in `backups/`.
- **Security:** Hardened UFW firewall rules on both nodes restricting cross-talk to explicitly allowed IPs and ports.

---

## 🚀 Getting Started & Installation

### 1. Clone the Repository
```bash
git clone [https://github.com/AccessiT3ch/central-weather.git](https://github.com/AccessiT3ch/central-weather.git)
cd central-weather
```

### 2. Build the C++ Polling Daemon
Ensure you have CMake and a C++ compiler installed, then build the poller:
```bash
mkdir build && cd build
cmake ..
make
cd ..
```

### 3. Set Up Systemd Services
To run both the C++ polling daemon and the FastAPI backend persistently in the background:

#### C++ Weather Poller Service
Create `/etc/systemd/system/weather-poller.service`:
```ini
[Unit]
Description=Central Weather C++ Polling Daemon
After=network.target

[Service]
User=USERNAME
WorkingDirectory=/home/USERNAME/central-weather
ExecStart=/home/USERNAME/central-weather/build/weather_poller
Restart=always
RestartSec=10

[Install]
WantedBy=multi-user.target
```

#### FastAPI Dashboard Service
Create `/etc/systemd/system/weather-dashboard.service`:
```ini
[Unit]
Description=Central Weather FastAPI Backend & Frontend
After=network.target

[Service]
User=USERNAME
WorkingDirectory=/home/USERNAME/central-weather/api
ExecStart=/usr/local/bin/uvicorn main:app --host 0.0.0.0 --port 8000
Restart=always
RestartSec=10

[Install]
WantedBy=multi-user.target
```

#### Enable and start both services
```bash
sudo systemctl daemon-reload
sudo systemctl enable --now weather-poller.service
sudo systemctl enable --now weather-dashboard.service
```

### 🔒 Firewall Configuration (UFW)
To maintain a hardened home network, configure UFW on your nodes:

* Edge Sensor Node (Pi Zero W): Permit SSH and telemetry polling specifically from the RPi 5 hub (192.168.1.NNN):
```bash
sudo ufw allow from 192.168.1.NNN to any port 22 proto tcp
sudo ufw allow from 192.168.1.NNN to any port 8080 proto tcp
sudo ufw enable
```

### 📂 Automated Backups
The C++ daemon utilizes SQLite's atomic `VACUUM INTO` command to generate standalone backups of `weather_history.db` every 24 hours, rotating old files automatically. Backups are saved to:
```text
/home/USERNAME/central-weather/backups/
```


