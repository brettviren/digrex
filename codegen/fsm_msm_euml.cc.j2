#include <string>
#include <vector>
#include <iostream>

#include <boost/msm/back/state_machine.hpp>
#include <boost/msm/front/euml/euml.hpp>

using namespace std;
using namespace boost::msm::front::euml;
using namespace boost::msm::front;
namespace msm = boost::msm;
namespace
{

    // events
    {% for event in events %}
    {% if event.attrs is defined %}
    {% for a in event.attrs %}
    BOOST_MSM_EUML_DECLARE_ATTRIBUTE({{a.type}}, {{a.name}})
    {% endfor %}
    BOOST_MSM_EUML_ATTRIBUTES((attributes_ {%for a in event.attrs%}<< {{ a.name }}{%endfor%}), {{event.name}}_attributes)
    BOOST_MSM_EUML_EVENT_WITH_ATTRIBUTES({{event.name}}, {{event.name}}_attributes)
    {% else %}
    BOOST_MSM_EUML_EVENT({{event.name}})
    {% endif %}
    {% endfor %}


    // enter exit actions
    {% for action in enter_exits %}
    BOOST_MSM_EUML_ACTION({{ action.name }}) {
        template <class Event, class FSM, class STATE>
        {{ action.return if action.return is defined else "void" }} operator()(Event const&, FSM&, STATE&) {
{% if action.body.msm_euml is defined %}{{ action.body.msm_euml|indent(12) }}{% endif %}
        }
    };    
    {% endfor %}

    // transition actions
    {% for action in transitions %}
    BOOST_MSM_EUML_ACTION({{ action.name }}) {
        template <class Event, class FSM, class SrcState, class TgtState>
        {{ action.return if action.return is defined else "void" }} operator()(Event const&, FSM&, SrcState&, TgtState&) {
{% if action.body.msm_euml is defined %}{{ action.body.msm_euml|indent(12) }}{% endif %}
        }
    };    
    {% endfor %}

    // states
    {% for state in states -%}
    BOOST_MSM_EUML_STATE(({{ state.enter if state.enter is defined else "no_action" }}, {{ state.exit if state.exit is defined else "no_action" }}), {{ state.name }})
    {% endfor %}

    // machines

    {% for mach in machines -%}
    // Transition table for {{ mach.name }}
    BOOST_MSM_EUML_TRANSITION_TABLE((
        {% for r in mach.table %}
        {{ r.source }} + {{ r.event }}{{ " [" + r.guard + "]" if r.guard is defined else "" }}{{ " / " + r.action if r.action is defined else " " -}}{{ " == " + r.target if r.target is defined else "" -}}{{ "" if loop.last else "," }}
    {%- endfor %}

    ), {{ mach.name + "_transition_table" }})
    BOOST_MSM_EUML_DECLARE_STATE_MACHINE((
        {{ mach.name + "_transition_table" }},
        init_ << {{mach.initial}},
        {{ mach.enter if mach.enter is defined else "no_action"}},
        {{ mach.exit  if mach.exit  is defined else "no_action"}}
        // fixme: add support for attributes, configure and no transition handlre
    ), {{ mach.name + "_"}})
    typedef msm::back::state_machine<{{ mach.name + "_" }}> {{ mach.name }};
    // fixme: add lookup for state name
    {% endfor %}

};


int main()
{
    {{ machines[0].name }} fsm;
    fsm.start();
    {% for one in test_chain %}
    fsm.process_event({{one}});
    std::cout << fsm.current_state()[0] << std::endl;
    {% endfor %}
}
