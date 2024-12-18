#ifndef CORE_TIMER_H
#define CORE_TIMER_H

#include <chrono>

namespace fantasy
{
    // Timer in seconds
    class Timer
    {
        using clock = std::chrono::steady_clock;
        using seconds = std::chrono::seconds;
        using microseconds = std::chrono::microseconds;

    public:
        Timer() { _prev_time = _start_time; }
        ~Timer() = default;

        float tick()
        {
            const clock::time_point current_time = clock::now();
            
            const auto time_delta = std::chrono::duration_cast<microseconds>(current_time - _prev_time).count();
            
            _prev_time = current_time;
            
            return static_cast<float>(time_delta) / CLOCK_SECOND_RATIO;
        }

        float peek() const
        {
            const clock::time_point current_time = clock::now();
            
            const auto time_delta = std::chrono::duration_cast<microseconds>(current_time - _prev_time).count();

            return static_cast<float>(time_delta) / CLOCK_SECOND_RATIO;
        }

        float elapsed() const
        {
            const clock::time_point current_time = clock::now();
            const auto time_elapsed = std::chrono::duration_cast<microseconds>(current_time - _start_time).count();
            return static_cast<float>(time_elapsed) / CLOCK_SECOND_RATIO;
        }

    private:
        static constexpr float CLOCK_SECOND_RATIO = seconds(1) * 1.0f / microseconds(1);

        const clock::time_point _start_time = clock::now();

        clock::time_point _prev_time;  
    };
}

#endif