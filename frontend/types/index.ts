// Type definitions for the matching engine dashboard

export type Side = 'buy' | 'sell';

export interface TradeEvent {
    trade_id: number;
    maker_order_id: number;
    taker_order_id: number;
    price: number; // in cents (15025 = $150.25)
    quantity: number; // shares
    timestamp: number; // milliseconds since epoch
    side: Side; // 'buy' or 'sell'
}

export interface OrderBookLevel {
    price: number;
    quantity: number;
    total: number; // cumulative quantity
    side: Side;
}

export interface OrderBook {
    bids: OrderBookLevel[];
    asks: OrderBookLevel[];
    spread: number;
    bestBid: number;
    bestAsk: number;
}

export interface LatencyMetrics {
    current: number; // nanoseconds
    average: number;
    p50: number;
    p90: number;
    p99: number;
    p999: number;
    max: number;
    min: number;
}

export interface MarketStats {
    volume24h: number;
    trades24h: number;
    high24h: number;
    low24h: number;
    lastPrice: number;
    change: number;
    changePercent: number;
}

export interface WebSocketMessage {
    type: 'trade' | 'trade_batch' | 'orderbook' | 'latency' | 'heartbeat' | 'welcome' | 'pong';
    data: any;
    timestamp: number;
}

// Mock data generator types
export interface MockConfig {
    symbol: string;
    minPrice: number;
    maxPrice: number;
    minQuantity: number;
    maxQuantity: number;
    updateIntervalMs: number;
}