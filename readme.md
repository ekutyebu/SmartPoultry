# SmartPoultry

A smart monitoring and control system for poultry environments, consisting of an ESP32 firmware project and a Next.js web application.

---

## 🛠️ Firmware (ESP32)

The ESP32 PlatformIO project is located in `firmware/SmartPoultryESP32`. To run build, upload, or monitoring commands, you **must first navigate to this directory**.

### 1. Navigate to the firmware project
```powershell
cd firmware/SmartPoultryESP32
```

### 2. Build and Upload Firmware
To build and upload the firmware to the ESP32:
```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" run --target upload
```

### 3. Open Serial Monitor
To watch the serial monitor output from the ESP32:
```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" device monitor
```

---

## 🌐 Web Application (Next.js)

The web dashboard is located in the `web` directory.

### 1. Navigate to the web project
```powershell
cd web
```

### 2. Database Setup (Prisma)
Ensure your environment variables are configured in `web/.env` and push the database schema:
```powershell
npm run prisma:db:push
```

### 3. Run Development Server
```powershell
npm run dev
```