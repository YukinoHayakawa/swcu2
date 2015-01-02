/*
 * Copyright 2014 Yukino Hayakawa<tennencoll@gmail.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <memory>

#include "../Interface/DialogManager.hpp"
#include "../Player/PlayerManager.hpp"
#include "Crew.hpp"
#include "CrewManager.hpp"

#include "CrewDialogs.hpp"

namespace swcu {

CrewControlPanelDialog::CrewControlPanelDialog(int playerid) :
    MenuDialog(playerid, "帮派")
{
}

void CrewControlPanelDialog::build()
{
    auto p          = PlayerManager::get().getPlayer(mPlayerId);
    if(p == nullptr)
    {
        return;
    }
    int playerid    = mPlayerId;
    mongo::OID oid  = p->getId();
    if(p->isCrewMember())
    {
        auto crew = CrewManager::get().getCrew(p->getCrew());
        mongo::OID crewid = crew->getId();
        addItem("我的帮派: " + crew->getColoredName(), [](){});
        if(p->getId() != crew->getLeader())
            addItem("退出帮派", std::bind(&Crew::removeMember, crew, oid));
        else
        {
            addItem("更改帮派名称", [playerid, crewid]() {
                DialogManager::get().push<CrewChangeNameDialog>(
                    playerid, crewid);
            });
            addItem("查看帮派成员", [playerid, crewid]() {
                DialogManager::get().push<CrewViewMembersDialog>(
                    playerid, crewid);
            });
        }
    }
    else
    {
        addItem("创建帮派", [playerid]() {
            DialogManager::get().push<CreateCrewDialog>(playerid);
        });
        addItem("加入帮派", [playerid, oid]() {
            DialogManager::get().push<CrewFindByNameDialog>(
            playerid, [playerid, oid](const mongo::OID& crew) {
                if(CrewManager::get().getCrew(crew)->applyToJoin(oid))
                {
                    SendClientMessage(playerid, 0xFFFFFFFF,
                        "加入申请已发送.");
                    return true;
                }
                return false;
            });
        });
    }
}

CrewViewMembersDialog::CrewViewMembersDialog(
    int playerid, const mongo::OID& crew) :
    ItemListDialog<std::string>(playerid, "帮派成员"), mCrew(crew)
{
}

void CrewViewMembersDialog::build()
{
    MONGO_WRAPPER({
        auto doc        = getDBConn()->findOne(
            Config::colNameCrew,
            BSON("_id" << mCrew)
        );
        auto members    = doc["members"].Obj();
        auto it         = mongo::BSONObjIterator(members);
        for(; it.more(); ++it)
        {
            auto member                 = it.next();
            std::string memberIdStr     = member.fieldName();
            mongo::OID memberId         = mongo::OID(memberIdStr);
            auto prof                   = getDBConn()->findOne(
                Config::colNamePlayer,
                BSON("_id" << memberId)
            );
            std::string pName           = prof["logname"].str();
            std::stringstream msg;
            msg << getCrewHierarchyStr(
                CrewHierarchy(member.numberInt()))
                << "\t" << pName;
            addItem(memberIdStr, msg.str());
        }
    });
}

bool CrewViewMembersDialog::process(std::string key)
{
    DialogManager::get().push<CrewEditMemberDialog>(
        mPlayerId, mCrew, mongo::OID(key));
    return true;
}

CrewEditMemberDialog::CrewEditMemberDialog(int playerid,
    const mongo::OID& crew, const mongo::OID& member) :
    MenuDialog(playerid, "编辑成员"), mCrew(crew), mMember(member)
{
}

void CrewEditMemberDialog::build()
{
    auto crew       = CrewManager::get().getCrew(mCrew);
    if(crew->getLeader() == mMember) return;
    Player p(mMember);
    auto hier       = crew->getMemberHierarchy(mMember);
    mongo::OID poid = mMember;

    addItem("用户名: " + p.getLogName(), [](){});

    if(hier == PENDING)
    {
        addItem("批准入会", std::bind(&Crew::approveToJoin, crew, poid));
    }
    else if(hier > PENDING)
    {
        addItem(std::string("阶级: ") + getCrewHierarchyStr(
            crew->getMemberHierarchy(mMember)), []() {});
        addItem("开除", std::bind(&Crew::removeMember, crew, poid));
    }
}

CrewChangeNameDialog::CrewChangeNameDialog(int playerid,
    const mongo::OID& crew) : InputDialog(playerid, "更改帮派名称"),
    mCrew(crew)
{
}

void CrewChangeNameDialog::build()
{
    setMessage("输入新的名称");
}

bool CrewChangeNameDialog::handleCallback(
    bool response, int /* listitem */, const std::string &inputtext)
{
    if(!response) return true;
    if(inputtext.length() == 0)
    {
        SendClientMessage(mPlayerId, 0xFFFFFFFF, "帮派名称不能为空");
        return false;
    }
    auto crew       = CrewManager::get().getCrew(mCrew);
    auto p          = PlayerManager::get().getPlayer(mPlayerId);
    if(p == nullptr) return true;
    if(crew->getLeader() != p->getId())
    {
        SendClientMessage(mPlayerId, 0xFFFFFFFF, "你不是帮派首领");
        return true;
    }
    if(crew->setName(inputtext))
    {
        SendClientMessage(mPlayerId, 0xFFFFFFFF, "名称更改成功");
        return true;
    }
    SendClientMessage(mPlayerId, 0xFFFFFFFF, "名称更改失败");
    return false;
}

