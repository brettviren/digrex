{
    name:: function(obj) obj.name,

    unique_helper(l, x):: if std.count(l,x) == 0 then l + [x] else l,
    unique_list(l):: std.foldl($.unique_helper, l, []),

    machine_states:: function(m)
    self.unique_list([ r.source for r in m.table ]
                     + [ r.target for r in m.table if std.objectHas(r,'target')]),

    test: self.machine_states({table:[
        {source:"foo",target:"bar"},
        {source:"baz"},
        {source:"foo",target:"quax"},
    ]})
}


