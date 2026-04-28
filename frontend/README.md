# Ultra-Low-Latency Matching Engine Dashboard

A Bloomberg Terminal/Binance Pro-style dashboard for visualizing real-time data from a C++ matching engine.

## Features

### 1. **Depth Chart**
- Live order book visualization with bids (green) and asks (red)
- Cumulative volume display
- Interactive tooltips with price and volume details
- Top bids/asks summary

### 2. **Time & Sales (Trade Tape)**
- Scrolling list of real-time executed trades
- Color-coded buy/sell indicators
- Auto-scroll with pause on hover
- Trade statistics and filtering

### 3. **Latency Dial**
- Speedometer-style gauge showing engine latency in nanoseconds
- Percentile metrics (P50, P90, P99, P99.9)
- Performance history chart
- SLA compliance indicators

### 4. **Bloomberg Terminal UI**
- Dark mode with professional trading terminal aesthetic
- Monospace fonts for data density
- Resizable grid layout
- Real-time status indicators
- Keyboard shortcuts

## Tech Stack

- **Frontend**: Next.js 15 (App Router) + TypeScript + Tailwind CSS
- **Visualization**: Recharts for depth chart
- **Real-time**: WebSocket client for C++ engine integration
- **Icons**: Lucide React
- **Date Formatting**: date-fns

## Data Sources

### Mock Data Mode
- Generates realistic order book with 20 price levels per side
- Simulates trade events with realistic timestamps
- Creates latency metrics with percentile distributions
- Perfect for development and demonstration

### Live Data Mode
- Connects to C++ matching engine WebSocket (port 8080)
- Supports binary TradeEvent protocol (32-byte struct)
- Also supports JSON format from WebSocketBroadcaster
- Automatic reconnection with exponential backoff

## Getting Started

### Prerequisites
- Node.js 18+ and npm
- C++ matching engine running on port 8080 (optional)

### Installation

```bash
cd frontend
npm install
```

### Development

```bash
npm run dev
```

Open [http://localhost:3000](http://localhost:3000) in your browser.

### Production Build

```bash
npm run build
npm start
```

## Keyboard Shortcuts

| Shortcut | Action |
|----------|--------|
| `Ctrl+H` | Toggle help panel |
| `Ctrl+R` | Refresh all data |
| `Ctrl+M` | Toggle between mock and live data |
| `Ctrl+C` | Clear trade tape |

## Architecture

### Data Flow
```
C++ Matching Engine → WebSocket (port 8080) → WebSocket Client → Data Store → React Components
```

### Component Structure
```
app/
├── layout.tsx          # Root layout with Bloomberg theme
├── page.tsx            # Main dashboard with grid layout
├── components/dashboard/
│   ├── DepthChart.tsx  # Order book visualization
│   ├── TradeTape.tsx   # Time & sales display
│   └── LatencyDial.tsx # Engine performance monitor
└── lib/
    ├── store.ts        # React state management
    ├── websocket.ts    # WebSocket client with binary decoder
    └── mockData.ts     # Mock data generators
```

## Integration with C++ Engine

The dashboard is designed to work with the matching engine's WebSocket gateway:

1. **Binary Protocol**: 32-byte `TradeEvent` struct
   ```cpp
   struct BinaryTradeEvent {
       uint64_t trade_id;
       uint64_t maker_order_id;
       uint64_t taker_order_id;
       uint32_t price;    // in cents
       uint32_t quantity; // shares
   };
   ```

2. **JSON Protocol**: Alternative format from WebSocketBroadcaster
   ```json
   {
     "type": "trade_batch",
     "events": [
       {
         "maker_id": 12345,
         "taker_id": 67890,
         "price": 15025,
         "quantity": 100,
         "timestamp": 1234567890
       }
     ]
   }
   ```

## Configuration

Edit `frontend/lib/store.ts` to adjust:
- `useMockData`: Toggle between mock and real data
- Update intervals for mock data generation
- WebSocket connection URL
- Maximum trade history size

## Performance Optimizations

- **Virtualized Lists**: Trade tape uses efficient rendering
- **Memoization**: Expensive calculations are memoized
- **Canvas Rendering**: Depth chart uses Recharts canvas backend
- **Throttled Updates**: Data updates are rate-limited
- **WebSocket Binary**: Efficient binary protocol for high-frequency data

## Future Enhancements

1. **Order Entry**: Submit orders directly from the UI
2. **Market Depth Heatmap**: Visualize order density
3. **Advanced Analytics**: Statistical analysis of market data
4. **Multi-symbol Support**: Monitor multiple trading pairs
5. **Alert System**: Custom price and volume alerts
6. **Historical Data**: View past market activity

## License

Proprietary - Part of the Ultra-Low-Latency Matching Engine project

## Acknowledgments

- Inspired by Bloomberg Terminal and Binance Pro interfaces
- Designed for high-frequency trading visualization
- Built with performance and data density in mind