CreateCrewDialog::CreateCrewDialog(int playerid) :
    InputDialog(playerid, "创建帮派")
{
}

void CreateCrewDialog::build()
{
    setMessage("请给你的帮派命名");
}

bool CreateCrewDialog::handleCallback(
    bool response, int /* listitem */, const std::string &inputtext)
{
    if(!response) return true;
    if(inputtext.length() == 0)
    {
        SendClientMessage(mPlayerId, 0xFFFFFFFF, "帮派名称不能为空");
        return false;
    }
    std::unique_ptr<Crew> crew(new Crew(inputtext));
    if(crew->isValid() && !crew->getLeader().isSet())
    {
        auto p = PlayerManager::get().getPlayer(mPlayerId);
        if(p != nullptr)
        {
            crew->setLeader(p->getId());
            SendClientMessage(mPlayerId, 0xFFFFFFFF, "帮派创建成功");
            return true;
        }
    }
    SendClientMessage(mPlayerId, 0xFFFFFFFF, "帮派创建失败");
    return false;
}

CrewFindByNameDialog::CrewFindByNameDialog(int playerid, CallbackType cb) :
    InputDialog(playerid, "查找帮派"), mCallback(cb)
{
}

void CrewFindByNameDialog::build()
{
    setMessage("请输入帮派的名称");
}

bool CrewFindByNameDialog::handleCallback(
    bool response, int /* listitem */, const std::string &inputtext)
{
    if(response)
    {
        DialogManager::get().push<_CrewFindByNameResultDialog>(
            mPlayerId, inputtext, mCallback);
    }
    return true;
}

_CrewFindByNameResultDialog::_CrewFindByNameResultDialog(
    int playerid, const std::string& keyword, CallbackType cb) :
    ItemListDialog<std::string>(playerid, "查找帮派"),
    mKeyWord(keyword), mCallback(cb)
{
}

void _CrewFindByNameResultDialog::build()
{
    MONGO_WRAPPER({
        auto cur = getDBConn()->query(
            Config::colNameCrew,
            QUERY("name" << BSON("$regex" << mKeyWord))
        );
        while(cur->more())
        {
            auto doc = cur->next();
            addItem(doc["_id"].OID().str(), UTF8ToGBK(doc["name"].str()));
        }
    });
}

bool _CrewFindByNameResultDialog::process(std::string key)
{
    return mCallback(mongo::OID(key));
}

}
