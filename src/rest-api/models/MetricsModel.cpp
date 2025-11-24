#include "MetricsModel.h"
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <crow.h>

namespace DISBridge::Models
{

    BridgeMetrics::BridgeMetrics()
    {
        resources.start_time = std::chrono::steady_clock::now();
    }

    // copy atomic values
    BridgeMetrics::BridgeMetrics(const BridgeMetrics& other)
    {
        // Network metrics
        network.total_packets_sent.store(other.network.total_packets_sent.load());
        network.total_packets_received.store(other.network.total_packets_received.load());
        network.total_bytes_sent.store(other.network.total_bytes_sent.load());
        network.total_bytes_received.store(other.network.total_bytes_received.load());
        network.dropped_packets.store(other.network.dropped_packets.load());
        network.failed_transmissions.store(other.network.failed_transmissions.load());

        {
            std::lock_guard<std::mutex> lock(other.network.metrics_mutex);
            network.avg_latency_ms = other.network.avg_latency_ms;
            network.packets_per_second = other.network.packets_per_second;
            network.bytes_per_second = other.network.bytes_per_second;
        }

        // SimConnect metrics
        simconnect.connection_attempts.store(other.simconnect.connection_attempts.load());
        simconnect.successful_connections.store(other.simconnect.successful_connections.load());
        simconnect.connection_failures.store(other.simconnect.connection_failures.load());
        simconnect.data_requests.store(other.simconnect.data_requests.load());
        simconnect.data_received.store(other.simconnect.data_received.load());
        simconnect.is_connected.store(other.simconnect.is_connected.load());

        {
            std::lock_guard<std::mutex> lock(other.simconnect.timing_mutex);
            simconnect.last_data_received = other.simconnect.last_data_received;
            simconnect.avg_data_rate_hz = other.simconnect.avg_data_rate_hz;
        }

        // Processing metrics
        processing.pdus_encoded.store(other.processing.pdus_encoded.load());
        processing.pdus_decoded.store(other.processing.pdus_decoded.load());
        processing.encoding_failures.store(other.processing.encoding_failures.load());
        processing.decoding_failures.store(other.processing.decoding_failures.load());

        {
            std::lock_guard<std::mutex> lock(other.processing.timing_mutex);
            processing.avg_encoding_time_ms = other.processing.avg_encoding_time_ms;
            processing.avg_decoding_time_ms = other.processing.avg_decoding_time_ms;
            processing.avg_round_trip_time_ms = other.processing.avg_round_trip_time_ms;
        }

        // Resource metrics
        resources.cpu_usage_percent.store(other.resources.cpu_usage_percent.load());
        resources.memory_usage_mb.store(other.resources.memory_usage_mb.load());
        resources.active_threads.store(other.resources.active_threads.load());
        resources.queue_depth.store(other.resources.queue_depth.load());

        {
            std::lock_guard<std::mutex> lock(other.resources.uptime_mutex);
            resources.start_time = other.resources.start_time;
            resources.uptime = other.resources.uptime;
        }

        // Error metrics
        errors.total_errors.store(other.errors.total_errors.load());
        errors.total_warnings.store(other.errors.total_warnings.load());
        errors.critical_errors.store(other.errors.critical_errors.load());

        {
            std::lock_guard<std::mutex> lock(other.errors.recent_errors_mutex);
            errors.recent_errors = other.errors.recent_errors;
        }

        // History data
        {
            std::lock_guard<std::mutex> lock(other.history_mutex);
            latency_history = other.latency_history;
            throughput_history = other.throughput_history;
        }
    }

