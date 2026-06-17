"use client"

import { useState, useEffect } from "react"
import { 
  Thermometer, 
  Droplets, 
  Wind, 
  Trash2, 
  AlertTriangle, 
  Settings, 
  Clock, 
  Activity, 
  Zap, 
  RefreshCw,
  Bell,
  Cpu,
  CornerDownRight,
  Database,
  Send
} from "lucide-react"
import { 
  AreaChart, 
  Area, 
  XAxis, 
  YAxis, 
  CartesianGrid, 
  Tooltip, 
  ResponsiveContainer,
  LineChart,
  Line
} from "recharts"

interface Telemetry {
  id: string
  temperature: number
  humidity: number
  gasLevel: number
  waterLevel: number
  feedLevelLow: boolean
  fanState: boolean
  heaterState: boolean
  pumpState: boolean
  feederActive: boolean
  dhtFault: boolean
  gasFault: boolean
  waterFault: boolean
  pumpSafetyTripped: boolean
  timestamp: string
}

interface Thresholds {
  tempHighLimit: number
  tempLowLimit: number
  gasHighLimit: number
  waterLowLimit: number
  autoMode: boolean
  telegramToken: string
  telegramChatId: string
}

interface FeedingSchedule {
  id: string
  feedHour: number
  feedMinute: number
  portionSizeSec: number
  isActive: boolean
}

interface AlertLog {
  id: string
  type: string
  severity: string
  message: string
  timestamp: string
}

