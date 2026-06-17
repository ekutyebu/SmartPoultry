import { NextResponse } from "next/server"
import { db } from "@/lib/db"

export async function GET(req: Request) {
  try {
    const { searchParams } = new URL(req.url)
    const houseId = searchParams.get("houseId") || "poultry_house_01"

    const schedules = await db.feedingSchedule.findMany({
      where: { houseId },
      orderBy: [
        { feedHour: "asc" },
        { feedMinute: "asc" }
      ]
    })

    return NextResponse.json(schedules)
  } catch (error) {
    return NextResponse.json({ error: "Internal Server Error" }, { status: 500 })
  }
}

export async function POST(req: Request) {
  try {
    const { houseId, feedHour, feedMinute, portionSizeSec } = await req.json()

    if (!houseId || feedHour === undefined || feedMinute === undefined) {
      return NextResponse.json({ error: "Missing parameters" }, { status: 400 })
    }

    const schedule = await db.feedingSchedule.create({
      data: {
        houseId,
        feedHour: parseInt(feedHour),
        feedMinute: parseInt(feedMinute),
        portionSizeSec: parseInt(portionSizeSec) || 2,
        isActive: true
      }
    })

    return NextResponse.json(schedule)
  } catch (error) {
    return NextResponse.json({ error: "Internal Server Error" }, { status: 500 })
  }
}

export async function DELETE(req: Request) {
  try {
    const { searchParams } = new URL(req.url)
    const id = searchParams.get("id")

    if (!id) {
      return NextResponse.json({ error: "Missing schedule ID" }, { status: 400 })
    }

    await db.feedingSchedule.delete({
      where: { id }
    })

    return NextResponse.json({ success: true })
  } catch (error) {
    return NextResponse.json({ error: "Internal Server Error" }, { status: 500 })
  }
}
