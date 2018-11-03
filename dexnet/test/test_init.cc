#include <iostream>
struct T {
    int x{};
    int y{1};
    int z;
};

int main() {
    T t{4,5};
    std::cout << t.x << " " << t.y << " " << t.z << std::endl;

    return 0;
}
