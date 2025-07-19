#include <SDL2/SDL.h>
#include <iostream>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <chrono>
#include <cmath>
#include <stdexcept>
#include <condition_variable> // 添加条件变量

using namespace std::chrono;

// 摇杆数据结构
struct JoystickData
{
    std::vector<float> axes;
    std::vector<bool> buttons;
};

class SimpleJoystick
{
public:
    SimpleJoystick()
    {
        if (SDL_Init(SDL_INIT_JOYSTICK) < 0)
        {
            throw std::runtime_error("SDL init failed: " + std::string(SDL_GetError()));
        }

        // 打开第一个可用摇杆
        if (SDL_NumJoysticks() > 0)
        {
            // 打开设备
            joystick_ = SDL_JoystickOpen(0);
            if (joystick_)
            {
                initJoystick();
            }
        }

        // 启动事件线程
        running_ = true;
        event_thread_ = std::thread(&SimpleJoystick::eventLoop, this);
    }

    ~SimpleJoystick()
    {
        running_ = false;
        if (event_thread_.joinable())
        {
            event_thread_.join();
        }
        if (joystick_)
        {
            SDL_JoystickClose(joystick_);
        }
        SDL_Quit();
    }

    JoystickData getData()
    {
        std::lock_guard<std::mutex> lock(data_mutex_);
        return current_data_;
    }

    bool isRunning() const
    {
        return running_;
    }

    void stop()
    {
        running_ = false;
    }

private:
    void initJoystick()
    {
        // 初始化数据结构
        int num_axes = SDL_JoystickNumAxes(joystick_);
        int num_buttons = SDL_JoystickNumButtons(joystick_);

        std::lock_guard<std::mutex> lock(data_mutex_);
        current_data_.axes.resize(num_axes, 0.0f);
        current_data_.buttons.resize(num_buttons, false);

        std::cout << "Joystick connected: " << SDL_JoystickName(joystick_) << std::endl
                  << "ID: " << SDL_JoystickInstanceID(joystick_) << std::endl
                  << "Axes: " << num_axes
                  << ", Buttons: " << num_buttons << std::endl;
    }

    void eventLoop()
    {
        constexpr int POLL_INTERVAL_MS = 60;

        while (running_)
        {
            SDL_Event event;
            while (SDL_PollEvent(&event))
            {
                switch (event.type)
                {
                case SDL_JOYAXISMOTION:
                    handleAxisEvent(event.jaxis);
                    break;
                case SDL_JOYBUTTONDOWN:
                case SDL_JOYBUTTONUP:
                    handleButtonEvent(event.jbutton);
                    break;
                case SDL_JOYDEVICEADDED:
                    if (!joystick_)
                    {
                        joystick_ = SDL_JoystickOpen(event.jdevice.which);
                        if (joystick_)
                            initJoystick();
                    }
                    break;
                case SDL_JOYDEVICEREMOVED:
                    if (joystick_ && event.jdevice.which == SDL_JoystickInstanceID(joystick_))
                    {
                        SDL_JoystickClose(joystick_);
                        joystick_ = nullptr;
                        std::cout << "Joystick disconnected" << std::endl;
                    }
                    break;
                }
            }
            std::this_thread::sleep_for(milliseconds(POLL_INTERVAL_MS));
        }
    }

    void handleAxisEvent(const SDL_JoyAxisEvent &event)
    {
        if (!joystick_)
            return;

        // 标准化轴值到 [-1.0, 1.0]
        float value = static_cast<float>(event.value) / 32767.0f;
        if (value > 1.0f)
            value = 1.0f;
        if (value < -1.0f)
            value = -1.0f;

        // 应用死区过滤
        constexpr float DEADZONE = 0.1f;
        if (fabs(value) < DEADZONE)
            value = 0.0f;

        std::lock_guard<std::mutex> lock(data_mutex_);
        if (event.axis < current_data_.axes.size())
        {
            current_data_.axes[event.axis] = value;
        }
    }

    void handleButtonEvent(const SDL_JoyButtonEvent &event)
    {
        if (!joystick_)
            return;

        std::lock_guard<std::mutex> lock(data_mutex_);
        if (event.button < current_data_.buttons.size())
        {
            current_data_.buttons[event.button] = (event.state == SDL_PRESSED);
        }
    }

    SDL_Joystick *joystick_ = nullptr;
    JoystickData current_data_;
    std::mutex data_mutex_;
    std::atomic_bool running_{false};
    std::thread event_thread_;
};

