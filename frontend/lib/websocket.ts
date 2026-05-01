import { TradeEvent, WebSocketMessage, OrderBook, LatencyMetrics, Side } from '@/types';

// WebSocket configuration
// NEXT_PUBLIC_ variables are baked into the bundle at build time by Next.js
const getWebSocketUrl = () => {
    // Env var is set at build time — present in both server and client bundles
    if (process.env.NEXT_PUBLIC_WS_URL) {
        return process.env.NEXT_PUBLIC_WS_URL;
    }
    // Default local development
    return 'ws://127.0.0.1:8080/ws';
};

const WS_CONFIG = {
    url: getWebSocketUrl(),
    reconnectInterval: 3000,
    maxReconnectAttempts: 10,
    exponentialBackoff: true,
    heartbeatInterval: 30000, // 30 seconds
};

// Binary TradeEvent decoder (matches C++ BinaryTradeEvent struct)
// struct BinaryTradeEvent {
//   uint64_t trade_id;
//   uint64_t maker_order_id;
//   uint64_t taker_order_id;
//   uint32_t timestamp;
//   uint32_t price;
//   uint32_t quantity;
//   uint8_t side;
//   uint8_t reserved[3];
// };
const BINARY_EVENT_SIZE = 40; // 3 * 8 + 3 * 4 + 1 + 3 = 40 bytes

function decodeBinaryTradeEvent(buffer: ArrayBuffer): TradeEvent {
    const view = new DataView(buffer);
    let offset = 0;

    // Read 64-bit integers (BigInt for full precision)
    const trade_id = Number(view.getBigUint64(offset, true)); // little-endian
    offset += 8;

    const maker_order_id = Number(view.getBigUint64(offset, true));
    offset += 8;

    const taker_order_id = Number(view.getBigUint64(offset, true));
    offset += 8;

    // Read 32-bit integers
    const timestamp = view.getUint32(offset, true);
    offset += 4;

    const price = view.getUint32(offset, true);
    offset += 4;

    const quantity = view.getUint32(offset, true);
    offset += 4;

    // Read side (0 = Buy, 1 = Sell)
    const sideByte = view.getUint8(offset);
    offset += 1;
    const side: Side = sideByte === 0 ? 'buy' : 'sell';

    // Skip reserved bytes
    offset += 3;

    return {
        trade_id,
        maker_order_id,
        taker_order_id,
        price,
        quantity,
        timestamp,
        side,
    };
}

// WebSocket client with reconnection and error handling
export class MatchingEngineWebSocket {
    private ws: WebSocket | null = null;
    private reconnectAttempts = 0;
    private reconnectTimer: NodeJS.Timeout | null = null;
    private heartbeatTimer: NodeJS.Timeout | null = null;
    private lastHeartbeat = 0;
    private connectionState: 'disconnected' | 'connecting' | 'connected' | 'error' = 'disconnected';
    private shouldReconnect = true; // Control whether to auto-reconnect

    // Event callbacks
    public onTrade: ((trade: TradeEvent) => void) | null = null;
    public onOrderBook: ((orderBook: OrderBook) => void) | null = null;
    public onLatency: ((latency: LatencyMetrics) => void) | null = null;
    public onOpen: (() => void) | null = null;
    public onClose: (() => void) | null = null;
    public onError: ((error: Event) => void) | null = null;
    public onStateChange: ((state: string) => void) | null = null;

    constructor(private url: string = WS_CONFIG.url) { }

    private setConnectionState(state: 'disconnected' | 'connecting' | 'connected' | 'error'): void {
        this.connectionState = state;
        this.onStateChange?.(state);
    }

    connect(): void {
        if (this.ws?.readyState === WebSocket.OPEN || this.connectionState === 'connecting') {
            return;
        }

        this.shouldReconnect = true;
        this.setConnectionState('connecting');
        console.log(`Connecting to WebSocket at ${this.url}...`);

        try {
            this.ws = new WebSocket(this.url);

            this.ws.onopen = () => {
                console.log(`✅ WebSocket connected to ${this.url}`);
                this.reconnectAttempts = 0;
                this.setConnectionState('connected');
                this.startHeartbeat();
                this.onOpen?.();
            };

            this.ws.onclose = (event) => {
                console.log(`WebSocket disconnected: code=${event.code}, reason=${event.reason}`);
                this.setConnectionState('disconnected');
                this.stopHeartbeat();
                this.onClose?.();
                this.attemptReconnect();
            };

            this.ws.onerror = (error) => {
                console.error('WebSocket error:', error);
                this.setConnectionState('error');
                this.onError?.(error);
                this.attemptReconnect();
            };

            this.ws.onmessage = (event) => {
                this.handleMessage(event);
            };

        } catch (error) {
            console.error('Failed to create WebSocket:', error);
            this.setConnectionState('error');
            this.attemptReconnect();
        }
    }

    private startHeartbeat(): void {
        this.stopHeartbeat();
        this.lastHeartbeat = Date.now();

        this.heartbeatTimer = setInterval(() => {
            const now = Date.now();
            if (now - this.lastHeartbeat > WS_CONFIG.heartbeatInterval * 2) {
                console.warn('Heartbeat timeout, reconnecting...');
                this.disconnect();
                this.attemptReconnect();
            } else if (this.ws?.readyState === WebSocket.OPEN) {
                // Send ping if supported
                this.ws.send(JSON.stringify({ type: 'ping', timestamp: now }));
            }
        }, WS_CONFIG.heartbeatInterval);
    }

