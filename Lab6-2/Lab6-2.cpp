#include <iostream>
#include <coroutine>
#include <deque>
#include <numeric>
#include <chrono>

struct AverageGenerator {
    struct promise_type;
    using handle_type = std::coroutine_handle<promise_type>;

    handle_type coro_handle;

    AverageGenerator(handle_type h) : coro_handle(h) {}
    ~AverageGenerator() { if (coro_handle) coro_handle.destroy(); }

    struct promise_type {
        double current_average = 0.0;

        AverageGenerator get_return_object() {
            return AverageGenerator{ handle_type::from_promise(*this) };
        }
        std::suspend_always initial_suspend() { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }
        void return_void() {}
        void unhandled_exception() { std::terminate(); }

        std::suspend_always yield_value(double value) {
            current_average = value;
            return {};
        }
    };

    bool resume() {
        if (!coro_handle || coro_handle.done()) return false;
        coro_handle.resume();
        return !coro_handle.done();
    }

    double get_result() const {
        return coro_handle.promise().current_average;
    }
};

AverageGenerator calculate_average_coro() {
    std::deque<double> history;
    double input;

    auto last_zero_time = std::chrono::steady_clock::time_point::min();
    bool zero_flag_active = false;

    while (true) {
        if (zero_flag_active) {
            auto now = std::chrono::steady_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_zero_time).count();

            if (duration < 1000) {
                std::cout << "\n[VIOLATION] Resumed too fast (" << duration << "ms < 1000ms)! Logic check failed.\n";
                co_return;
            }
            else {
                std::cout << "\n[OK] Time passed (" << duration << "ms). Resuming work...\n";
                zero_flag_active = false;
            }
        }

        std::cout << "\nEnter number: ";
        if (!(std::cin >> input)) break;

        if (input == 0.0) {
            std::cout << "-> Zero detected. Cooldown 1 sec activated." << std::endl;
            zero_flag_active = true;
            last_zero_time = std::chrono::steady_clock::now();

            double avg = history.empty() ? 0.0 : std::accumulate(history.begin(), history.end(), 0.0) / history.size();

            co_yield avg;
            continue;
        }

        history.push_back(input);
        if (history.size() > 3) history.pop_front();

        double sum = std::accumulate(history.begin(), history.end(), 0.0);
        co_yield history.empty() ? 0.0 : sum / history.size();
    }
}



int main() {
    std::cout << "--- Coroutine Lab: Manual Control ---\n";
    std::cout << "1. Enter numbers to count average.\n";
    std::cout << "2. If you enter '0', the coroutine pauses.\n";
    std::cout << "3. YOU must decide when to resume it by pressing ENTER.\n\n";

    auto generator = calculate_average_coro();

    generator.resume();

    while (true) {
        
        std::cout << " -> Yielded Average: " << generator.get_result() << std::endl;

        std::cout << ">>> Press ENTER to attempt resume (wait 1s if previous was 0)...";

        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        std::cin.get();

        bool active = generator.resume();

        if (!active) {
            std::cout << "Coroutine finished (or terminated due to violation)." << std::endl;
            break;
        }
    }

    return 0;
}