    // Assignment operator
    BridgeMetrics& BridgeMetrics::operator=(const BridgeMetrics& other)
    {
        if (this != &other)
        {
            // Network metrics
            network.total_packets_sent.store(other.network.total_packets_sent.load());
            network.total_packets_received.store(other.network.total_packets_received.load());
            network.total_bytes_sent.store(other.network.total_bytes_sent.load());
            network.total_bytes_received.store(other.network.total_bytes_received.load());
            network.dropped_packets.store(other.network.dropped_packets.load());
            network.failed_transmissions.store(other.network.failed_transmissions.load());

            {
                std::lock_guard<std::mutex> lock(other.network.metrics_mutex);
                network.avg_latency_ms = other.network.avg_latency_ms;
                network.packets_per_second = other.network.packets_per_second;
                network.bytes_per_second = other.network.bytes_per_second;
            }

            // SimConnect metrics
            simconnect.connection_attempts.store(other.simconnect.connection_attempts.load());
            simconnect.successful_connections.store(other.simconnect.successful_connections.load());
            simconnect.connection_failures.store(other.simconnect.connection_failures.load());
            simconnect.data_requests.store(other.simconnect.data_requests.load());
            simconnect.data_received.store(other.simconnect.data_received.load());
            simconnect.is_connected.store(other.simconnect.is_connected.load());

            {
                std::lock_guard<std::mutex> lock(other.simconnect.timing_mutex);
                simconnect.last_data_received = other.simconnect.last_data_received;
                simconnect.avg_data_rate_hz = other.simconnect.avg_data_rate_hz;
            }

            // Processing metrics
            processing.pdus_encoded.store(other.processing.pdus_encoded.load());
            processing.pdus_decoded.store(other.processing.pdus_decoded.load());
            processing.encoding_failures.store(other.processing.encoding_failures.load());
            processing.decoding_failures.store(other.processing.decoding_failures.load());

            {
                std::lock_guard<std::mutex> lock(other.processing.timing_mutex);
                processing.avg_encoding_time_ms = other.processing.avg_encoding_time_ms;
                processing.avg_decoding_time_ms = other.processing.avg_decoding_time_ms;
                processing.avg_round_trip_time_ms = other.processing.avg_round_trip_time_ms;
            }

            // Resource metrics
            resources.cpu_usage_percent.store(other.resources.cpu_usage_percent.load());
            resources.memory_usage_mb.store(other.resources.memory_usage_mb.load());
            resources.active_threads.store(other.resources.active_threads.load());
            resources.queue_depth.store(other.resources.queue_depth.load());

            {
                std::lock_guard<std::mutex> lock(other.resources.uptime_mutex);
                resources.start_time = other.resources.start_time;
                resources.uptime = other.resources.uptime;
            }

            // Error metrics
            errors.total_errors.store(other.errors.total_errors.load());
            errors.total_warnings.store(other.errors.total_warnings.load());
            errors.critical_errors.store(other.errors.critical_errors.load());

            {
                std::lock_guard<std::mutex> lock(other.errors.recent_errors_mutex);
                errors.recent_errors = other.errors.recent_errors;
            }

            // History data
            {
                std::lock_guard<std::mutex> lock(other.history_mutex);
                latency_history = other.latency_history;
                throughput_history = other.throughput_history;
            }
        }
        return *this;
    }

    void BridgeMetrics::recordPacketSent(size_t bytes)
    {
        network.total_packets_sent.fetch_add(1, std::memory_order_relaxed);
        network.total_bytes_sent.fetch_add(bytes, std::memory_order_relaxed);

        // Update throughput history
        std::lock_guard<std::mutex> lock(history_mutex);
        throughput_history.emplace_back(std::chrono::steady_clock::now(), bytes);

        // Cleanup old history if needed
        if (throughput_history.size() > MAX_HISTORY_SIZE)
        {
            throughput_history.erase(throughput_history.begin());
        }
    }

    void BridgeMetrics::recordPacketReceived(size_t bytes)
    {
        network.total_packets_received.fetch_add(1, std::memory_order_relaxed);
        network.total_bytes_received.fetch_add(bytes, std::memory_order_relaxed);

        // Update throughput history
        std::lock_guard<std::mutex> lock(history_mutex);
        throughput_history.emplace_back(std::chrono::steady_clock::now(), bytes);

        if (throughput_history.size() > MAX_HISTORY_SIZE)
        {
            throughput_history.erase(throughput_history.begin());
        }
    }

    void BridgeMetrics::recordLatency(double latency_ms)
    {
        std::lock_guard<std::mutex> lock(history_mutex);
        latency_history.emplace_back(std::chrono::steady_clock::now(), latency_ms);

        // Cleanup old latency data
        if (latency_history.size() > MAX_HISTORY_SIZE)
        {
            latency_history.erase(latency_history.begin());
        }
    }

