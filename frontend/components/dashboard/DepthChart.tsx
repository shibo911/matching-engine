'use client';

import { useMemo, useState, useEffect } from 'react';
import {
    AreaChart,
    Area,
    XAxis,
    YAxis,
    CartesianGrid,
    Tooltip,
    ResponsiveContainer
} from 'recharts';
import { OrderBook } from '@/types';
import { formatPrice, formatQuantity } from '@/lib/mockData';

interface DepthChartProps {
    orderBook: OrderBook;
    height?: number;
    showGrid?: boolean;
    showTooltip?: boolean;
}

interface ChartDataPoint {
    price: number;
    bidVolume: number;
    askVolume: number;
    cumulativeBid: number;
    cumulativeAsk: number;
    side: 'bid' | 'ask' | 'spread';
}

export default function DepthChart({
    orderBook,
    height = 400,
    showGrid = true,
    showTooltip = true
}: DepthChartProps) {
    const [mounted, setMounted] = useState(false);
    const [containerReady, setContainerReady] = useState(false);

    useEffect(() => {
        setMounted(true);
        // Small delay to ensure container is rendered
        const timer = setTimeout(() => {
            setContainerReady(true);
        }, 50);
        return () => clearTimeout(timer);
    }, []);
    // Transform order book data for the depth chart
    const chartData = useMemo(() => {
        const data: ChartDataPoint[] = [];

        // Process bids (highest to lowest)
        let bidCumulative = 0;
        const bidLevels = [...orderBook.bids].sort((a, b) => b.price - a.price);

        for (const level of bidLevels) {
            bidCumulative += level.quantity;
            data.push({
                price: level.price,
                bidVolume: level.quantity,
                askVolume: 0,
                cumulativeBid: bidCumulative,
                cumulativeAsk: 0,
                side: 'bid',
            });
        }

        // Add spread area (empty between best bid and best ask)
        if (orderBook.bestBid && orderBook.bestAsk) {
            const spreadMid = (orderBook.bestBid + orderBook.bestAsk) / 2;
            data.push({
                price: spreadMid,
                bidVolume: 0,
                askVolume: 0,
                cumulativeBid: bidCumulative,
                cumulativeAsk: 0,
                side: 'spread',
            });
        }

        // Process asks (lowest to highest)
        let askCumulative = 0;
        const askLevels = [...orderBook.asks].sort((a, b) => a.price - b.price);

        for (const level of askLevels) {
            askCumulative += level.quantity;
            data.push({
                price: level.price,
                bidVolume: 0,
                askVolume: level.quantity,
                cumulativeBid: bidCumulative,
                cumulativeAsk: askCumulative,
                side: 'ask',
            });
        }

        return data.sort((a, b) => a.price - b.price);
    }, [orderBook]);

    // Calculate chart domain
    const { minPrice, maxPrice, maxVolume } = useMemo(() => {
        if (chartData.length === 0) {
            return { minPrice: 0, maxPrice: 0, maxVolume: 0 };
        }

        const prices = chartData.map(d => d.price);
        const volumes = chartData.map(d => Math.max(d.cumulativeBid, d.cumulativeAsk));

        return {
            minPrice: Math.min(...prices),
            maxPrice: Math.max(...prices),
            maxVolume: Math.max(...volumes) * 1.1, // Add 10% padding
        };
    }, [chartData]);

    // Custom tooltip
    const CustomTooltip = ({ active, payload }: any) => {
        if (!active || !payload || !payload.length) return null;

        const data = payload[0].payload;
        const isBid = data.side === 'bid';
        const isAsk = data.side === 'ask';

        return (
            <div className="tooltip">
                <div className="font-mono text-xs">
                    <div className="flex justify-between gap-4">
                        <span className="text-gray-400">Price:</span>
                        <span className="font-semibold">{formatPrice(data.price)}</span>
                    </div>
                    {isBid && (
                        <>
                            <div className="flex justify-between gap-4">
                                <span className="text-gray-400">Bid Volume:</span>
                                <span className="text-terminal-green">{formatQuantity(data.bidVolume)}</span>
                            </div>
                            <div className="flex justify-between gap-4">
                                <span className="text-gray-400">Cumulative:</span>
                                <span className="text-terminal-green">{formatQuantity(data.cumulativeBid)}</span>
                            </div>
                        </>
                    )}
                    {isAsk && (
                        <>
                            <div className="flex justify-between gap-4">
                                <span className="text-gray-400">Ask Volume:</span>
                                <span className="text-terminal-red">{formatQuantity(data.askVolume)}</span>
                            </div>
                            <div className="flex justify-between gap-4">
                                <span className="text-gray-400">Cumulative:</span>
                                <span className="text-terminal-red">{formatQuantity(data.cumulativeAsk)}</span>
                            </div>
                        </>
                    )}
                    {data.side === 'spread' && (
                        <div className="flex justify-between gap-4">
                            <span className="text-gray-400">Spread:</span>
                            <span className="text-terminal-yellow">{formatPrice(orderBook.spread)}</span>
                        </div>
                    )}
                </div>
            </div>
        );
    };

    // Custom axis formatters
    const formatPriceAxis = (value: number) => {
        return `$${(value / 100).toFixed(0)}`;
    };

    const formatVolumeAxis = (value: number) => {
        if (value >= 1000000) return `${(value / 1000000).toFixed(1)}M`;
        if (value >= 1000) return `${(value / 1000).toFixed(0)}K`;
        return value.toString();
    };

    return (
        <div className="panel h-full">
            <div className="panel-header">
                <div className="flex items-center gap-2">
                    <span className="text-terminal-cyan">DEPTH CHART</span>
                    <div className="flex items-center gap-4 text-xs">
                        <div className="flex items-center gap-1">
                            <div className="w-3 h-3 bg-terminal-green"></div>
                            <span>Bids: {formatPrice(orderBook.bestBid)}</span>
                        </div>
                        <div className="flex items-center gap-1">
                            <div className="w-3 h-3 bg-terminal-red"></div>
                            <span>Asks: {formatPrice(orderBook.bestAsk)}</span>
                        </div>
                        <div className="text-terminal-yellow">
                            Spread: {formatPrice(orderBook.spread)}
                        </div>
                    </div>
                </div>
                <div className="text-xs text-gray-400">
                    {chartData.length} levels
                </div>
            </div>

            <div className="panel-content p-0">
                <div style={{ height: `${height}px`, width: '100%', minHeight: '300px', minWidth: '0', position: 'relative' }}>
                    {mounted && containerReady ? (
                        <ResponsiveContainer width="100%" height="100%" minWidth={0} minHeight={300} debounce={1} aspect={1.5} key="chart-container">
                            <AreaChart
                                data={chartData}
                                margin={{ top: 10, right: 30, left: 0, bottom: 0 }}
                            >
                                {showGrid && (
                                    <CartesianGrid
                                        strokeDasharray="3 3"
                                        stroke="#1e293b"
                                        horizontal={true}
                                        vertical={false}
                                    />
                                )}

                                <XAxis
                                    dataKey="price"
                                    type="number"
                                    domain={[minPrice, maxPrice]}
                                    tickFormatter={formatPriceAxis}
                                    stroke="#475569"
                                    fontSize={11}
                                    tickLine={false}
                                    axisLine={{ stroke: '#475569' }}
                                />

                                <YAxis
                                    type="number"
                                    domain={[0, maxVolume]}
                                    tickFormatter={formatVolumeAxis}
                                    stroke="#475569"
                                    fontSize={11}
                                    tickLine={false}
                                    axisLine={{ stroke: '#475569' }}
                                    orientation="right"
                                />

                                {showTooltip && <Tooltip content={<CustomTooltip />} />}

                                {/* Bid area (green) */}
                                <Area
                                    type="monotone"
                                    dataKey="cumulativeBid"
                                    stroke="#00ff00"
                                    strokeWidth={1.5}
                                    fill="url(#bidGradient)"
                                    fillOpacity={0.6}
                                    isAnimationActive={false}
                                />

                                {/* Ask area (red) */}
                                <Area
                                    type="monotone"
                                    dataKey="cumulativeAsk"
                                    stroke="#ff0000"
                                    strokeWidth={1.5}
                                    fill="url(#askGradient)"
                                    fillOpacity={0.6}
                                    isAnimationActive={false}
                                />

                                {/* Gradients */}
                                <defs>
                                    <linearGradient id="bidGradient" x1="0" y1="0" x2="0" y2="1">
                                        <stop offset="5%" stopColor="#00ff00" stopOpacity={0.8} />
                                        <stop offset="95%" stopColor="#00ff00" stopOpacity={0.1} />
                                    </linearGradient>
                                    <linearGradient id="askGradient" x1="0" y1="0" x2="0" y2="1">
                                        <stop offset="5%" stopColor="#ff0000" stopOpacity={0.8} />
                                        <stop offset="95%" stopColor="#ff0000" stopOpacity={0.1} />
                                    </linearGradient>
                                </defs>
                            </AreaChart>
                        </ResponsiveContainer>
                    ) : (
                        <div className="flex items-center justify-center h-full text-gray-500 text-sm">
                            Loading chart...
                        </div>
                    )}
                </div>

                {/* Order book summary */}
                <div className="grid grid-cols-2 gap-4 p-4 border-t border-border">
                    <div>
                        <div className="text-xs text-gray-400 mb-1">Top Bids</div>
                        <div className="space-y-1">
                            {orderBook.bids.slice(0, 3).map((bid, i) => (
                                <div key={i} className="flex justify-between text-xs">
                                    <span className="text-terminal-green">{formatPrice(bid.price)}</span>
                                    <span>{formatQuantity(bid.quantity)}</span>
                                    <span className="text-gray-500">{formatQuantity(bid.total)}</span>
                                </div>
                            ))}
                        </div>
                    </div>

                    <div>
                        <div className="text-xs text-gray-400 mb-1">Top Asks</div>
                        <div className="space-y-1">
                            {orderBook.asks.slice(0, 3).map((ask, i) => (
                                <div key={i} className="flex justify-between text-xs">
                                    <span className="text-terminal-red">{formatPrice(ask.price)}</span>
                                    <span>{formatQuantity(ask.quantity)}</span>
                                    <span className="text-gray-500">{formatQuantity(ask.total)}</span>
                                </div>
                            ))}
                        </div>
                    </div>
                </div>
            </div>
        </div>
    );
}