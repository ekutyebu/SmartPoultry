import { NextResponse } from "next/server"
import { db } from "@/lib/db"

export async function GET(req: Request) {
  try {
    const { searchParams } = new URL(req.url)
    const houseId = searchParams.get("houseId") || "poultry_house_01"
    const limit = parseInt(searchParams.get("limit") || "30")

    const alerts = await db.alertLog.findMany({
      where: { houseId },
      orderBy: { timestamp: "desc" },
      take: limit
    })

    return NextResponse.json(alerts)
  } catch (error) {
    return NextResponse.json({ error: "Internal Server Error" }, { status: 500 })
  }
}
