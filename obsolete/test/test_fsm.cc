
#include <map>
#include <deque>
#include <string>
#include <iostream>
#include <functional>

typedef std::string event_t;

typedef std::string client_t;

typedef std::function<event_t(client_t)> action_func_t;
typedef std::string action_name_t;
typedef std::map<action_name_t, action_func_t> action_repository_t;
typedef std::string state_t;
typedef std::pair<action_name_t, state_t> target_t;
typedef std::map<event_t, target_t> transitions_t;
typedef std::map<state_t, transitions_t> fsm_t;
typedef std::deque<event_t> event_queue_t;

event_t hello_action(client_t client)
{
    std::cerr << "hello client #" << client << std::endl;
    return "check";
}

event_t restart_action(client_t client)
{
    std::cerr << "check client #" << client << std::endl;
    return "";
}

event_t check_action(client_t client)
{
    std::cerr << "check client #" << client << std::endl;

    if (client == "malory") {
        return "no";
    }
    return "ok";
}

event_t terminate_action(client_t client)
{
    std::cerr << "terminate client #" << client << std::endl;
    return "";
}

/*
  start (join) -> [hello] -> checking.
    [hello] -> {ok, no}
  checking (ok) -> [restart] -> start.
  checking (no) -> [terminate] -> stop.

  stateA -> [action] -> stateB
             ...> event
             ...> event

 */
int main ()
{
    action_repository_t actrepo {
        {"hello",hello_action},
        {"check", check_action },
        {"restart", restart_action},
        {"terminate", terminate_action }
    };
    

    transitions_t start_trans{ { "join", {"check","checking"} } };
    transitions_t checking_trans{
        {"no", {"terminate", "stop"} },
        {"ok", {"restart", "start"} },
    };
    transitions_t stop_trans{};

    fsm_t fsm{ {"start", start_trans}, {"checking", checking_trans},  {"stop", stop_trans}};

    event_queue_t events{"join"};
    state_t state = "start";
    //client_t client = "alice";
    client_t client = "malory";
    while (true) {
        std::cerr << "State: " << state << std::endl;

        auto trans = fsm[state];
        if (trans.empty()) {
            std::cerr << "No transitions for state " << state << std::endl;
            break;
        }

        if (events.empty()) {
            std::cerr << "No more events\n";
            break;
        }
        auto event = events.front();
        events.pop_front();
        std::cerr << "Event: " << event << std::endl;

        auto tran = trans.find(event);
        if (tran == trans.end()) {
            std::cerr << "No transition for event " << event << std::endl;
            break;
        }
        auto &target = tran->second;

        auto action_name = target.first;
        if (action_name.empty()) {
            std::cerr << "No action for state " << state << " event " << event << std::endl;
        }
        else {
            std::cerr << "Action: " << action_name << std::endl;
            auto actit = actrepo.find(action_name);
            if (actit == actrepo.end()) {
                std::cerr << "no such action " << action_name << std::endl;
                break;
            }
            auto action = actit->second;
            event = action(client);
            if (!event.empty()) {
                events.push_front(event);
            }
        }
        state = target.second;
    }

    return 0;
}