    private stopHeartbeat(): void {
        if (this.heartbeatTimer) {
            clearInterval(this.heartbeatTimer);
            this.heartbeatTimer = null;
        }
    }

    private handleMessage(event: MessageEvent): void {
        try {
            // Update heartbeat timestamp on any message
            this.lastHeartbeat = Date.now();

            // Check if data is binary (ArrayBuffer) - TradeEvents
            if (event.data instanceof ArrayBuffer) {
                const buffer = event.data;

                // Check if it's a batch of TradeEvents
                if (buffer.byteLength % BINARY_EVENT_SIZE === 0) {
                    const eventCount = buffer.byteLength / BINARY_EVENT_SIZE;

                    for (let i = 0; i < eventCount; i++) {
                        const eventBuffer = buffer.slice(
                            i * BINARY_EVENT_SIZE,
                            (i + 1) * BINARY_EVENT_SIZE
                        );
                        const trade = decodeBinaryTradeEvent(eventBuffer);
                        this.onTrade?.(trade);
                    }
                } else {
                    console.warn(`Unexpected binary message size: ${buffer.byteLength} bytes`);
                }
            }
            // Check if data is JSON (text) - OrderBook, Latency, or other messages
            else if (typeof event.data === 'string') {
                try {
                    const message = JSON.parse(event.data) as WebSocketMessage;

                    switch (message.type) {
                        case 'trade_batch':
                            if (Array.isArray(message.data?.events)) {
                                const trades: TradeEvent[] = message.data.events.map((e: any) => ({
                                    trade_id: e.trade_id || Date.now(),
                                    maker_order_id: e.maker_id || 0,
                                    taker_order_id: e.taker_id || 0,
                                    price: (e.price || 0) / 100, // C++ stores price in cents
                                    quantity: e.quantity || 0,
                                    timestamp: e.timestamp || Date.now(),
                                    side: e.side || 'buy',
                                }));
                                trades.forEach(trade => this.onTrade?.(trade));
                            }
                            break;

                        case 'orderbook':
                            if (message.data) {
                                const orderBook: OrderBook = {
                                    bids: message.data.bids || [],
                                    asks: message.data.asks || [],
                                    spread: message.data.spread || 0,
                                    bestBid: message.data.bestBid || 0,
                                    bestAsk: message.data.bestAsk || 0,
                                };
                                this.onOrderBook?.(orderBook);
                            }
                            break;

                        case 'latency':
                            if (message.data) {
                                const latency: LatencyMetrics = {
                                    current: message.data.current || 0,
                                    average: message.data.average || 0,
                                    p50: message.data.p50 || 0,
                                    p90: message.data.p90 || 0,
                                    p99: message.data.p99 || 0,
                                    p999: message.data.p999 || 0,
                                    max: message.data.max || 0,
                                    min: message.data.min || 0,
                                };
                                this.onLatency?.(latency);
                            }
                            break;

                        case 'pong':
                            // Heartbeat response, no action needed
                            break;

                        case 'welcome':
                            console.log('Connected to matching engine:', message.data?.version || 'unknown');
                            break;

                        default:
                            console.warn('Unknown WebSocket message type:', message.type);
                    }
                } catch (parseError) {
                    console.error('Failed to parse JSON message:', parseError, event.data);
                }
            }
        } catch (error) {
            console.error('Error handling WebSocket message:', error);
        }
    }

    private attemptReconnect(): void {
        if (!this.shouldReconnect) {
            console.log('Auto-reconnection disabled');
            return;
        }

        if (this.reconnectAttempts >= WS_CONFIG.maxReconnectAttempts) {
            console.error(`Max reconnection attempts (${WS_CONFIG.maxReconnectAttempts}) reached`);
            this.setConnectionState('error');
            return;
        }

        this.reconnectAttempts++;

        // Exponential backoff
        const delay = WS_CONFIG.exponentialBackoff
            ? Math.min(30000, WS_CONFIG.reconnectInterval * Math.pow(1.5, this.reconnectAttempts - 1))
            : WS_CONFIG.reconnectInterval;

        console.log(`Attempting to reconnect in ${delay}ms (${this.reconnectAttempts}/${WS_CONFIG.maxReconnectAttempts})...`);

        this.reconnectTimer = setTimeout(() => {
            this.connect();
        }, delay);
    }

    disconnect(): void {
        this.shouldReconnect = false;

        if (this.reconnectTimer) {
            clearTimeout(this.reconnectTimer);
            this.reconnectTimer = null;
        }

        if (this.ws) {
            this.ws.close();
            this.ws = null;
        }
    }

    send(message: any): void {
        if (this.ws?.readyState === WebSocket.OPEN) {
            this.ws.send(JSON.stringify(message));
        }
    }

    get isConnected(): boolean {
        return this.ws?.readyState === WebSocket.OPEN;
    }
}

// Singleton instance
export const wsClient = new MatchingEngineWebSocket();