export default function Dashboard() {
  const houseId = "poultry_house_01"
  const [mounted, setMounted] = useState(false)
  const [loading, setLoading] = useState(true)
  const [syncing, setSyncing] = useState(false)
  
  // States
  const [telemetryHistory, setTelemetryHistory] = useState<Telemetry[]>([])
  const [latestData, setLatestData] = useState<Telemetry | null>(null)
  const [thresholds, setThresholds] = useState<Thresholds>({
    tempHighLimit: 30,
    tempLowLimit: 20,
    gasHighLimit: 400,
    waterLowLimit: 25,
    autoMode: true,
    telegramToken: "",
    telegramChatId: ""
  })
  const [schedules, setSchedules] = useState<FeedingSchedule[]>([])
  const [alerts, setAlerts] = useState<AlertLog[]>([])

  // Form Inputs
  const [editThresholds, setEditThresholds] = useState<Thresholds | null>(null)
  const [newSchedule, setNewSchedule] = useState({ hour: 8, minute: 0, duration: 2 })
  const [submittingSettings, setSubmittingSettings] = useState(false)

  // Mount logic to prevent SSR issues with charts
  useEffect(() => {
    setMounted(true)
    fetchData()
    const interval = setInterval(fetchData, 4000) // Poll every 4 seconds
    return () => clearInterval(interval)
  }, [])

  const fetchData = async () => {
    try {
      setSyncing(true)
      // 1. Fetch recent telemetry
      const telRes = await fetch(`/api/telemetry?houseId=${houseId}&limit=30`)
      if (telRes.ok) {
        const data = await telRes.json()
        setTelemetryHistory(data)
        if (data.length > 0) {
          setLatestData(data[data.length - 1])
        }
      }

      // 2. Fetch thresholds
      const setRes = await fetch(`/api/settings?houseId=${houseId}`)
      if (setRes.ok) {
        const data = await setRes.json()
        setThresholds(data)
        if (!editThresholds) setEditThresholds(data)
      }

      // 3. Fetch feeding schedules
      const schRes = await fetch(`/api/schedules?houseId=${houseId}`)
      if (schRes.ok) {
        setSchedules(await schRes.json())
      }

      // 4. Fetch recent alerts
      const alRes = await fetch(`/api/alerts?houseId=${houseId}&limit=12`)
      if (alRes.ok) {
        setAlerts(await alRes.json())
      }

      setLoading(false)
    } catch (err) {
      console.error("Error fetching dashboard data:", err)
    } finally {
      setSyncing(false)
    }
  }

  // Actuator Trigger Override
  const sendActuatorCommand = async (actuator: string, command: string) => {
    try {
      const res = await fetch("/api/commands", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ houseId, actuator, command })
      })
      if (res.ok) {
        fetchData()
      }
    } catch (err) {
      console.error("Error sending override command:", err)
    }
  }

  // Save System settings
  const handleSaveSettings = async (e: React.FormEvent) => {
    e.preventDefault()
    if (!editThresholds) return
    setSubmittingSettings(true)
    try {
      const res = await fetch("/api/settings", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ houseId, ...editThresholds })
      })
      if (res.ok) {
        const saved = await res.json()
        setThresholds(saved)
      }
    } catch (err) {
      console.error("Error saving thresholds settings:", err)
    } finally {
      setSubmittingSettings(false)
    }
  }

  // Add a new Feeding time
  const handleAddSchedule = async (e: React.FormEvent) => {
    e.preventDefault()
    try {
      const res = await fetch("/api/schedules", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({
          houseId,
          feedHour: newSchedule.hour,
          feedMinute: newSchedule.minute,
          portionSizeSec: newSchedule.duration
        })
      })
      if (res.ok) {
        setNewSchedule({ hour: 8, minute: 0, duration: 2 })
        fetchData()
      }
    } catch (err) {
      console.error("Error creating feeding schedule:", err)
    }
  }

  // Delete feeding schedule
  const handleDeleteSchedule = async (id: string) => {
    try {
      const res = await fetch(`/api/schedules?id=${id}`, { method: "DELETE" })
      if (res.ok) {
        fetchData()
      }
    } catch (err) {
      console.error("Error deleting feeding schedule:", err)
    }
  }

  if (loading) {
    return (
      <div className="flex flex-col items-center justify-center min-h-screen bg-[#060913] text-gray-200">
        <Cpu className="w-12 h-12 text-blue-500 animate-pulse mb-4" />
        <h1 className="text-xl font-bold font-mono tracking-wider">CONNECTING TO SMART POULTRY SYSTEM</h1>
        <p className="text-sm text-gray-400 mt-2">Contacting central database...</p>
      </div>
    )
  }

  const formatChartTime = (timeStr: string) => {
    try {
      const d = new Date(timeStr)
      return d.toLocaleTimeString([], { hour: '2-digit', minute: '2-digit', second: '2-digit' })
    } catch {
      return ""
    }
  }

  return (
    <div className="min-h-screen bg-[#060913] text-gray-100 font-sans grid-bg pb-12">
      {/* Header bar */}
      <header className="border-b border-[#1f2937]/50 bg-black/40 backdrop-blur-md sticky top-0 z-50 px-4 sm:px-6 py-3 sm:py-4">
        <div className="max-w-7xl mx-auto flex flex-col sm:flex-row justify-between items-center gap-4 sm:gap-0">
          <div className="flex items-center space-x-3 w-full sm:w-auto justify-center sm:justify-start">
            <div className="w-3 h-3 bg-emerald-500 rounded-full animate-ping" />
            <div>
              <h1 className="text-xl sm:text-2xl font-bold font-outfit tracking-wide bg-gradient-to-r from-blue-400 to-indigo-400 bg-clip-text text-transparent">
                PoultryControl Pro
              </h1>
              <p className="text-[10px] sm:text-xs text-gray-400">IoT Autonomous Poultry Farm Controller &bull; House 01</p>
            </div>
          </div>
          
          <div className="flex items-center space-x-4 w-full sm:w-auto justify-center sm:justify-end">
            <span className="text-xs bg-slate-900 border border-slate-800 text-slate-400 px-3 py-1 rounded-full flex items-center gap-1.5">
              <Database className="w-3.5 h-3.5" /> PostgreSQL Connected
            </span>
            <button 
              onClick={fetchData}
              className={`p-2 rounded-lg bg-gray-900 border border-gray-800 hover:bg-gray-800 transition ${syncing ? 'animate-spin' : ''}`}
            >
              <RefreshCw className="w-4 h-4 text-blue-400" />
            </button>
          </div>
        </div>
      </header>

      <main className="max-w-7xl mx-auto px-4 sm:px-6 mt-6 sm:mt-8 grid grid-cols-1 lg:grid-cols-3 gap-6 sm:gap-8">
        {/* Left Column: Real-time gauges and manual triggers */}
        <div className="lg:col-span-2 space-y-8">
          
          {/* Failsafe Safety Alert */}
          {latestData?.pumpSafetyTripped && (
            <div className="p-4 rounded-xl bg-red-950/40 border border-red-500/50 text-red-200 flex items-start space-x-3 animate-pulse">
              <AlertTriangle className="w-6 h-6 text-red-500 flex-shrink-0 mt-0.5" />
              <div className="flex-grow">
                <h3 className="font-bold text-red-400">CRITICAL SAFETY: WATER PUMP DISK OVERLOAD</h3>
                <p className="text-xs text-red-300">The drinking water pump was auto-disabled after exceeding its safe continuous run timer (30s). Verify physical farm plumbing status and reset the latch.</p>
                <button
                  onClick={() => sendActuatorCommand("RESET_PUMP", "RESET")}
                  className="mt-3 px-4 py-1.5 bg-red-600 hover:bg-red-700 text-white rounded-lg font-semibold text-xs transition shadow-lg"
                >
                  RESET SAFETY VALVE LATCH
                </button>
              </div>
            </div>
          )}

          {/* Gauges Grid */}
          <div className="grid grid-cols-1 sm:grid-cols-2 xl:grid-cols-4 gap-4 sm:gap-6">
            
            {/* Temperature card */}
            <div className="glass-panel glass-panel-hover rounded-2xl p-5 relative overflow-hidden">
              <div className="flex justify-between items-center">
                <span className="text-sm font-semibold text-gray-400">Temperature</span>
                <Thermometer className="w-5 h-5 text-red-400" />
              </div>
              <div className="mt-4 flex items-baseline">
                <span className="text-4xl font-extrabold font-outfit text-white">
                  {!latestData ? "--" : latestData.dhtFault ? "ERR" : `${latestData.temperature.toFixed(1)}°C`}
                </span>
              </div>
              <div className="mt-2 flex items-center space-x-2">
                <span className={`w-2.5 h-2.5 rounded-full ${
                  !latestData ? 'bg-gray-600' :
                  latestData.dhtFault ? 'bg-red-500' :
                  latestData.temperature > thresholds.tempHighLimit ? 'bg-amber-500 animate-ping' :
                  latestData.temperature < thresholds.tempLowLimit ? 'bg-blue-500' : 'bg-emerald-500'
                }`} />
                <span className="text-xs text-gray-400">
                  {!latestData ? 'No Data' :
                   latestData.dhtFault ? 'Sensor Failure' :
                   latestData.temperature > thresholds.tempHighLimit ? 'Ventilating (High)' :
                   latestData.temperature < thresholds.tempLowLimit ? 'Heating (Low)' : 'Optimal Climate'}
                </span>
              </div>
            </div>

            {/* Humidity card */}
            <div className="glass-panel glass-panel-hover rounded-2xl p-5 relative overflow-hidden">
              <div className="flex justify-between items-center">
                <span className="text-sm font-semibold text-gray-400">Humidity</span>
                <Droplets className="w-5 h-5 text-blue-400" />
              </div>
              <div className="mt-4 flex items-baseline">
                <span className="text-4xl font-extrabold font-outfit text-white">
                  {!latestData ? "--" : latestData.dhtFault ? "ERR" : `${latestData.humidity.toFixed(1)}%`}
                </span>
              </div>
              <div className="mt-2 flex items-center space-x-2">
                <span className={`w-2.5 h-2.5 rounded-full ${
                  !latestData ? 'bg-gray-600' :
                  latestData.dhtFault ? 'bg-red-500' :
                  latestData.humidity > 75 ? 'bg-orange-500' :
                  latestData.humidity < 45 ? 'bg-orange-400' : 'bg-emerald-500'
                }`} />
                <span className="text-xs text-gray-400">
                  {!latestData ? 'No Data' :
                   latestData.dhtFault ? 'Sensor Failure' :
                   latestData.humidity > 75 ? 'Wet environment' :
                   latestData.humidity < 45 ? 'Dry environment' : 'Comfortable'}
                </span>
              </div>
            </div>

            {/* Gas Level card */}
            <div className="glass-panel glass-panel-hover rounded-2xl p-5 relative overflow-hidden">
              <div className="flex justify-between items-center">
                <span className="text-sm font-semibold text-gray-400">Ammonia / Gas</span>
                <Wind className="w-5 h-5 text-emerald-400" />
              </div>
              <div className="mt-4 flex items-baseline">
                <span className="text-4xl font-extrabold font-outfit text-white">
                  {!latestData ? "--" : latestData.gasFault ? "ERR" : `${latestData.gasLevel.toFixed(0)} PPM`}
                </span>
              </div>
              <div className="mt-2 flex items-center space-x-2">
                <span className={`w-2.5 h-2.5 rounded-full ${
                  !latestData ? 'bg-gray-600' :
                  latestData.gasFault ? 'bg-red-500' :
                  latestData.gasLevel > thresholds.gasHighLimit ? 'bg-red-500 animate-ping' : 'bg-emerald-500'
                }`} />
                <span className="text-xs text-gray-400">
                  {!latestData ? 'No Data' :
                   latestData.gasFault ? 'Sensor Failure' :
                   latestData.gasLevel > thresholds.gasHighLimit ? 'Toxic level detected!' : 'Clean air'}
                </span>
              </div>
            </div>

            {/* Water Level card */}
            <div className="glass-panel glass-panel-hover rounded-2xl p-5 relative overflow-hidden">
              <div className="flex justify-between items-center">
                <span className="text-sm font-semibold text-gray-400">Drinking Water</span>
                <Droplets className="w-5 h-5 text-indigo-400" />
              </div>
              <div className="mt-4 flex items-baseline">
                <span className="text-4xl font-extrabold font-outfit text-white">
                  {!latestData ? "--" : latestData.waterFault ? "ERR" : `${latestData.waterLevel.toFixed(0)}%`}
                </span>
              </div>
              <div className="mt-2 flex items-center space-x-2">
                <span className={`w-2.5 h-2.5 rounded-full ${
                  !latestData ? 'bg-gray-600' :
                  latestData.waterFault ? 'bg-red-500' :
                  latestData.waterLevel < thresholds.waterLowLimit ? 'bg-red-500 animate-ping' : 'bg-emerald-500'
                }`} />
                <span className="text-xs text-gray-400">
                  {!latestData ? 'No Data' :
                   latestData.waterFault ? 'Sensor Failure' :
                   latestData.waterLevel < thresholds.waterLowLimit ? 'Low (refilling...)' : 'Tank sufficient'}
                </span>
              </div>
            </div>

          </div>

          {/* Actuator Controls Panel */}
          <div className="glass-panel rounded-2xl p-4 sm:p-6">
            <div className="flex flex-col sm:flex-row justify-between items-start sm:items-center gap-4 mb-6">
              <div>
                <h2 className="text-lg font-bold font-outfit text-white">Actuators Override Desk</h2>
                <p className="text-xs text-gray-400">Local automation handles operations automatically unless switched to Manual.</p>
              </div>
              <div className="flex items-center space-x-2">
                <span className="text-xs font-mono text-gray-400">AUTOMATION MODE</span>
                <button
                  onClick={async () => {
                    const res = await fetch("/api/settings", {
                      method: "POST",
                      headers: { "Content-Type": "application/json" },
                      body: JSON.stringify({ houseId, ...thresholds, autoMode: !thresholds.autoMode })
                    })
                    if (res.ok) fetchData()
                  }}
                  className={`px-4 py-1.5 rounded-full font-bold text-xs transition-colors duration-200 border ${
                    thresholds.autoMode 
                      ? 'bg-indigo-600/30 border-indigo-500/50 text-indigo-300' 
                      : 'bg-amber-600/30 border-amber-500/50 text-amber-300'
                  }`}
                >
                  {thresholds.autoMode ? 'AUTO MODE' : 'MANUAL OVERRIDE'}
                </button>
              </div>
            </div>

            <div className="grid grid-cols-1 sm:grid-cols-2 xl:grid-cols-4 gap-4 sm:gap-6">
              {/* Fan Control */}
              <div className="p-4 rounded-xl bg-black/30 border border-[#1f2937]/50 flex flex-col justify-between">
                <div>
                  <div className="flex justify-between items-center mb-1">
                    <span className="text-sm font-semibold text-gray-400">Cooling Fan</span>
                    <Wind className={`w-4.5 h-4.5 text-blue-400 ${latestData?.fanState ? 'animate-spin' : ''}`} />
                  </div>
                  <span className={`text-xs font-bold font-mono px-2 py-0.5 rounded ${
                    !latestData ? 'bg-gray-950 text-gray-400' :
                    latestData.fanState ? 'bg-emerald-950 text-emerald-400' : 'bg-red-950 text-red-400'
                  }`}>
                    {!latestData ? 'UNKNOWN' : latestData.fanState ? 'RUNNING' : 'STOPPED'}
                  </span>
                </div>
                <div className="mt-4 flex space-x-2">
                  <button
                    disabled={thresholds.autoMode || !latestData}
                    onClick={() => sendActuatorCommand("FAN", "ON")}
                    className={`flex-grow py-1 rounded text-xs font-bold bg-emerald-600 hover:bg-emerald-700 disabled:opacity-30 disabled:cursor-not-allowed`}
                  >
                    ON
                  </button>
                  <button
                    disabled={thresholds.autoMode || !latestData}
                    onClick={() => sendActuatorCommand("FAN", "OFF")}
                    className={`flex-grow py-1 rounded text-xs font-bold bg-red-600 hover:bg-red-700 disabled:opacity-30 disabled:cursor-not-allowed`}
                  >
                    OFF
                  </button>
                </div>
              </div>

              {/* Heater Control */}
              <div className="p-4 rounded-xl bg-black/30 border border-[#1f2937]/50 flex flex-col justify-between">
                <div>
                  <div className="flex justify-between items-center mb-1">
                    <span className="text-sm font-semibold text-gray-400">Heating Lamp</span>
                    <Zap className={`w-4.5 h-4.5 text-orange-400 ${latestData?.heaterState ? 'animate-pulse' : ''}`} />
                  </div>
                  <span className={`text-xs font-bold font-mono px-2 py-0.5 rounded ${
                    !latestData ? 'bg-gray-950 text-gray-400' :
                    latestData.heaterState ? 'bg-emerald-950 text-emerald-400' : 'bg-red-950 text-red-400'
                  }`}>
                    {!latestData ? 'UNKNOWN' : latestData.heaterState ? 'ON' : 'OFF'}
                  </span>
                </div>
                <div className="mt-4 flex space-x-2">
                  <button
                    disabled={thresholds.autoMode || !latestData}
                    onClick={() => sendActuatorCommand("HEATER", "ON")}
                    className={`flex-grow py-1 rounded text-xs font-bold bg-emerald-600 hover:bg-emerald-700 disabled:opacity-30 disabled:cursor-not-allowed`}
                  >
                    ON
                  </button>
                  <button
                    disabled={thresholds.autoMode || !latestData}
                    onClick={() => sendActuatorCommand("HEATER", "OFF")}
                    className={`flex-grow py-1 rounded text-xs font-bold bg-red-600 hover:bg-red-700 disabled:opacity-30 disabled:cursor-not-allowed`}
                  >
                    OFF
                  </button>
                </div>
              </div>

              {/* Pump Control */}
              <div className="p-4 rounded-xl bg-black/30 border border-[#1f2937]/50 flex flex-col justify-between">
                <div>
                  <div className="flex justify-between items-center mb-1">
                    <span className="text-sm font-semibold text-gray-400">Water Pump</span>
                    <Droplets className="w-4.5 h-4.5 text-indigo-400" />
                  </div>
                  <span className={`text-xs font-bold font-mono px-2 py-0.5 rounded ${
                    !latestData ? 'bg-gray-950 text-gray-400' :
                    latestData.pumpState ? 'bg-emerald-950 text-emerald-400' : 'bg-red-950 text-red-400'
                  }`}>
                    {!latestData ? 'UNKNOWN' : latestData.pumpState ? 'REFILLING' : 'IDLE'}
                  </span>
                </div>
                <div className="mt-4 flex space-x-2">
                  <button
                    disabled={thresholds.autoMode || !latestData || latestData.pumpSafetyTripped}
                    onClick={() => sendActuatorCommand("PUMP", "ON")}
                    className={`flex-grow py-1 rounded text-xs font-bold bg-emerald-600 hover:bg-emerald-700 disabled:opacity-30 disabled:cursor-not-allowed`}
                  >
                    ON
                  </button>
                  <button
                    disabled={thresholds.autoMode || !latestData || latestData.pumpSafetyTripped}
                    onClick={() => sendActuatorCommand("PUMP", "OFF")}
                    className={`flex-grow py-1 rounded text-xs font-bold bg-red-600 hover:bg-red-700 disabled:opacity-30 disabled:cursor-not-allowed`}
                  >
                    OFF
                  </button>
                </div>
              </div>

              {/* Feeder Controller (Trigger only) */}
              <div className="p-4 rounded-xl bg-black/30 border border-[#1f2937]/50 flex flex-col justify-between">
                <div>
                  <div className="flex justify-between items-center mb-1">
                    <span className="text-sm font-semibold text-gray-400">Feed Silo</span>
                    <Clock className="w-4.5 h-4.5 text-yellow-400" />
                  </div>
                  <span className={`text-xs font-bold font-mono px-2 py-0.5 rounded ${
                    !latestData ? 'bg-gray-950 text-gray-400' :
                    latestData.feedLevelLow ? 'bg-red-950 text-red-400' : 'bg-emerald-950 text-emerald-400'
                  }`}>
                    {!latestData ? 'UNKNOWN' : latestData.feedLevelLow ? 'LOW FEED' : 'NORMAL'}
                  </span>
                </div>
                <div className="mt-4">
                  <button
                    disabled={!latestData}
                    onClick={() => sendActuatorCommand("FEEDER", "1500")}
                    className={`w-full py-1.5 rounded text-xs font-bold bg-blue-600 hover:bg-blue-700 transition disabled:opacity-30`}
                  >
                    DISPENSE PORTION
                  </button>
                </div>
              </div>

            </div>
          </div>

          {/* Historical Trends Charts */}
          {mounted && (
            <div className="space-y-8">
              {/* Climate Chart */}
              <div className="glass-panel rounded-2xl p-6">
                <div className="flex items-center space-x-2 mb-6">
                  <Activity className="w-5 h-5 text-blue-400" />
                  <h2 className="text-lg font-bold font-outfit text-white">Climate Parameters (Temp & Humidity)</h2>
                </div>
                <div className="h-64">
                  <ResponsiveContainer width="100%" height="100%">
                    <AreaChart data={telemetryHistory} margin={{ top: 10, right: 10, left: -20, bottom: 0 }}>
                      <defs>
                        <linearGradient id="colorTemp" x1="0" y1="0" x2="0" y2="1">
                          <stop offset="5%" stopColor="#ef4444" stopOpacity={0.2}/>
                          <stop offset="95%" stopColor="#ef4444" stopOpacity={0}/>
                        </linearGradient>
                        <linearGradient id="colorHum" x1="0" y1="0" x2="0" y2="1">
                          <stop offset="5%" stopColor="#3b82f6" stopOpacity={0.2}/>
                          <stop offset="95%" stopColor="#3b82f6" stopOpacity={0}/>
                        </linearGradient>
                      </defs>
                      <CartesianGrid strokeDasharray="3 3" stroke="rgba(255,255,255,0.05)" />
                      <XAxis dataKey="timestamp" tickFormatter={formatChartTime} stroke="#4b5563" style={{ fontSize: '10px' }} />
                      <YAxis stroke="#4b5563" style={{ fontSize: '10px' }} />
                      <Tooltip 
                        contentStyle={{ backgroundColor: '#111827', borderColor: '#374151', borderRadius: '8px' }}
                        labelFormatter={(label) => new Date(label).toLocaleString()}
                      />
                      <Area type="monotone" dataKey="temperature" name="Temp (°C)" stroke="#ef4444" fillOpacity={1} fill="url(#colorTemp)" strokeWidth={2} />
                      <Area type="monotone" dataKey="humidity" name="Humid (%)" stroke="#3b82f6" fillOpacity={1} fill="url(#colorHum)" strokeWidth={2} />
                    </AreaChart>
                  </ResponsiveContainer>
                </div>
              </div>

              {/* Ammonia & Water Chart */}
              <div className="glass-panel rounded-2xl p-6">
                <div className="flex items-center space-x-2 mb-6">
                  <Activity className="w-5 h-5 text-emerald-400" />
                  <h2 className="text-lg font-bold font-outfit text-white">Air Quality (Ammonia) & Water Level</h2>
                </div>
                <div className="h-64">
                  <ResponsiveContainer width="100%" height="100%">
                    <LineChart data={telemetryHistory} margin={{ top: 10, right: 10, left: -20, bottom: 0 }}>
                      <CartesianGrid strokeDasharray="3 3" stroke="rgba(255,255,255,0.05)" />
                      <XAxis dataKey="timestamp" tickFormatter={formatChartTime} stroke="#4b5563" style={{ fontSize: '10px' }} />
                      <YAxis stroke="#4b5563" style={{ fontSize: '10px' }} />
                      <Tooltip 
                        contentStyle={{ backgroundColor: '#111827', borderColor: '#374151', borderRadius: '8px' }}
                        labelFormatter={(label) => new Date(label).toLocaleString()}
                      />
                      <Line type="monotone" dataKey="gasLevel" name="Ammonia (PPM)" stroke="#10b981" strokeWidth={2} dot={false} />
                      <Line type="monotone" dataKey="waterLevel" name="Water (%)" stroke="#818cf8" strokeWidth={2} dot={false} />
                    </LineChart>
                  </ResponsiveContainer>
                </div>
              </div>
            </div>
          )}
        </div>

        {/* Right Column: Settings, Schedules, Alerts */}
        <div className="space-y-8">

          {/* Threshold Configurations Card */}
          <div className="glass-panel rounded-2xl p-6">
            <div className="flex items-center space-x-2 mb-4 border-b border-[#1f2937]/50 pb-3">
              <Settings className="w-5 h-5 text-blue-400" />
              <h2 className="text-lg font-bold font-outfit text-white">Automation Settings</h2>
            </div>
            
            {editThresholds && (
              <form onSubmit={handleSaveSettings} className="space-y-4">
                <div className="grid grid-cols-2 gap-4">
                  <div>
                    <label className="text-xs text-gray-400 block mb-1">Max Temp (°C)</label>
                    <input 
                      type="number" 
                      step="0.1" 
                      value={editThresholds.tempHighLimit}
                      onChange={e => setEditThresholds({...editThresholds, tempHighLimit: parseFloat(e.target.value)})}
                      className="w-full bg-black/40 border border-slate-800 rounded-lg p-2 text-sm text-white focus:outline-none focus:ring-1 focus:ring-blue-500" 
                    />
                  </div>
                  <div>
                    <label className="text-xs text-gray-400 block mb-1">Min Temp (°C)</label>
                    <input 
                      type="number" 
                      step="0.1" 
                      value={editThresholds.tempLowLimit}
                      onChange={e => setEditThresholds({...editThresholds, tempLowLimit: parseFloat(e.target.value)})}
                      className="w-full bg-black/40 border border-slate-800 rounded-lg p-2 text-sm text-white focus:outline-none focus:ring-1 focus:ring-blue-500" 
                    />
                  </div>
                </div>

                <div className="grid grid-cols-2 gap-4">
                  <div>
                    <label className="text-xs text-gray-400 block mb-1">Max Gas (PPM)</label>
                    <input 
                      type="number" 
                      value={editThresholds.gasHighLimit}
                      onChange={e => setEditThresholds({...editThresholds, gasHighLimit: parseInt(e.target.value)})}
                      className="w-full bg-black/40 border border-slate-800 rounded-lg p-2 text-sm text-white focus:outline-none focus:ring-1 focus:ring-blue-500" 
                    />
                  </div>
                  <div>
                    <label className="text-xs text-gray-400 block mb-1">Low Water Limit (%)</label>
                    <input 
                      type="number" 
                      value={editThresholds.waterLowLimit}
                      onChange={e => setEditThresholds({...editThresholds, waterLowLimit: parseInt(e.target.value)})}
                      className="w-full bg-black/40 border border-slate-800 rounded-lg p-2 text-sm text-white focus:outline-none focus:ring-1 focus:ring-blue-500" 
                    />
                  </div>
                </div>

                <div className="border-t border-slate-800/60 pt-3 mt-2">
                  <label className="text-xs text-indigo-400 block mb-1">Telegram Integration</label>
                  <div className="space-y-3">
                    <div>
                      <label className="text-[10px] text-gray-400 block mb-0.5">Bot Token</label>
                      <input 
                        type="password" 
                        placeholder="Telegram Bot Token"
                        value={editThresholds.telegramToken || ""}
                        onChange={e => setEditThresholds({...editThresholds, telegramToken: e.target.value})}
                        className="w-full bg-black/40 border border-slate-800 rounded-lg p-2 text-xs text-white focus:outline-none focus:ring-1 focus:ring-blue-500" 
                      />
                    </div>
                    <div>
                      <label className="text-[10px] text-gray-400 block mb-0.5">Chat ID</label>
                      <input 
                        type="text" 
                        placeholder="Telegram Chat ID"
                        value={editThresholds.telegramChatId || ""}
                        onChange={e => setEditThresholds({...editThresholds, telegramChatId: e.target.value})}
                        className="w-full bg-black/40 border border-slate-800 rounded-lg p-2 text-xs text-white focus:outline-none focus:ring-1 focus:ring-blue-500" 
                      />
                    </div>
                  </div>
                </div>

                <button 
                  type="submit" 
                  disabled={submittingSettings}
                  className="w-full py-2 bg-blue-600 hover:bg-blue-700 disabled:bg-blue-800 text-white rounded-lg text-sm font-bold mt-4 transition shadow-lg flex items-center justify-center space-x-2"
                >
                  <Send className="w-3.5 h-3.5" />
                  <span>{submittingSettings ? 'Saving...' : 'Sync Configurations'}</span>
                </button>
              </form>
            )}
          </div>

          {/* Feeding Schedules Manager */}
          <div className="glass-panel rounded-2xl p-6">
            <div className="flex items-center space-x-2 mb-4 border-b border-[#1f2937]/50 pb-3">
              <Clock className="w-5 h-5 text-yellow-400" />
              <h2 className="text-lg font-bold font-outfit text-white">Silo Feeding Scheduler</h2>
            </div>

            {/* List Active Schedules */}
            <div className="space-y-3 max-h-48 overflow-y-auto mb-4 pr-1">
              {schedules.length === 0 ? (
                <p className="text-xs text-gray-400 italic">No scheduled feed cycles registered.</p>
              ) : (
                schedules.map((sch) => (
                  <div key={sch.id} className="flex justify-between items-center bg-black/20 border border-slate-800 p-2.5 rounded-lg">
                    <div className="flex items-center space-x-2">
                      <Clock className="w-3.5 h-3.5 text-gray-400" />
                      <span className="text-sm font-bold text-white">
                        {sch.feedHour.toString().padStart(2, "0")}:{sch.feedMinute.toString().padStart(2, "0")}
                      </span>
                      <span className="text-[10px] text-gray-400">({sch.portionSizeSec}s dispense)</span>
                    </div>
                    <button
                      onClick={() => handleDeleteSchedule(sch.id)}
                      className="p-1 rounded text-red-400 hover:bg-red-950/50 hover:text-red-300 transition"
                    >
                      <Trash2 className="w-3.5 h-3.5" />
                    </button>
                  </div>
                ))
              )}
            </div>

            {/* Add Schedule Form */}
            <form onSubmit={handleAddSchedule} className="border-t border-slate-800/60 pt-4 space-y-3">
              <div className="grid grid-cols-3 gap-2">
                <div>
                  <label className="text-[10px] text-gray-400 block mb-0.5">Hour (0-23)</label>
                  <input 
                    type="number" 
                    min="0" 
                    max="23"
                    value={newSchedule.hour}
                    onChange={e => setNewSchedule({...newSchedule, hour: parseInt(e.target.value)})}
                    className="w-full bg-black/40 border border-slate-800 rounded-lg p-2 text-xs text-center text-white" 
                  />
                </div>
                <div>
                  <label className="text-[10px] text-gray-400 block mb-0.5">Min (0-59)</label>
                  <input 
                    type="number" 
                    min="0" 
                    max="59"
                    value={newSchedule.minute}
                    onChange={e => setNewSchedule({...newSchedule, minute: parseInt(e.target.value)})}
                    className="w-full bg-black/40 border border-slate-800 rounded-lg p-2 text-xs text-center text-white" 
                  />
                </div>
                <div>
                  <label className="text-[10px] text-gray-400 block mb-0.5">Dispense (sec)</label>
                  <input 
                    type="number" 
                    min="1" 
                    max="10"
                    value={newSchedule.duration}
                    onChange={e => setNewSchedule({...newSchedule, duration: parseInt(e.target.value)})}
                    className="w-full bg-black/40 border border-slate-800 rounded-lg p-2 text-xs text-center text-white" 
                  />
                </div>
              </div>
              <button 
                type="submit" 
                className="w-full py-1.5 bg-yellow-600 hover:bg-yellow-700 text-white rounded-lg text-xs font-bold transition"
              >
                Add Scheduled Feed Trigger
              </button>
            </form>
          </div>

          {/* Incident Warning Logs */}
          <div className="glass-panel rounded-2xl p-6">
            <div className="flex items-center space-x-2 mb-4 border-b border-[#1f2937]/50 pb-3">
              <Bell className="w-5 h-5 text-red-400" />
              <h2 className="text-lg font-bold font-outfit text-white">Live Incident Audit</h2>
            </div>
            
            <div className="space-y-3 max-h-60 overflow-y-auto pr-1">
              {alerts.length === 0 ? (
                <p className="text-xs text-gray-400 italic">No events or warning logs recorded.</p>
              ) : (
                alerts.map((al) => (
                  <div key={al.id} className="text-xs border-b border-slate-800/40 pb-2 flex items-start space-x-2">
                    <span className={`w-2 h-2 rounded-full mt-1.5 flex-shrink-0 ${
                      al.severity === 'CRITICAL' ? 'bg-red-500 animate-pulse' :
                      al.severity === 'WARNING' ? 'bg-amber-500' : 'bg-blue-400'
                    }`} />
                    <div className="flex-grow">
                      <div className="flex justify-between text-[10px] text-gray-400 mb-0.5">
                        <span className="font-mono">{al.type}</span>
                        <span>{new Date(al.timestamp).toLocaleTimeString()}</span>
                      </div>
                      <p className="text-gray-200 leading-relaxed">{al.message}</p>
                    </div>
                  </div>
                ))
              )}
            </div>
          </div>

        </div>
      </main>
    </div>
  )
}
