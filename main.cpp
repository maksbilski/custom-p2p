#include <thread>
#include <memory>
#include <iostream>
#include <atomic>
#include <csignal>
#include <string>
#include <stdexcept>

class UserInterface {
public:
    UserInterface() = default;

    void run() {
        std::cout << "\nP2P File Sharing System - Commands:\n"
                  << "1. List local resources\n"
                  << "2. List network resources\n"
                  << "3. Add local resource\n"
                  << "4. Download resource\n"
                  << "5. Exit\n"
                  << "\nEnter command: ";

        std::string input;
        std::getline(std::cin, input);

        try {
            int choice = std::stoi(input);
            switch(choice) {
                case 1:
                    std::cout << "Local resources would be listed here\n";
                    break;
                case 2:
                    std::cout << "Network resources would be listed here\n";
                    break;
                case 3:
                    std::cout << "Adding local resource functionality would be here\n";
                    break;
                case 4:
                    std::cout << "Download functionality would be here\n";
                    break;
                case 5:
                    running = false;
                    break;
                default:
                    std::cout << "Invalid command\n";
            }
        } catch (const std::invalid_argument&) {
            std::cout << "Please enter a valid number\n";
        }
    }
    static std::atomic<bool> running;


private:
    friend void signalHandler(int);
};

std::atomic<bool> UserInterface::running{true};

void signalHandler(int signum) {
    std::cout << "\nShutdown signal received. Cleaning up...\n";
    UserInterface::running = false;
}

int main() {
    try {
        std::signal(SIGINT, signalHandler);
        std::signal(SIGTERM, signalHandler);

        auto ui = std::make_unique<UserInterface>();

        while (UserInterface::running) {
            ui->run();
        }

        std::cout << "Application shutdown complete.\n";
        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
}
