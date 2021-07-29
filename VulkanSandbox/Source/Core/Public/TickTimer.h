#pragma once
#include <chrono>

class TickTimer
{
public:
    TickTimer();

    void Update();
    uint32_t GetAccumulatedTicks() const;

    static inline constexpr uint32_t MILLISEC_PER_TICK = 16;
    static inline constexpr uint32_t MICROSEC_PER_TICK = 16666;
    static inline constexpr uint32_t MAX_TICKS_PER_FRAME = 2048;

private:
    std::chrono::high_resolution_clock::time_point current_time_point_;
    std::chrono::high_resolution_clock::time_point prev_time_point_;

    uint32_t time_accumulator_ = 0;
    uint32_t accumulated_ticks_ = 0;
};
