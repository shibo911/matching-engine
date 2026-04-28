import { useState, useEffect, useCallback, useRef } from 'react';
import { TradeEvent, OrderBook, LatencyMetrics, MarketStats } from '@/types';
import {
    generateMockTradeEvent,
    generateMockOrderBook,
    generateMockLatencyMetrics,
    generateMockMarketStats
} from './mockData';
import { wsClient } from './websocket';

// Configuration
const CONFIG = {
    maxTrades: 100,
    orderBookUpdateInterval: 1000, // ms
    latencyUpdateInterval: 2000, // ms
    marketStatsUpdateInterval: 5000, // ms
    useMockData: true, // Use mock data by default (change to false when backend is running)
    fallbackToMock: true, // Fall back to mock data if WebSocket fails
    maxReconnectAttempts: 5,
};

// Create empty/default initial states for SSR
const emptyOrderBook: OrderBook = {
    bids: [],
    asks: [],
    spread: 0,
    bestBid: 0,
    bestAsk: 0,
};

const emptyLatency: LatencyMetrics = {
    current: 0,
    average: 0,
    p50: 0,
    p90: 0,
    p99: 0,
    p999: 0,
    max: 0,
    min: 0,
};

const emptyMarketStats: MarketStats = {
    volume24h: 0,
    trades24h: 0,
    high24h: 0,
    low24h: 0,
    lastPrice: 0,
    change: 0,
    changePercent: 0,
};

export function useMatchingEngineStore() {
    // State - initialize with empty data for SSR, will be populated on client
    const [trades, setTrades] = useState<TradeEvent[]>([]);
    const [orderBook, setOrderBook] = useState<OrderBook>(emptyOrderBook);
    const [latency, setLatency] = useState<LatencyMetrics>(emptyLatency);
    const [marketStats, setMarketStats] = useState<MarketStats>(emptyMarketStats);
    const [isConnected, setIsConnected] = useState(false);
    const [useMockData, setUseMockData] = useState(CONFIG.useMockData);
    const [isInitialized, setIsInitialized] = useState(false);

    // Refs for intervals
    const orderBookIntervalRef = useRef<NodeJS.Timeout | null>(null);
    const latencyIntervalRef = useRef<NodeJS.Timeout | null>(null);
    const marketStatsIntervalRef = useRef<NodeJS.Timeout | null>(null);
    const mockTradeIntervalRef = useRef<NodeJS.Timeout | null>(null);

    // Add a new trade to the list (maintain max size)
    const addTrade = useCallback((trade: TradeEvent) => {
        setTrades(prev => {
            const newTrades = [trade, ...prev];
            if (newTrades.length > CONFIG.maxTrades) {
                return newTrades.slice(0, CONFIG.maxTrades);
            }
            return newTrades;
        });
    }, []);

    // Initialize WebSocket connection
    useEffect(() => {
        if (!useMockData) {
            wsClient.onTrade = (trade) => {
                addTrade(trade);
            };

            wsClient.onOrderBook = (orderBook) => {
                setOrderBook(orderBook);
            };

            wsClient.onLatency = (latency) => {
                setLatency(latency);
            };

            wsClient.onOpen = () => {
                setIsConnected(true);
                console.log('Connected to matching engine WebSocket');
            };

            wsClient.onClose = () => {
                setIsConnected(false);
                console.log('Disconnected from matching engine WebSocket');
            };

            wsClient.connect();
        }

        // Always clean up WebSocket when component unmounts or useMockData changes
        return () => {
            wsClient.disconnect();
        };
    }, [useMockData, addTrade]);

    // Initialize data on client only
    useEffect(() => {
        if (useMockData && !isInitialized) {
            // Initialize with mock data
            setOrderBook(generateMockOrderBook());
            setLatency(generateMockLatencyMetrics());
            setMarketStats(generateMockMarketStats());
            setIsInitialized(true);
        }
    }, [useMockData, isInitialized]);

    // Initialize mock data intervals
    useEffect(() => {
        if (useMockData && isInitialized) {
            // Mock trade generation
            let tradeSequence = 0;
            mockTradeIntervalRef.current = setInterval(() => {
                // Generate 1-3 trades per interval
                const tradeCount = Math.floor(Math.random() * 3) + 1;
                for (let i = 0; i < tradeCount; i++) {
                    addTrade(generateMockTradeEvent(tradeSequence++));
                }
            }, 500); // Every 500ms

            // Order book updates
            orderBookIntervalRef.current = setInterval(() => {
                setOrderBook(generateMockOrderBook());
            }, CONFIG.orderBookUpdateInterval);

            // Latency updates
            latencyIntervalRef.current = setInterval(() => {
                setLatency(generateMockLatencyMetrics());
            }, CONFIG.latencyUpdateInterval);

            // Market stats updates
            marketStatsIntervalRef.current = setInterval(() => {
                setMarketStats(generateMockMarketStats());
            }, CONFIG.marketStatsUpdateInterval);
        }

        // Cleanup
        return () => {
            if (mockTradeIntervalRef.current) clearInterval(mockTradeIntervalRef.current);
            if (orderBookIntervalRef.current) clearInterval(orderBookIntervalRef.current);
            if (latencyIntervalRef.current) clearInterval(latencyIntervalRef.current);
            if (marketStatsIntervalRef.current) clearInterval(marketStatsIntervalRef.current);
        };
    }, [useMockData, addTrade, isInitialized]);

    // Toggle between mock and real data
    const toggleDataSource = useCallback(() => {
        setUseMockData(prev => !prev);
    }, []);

    // Clear all trades
    const clearTrades = useCallback(() => {
        setTrades([]);
    }, []);

    // Manually add a mock trade (for testing)
    const addMockTrade = useCallback(() => {
        addTrade(generateMockTradeEvent(Date.now()));
    }, [addTrade]);

    // Refresh order book
    const refreshOrderBook = useCallback(() => {
        setOrderBook(generateMockOrderBook());
    }, []);

    // Refresh latency metrics
    const refreshLatency = useCallback(() => {
        setLatency(generateMockLatencyMetrics());
    }, []);

    // Refresh market stats
    const refreshMarketStats = useCallback(() => {
        setMarketStats(generateMockMarketStats());
    }, []);

    return {
        // State
        trades,
        orderBook,
        latency,
        marketStats,
        isConnected,
        useMockData,

        // Actions
        addTrade,
        clearTrades,
        addMockTrade,
        refreshOrderBook,
        refreshLatency,
        refreshMarketStats,
        toggleDataSource,

        // Derived data
        lastTrade: trades[0],
        tradeCount: trades.length,
        totalVolume: trades.reduce((sum, trade) => sum + trade.quantity, 0),
    };
}

// Export store context for global state (if needed)
export type MatchingEngineStore = ReturnType<typeof useMatchingEngineStore>;