    void BridgeMetrics::recordEncodingTime(double time_ms)
    {
        processing.pdus_encoded.fetch_add(1, std::memory_order_relaxed);

        std::lock_guard<std::mutex> lock(processing.timing_mutex);

        // Calculate exponential moving average (EMA)
        constexpr double alpha = 0.1; // Smoothing factor
        if (processing.avg_encoding_time_ms == 0.0)
        {
            processing.avg_encoding_time_ms = time_ms;
        }
        else
        {
            processing.avg_encoding_time_ms =
                alpha * time_ms + (1.0 - alpha) * processing.avg_encoding_time_ms;
        }
    }

    void BridgeMetrics::recordDecodingTime(double time_ms)
    {
        processing.pdus_decoded.fetch_add(1, std::memory_order_relaxed);

        std::lock_guard<std::mutex> lock(processing.timing_mutex);

        // Calculate exponential moving average
        constexpr double alpha = 0.1;
        if (processing.avg_decoding_time_ms == 0.0)
        {
            processing.avg_decoding_time_ms = time_ms;
        }
        else
        {
            processing.avg_decoding_time_ms =
                alpha * time_ms + (1.0 - alpha) * processing.avg_decoding_time_ms;
        }
    }

    void BridgeMetrics::recordError(const std::string& error_message)
    {
        errors.total_errors.fetch_add(1, std::memory_order_relaxed);

        std::lock_guard<std::mutex> lock(errors.recent_errors_mutex);
        errors.recent_errors.emplace_back(std::chrono::steady_clock::now(), error_message);

        // Keep only recent errors
        if (errors.recent_errors.size() > ErrorMetrics::MAX_RECENT_ERRORS)
        {
            errors.recent_errors.erase(errors.recent_errors.begin());
        }
    }

    void BridgeMetrics::recordWarning(const std::string& warning_message)
    {
        errors.total_warnings.fetch_add(1, std::memory_order_relaxed);

        std::lock_guard<std::mutex> lock(errors.recent_errors_mutex);
        errors.recent_errors.emplace_back(std::chrono::steady_clock::now(),
                                          "[WARNING] " + warning_message);

        if (errors.recent_errors.size() > ErrorMetrics::MAX_RECENT_ERRORS)
        {
            errors.recent_errors.erase(errors.recent_errors.begin());
        }
    }

    void BridgeMetrics::updateMovingAverages()
    {
        updateNetworkAverages();
        updateProcessingAverages();
        cleanupOldErrors();
    }

    void BridgeMetrics::updateNetworkAverages()
    {
        std::lock_guard<std::mutex> hist_lock(history_mutex);
        std::lock_guard<std::mutex> net_lock(network.metrics_mutex);

        auto now = std::chrono::steady_clock::now();
        auto cutoff_time = now - HISTORY_WINDOW;

        // Remove old data points
        latency_history.erase(
            std::remove_if(latency_history.begin(), latency_history.end(),
                [cutoff_time](const auto& entry) { return entry.first < cutoff_time; }),
            latency_history.end()
        );

        throughput_history.erase(
            std::remove_if(throughput_history.begin(), throughput_history.end(),
                [cutoff_time](const auto& entry) { return entry.first < cutoff_time; }),
            throughput_history.end()
        );

        // Calculate average latency
        if (!latency_history.empty())
        {
            double sum_latency = 0.0;
            for (const auto& [timestamp, latency] : latency_history)
            {
                sum_latency += latency;
            }
            network.avg_latency_ms = sum_latency / latency_history.size();
        }

        // Calculate throughput (packets/sec and bytes/sec)
        if (!throughput_history.empty() && throughput_history.size() > 1)
        {
            auto time_span = std::chrono::duration_cast<std::chrono::seconds>(
                throughput_history.back().first - throughput_history.front().first
            ).count();

            if (time_span > 0)
            {
                network.packets_per_second = static_cast<double>(throughput_history.size()) / time_span;

                size_t total_bytes = 0;
                for (const auto& [timestamp, bytes] : throughput_history)
                {
                    total_bytes += bytes;
                }
                network.bytes_per_second = static_cast<double>(total_bytes) / time_span;
            }
        }
    }

