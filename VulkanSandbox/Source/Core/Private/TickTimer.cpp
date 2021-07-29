#include "TickTimer.h"

TickTimer::TickTimer()
    : current_time_point_(std::chrono::high_resolution_clock::now())
    , prev_time_point_(std::chrono::high_resolution_clock::now())
{
}

void TickTimer::Update()
{
    current_time_point_ = std::chrono::high_resolution_clock::now();
    const uint32_t ElapsedMicroSec = static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::microseconds>(current_time_point_ - prev_time_point_).count());
    prev_time_point_ = current_time_point_;
    time_accumulator_ += ElapsedMicroSec;

    accumulated_ticks_ = time_accumulator_ / MICROSEC_PER_TICK;
    time_accumulator_ = std::max((uint32_t)0, time_accumulator_ - (accumulated_ticks_ * MICROSEC_PER_TICK)); // Keep track of the remainder for the upcoming frames

    // Cap accumulated ticks in case of massively overshooting the target (could be the case if we stop while debugging)
    accumulated_ticks_ = std::min(accumulated_ticks_, MAX_TICKS_PER_FRAME);
}

uint32_t TickTimer::GetAccumulatedTicks() const
{
    return accumulated_ticks_;
}
