// jsonnet -S -m . test_super.jsonnet
// supervisord -c supervisor.conf
// supervisorctl -c supervisor.conf status
// supervisorctl -c supervisor.conf stop foo
// supervisorctl -c supervisor.conf shutdown

local superconf = {
    sections: {
        unix_http_server: {
            file: "%(here)s/supervisor.sock",
        },
        inet_http_server: {
            port: "0.0.0.0:9002"
        },
        supervisord: {
            logfile: "%(here)s/supervisor.log",
            loglevel: "info",
            pidfile: "%(here)s/supervisor.pid",
          
            
        },
        supervisorctl : {
            serverurl : "http://127.0.0.1:9002",
        },
        "rpcinterface:supervisor" : {
            "supervisor.rpcinterface_factory" : "supervisor.rpcinterface:make_main_rpcinterface"
        },
        "program:foo" : {
            command: "/bin/cat",
        },
    },
};


{
    "supervisor.conf": std.manifestIni(superconf)
}
