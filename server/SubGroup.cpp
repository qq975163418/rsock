//
// Created by System Administrator on 1/19/18.
//

#include <cstdlib>
#include "plog/Log.h"
#include "SubGroup.h"
#include "../bean/ConnInfo.h"
#include "SConn.h"
#include "../conn/INetGroup.h"
#include "../util/rsutil.h"

using namespace std::placeholders;

SubGroup::SubGroup(const std::string &groupId, uv_loop_t *loop, const struct sockaddr *target, INetGroup *fakeNetGroup,
                   IConn *btm, const std::string &printableStr)
        : IAppGroup(groupId, fakeNetGroup, btm, false, printableStr) {
    mLoop = loop;
    mTarget = new_addr(target);
}

void SubGroup::Close() {
    IAppGroup::Close();
    if (mTarget) {
        free(mTarget);
        mTarget = nullptr;
    }
}

int SubGroup::OnRecv(ssize_t nread, const rbuf_t &rbuf) {
    if (nread > 0) {
        ConnInfo *info = static_cast<ConnInfo *>(rbuf.data);
        assert(info);
        EncHead *head = info->head;
        assert(head);

        auto key = ConnInfo::BuildConvKey(info->dst, head->Conv());
        auto conn = ConnOfKey(key);
        if (!conn) {
            conn = newConn(key, head->Conv());
        }
        if (conn) {
            return conn->Input(nread, rbuf);
        }
        LOGD << "no such conn: " << key;
        return SendConvRst(head->Conv());
    }
    return nread;
}

IConn *SubGroup::newConn(const std::string &key, uint32_t conv) {
    IConn *conn = new SConn(key, mLoop, mTarget, conv);
    int nret = conn->Init();
    if (nret) {
        conn->Close();
        delete conn;
        return nullptr;
    }

    auto out = std::bind(&SubGroup::sconnSend, this, _1, _2);
    AddConn(conn, out, nullptr);

    LOGD << "new SConn, key: " << conn->Key();

    return conn;
}

int SubGroup::sconnSend(ssize_t nread, const rbuf_t &rbuf) {
    SConn *conn = static_cast<SConn *>(rbuf.data);
    mHead.SetConv(conn->Conv());
    const rbuf_t buf = new_buf(nread, rbuf, &mHead);
    return Send(nread, buf);
}