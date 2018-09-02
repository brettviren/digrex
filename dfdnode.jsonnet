local pasource = {
    type: "PASource",
    outputs: [
        {
            socktype : 1,
            url: "tcp://127.0.0.1:9876",
            method: "bind",
        }
    ],
    fragment_ident: 42
};

{
    role: pasource
}
