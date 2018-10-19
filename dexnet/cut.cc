    const auto *cfg = static_cast<const ToyTPSourceConfig*>(vargs);
    assert(cfg);

    auto outsock = zsock_new(cfg->outbox().type());
    assert (outsock);
    const int outport = zsock_bind(outsock, pbhelpers::socket_address(cfg->outbox()));
    assert (outport != -1);     // need to tell pipe

    auto insock = zsock_new(cfg->inbox().type());
    assert (insock);
    const int inok = zsock_connect(insock, pbhelpers::socket_address(cfg->inbox());
    assert (inok != -1);
