import { NextResponse } from "next/server"
import { db } from "@/lib/db"

export async function POST(req: Request) {
  try {
    const { houseId, actuator, command } = await req.json()

    if (!houseId || !actuator || !command) {
      return NextResponse.json({ error: "Missing required parameters" }, { status: 400 })
    }

    // Insert command into queue
    const newCommand = await db.actuatorCommand.create({
      data: {
        houseId,
        actuator, // "FAN", "HEATER", "PUMP", "FEEDER", "RESET_PUMP"
        command,    // "ON", "OFF", "TRIGGER", "RESET", or portion millis (e.g., "1500")
        isProcessed: false
      }
    })

    // Log the override attempt in AlertLog for audit/traceability
    await db.alertLog.create({
      data: {
        houseId,
        type: "MANUAL_OVERRIDE",
        message: `Manual command registered: ${actuator} -> ${command}`,
        severity: "INFO"
      }
    })

    return NextResponse.json({ success: true, command: newCommand })
  } catch (error: any) {
    console.error("Command register error:", error)
    return NextResponse.json({ error: "Internal Server Error", details: error.message }, { status: 500 })
  }
}
