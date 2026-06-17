import { NextResponse } from "next/server"
import { db } from "@/lib/db"

export async function GET(req: Request) {
  try {
    const { searchParams } = new URL(req.url)
    const houseId = searchParams.get("houseId") || "poultry_house_01"

    let thresholds = await db.thresholds.findUnique({
      where: { houseId }
    })

    if (!thresholds) {
      thresholds = await db.thresholds.create({
        data: {
          houseId,
          tempHighLimit: 30.0,
          tempLowLimit: 20.0,
          gasHighLimit: 400.0,
          waterLowLimit: 25.0,
          autoMode: true
        }
      })
    }

    return NextResponse.json(thresholds)
  } catch (error) {
    return NextResponse.json({ error: "Internal Server Error" }, { status: 500 })
  }
}

export async function POST(req: Request) {
  try {
    const body = await req.json()
    const {
      houseId,
      tempHighLimit,
      tempLowLimit,
      gasHighLimit,
      waterLowLimit,
      autoMode,
      telegramToken,
      telegramChatId
    } = body

    if (!houseId) {
      return NextResponse.json({ error: "Missing houseId" }, { status: 400 })
    }

    const updatedThresholds = await db.thresholds.upsert({
      where: { houseId },
      update: {
        tempHighLimit: parseFloat(tempHighLimit),
        tempLowLimit: parseFloat(tempLowLimit),
        gasHighLimit: parseFloat(gasHighLimit),
        waterLowLimit: parseFloat(waterLowLimit),
        autoMode: !!autoMode,
        telegramToken: telegramToken || null,
        telegramChatId: telegramChatId || null
      },
      create: {
        houseId,
        tempHighLimit: parseFloat(tempHighLimit) || 30.0,
        tempLowLimit: parseFloat(tempLowLimit) || 20.0,
        gasHighLimit: parseFloat(gasHighLimit) || 400.0,
        waterLowLimit: parseFloat(waterLowLimit) || 25.0,
        autoMode: autoMode !== undefined ? !!autoMode : true,
        telegramToken: telegramToken || null,
        telegramChatId: telegramChatId || null
      }
    })

    // Log configuration adjustment
    await db.alertLog.create({
      data: {
        houseId,
        type: "SETTINGS_CHANGE",
        message: `System configurations updated: Temp(${tempLowLimit}-${tempHighLimit}°C), Gas(${gasHighLimit} PPM), Mode(${autoMode ? 'AUTO' : 'MANUAL'})`,
        severity: "INFO"
      }
    })

    return NextResponse.json(updatedThresholds)
  } catch (error: any) {
    console.error("Settings update error:", error)
    return NextResponse.json({ error: "Internal Server Error", details: error.message }, { status: 500 })
  }
}
