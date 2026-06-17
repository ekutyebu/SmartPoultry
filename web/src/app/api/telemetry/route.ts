import { NextResponse } from "next/server"
import { db } from "@/lib/db"

export async function POST(req: Request) {
  try {
    const body = await req.json()
    const {
      houseId,
      temperature,
      humidity,
      gasLevel,
      waterLevel,
      feedLevelLow,
      fanState,
      heaterState,
      pumpState,
      feederActive,
      dhtFault,
      gasFault,
      waterFault,
      pumpSafetyTripped
    } = body

    if (!houseId) {
      return NextResponse.json({ error: "Missing houseId" }, { status: 400 })
    }

    // 1. Ensure Poultry House exists (Auto-create if not present)
    let house = await db.poultryHouse.findUnique({
      where: { id: houseId },
      include: { thresholds: true }
    })

    if (!house) {
      house = await db.poultryHouse.create({
        data: {
          id: houseId,
          name: "Poultry House 1",
          location: "Main Farm Sector A",
          thresholds: {
            create: {
              tempHighLimit: 30.0,
              tempLowLimit: 20.0,
              gasHighLimit: 400.0,
              waterLowLimit: 25.0,
              autoMode: true
            }
          }
        },
        include: { thresholds: true }
      })
    }

    // 2. Insert Telemetry Reading
    await db.telemetry.create({
      data: {
        houseId,
        temperature: parseFloat(temperature),
        humidity: parseFloat(humidity),
        gasLevel: parseFloat(gasLevel),
        waterLevel: parseFloat(waterLevel),
        feedLevelLow: !!feedLevelLow,
        fanState: !!fanState,
        heaterState: !!heaterState,
        pumpState: !!pumpState,
        feederActive: !!feederActive,
        dhtFault: !!dhtFault,
        gasFault: !!gasFault,
        waterFault: !!waterFault,
        pumpSafetyTripped: !!pumpSafetyTripped
      }
    })

    // 3. Process Alerts Logging (Record incidents to DB if not already open)
    const thresholds = house.thresholds!
    const activeAlerts: { type: string; message: string; severity: string }[] = []

    if (dhtFault) {
      activeAlerts.push({ type: "SENSOR_FAULT", message: "DHT22 Climate sensor failure detected!", severity: "CRITICAL" })
    } else {
      if (temperature > thresholds.tempHighLimit + 3.0) {
        activeAlerts.push({ type: "TEMP_HIGH", message: `Critical high temperature: ${temperature.toFixed(1)}°C`, severity: "CRITICAL" })
      }
      if (temperature < thresholds.tempLowLimit - 3.0) {
        activeAlerts.push({ type: "TEMP_LOW", message: `Critical low temperature: ${temperature.toFixed(1)}°C`, severity: "CRITICAL" })
      }
    }

    if (gasFault) {
      activeAlerts.push({ type: "SENSOR_FAULT", message: "MQ2 Air quality sensor failure detected!", severity: "CRITICAL" })
    } else if (gasLevel > thresholds.gasHighLimit) {
      activeAlerts.push({ type: "GAS_HIGH", message: `Dangerous gas levels detected: ${gasLevel.toFixed(0)} PPM`, severity: "WARNING" })
    }

    if (feedLevelLow) {
      activeAlerts.push({ type: "LOW_FEED", message: "Feed container is nearly empty!", severity: "WARNING" })
    }

    if (pumpSafetyTripped) {
      activeAlerts.push({ type: "PUMP_SAFETY", message: "Water pump run-time safety switch tripped!", severity: "CRITICAL" })
    }

    // Write alerts to AlertLog if not already logged recently (e.g. within last 5 minutes)
    for (const alert of activeAlerts) {
      const recentAlert = await db.alertLog.findFirst({
        where: {
          houseId,
          type: alert.type,
          timestamp: { gte: new Date(Date.now() - 5 * 60 * 1000) }
        }
      })
      if (!recentAlert) {
        await db.alertLog.create({
          data: {
            houseId,
            type: alert.type,
            message: alert.message,
            severity: alert.severity
          }
        })
      }
    }

    // 4. Retrieve Pending Actuator Commands
    const pendingCommands = await db.actuatorCommand.findMany({
      where: {
        houseId,
        isProcessed: false
      }
    })

    let fanCommand = null
    let heaterCommand = null
    let pumpCommand = null
    let dispenseFeed = false
    let feedDuration = 1500
    let resetPumpSafety = false

    for (const cmd of pendingCommands) {
      if (cmd.actuator === "FAN") fanCommand = cmd.command
      if (cmd.actuator === "HEATER") heaterCommand = cmd.command
      if (cmd.actuator === "PUMP") pumpCommand = cmd.command
      if (cmd.actuator === "FEEDER") {
        dispenseFeed = true
        feedDuration = parseInt(cmd.command) || 1500
      }
      if (cmd.actuator === "RESET_PUMP") resetPumpSafety = true

      // Mark command as processed
      await db.actuatorCommand.update({
        where: { id: cmd.id },
        data: { isProcessed: true }
      })
    }

    // 5. Check Feeding Schedules (Cloud-side trigger backup)
    const now = new Date()
    const utcHours = now.getUTCHours()
    // Convert UTC to local farm time (assume user timezone, say local hour/minute or standard offset)
    // For simplicity, we match the hour/minute in server local/UTC directly.
    const currentHour = now.getHours()
    const currentMinute = now.getMinutes()

    const matchingSchedules = await db.feedingSchedule.findMany({
      where: {
        houseId,
        feedHour: currentHour,
        feedMinute: currentMinute,
        isActive: true
      }
    })

    if (matchingSchedules.length > 0) {
      // Check if we already triggered feeder in the last 60 seconds to avoid repeating
      const recentFeederCmd = await db.actuatorCommand.findFirst({
        where: {
          houseId,
          actuator: "FEEDER",
          timestamp: { gte: new Date(Date.now() - 60 * 1000) }
        }
      })

      if (!recentFeederCmd) {
        dispenseFeed = true
        feedDuration = matchingSchedules[0].portionSizeSec * 1000
        
        // Log feeding event
        await db.alertLog.create({
          data: {
            houseId,
            type: "FEEDING_EVENT",
            message: `Scheduled feeding started: ${matchingSchedules[0].portionSizeSec}s dispense.`,
            severity: "INFO"
          }
        })
      }
    }

    // 6. Response Payload back to ESP32
    return NextResponse.json({
      tempHighLimit: thresholds.tempHighLimit,
      tempLowLimit: thresholds.tempLowLimit,
      gasHighLimit: thresholds.gasHighLimit,
      waterLowLimit: thresholds.waterLowLimit,
      autoMode: thresholds.autoMode,
      fanCommand,
      heaterCommand,
      pumpCommand,
      dispenseFeed,
      feedDuration,
      resetPumpSafety
    })

  } catch (error: any) {
    console.error("Telemetry sync error:", error)
    return NextResponse.json({ error: "Internal Server Error", details: error.message }, { status: 500 })
  }
}

export async function GET(req: Request) {
  try {
    const { searchParams } = new URL(req.url)
    const houseId = searchParams.get("houseId") || "poultry_house_01"
    const limit = parseInt(searchParams.get("limit") || "50")

    const data = await db.telemetry.findMany({
      where: { houseId },
      orderBy: { timestamp: "desc" },
      take: limit
    })

    // Reverse to chronological order for charts
    return NextResponse.json(data.reverse())
  } catch (error) {
    return NextResponse.json({ error: "Internal Server Error" }, { status: 500 })
  }
}