    void BridgeMetrics::updateProcessingAverages()
    {
        std::lock_guard<std::mutex> lock(processing.timing_mutex);

        // Calculate round-trip time (encoding + decoding)
        processing.avg_round_trip_time_ms =
            processing.avg_encoding_time_ms + processing.avg_decoding_time_ms;
    }

    void BridgeMetrics::cleanupOldErrors()
    {
        std::lock_guard<std::mutex> lock(errors.recent_errors_mutex);

        auto now = std::chrono::steady_clock::now();
        auto cutoff_time = now - std::chrono::hours(24); // Keep 24 hours of errors

        errors.recent_errors.erase(
            std::remove_if(errors.recent_errors.begin(), errors.recent_errors.end(),
                [cutoff_time](const auto& entry) { return entry.first < cutoff_time; }),
            errors.recent_errors.end()
        );
    }

    std::chrono::duration<double> BridgeMetrics::getUptime() const
    {
        std::lock_guard<std::mutex> lock(resources.uptime_mutex);
        return std::chrono::steady_clock::now() - resources.start_time;
    }

    std::string BridgeMetrics::toJson() const
    {
        crow::json::wvalue json;

        // Network metrics
        json["network"]["packets_sent"] = network.total_packets_sent.load();
        json["network"]["packets_received"] = network.total_packets_received.load();
        json["network"]["bytes_sent"] = network.total_bytes_sent.load();
        json["network"]["bytes_received"] = network.total_bytes_received.load();
        json["network"]["dropped_packets"] = network.dropped_packets.load();
        json["network"]["failed_transmissions"] = network.failed_transmissions.load();

        {
            std::lock_guard<std::mutex> lock(network.metrics_mutex);
            json["network"]["avg_latency_ms"] = network.avg_latency_ms;
            json["network"]["packets_per_second"] = network.packets_per_second;
            json["network"]["bytes_per_second"] = network.bytes_per_second;
        }

        // SimConnect metrics
        json["simconnect"]["connection_attempts"] = simconnect.connection_attempts.load();
        json["simconnect"]["successful_connections"] = simconnect.successful_connections.load();
        json["simconnect"]["connection_failures"] = simconnect.connection_failures.load();
        json["simconnect"]["data_requests"] = simconnect.data_requests.load();
        json["simconnect"]["data_received"] = simconnect.data_received.load();
        json["simconnect"]["is_connected"] = simconnect.is_connected.load();

        {
            std::lock_guard<std::mutex> lock(simconnect.timing_mutex);
            json["simconnect"]["avg_data_rate_hz"] = simconnect.avg_data_rate_hz;
        }

        // Processing metrics
        json["processing"]["pdus_encoded"] = processing.pdus_encoded.load();
        json["processing"]["pdus_decoded"] = processing.pdus_decoded.load();
        json["processing"]["encoding_failures"] = processing.encoding_failures.load();
        json["processing"]["decoding_failures"] = processing.decoding_failures.load();

        {
            std::lock_guard<std::mutex> lock(processing.timing_mutex);
            json["processing"]["avg_encoding_time_ms"] = processing.avg_encoding_time_ms;
            json["processing"]["avg_decoding_time_ms"] = processing.avg_decoding_time_ms;
            json["processing"]["avg_round_trip_time_ms"] = processing.avg_round_trip_time_ms;
        }

        // Resource metrics
        json["resources"]["cpu_usage_percent"] = resources.cpu_usage_percent.load();
        json["resources"]["memory_usage_mb"] = resources.memory_usage_mb.load();
        json["resources"]["active_threads"] = resources.active_threads.load();
        json["resources"]["queue_depth"] = resources.queue_depth.load();

        auto uptime_seconds = getUptime().count();
        json["resources"]["uptime_seconds"] = uptime_seconds;

        // Error metrics
        json["errors"]["total_errors"] = errors.total_errors.load();
        json["errors"]["total_warnings"] = errors.total_warnings.load();
        json["errors"]["critical_errors"] = errors.critical_errors.load();

        {
            std::lock_guard<std::mutex> lock(errors.recent_errors_mutex);
            json["errors"]["recent_error_count"] = errors.recent_errors.size();
        }

        return json.dump();
    }

}
