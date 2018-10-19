#include <boost/di.hpp>
#include <iostream>

namespace di = boost::di;

struct renderer {
    int device;
};

class view {
    std::string m_title;
public:
    view(std::string title, const renderer&) : m_title(title) {
        std::cout << "view: " << title <<std::endl;
    }
    std::string title() { return m_title; }
};

class model {};

class controller {
public:
    controller(model&, view& v) {
        std::cout << "view: " << v.title() <<std::endl;
    }
};

class user {};

class app {
public:
    app(controller&, user&) {}
};

int main() {
    /**
     * renderer renderer_;
     * view view_{"", renderer_};
     * model model_;
     * controller controller_{model_, view_};
     * user user_;
     * app app_{controller_, user_};
     */

    auto injector = di::make_injector(
        di::bind<std::string>().to(
        );
    injector.create<app>();
}

