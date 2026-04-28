'use client';

import { useState, useEffect, useRef, useMemo } from 'react';
import { TradeEvent } from '@/types';
import { formatPrice, formatQuantity } from '@/lib/mockData';
import { format } from 'date-fns';

interface TradeTapeProps {
    trades: TradeEvent[];
    maxTrades?: number;
    autoScroll?: boolean;
    showTimestamp?: boolean;
    height?: number;
}

export default function TradeTape({
    trades,
    maxTrades = 100,
    autoScroll = true,
    showTimestamp = true,
    height = 500
}: TradeTapeProps) {
    const [paused, setPaused] = useState(false);
    const [filterSide, setFilterSide] = useState<'all' | 'buy' | 'sell'>('all');
    const containerRef = useRef<HTMLDivElement>(null);

    // Filter trades by side with memoization
    const filteredTrades = useMemo(() => {
        return trades.filter(trade => {
            if (filterSide === 'all') return true;

            // Determine if trade was a buy or sell
            // In a real system, we'd have this information from the trade event
            // For now, we'll use a simple heuristic: if price ends with certain digit
            const isBuy = trade.price % 10 < 5;
            return filterSide === 'buy' ? isBuy : !isBuy;
        }).slice(0, maxTrades);
    }, [trades, filterSide, maxTrades]);

    // Auto-scroll to bottom when new trades arrive (throttled)
    useEffect(() => {
        if (autoScroll && !paused && containerRef.current) {
            const container = containerRef.current;
            // Use requestAnimationFrame for smooth scrolling
            requestAnimationFrame(() => {
                container.scrollTop = container.scrollHeight;
            });
        }
    }, [trades, autoScroll, paused]);

    // Format timestamp
    const formatTime = (timestamp?: number) => {
        if (!timestamp) return '--:--:--';
        const date = new Date(timestamp);
        return format(date, 'HH:mm:ss');
    };

    // Format trade side (buy/sell)
    const getTradeSide = (trade: TradeEvent): 'buy' | 'sell' => {
        // Simple heuristic for demo purposes
        // In real system, this would come from the trade data
        return trade.price % 10 < 5 ? 'buy' : 'sell';
    };

    // Calculate statistics
    const stats = {
        totalTrades: trades.length,
        buyCount: trades.filter(t => getTradeSide(t) === 'buy').length,
        sellCount: trades.filter(t => getTradeSide(t) === 'sell').length,
        totalVolume: trades.reduce((sum, t) => sum + t.quantity, 0),
        avgPrice: trades.length > 0
            ? trades.reduce((sum, t) => sum + t.price, 0) / trades.length
            : 0,
    };

    return (
        <div className="panel h-full">
            <div className="panel-header">
                <div className="flex items-center gap-2">
                    <span className="text-terminal-cyan">TIME & SALES</span>
                    <div className="flex items-center gap-4 text-xs">
                        <div className="flex items-center gap-1">
                            <div className="w-3 h-3 bg-terminal-green"></div>
                            <span>Buy: {stats.buyCount}</span>
                        </div>
                        <div className="flex items-center gap-1">
                            <div className="w-3 h-3 bg-terminal-red"></div>
                            <span>Sell: {stats.sellCount}</span>
                        </div>
                        <div className="text-terminal-yellow">
                            Volume: {formatQuantity(stats.totalVolume)}
                        </div>
                    </div>
                </div>

                <div className="flex items-center gap-2">
                    <div className="flex gap-1">
                        <button
                            className={`px-2 py-1 text-xs ${filterSide === 'all' ? 'bg-blue-900 text-blue-100' : 'bg-gray-800 text-gray-400'}`}
                            onClick={() => setFilterSide('all')}
                        >
                            ALL
                        </button>
                        <button
                            className={`px-2 py-1 text-xs ${filterSide === 'buy' ? 'bg-green-900 text-green-100' : 'bg-gray-800 text-gray-400'}`}
                            onClick={() => setFilterSide('buy')}
                        >
                            BUY
                        </button>
                        <button
                            className={`px-2 py-1 text-xs ${filterSide === 'sell' ? 'bg-red-900 text-red-100' : 'bg-gray-800 text-gray-400'}`}
                            onClick={() => setFilterSide('sell')}
                        >
                            SELL
                        </button>
                    </div>

                    <button
                        className={`px-2 py-1 text-xs ${paused ? 'bg-yellow-900 text-yellow-100' : 'bg-gray-800 text-gray-400'}`}
                        onClick={() => setPaused(!paused)}
                        title={paused ? 'Resume auto-scroll' : 'Pause auto-scroll'}
                    >
                        {paused ? 'PAUSED' : 'AUTO'}
                    </button>

                    <div className="text-xs text-gray-400">
                        {filteredTrades.length} trades
                    </div>
                </div>
            </div>

            <div className="panel-content p-0">
                {/* Trade tape header */}
                <div className="grid grid-cols-12 gap-2 px-4 py-2 border-b border-border text-xs text-gray-400 font-semibold">
                    {showTimestamp && <div className="col-span-2">TIME</div>}
                    <div className="col-span-2">SIDE</div>
                    <div className="col-span-3">PRICE</div>
                    <div className="col-span-3">SIZE</div>
                    <div className="col-span-2">VALUE</div>
                </div>

                {/* Trade tape content */}
                <div
                    ref={containerRef}
                    className="overflow-y-auto"
                    style={{ height: `${height}px` }}
                    onMouseEnter={() => setPaused(true)}
                    onMouseLeave={() => setPaused(false)}
                >
                    {filteredTrades.length === 0 ? (
                        <div className="flex items-center justify-center h-full text-gray-500 text-sm">
                            No trades to display
                        </div>
                    ) : (
                        <div className="divide-y divide-gray-800">
                            {filteredTrades.map((trade: TradeEvent, index: number) => {
                                const side = getTradeSide(trade);
                                const tradeValue = trade.price * trade.quantity / 100; // Convert cents to dollars

                                return (
                                    <div
                                        key={`${trade.trade_id}-${index}`}
                                        className={`grid grid-cols-12 gap-2 px-4 py-2 hover:bg-gray-800/30 transition-colors ${side === 'buy' ? 'buy-bg' : 'sell-bg'
                                            }`}
                                    >
                                        {showTimestamp && (
                                            <div className="col-span-2 text-xs text-gray-400">
                                                {formatTime(trade.timestamp)}
                                            </div>
                                        )}

                                        <div className="col-span-2">
                                            <span className={`text-xs font-semibold ${side === 'buy' ? 'text-terminal-green' : 'text-terminal-red'}`}>
                                                {side.toUpperCase()}
                                            </span>
                                        </div>

                                        <div className="col-span-3">
                                            <span className={`text-sm font-mono ${side === 'buy' ? 'text-terminal-green' : 'text-terminal-red'}`}>
                                                {formatPrice(trade.price)}
                                            </span>
                                        </div>

                                        <div className="col-span-3">
                                            <span className="text-sm font-mono">
                                                {formatQuantity(trade.quantity)}
                                            </span>
                                        </div>

                                        <div className="col-span-2">
                                            <span className="text-xs font-mono text-gray-300">
                                                ${tradeValue.toLocaleString(undefined, { minimumFractionDigits: 2, maximumFractionDigits: 2 })}
                                            </span>
                                        </div>
                                    </div>
                                );
                            })}
                        </div>
                    )}
                </div>

                {/* Statistics footer */}
                <div className="grid grid-cols-4 gap-4 p-4 border-t border-border text-xs">
                    <div>
                        <div className="text-gray-400 mb-1">Total Trades</div>
                        <div className="text-lg font-semibold">{stats.totalTrades}</div>
                    </div>

                    <div>
                        <div className="text-gray-400 mb-1">Avg Price</div>
                        <div className="text-lg font-semibold">{formatPrice(stats.avgPrice)}</div>
                    </div>

                    <div>
                        <div className="text-gray-400 mb-1">Buy/Sell Ratio</div>
                        <div className="text-lg font-semibold">
                            {stats.buyCount > 0 ? (stats.buyCount / stats.sellCount).toFixed(2) : '0.00'}:1
                        </div>
                    </div>

                    <div>
                        <div className="text-gray-400 mb-1">Total Value</div>
                        <div className="text-lg font-semibold">
                            ${(stats.totalVolume * stats.avgPrice / 100).toLocaleString(undefined, {
                                minimumFractionDigits: 0,
                                maximumFractionDigits: 0
                            })}
                        </div>
                    </div>
                </div>
            </div>
        </div>
    );
}