#ifndef CORE_TIMER_H
#define CORE_TIMER_H

#include <chrono>

namespace FTS
{
    // Timer in seconds
    class FTimer
    {
        using clock = std::chrono::steady_clock;
        using seconds = std::chrono::seconds;
        using microseconds = std::chrono::microseconds;

    public:
        FTimer() { PrevTime = StartTime; }
        ~FTimer() = default;

        float Tick()
        {
            const clock::time_point CurrentTime = clock::now();
            
            const auto DeltaTime = std::chrono::duration_cast<microseconds>(CurrentTime - PrevTime).count();
            
            PrevTime = CurrentTime;
            
            return static_cast<float>(DeltaTime) / ClockSecondRatio;
        }

        float Peek() const
        {
            const clock::time_point CurrentTime = clock::now();
            
            const auto DeltaTime = std::chrono::duration_cast<microseconds>(CurrentTime - PrevTime).count();

            return static_cast<float>(DeltaTime) / ClockSecondRatio;
        }

        float Elapsed() const
        {
            const clock::time_point CurrentTime = clock::now();
            const auto ElapsedTime = std::chrono::duration_cast<microseconds>(CurrentTime - StartTime).count();
            return static_cast<float>(ElapsedTime) / ClockSecondRatio;
        }

    private:
        static constexpr float ClockSecondRatio = seconds(1) * 1.0f / microseconds(1);

        const clock::time_point StartTime = clock::now();

        clock::time_point PrevTime;  
    };
}

#endif