// 键盘监听线程
void keyboardListener(std::atomic_bool &running, SimpleJoystick &joystick)
{
    std::cout << "\n键盘控制已启用:\n"
              << "  按 's' 暂停/继续摇杆数据采集\n"
              << "  按 'q' 退出程序\n"
              << "  按 'r' 重新连接摇杆\n"
              << "等待键盘输入..." << std::endl;

    while (running)
    {
        // 非阻塞键盘输入检测
        if (std::cin.peek() != EOF)
        {
            char cmd = std::cin.get();
            switch (cmd)
            {
            case 's': // 暂停/继续
                if (joystick.isRunning())
                {
                    joystick.stop();
                    std::cout << "摇杆数据采集已暂停" << std::endl;
                }
                else
                {
                    // 无法直接重启，需要重新创建对象
                    std::cout << "无法直接重启，请重新运行程序" << std::endl;
                }
                break;

            case 'q': // 退出
                running = false;
                joystick.stop();
                std::cout << "退出程序..." << std::endl;
                break;

            case 'r': // 重新连接
                if (!joystick.isRunning())
                {
                    std::cout << "无法重新连接，采集已停止" << std::endl;
                }
                else
                {
                    std::cout << "尝试重新连接摇杆..." << std::endl;
                    // // 在实际应用中，这里应该实现重新连接逻辑
                    // // 这里只是模拟
                    // std::cout << "摇杆重新连接功能需在完整实现中添加" << std::endl;
                }
                break;

            case '\n': // 忽略回车
                break;

            default:
                std::cout << "未知命令: " << cmd << std::endl;
                std::cout << "可用命令: s=暂停/继续, q=退出, r=重新连接" << std::endl;
            }
        }
        std::this_thread::sleep_for(milliseconds(100));
    }
}

// // 处理按钮命令的函数
// void handleButtonCommand(int buttonId)
// {
//     // 根据按钮ID执行不同的命令
//     switch (buttonId)
//     {
//     case 0: // A按钮
//         std::cout << "\n命令: A按钮按下 - " << std::endl;
//         break;
//     case 1: // B按钮
//         std::cout << "\n命令: B按钮按下 - " << std::endl;
//         break;
//     case 2: // X按钮
//         std::cout << "\n命令: X按钮按下 - " << std::endl;
//         break;
//     case 3: // Y按钮
//         std::cout << "\n命令: Y按钮按下 - " << std::endl;
//         break;
//     case 4: // LB按钮
//         std::cout << "\n命令: LB按钮按下 - " << std::endl;
//         break;
//     case 5: // RB按钮
//         std::cout << "\n命令: RB按钮按下 -  " << std::endl;
//         break;
//     default:
//         std::cout << "\n命令: 按钮" << buttonId << "按下" << std::endl;
//     }
// }

int main()
{
    try
    {
        std::atomic_bool program_running{true};
        SimpleJoystick joystick;

        // 启动键盘监听线程
        std::thread kb_thread(keyboardListener, std::ref(program_running), std::ref(joystick));

        while (program_running)
        {
            if (joystick.isRunning())
            {
                // 获取当前摇杆状态
                auto data = joystick.getData();

                // 打印轴状态
                std::cout << "Axes: [";
                for (float axis : data.axes)
                {
                    printf("%5.2f ", axis);
                }
                std::cout << "] Buttons: [";

                // 打印按钮状态
                for (bool pressed : data.buttons)
                {
                    std::cout << (pressed ? '1' : '0');
                }
                std::cout << "]        \r" << std::flush;
                // 检测按钮状态变化并发送命令
                static std::vector<bool> last_button_state = data.buttons; // 保存上一次按钮状态

                // 遍历所有按钮
                for (int i = 0; i < data.buttons.size(); i++)
                {
                    // 检测按钮按下事件
                    if (data.buttons[i] && !last_button_state[i])
                    {
                        // 根据新的按钮映射处理命令
                        if (i == 0)
                        { // X按钮 (第一位)
                            std::cout << "\n发送命令: 3 (按钮X)" << std::endl;
                        }
                        else if (i == 1)
                        { // A按钮 (第二位)
                            std::cout << "\n发送命令: 1 (按钮A)" << std::endl;
                        }
                        else if (i == 2)
                        { // B按钮 (第三位)
                            std::cout << "\n发送命令: 2 (按钮B)" << std::endl;
                        }
                        else if (i == 3)
                        { // Y按钮 (第四位)
                            std::cout << "\n发送命令: 4 (按钮Y)" << std::endl;
                        }
                    }
                }

                last_button_state = data.buttons; // 更新按钮状态
            }

            std::this_thread::sleep_for(milliseconds(10));
        }

        // 等待键盘线程结束
        if (kb_thread.joinable())
        {
            kb_thread.join();
        }

        std::cout << "\n程序已安全退出" << std::endl;
    }
    catch (const std::exception &e)
    {
        std::cerr << "错误: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
