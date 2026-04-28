'use client';

import { useState, useEffect, Suspense } from 'react';
import dynamic from 'next/dynamic';
import TradeTape from '@/components/dashboard/TradeTape';
import LatencyDial from '@/components/dashboard/LatencyDial';
import { useMatchingEngineStore } from '@/lib/store';
import { Activity, Cpu, Database, Zap, Wifi, WifiOff, RefreshCw, Power } from 'lucide-react';

// Dynamically import DepthChart with no SSR to avoid Recharts issues
const DepthChart = dynamic(() => import('@/components/dashboard/DepthChart'), {
  ssr: false,
  loading: () => (
    <div className="panel h-full">
      <div className="panel-header">
        <span className="text-terminal-cyan">DEPTH CHART</span>
        <div className="text-xs text-gray-400">Loading...</div>
      </div>
      <div className="panel-content flex items-center justify-center" style={{ height: '500px' }}>
        <div className="text-gray-500">Initializing chart...</div>
      </div>
    </div>
  ),
});

export default function Home() {
  const store = useMatchingEngineStore();
  const [currentTime, setCurrentTime] = useState<Date | null>(null);
  const [showHelp, setShowHelp] = useState(false);

  // Update current time every second (client-side only)
  useEffect(() => {
    setCurrentTime(new Date());
    const timer = setInterval(() => {
      setCurrentTime(new Date());
    }, 1000);

    return () => clearInterval(timer);
  }, []);

  // Format time for display (handles null for SSR)
  const formatTime = (date: Date | null) => {
    if (!date) return '--:--:--';
    return date.toLocaleTimeString('en-US', {
      hour12: false,
      hour: '2-digit',
      minute: '2-digit',
      second: '2-digit',
    });
  };

  // Format date for display (handles null for SSR)
  const formatDate = (date: Date | null) => {
    if (!date) return '--- --, ----';
    return date.toLocaleDateString('en-US', {
      weekday: 'short',
      year: 'numeric',
      month: 'short',
      day: 'numeric',
    });
  };

  // Keyboard shortcuts
  useEffect(() => {
    const handleKeyDown = (e: KeyboardEvent) => {
      // Ctrl+H to toggle help
      if (e.ctrlKey && e.key === 'h') {
        e.preventDefault();
        setShowHelp(prev => !prev);
      }

      // Ctrl+R to refresh data
      if (e.ctrlKey && e.key === 'r') {
        e.preventDefault();
        store.refreshOrderBook();
        store.refreshLatency();
        store.refreshMarketStats();
      }

      // Ctrl+M to toggle mock/real data
      if (e.ctrlKey && e.key === 'm') {
        e.preventDefault();
        store.toggleDataSource();
      }

      // Ctrl+C to clear trades
      if (e.ctrlKey && e.key === 'c') {
        e.preventDefault();
        store.clearTrades();
      }
    };

    window.addEventListener('keydown', handleKeyDown);
    return () => window.removeEventListener('keydown', handleKeyDown);
  }, [store]);

  return (
    <div className="h-screen flex flex-col bg-background text-foreground overflow-hidden">
      {/* Terminal Header */}
      <header className="bg-header border-b border-border px-4 py-2 flex items-center justify-between">
        <div className="flex items-center gap-4">
          <div className="flex items-center gap-2">
            <Zap className="w-5 h-5 text-terminal-yellow" />
            <h1 className="text-lg font-bold text-terminal-cyan">
              ULTRA-LOW-LATENCY MATCHING ENGINE
            </h1>
          </div>

          <div className="flex items-center gap-2 text-sm">
            <div className="flex items-center gap-1">
              <Cpu className="w-4 h-4 text-terminal-green" />
              <span>C++ Engine v1.0</span>
            </div>
            <div className="flex items-center gap-1">
              <Database className="w-4 h-4 text-terminal-blue" />
              <span>WebSocket Gateway</span>
            </div>
          </div>
        </div>

        <div className="flex items-center gap-6">
          <div className="text-right">
            <div className="text-sm font-semibold">{formatTime(currentTime)}</div>
            <div className="text-xs text-gray-400">{formatDate(currentTime)}</div>
          </div>

          <div className="flex items-center gap-2">
            <div className={`flex items-center gap-1 ${store.isConnected ? 'text-terminal-green' : 'text-terminal-red'}`}>
              {store.isConnected ? <Wifi className="w-4 h-4" /> : <WifiOff className="w-4 h-4" />}
              <span className="text-xs">{store.isConnected ? 'CONNECTED' : 'DISCONNECTED'}</span>
            </div>

            <div className="flex items-center gap-1 text-xs">
              <Activity className="w-4 h-4 text-terminal-purple" />
              <span>{store.tradeCount} trades</span>
            </div>
          </div>
        </div>
      </header>

      {/* Control Bar */}
      <div className="bg-gray-900 border-b border-border px-4 py-1 flex items-center justify-between text-xs">
        <div className="flex items-center gap-4">
          <button
            onClick={store.toggleDataSource}
            className={`px-3 py-1 rounded flex items-center gap-1 ${store.useMockData ? 'bg-yellow-900 text-yellow-100' : 'bg-green-900 text-green-100'}`}
          >
            <Power className="w-3 h-3" />
            {store.useMockData ? 'MOCK DATA' : 'LIVE DATA'}
          </button>

          <button
            onClick={store.addMockTrade}
            className="px-3 py-1 rounded bg-blue-900 text-blue-100 flex items-center gap-1"
          >
            <RefreshCw className="w-3 h-3" />
            SIM TRADE
          </button>

          <button
            onClick={store.clearTrades}
            className="px-3 py-1 rounded bg-red-900 text-red-100"
          >
            CLEAR TAPE
          </button>

          <div className="text-gray-400">
            Data Source: {store.useMockData ? 'Mock Generator' : 'WebSocket (ws://127.0.0.1:8080)'}
          </div>
        </div>

        <div className="flex items-center gap-4">
          <button
            onClick={() => setShowHelp(!showHelp)}
            className="px-3 py-1 rounded bg-gray-800 text-gray-300"
          >
            {showHelp ? 'HIDE HELP' : 'SHOW HELP (Ctrl+H)'}
          </button>

          <div className="text-gray-400">
            Press <kbd className="px-1 py-0.5 bg-gray-800 rounded text-xs">Ctrl+H</kbd> for help
          </div>
        </div>
      </div>

      {/* Help Panel */}
      {showHelp && (
        <div className="bg-gray-900 border-b border-border px-4 py-3 text-xs">
          <div className="grid grid-cols-3 gap-4">
            <div>
              <div className="text-terminal-cyan mb-1">KEYBOARD SHORTCUTS</div>
              <div className="space-y-1">
                <div><kbd className="px-1 bg-gray-800">Ctrl+H</kbd> Toggle help</div>
                <div><kbd className="px-1 bg-gray-800">Ctrl+R</kbd> Refresh data</div>
                <div><kbd className="px-1 bg-gray-800">Ctrl+M</kbd> Toggle mock/live</div>
                <div><kbd className="px-1 bg-gray-800">Ctrl+C</kbd> Clear trade tape</div>
              </div>
            </div>

            <div>
              <div className="text-terminal-cyan mb-1">DATA SOURCES</div>
              <div className="space-y-1">
                <div>• Mock Data: Generated synthetic order book & trades</div>
                <div>• Live Data: C++ Engine WebSocket (port 8080)</div>
                <div>• Trade Events: Binary WebSocket protocol</div>
                <div>• Latency: Simulated engine performance metrics</div>
              </div>
            </div>

            <div>
              <div className="text-terminal-cyan mb-1">COMPONENTS</div>
              <div className="space-y-1">
                <div>• Depth Chart: Order book visualization</div>
                <div>• Time & Sales: Real-time trade tape</div>
                <div>• Latency Dial: Engine performance monitor</div>
                <div>• Connection: WebSocket status indicator</div>
              </div>
            </div>
          </div>
        </div>
      )}

      {/* Main Dashboard Grid */}
      <main className="flex-1 grid grid-cols-1 lg:grid-cols-3 gap-4 p-4 overflow-hidden">
        {/* Left Column: Depth Chart */}
        <div className="lg:col-span-2">
          <DepthChart
            orderBook={store.orderBook}
            height={500}
          />
        </div>

        {/* Right Column: Trade Tape */}
        <div className="lg:col-span-1">
          <TradeTape
            trades={store.trades}
            height={500}
            autoScroll={true}
          />
        </div>

        {/* Bottom Row: Latency Dial */}
        <div className="lg:col-span-3">
          <LatencyDial
            latency={store.latency}
            height={300}
            showPercentiles={true}
            showHistory={true}
          />
        </div>
      </main>

      {/* Status Footer */}
      <footer className="bg-header border-t border-border px-4 py-2 flex items-center justify-between text-xs">
        <div className="flex items-center gap-6">
          <div className="flex items-center gap-2">
            <div className="w-2 h-2 rounded-full bg-terminal-green animate-pulse"></div>
            <span>ENGINE STATUS: <span className="text-terminal-green">OPERATIONAL</span></span>
          </div>

          <div className="flex items-center gap-2">
            <div className="w-2 h-2 rounded-full bg-terminal-green"></div>
            <span>MEMORY: <span className="text-terminal-green">734 MB ALLOCATED</span></span>
          </div>

          <div className="flex items-center gap-2">
            <div className="w-2 h-2 rounded-full bg-terminal-green"></div>
            <span>THROUGHPUT: <span className="text-terminal-green">{store.tradeCount} TRADES/SEC</span></span>
          </div>
        </div>

        <div className="flex items-center gap-4 text-gray-400">
          <div>SYMBOL: AAPL</div>
          <div>|</div>
          <div>PRICE RANGE: $150.00 - $155.00</div>
          <div>|</div>
          <div>TICK SIZE: $0.01</div>
          <div>|</div>
          <div>LAST UPDATE: {formatTime(currentTime)}</div>
        </div>
      </footer>
    </div>
  );
}
