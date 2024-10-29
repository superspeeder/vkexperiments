#include "app.hpp"

int main() {
    {
        const auto app = std::make_unique<vke::App>();
        app->run();
    }
    return 0;
}
