'use client';

import { useState, useEffect } from 'react';
import { LatencyMetrics } from '@/types';
import { formatLatency } from '@/lib/mockData';

interface LatencyDialProps {
    latency: LatencyMetrics;
    height?: number;
    showPercentiles?: boolean;
    showHistory?: boolean;
}

// Color based on latency value (green -> yellow -> red)
const getLatencyColor = (latency: number): string => {
    if (latency < 100) return '#00ff00'; // Green: excellent
    if (latency < 200) return '#aaff00'; // Lime green: good
    if (latency < 500) return '#ffff00'; // Yellow: acceptable
    if (latency < 1000) return '#ffaa00'; // Orange: warning
    return '#ff0000'; // Red: critical
};

// Calculate needle rotation (0-180 degrees)
const calculateRotation = (latency: number, maxLatency: number = 2000): number => {
    const normalized = Math.min(latency / maxLatency, 1);
    return normalized * 180; // 0-180 degrees
};

export default function LatencyDial({
    latency,
    height = 300,
    showPercentiles = true,
    showHistory = true
}: LatencyDialProps) {
    const [needleRotation, setNeedleRotation] = useState(0);
    const [history, setHistory] = useState<number[]>([]);
    const maxHistory = 50;

    // Update needle rotation when latency changes
    useEffect(() => {
        const rotation = calculateRotation(latency.current);
        setNeedleRotation(rotation);

        // Add to history
        setHistory(prev => {
            const newHistory = [...prev, latency.current];
            if (newHistory.length > maxHistory) {
                return newHistory.slice(-maxHistory);
            }
            return newHistory;
        });
    }, [latency]);

    // Calculate statistics
    const stats = {
        color: getLatencyColor(latency.current),
        status: latency.current < 100 ? 'EXCELLENT' :
            latency.current < 200 ? 'GOOD' :
                latency.current < 500 ? 'ACCEPTABLE' :
                    latency.current < 1000 ? 'WARNING' : 'CRITICAL',
        percentile95: latency.p99, // Using p99 as proxy for 95th percentile
        percentile99: latency.p999, // Using p999 as proxy for 99th percentile
    };

    return (
        <div className="panel h-full">
            <div className="panel-header">
                <div className="flex items-center gap-2">
                    <span className="text-terminal-cyan">LATENCY DIAL</span>
                    <div className="flex items-center gap-4 text-xs">
                        <div className="flex items-center gap-1">
                            <div
                                className="w-3 h-3 rounded-full animate-pulse"
                                style={{ backgroundColor: stats.color }}
                            ></div>
                            <span>Status: {stats.status}</span>
                        </div>
                        <div className="text-terminal-yellow">
                            Current: {formatLatency(latency.current)}
                        </div>
                    </div>
                </div>
                <div className="text-xs text-gray-400">
                    C++ Engine Internal Execution
                </div>
            </div>

            <div className="panel-content">
                <div className="flex flex-col lg:flex-row gap-6">
                    {/* Speedometer Gauge */}
                    <div className="flex-1">
                        <div className="relative" style={{ height: `${height}px` }}>
                            {/* Gauge background */}
                            <div className="absolute inset-0 flex items-center justify-center">
                                {/* Outer ring */}
                                <div className="absolute w-64 h-32 rounded-t-full border-4 border-gray-800 border-b-0"></div>

                                {/* Color segments */}
                                <div className="absolute w-64 h-32 rounded-t-full border-4 border-terminal-green border-b-0"
                                    style={{ clipPath: 'inset(0 0 50% 0)' }}></div>
                                <div className="absolute w-64 h-32 rounded-t-full border-4 border-terminal-yellow border-b-0"
                                    style={{ clipPath: 'inset(0 0 50% 0)', transform: 'rotate(45deg)' }}></div>
                                <div className="absolute w-64 h-32 rounded-t-full border-4 border-terminal-red border-b-0"
                                    style={{ clipPath: 'inset(0 0 50% 0)', transform: 'rotate(135deg)' }}></div>

                                {/* Labels */}
                                <div className="absolute top-4 left-1/4 text-xs text-terminal-green">100 ns</div>
                                <div className="absolute top-4 right-1/4 text-xs text-terminal-yellow">500 ns</div>
                                <div className="absolute top-12 left-4 text-xs text-terminal-red">2000 ns</div>
                                <div className="absolute top-12 right-4 text-xs text-terminal-red">∞</div>

                                {/* Needle */}
                                <div className="absolute bottom-0 left-1/2 transform -translate-x-1/2">
                                    <div
                                        className="w-1 h-32 bg-white origin-bottom transition-transform duration-300"
                                        style={{
                                            transform: `rotate(${needleRotation}deg)`,
                                            transformOrigin: 'bottom center'
                                        }}
                                    ></div>
                                    <div className="w-4 h-4 bg-white rounded-full -mt-2"></div>
                                </div>

                                {/* Center value */}
                                <div className="absolute bottom-8 left-1/2 transform -translate-x-1/2 text-center">
                                    <div className="text-3xl font-bold" style={{ color: stats.color }}>
                                        {formatLatency(latency.current)}
                                    </div>
                                    <div className="text-xs text-gray-400">Current Latency</div>
                                </div>
                            </div>
                        </div>
                    </div>

                    {/* Statistics panel */}
                    <div className="flex-1">
                        <div className="grid grid-cols-2 gap-4">
                            {/* Current latency */}
                            <div className="p-4 bg-gray-900/50 rounded border border-border">
                                <div className="text-xs text-gray-400 mb-1">Current</div>
                                <div className="text-2xl font-semibold" style={{ color: stats.color }}>
                                    {formatLatency(latency.current)}
                                </div>
                                <div className="text-xs text-gray-500">Real-time</div>
                            </div>

                            {/* Average latency */}
                            <div className="p-4 bg-gray-900/50 rounded border border-border">
                                <div className="text-xs text-gray-400 mb-1">Average</div>
                                <div className="text-2xl font-semibold text-terminal-cyan">
                                    {formatLatency(latency.average)}
                                </div>
                                <div className="text-xs text-gray-500">Mean</div>
                            </div>

                            {/* Percentiles */}
                            {showPercentiles && (
                                <>
                                    <div className="p-4 bg-gray-900/50 rounded border border-border">
                                        <div className="text-xs text-gray-400 mb-1">P50</div>
                                        <div className="text-xl font-semibold text-terminal-green">
                                            {formatLatency(latency.p50)}
                                        </div>
                                        <div className="text-xs text-gray-500">Median</div>
                                    </div>

                                    <div className="p-4 bg-gray-900/50 rounded border border-border">
                                        <div className="text-xs text-gray-400 mb-1">P90</div>
                                        <div className="text-xl font-semibold text-terminal-yellow">
                                            {formatLatency(latency.p90)}
                                        </div>
                                        <div className="text-xs text-gray-500">90th %ile</div>
                                    </div>

                                    <div className="p-4 bg-gray-900/50 rounded border border-border">
                                        <div className="text-xs text-gray-400 mb-1">P99</div>
                                        <div className="text-xl font-semibold text-terminal-red">
                                            {formatLatency(latency.p99)}
                                        </div>
                                        <div className="text-xs text-gray-500">99th %ile</div>
                                    </div>

                                    <div className="p-4 bg-gray-900/50 rounded border border-border">
                                        <div className="text-xs text-gray-400 mb-1">P99.9</div>
                                        <div className="text-xl font-semibold text-terminal-red">
                                            {formatLatency(latency.p999)}
                                        </div>
                                        <div className="text-xs text-gray-500">99.9th %ile</div>
                                    </div>
                                </>
                            )}
                        </div>

                        {/* Min/Max */}
                        <div className="grid grid-cols-2 gap-4 mt-4">
                            <div className="p-3 bg-gray-900/30 rounded border border-border">
                                <div className="text-xs text-gray-400 mb-1">Minimum</div>
                                <div className="text-lg font-semibold text-terminal-green">
                                    {formatLatency(latency.min)}
                                </div>
                            </div>

                            <div className="p-3 bg-gray-900/30 rounded border border-border">
                                <div className="text-xs text-gray-400 mb-1">Maximum</div>
                                <div className="text-lg font-semibold text-terminal-red">
                                    {formatLatency(latency.max)}
                                </div>
                            </div>
                        </div>
                    </div>
                </div>

                {/* History chart */}
                {showHistory && history.length > 0 && (
                    <div className="mt-6">
                        <div className="text-xs text-gray-400 mb-2">Latency History (Last {history.length} samples)</div>
                        <div className="h-20 relative">
                            {/* Grid lines */}
                            <div className="absolute inset-0 flex flex-col justify-between">
                                {[0, 25, 50, 75, 100].map((percent) => (
                                    <div
                                        key={percent}
                                        className="border-t border-gray-800"
                                        style={{ top: `${percent}%` }}
                                    ></div>
                                ))}
                            </div>

                            {/* Latency line */}
                            <div className="absolute inset-0 flex items-end">
                                {history.map((value, index) => {
                                    const maxValue = Math.max(...history, 1000);
                                    const heightPercent = Math.min((value / maxValue) * 100, 100);
                                    const color = getLatencyColor(value);

                                    return (
                                        <div
                                            key={index}
                                            className="flex-1 mx-px"
                                            style={{
                                                height: `${heightPercent}%`,
                                                backgroundColor: color,
                                                opacity: 0.7,
                                            }}
                                            title={`${formatLatency(value)}`}
                                        />
                                    );
                                })}
                            </div>

                            {/* Labels */}
                            <div className="absolute bottom-0 left-0 right-0 flex justify-between text-xs text-gray-500">
                                <span>0 ns</span>
                                <span>{formatLatency(Math.max(...history))}</span>
                            </div>
                        </div>
                    </div>
                )}

                {/* Performance indicators */}
                <div className="mt-6 grid grid-cols-3 gap-4">
                    <div className="text-center p-3 bg-gray-900/30 rounded">
                        <div className="text-xs text-gray-400">Engine Status</div>
                        <div className={`text-sm font-semibold ${latency.current < 500 ? 'text-terminal-green' : 'text-terminal-red'}`}>
                            {latency.current < 500 ? 'OPTIMAL' : 'DEGRADED'}
                        </div>
                    </div>

                    <div className="text-center p-3 bg-gray-900/30 rounded">
                        <div className="text-xs text-gray-400">SLA Compliance</div>
                        <div className={`text-sm font-semibold ${latency.p99 < 1000 ? 'text-terminal-green' : 'text-terminal-red'}`}>
                            {latency.p99 < 1000 ? 'WITHIN SLA' : 'VIOLATION'}
                        </div>
                    </div>

                    <div className="text-center p-3 bg-gray-900/30 rounded">
                        <div className="text-xs text-gray-400">Trend</div>
                        <div className="text-sm font-semibold text-terminal-cyan">
                            {history.length > 1 && history[history.length - 1] < history[history.length - 2] ? 'IMPROVING' : 'STABLE'}
                        </div>
                    </div>
                </div>
            </div>
        </div>
    );
}