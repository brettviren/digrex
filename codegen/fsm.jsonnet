{
    name:: function(obj) obj.name,

    state:: function(name, enter=null, exit=null, body="") {
        type:'state',
        name:name, enter:enter, exit:exit, body: body
    },
    event:: function(name, attrs={}, body="") { name: name, attrs: attrs, body: body },
    action:: function(name, body="") { name: name, body: body },

    unique_helper(l, x):: if std.count(l,x) == 0 then l + [x] else l,
    unique_list(l):: std.foldl($.unique_helper, l, []),

    // this follows order defined by Boost MSM
    machine_states:: function(m)
    self.unique_list([ r.source for r in m.table ]
                     + [ r.target for r in m.table if std.objectHas(r,'target')]),
    machine_actions:: function(m)
    self.unique_list([ r.action for r in m.table if std.objectHas(r,'action')]),


    test: self.machine_states({table:[
        {source:"foo",target:"bar"},
        {source:"baz"},
        {source:"foo",target:"quax"},
    ]})
}


