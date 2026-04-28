import { TradeEvent, OrderBook, OrderBookLevel, LatencyMetrics, MarketStats, Side } from '@/types';

// Configuration for mock data generation
const MOCK_CONFIG = {
    symbol: 'AAPL',
    minPrice: 15000, // $150.00 in cents
    maxPrice: 15500, // $155.00 in cents
    minQuantity: 1,
    maxQuantity: 1000,
    spread: 25, // $0.25 in cents
    levels: 20, // Number of price levels per side
};

// Generate a random integer between min and max (inclusive)
const randomInt = (min: number, max: number): number =>
    Math.floor(Math.random() * (max - min + 1)) + min;

// Generate a random float between min and max
const randomFloat = (min: number, max: number): number =>
    Math.random() * (max - min) + min;

// Generate mock trade events
export function generateMockTradeEvent(sequence: number): TradeEvent {
    const price = randomInt(MOCK_CONFIG.minPrice, MOCK_CONFIG.maxPrice);
    const quantity = randomInt(MOCK_CONFIG.minQuantity, MOCK_CONFIG.maxQuantity);
    const side: Side = Math.random() > 0.5 ? 'buy' : 'sell';

    return {
        trade_id: Date.now() * 1000 + sequence, // Unique-ish ID
        maker_order_id: randomInt(1000000, 9999999),
        taker_order_id: randomInt(1000000, 9999999),
        price,
        quantity,
        timestamp: Date.now(),
        side,
    };
}

// Generate mock order book
export function generateMockOrderBook(): OrderBook {
    const midPrice = (MOCK_CONFIG.minPrice + MOCK_CONFIG.maxPrice) / 2;
    const spread = MOCK_CONFIG.spread;
    const bestBid = midPrice - spread / 2;
    const bestAsk = midPrice + spread / 2;

    const bids: OrderBookLevel[] = [];
    const asks: OrderBookLevel[] = [];

    let bidCumulative = 0;
    let askCumulative = 0;

    // Generate bids (descending from best bid)
    for (let i = 0; i < MOCK_CONFIG.levels; i++) {
        const price = bestBid - i * 5; // $0.05 increments
        const quantity = randomInt(100, 5000);
        bidCumulative += quantity;

        bids.push({
            price,
            quantity,
            total: bidCumulative,
            side: 'buy',
        });
    }

    // Generate asks (ascending from best ask)
    for (let i = 0; i < MOCK_CONFIG.levels; i++) {
        const price = bestAsk + i * 5; // $0.05 increments
        const quantity = randomInt(100, 5000);
        askCumulative += quantity;

        asks.push({
            price,
            quantity,
            total: askCumulative,
            side: 'sell',
        });
    }

    return {
        bids: bids.reverse(), // Reverse so highest bid is first
        asks,
        spread,
        bestBid,
        bestAsk,
    };
}

// Generate mock latency metrics
export function generateMockLatencyMetrics(): LatencyMetrics {
    const baseLatency = randomFloat(50, 200); // 50-200 nanoseconds

    return {
        current: randomFloat(40, 300),
        average: baseLatency,
        p50: baseLatency * 0.9,
        p90: baseLatency * 1.5,
        p99: baseLatency * 2.5,
        p999: baseLatency * 4,
        max: baseLatency * 5,
        min: baseLatency * 0.8,
    };
}

// Generate mock market stats
export function generateMockMarketStats(): MarketStats {
    const lastPrice = randomInt(MOCK_CONFIG.minPrice, MOCK_CONFIG.maxPrice);
    const change = randomInt(-100, 100);
    const changePercent = (change / lastPrice) * 100;

    return {
        volume24h: randomInt(1000000, 50000000),
        trades24h: randomInt(10000, 500000),
        high24h: lastPrice + randomInt(50, 200),
        low24h: lastPrice - randomInt(50, 200),
        lastPrice,
        change,
        changePercent,
    };
}

// Format price from cents to dollars
export function formatPrice(priceInCents: number): string {
    return `$${(priceInCents / 100).toFixed(2)}`;
}

// Format quantity with commas (locale-independent)
export function formatQuantity(quantity: number): string {
    // Use simple comma formatting without locale sensitivity
    return quantity.toString().replace(/\B(?=(\d{3})+(?!\d))/g, ',');
}

// Format latency in nanoseconds with appropriate units
export function formatLatency(nanoseconds: number): string {
    if (nanoseconds < 1000) {
        return `${nanoseconds.toFixed(0)} ns`;
    } else if (nanoseconds < 1000000) {
        return `${(nanoseconds / 1000).toFixed(2)} µs`;
    } else {
        return `${(nanoseconds / 1000000).toFixed(2)} ms`;
    }
}