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

#include <sampgdk/a_players.h>
#include <sstream>

#include "PlayerManager.hpp"
#include "../Interface/DialogManager.hpp"
#include "../Map/MapDialogs.hpp"
#include "../Weapon/WeaponShopDialog.hpp"
#include "../GangZone/GangZoneManager.hpp"

#include "PlayerDialogs.hpp"

namespace swcu {

PlayerRegisterDialog::PlayerRegisterDialog(int playerid) :
    InputDialog(playerid, "欢迎来到SWCU自由街区"), mState(INIT)
{
}

void PlayerRegisterDialog::build()
{
    switch(mState)
    {
        case INIT:
        setMessage("请输入一个至少6位长的密码完成注册.");
        break;
        case MUST_REG:
        setMessage("你必须注册一个账号才能进入游戏.");
        break;
        case ERROR:
        setMessage("你的密码不符合要求, 或者服务器发生了内部错误.\n"
            "请重新尝试, 如果反复出现错误请麻烦联系管理员.");
        break;
    }
}
bool PlayerRegisterDialog::handleCallback(
    bool response, int /* listitem */, const std::string &inputtext)
{
    if(!response)
    {
        mState = MUST_REG;
        return false;
    }
    auto p = PlayerManager::get().getPlayer(mPlayerId);
    if(p == nullptr)
    {
        SendClientMessage(mPlayerId, 0xFFFFFFFF, "未找到该玩家.");
        LOG(ERROR) << "Player not found.";
        return true;
    }
    if(p->isRegistered())
    {
        SendClientMessage(mPlayerId, 0xFFFFFFFF, "你已经注册过账号了.");
        return true;
    }
    if(p->createProfile(inputtext))
    {
        p->setLoggedIn(true);
        SendClientMessage(mPlayerId, 0xFFFFFFFF, "你的账号注册成功.");
        return true;
    }
    else
    {
        mState = ERROR;
        return false;
    }
}

PlayerLoginDialog::PlayerLoginDialog(int playerid) :
    InputDialog(playerid, "回到自由街区"), mState(INIT)
{
}

void PlayerLoginDialog::build()
{
    switch(mState)
    {
        case INIT:
        setMessage("请输入密码来登录.");
        break;
        case MUST_LOGIN:
        setMessage("你必须登录才能加入游戏.");
        break;
        case BAD_LOGIN:
        setMessage("密码错误, 请重试.");
        break;
    }
}

bool PlayerLoginDialog::handleCallback(
     bool response, int /* listitem */, const std::string &inputtext)
{
    if(!response)
    {
        mState = MUST_LOGIN;
        return false;
    }
    auto p = PlayerManager::get().getPlayer(mPlayerId);
    if(p == nullptr)
    {
        SendClientMessage(mPlayerId, 0xFFFFFFFF, "未找到该玩家.");
        LOG(ERROR) << "Player not found.";
        return true;
    }
    if(p->isLoggedIn())
    {
        SendClientMessage(mPlayerId, 0xFFFFFFFF,
            "你已经登录过了, 不需要重复登录.");
        return true;
    }
    if(p->verifyPassword(inputtext))
    {
        p->setLoggedIn(true);
        SendClientMessage(mPlayerId, 0xFFFFFFFF, "登录成功.");
        return true;
    }
    else
    {
        mState = BAD_LOGIN;
        return false;
    }
}

PlayerEditProfileDialog::PlayerEditProfileDialog(int playerid) :
    MenuDialog(playerid, "我的资料")
{
    auto p = PlayerManager::get().getPlayer(playerid);
    if(p == nullptr)
    {
        SendClientMessage(playerid, 0xFFFFFFFF, "未找到该玩家.");
        LOG(ERROR) << "Player not found.";
        return;
    }
    if(!p->isLoggedIn())
    {
        SendClientMessage(playerid, 0xFFFFFFFF, "你还没有登录.");
        return;
    }
}

void PlayerEditProfileDialog::build()
{
    auto p = PlayerManager::get().getPlayer(mPlayerId);
    if(p == nullptr)
    {
        return;
    }
    mItemList.clear();
    int playerid = mPlayerId;
    addItem(
        "我的登录名: " + p->getLogName(),
        [playerid]() {
            DialogManager::get().push<PlayerChangeLogNameDialog>(playerid);
        });
    addItem(
        "更改密码",
        [playerid]() {
            DialogManager::get().push<PlayerChangePasswordDialog>(playerid);
        });
    addItem(
        "我的昵称: " + p->getNickname() + "{FFFFFF}",
        [playerid]() {
            DialogManager::get().push<PlayerChangeNicknameDialog>(playerid);
        });
    {
        std::stringstream al;
        al << "我的管理员等级: " << p->getAdminLevel();
        addItem(al.str(), []() {});
    }
    addItem("我的警衔: " + p->getPoliceRankStr(), [](){});
}

PlayerViewProfileDialog::PlayerViewProfileDialog(
    int playerid, int targetplayer) :
    MessageDialog(playerid, ""), mTargetPlayer(targetplayer)
{
}

void PlayerViewProfileDialog::build()
{
    auto tar = PlayerManager::get().getPlayer(mTargetPlayer);
    auto p = PlayerManager::get().getPlayer(mPlayerId);
    if(tar == nullptr || p == nullptr)
    {
        mTitle = "玩家资料";
        mMessage = "未找到该玩家.";
    }
    else
    {
        mTitle = tar->getLogName();
        mTitle += "的资料";
        if(tar->isLoggedIn())
        {
            std::stringstream msg;
            msg << "ID: "
                << tar->getIdStr() << "\n"
                << "登录名: "
                << tar->getLogName() << "\n"
                << "昵称: "
                // In case of colored text, reset color at the end.
                << tar->getNickname() << "{FFFFFF}\n"
                << "注册时间: "
                << tar->getJoinTime() << "\n"
                << "游戏时间: "
                << tar->getGameTime() << "\n"
                << "管理员等级: "
                << tar->getAdminLevel() << "\n"
                << "警衔: "
                << tar->getPoliceRankStr();
            mMessage = msg.str();
        }
        else
        {
            mMessage = "对方没有登录.";
        }
    }
}

PlayerChangePasswordDialog::PlayerChangePasswordDialog(int playerid) :
    InputDialog(playerid, "更改密码"),
    mState(INIT)
{
}

void PlayerChangePasswordDialog::build()
{
    switch(mState)
    {
        case INIT:
        setMessage("请输入一个6位以上的密码.");
        break;
        case FAIL:
        setMessage("你的密码不符合要求, 或者服务器发生了内部错误.\n"
            "请重新尝试, 如果反复出现错误请麻烦联系管理员.");
        break;
    }
}

bool PlayerChangePasswordDialog::handleCallback(
    bool response, int /* listitem */, const std::string &inputtext)
{
    if(!response)
    {
        return true;
    }
    auto p = PlayerManager::get().getPlayer(mPlayerId);
    if(p == nullptr)
    {
        SendClientMessage(mPlayerId, 0xFFFFFFFF, "未找到该玩家.");
        LOG(ERROR) << "Player not found.";
        return true;
    }
    if(!p->isLoggedIn())
    {
        SendClientMessage(mPlayerId, 0xFFFFFFFF, "你还没有登录.");
        return true;
    }
    if(!p->changePassword(inputtext))
    {
        mState = FAIL;
        return false;
    }
    else
    {
        SendClientMessage(mPlayerId, 0xFFFFFFFF, "密码更改成功.");
        return true;
    }
}

PlayerChangeLogNameDialog::PlayerChangeLogNameDialog(int playerid) :
    InputDialog(playerid, "更改登录名"),
    mState(INIT)
{
}

void PlayerChangeLogNameDialog::build()
{
    switch(mState)
    {
        case INIT:
        setMessage("请输入一个新的名字.");
        break;
        case FAIL:
        setMessage("名称不符合要求, 或者服务器发生了内部错误.\n"
            "请重试. 如果反复出现错误请麻烦联系管理员.");
        break;
    }
}

bool PlayerChangeLogNameDialog::handleCallback(
    bool response, int /* listitem */, const std::string &inputtext)
{
    if(!response)
    {
        return true;
    }
    auto p = PlayerManager::get().getPlayer(mPlayerId);
    if(p == nullptr)
    {
        SendClientMessage(mPlayerId, 0xFFFFFFFF, "未找到该玩家.");
        LOG(ERROR) << "Player not found.";
        return true;
    }
    if(!p->isLoggedIn())
    {
        SendClientMessage(mPlayerId, 0xFFFFFFFF, "你还没有登录.");
        return true;
    }
    if(!p->setLogName(inputtext))
    {
        mState = FAIL;
        return false;
    }
    else
    {
        SendClientMessage(mPlayerId, 0xFFFFFFFF, "登录名更改成功.");
        return true;
    }
}

PlayerChangeNicknameDialog::PlayerChangeNicknameDialog(int playerid) :
    InputDialog(playerid, "更改昵称"),
    mState(INIT)
{
}

void PlayerChangeNicknameDialog::build()
{
    switch(mState)
    {
        case INIT:
        setMessage("请输入一个新的名字.");
        break;
        case FAIL:
        setMessage("名称不符合要求, 或者服务器发生了内部错误.\n"
            "请重试. 如果反复出现错误请麻烦联系管理员.");
        break;
    }
}

bool PlayerChangeNicknameDialog::handleCallback(
    bool response, int /* listitem */, const std::string &inputtext)
{
    if(!response)
    {
        return true;
    }
    auto p = PlayerManager::get().getPlayer(mPlayerId);
    if(p == nullptr)
    {
        SendClientMessage(mPlayerId, 0xFFFFFFFF, "未找到该玩家.");
        LOG(ERROR) << "Player not found.";
        return true;
    }
    if(!p->isLoggedIn())
    {
        SendClientMessage(mPlayerId, 0xFFFFFFFF, "你还没有登录.");
        return true;
    }
    if(!p->setNickname(inputtext))
    {
        mState = FAIL;
        return false;
    }
    else
    {
        SendClientMessage(mPlayerId, 0xFFFFFFFF, "昵称更改成功.");
        return true;
    }
}

PlayerControlDialog::PlayerControlDialog(int playerid, int targetplayer)
    : MenuDialog(playerid, "玩家"), mTargetPlayer(targetplayer)
{
}

void PlayerControlDialog::build()
{
    auto target = PlayerManager::get().getPlayer(mTargetPlayer);
    auto p = PlayerManager::get().getPlayer(mPlayerId);
    if(target == nullptr || p == nullptr)
    {
        return;
    }
    setTitle(target->getLogName());

    int playerid = mPlayerId, targetid = mTargetPlayer;

    // General Functions
    // * Private Message
    addItem("发送密语", [playerid, targetid]() {
        DialogManager::get().push<PlayerSendMessageDialog>(
            playerid, targetid
        );
    });
    // * View Profile
    addItem("查看资料", [playerid, targetid]() {
        DialogManager::get().push<PlayerViewProfileDialog>(
            playerid, targetid
        );
    });
    // * Spectate

    // Police Rank POLICE_OFFICER
    int rank = p->getPoliceRank();

    if(rank > CIVILIAN && target->getWantedLevel() > 0)
    {
        float x, y, z;
        GetPlayerPos(targetid, &x, &y, &z);
        float d = GetPlayerDistanceFromPoint(playerid, x, y, z);
        if(d < 10.0)
        {
            addItem("逮捕", [playerid, targetid]() {
                DialogManager::get().push<ArrestDialog>(playerid, targetid);
            });
        }
    }

    if(rank >= POLICE_OFFICER && rank < POLICE_DEPUTY_CHIEF)
    {
        if(!target->hasFlags(STATUS_JAILED))
        {
            addItem("通缉", [playerid, targetid]() {
                DialogManager::get().push<PlayerSetWantedLevelDialog>(
                    playerid, targetid, 1, 2
                );
            });
        }
    }

    // Police Rank POLICE_DEPUTY_CHIEF
    if(rank >= POLICE_DEPUTY_CHIEF && rank < CHIEF_OF_POLICE)
    {
        if(!target->hasFlags(STATUS_JAILED))
        {
            addItem("通缉", [playerid, targetid]() {
                DialogManager::get().push<PlayerSetWantedLevelDialog>(
                    playerid, targetid, 0, 4
                );
            });
        }
    }

    // Police Rank CHIEF_OF_POLICE
    if(rank == CHIEF_OF_POLICE)
    {
        if(!target->hasFlags(STATUS_JAILED))
        {
            addItem("通缉", [playerid, targetid]() {
                DialogManager::get().push<PlayerSetWantedLevelDialog>(
                    playerid, targetid, 0, 6
                );
            });
        }
    }

    // Police Rank >= POLICE_DEPUTY_CHIEF
    if(rank >= POLICE_DEPUTY_CHIEF)
    {
        if(target->hasFlags(STATUS_JAILED))
        {
            addItem("释放", [targetid]() {
                auto target = PlayerManager::get().getPlayer(targetid);
                if(target != nullptr) target->freeFromPrison();
            });
        }
    }

    // Admin Level 1
    if(p->getAdminLevel() >= 1)
    {
        // * Go To
        addItem("传送过去", [playerid, targetid]() {
            auto p = PlayerManager::get().getPlayer(playerid);
            if(p != nullptr) p->teleportTo(targetid);
        });
        // * Get There
        addItem("拉过来", [playerid, targetid]() {
            auto target = PlayerManager::get().getPlayer(targetid);
            if(target != nullptr) target->teleportTo(playerid);
        });
        // * Mute / Unmute
        if(target->hasFlags(STATUS_MUTED))
        {
            addItem("允许发言", [targetid]() {
                auto target = PlayerManager::get().getPlayer(targetid);
                if(target != nullptr) target->removeFlags(STATUS_MUTED);
            });
        }
        else
        {
            addItem("禁止发言", [targetid]() {
                auto target = PlayerManager::get().getPlayer(targetid);
                if(target != nullptr) target->addFlags(STATUS_MUTED);
            });
        }
        // * Eject
        if(IsPlayerInAnyVehicle(targetid))
        {
            addItem("拉下车",
                std::bind(&RemovePlayerFromVehicle, targetid)
            );
        }
    }

    // Admin Level 2
    if(p->getAdminLevel() >= 2)
    {
        // * Freeze / Unfreeze
        if(target->hasFlags(STATUS_FREEZED))
        {
            addItem("解冻", [targetid]() {
                auto target = PlayerManager::get().getPlayer(targetid);
                if(target != nullptr) target->removeFlags(STATUS_FREEZED);
                TogglePlayerControllable(targetid, 1);
            });
        }
        else
        {
            addItem("冻结", [targetid]() {
                auto target = PlayerManager::get().getPlayer(targetid);
                if(target != nullptr) target->addFlags(STATUS_FREEZED);
                TogglePlayerControllable(targetid, 0);
            });
        }
        // * Kill
        addItem("重置生命",
            std::bind(&SetPlayerHealth, targetid, 0.0)
        );
        // * Force Respawn
        addItem("强制重新选人", [targetid]() {
            ForceClassSelection(targetid);
            SetPlayerHealth(targetid, 0.0);
        });
        // * Kick
        addItem("踢出服务器",
            std::bind(&Kick, targetid)
        );
    }

    // Admin Level 3
    if(p->getAdminLevel() >= 3)
    {
        // * Explode
        addItem("爆炸", [targetid]() {
            float x, y, z;
            GetPlayerPos(targetid, &x, &y, &z);
            CreateExplosion(x, y, z, 0, 5.0);
        });
        // * Ban
        addItem("封禁", [targetid]() {
            auto target = PlayerManager::get().getPlayer(targetid);
            if(target != nullptr) target->addFlags(STATUS_BANNED);
            Kick(targetid);
        });
        // * Set Admin Level
        addItem("更改管理员等级",
        [playerid, targetid]() {
            DialogManager::get().push<PlayerSetAdminLevelDialog>(
                playerid, targetid
            );
        });
        // * Set Police Rank
        addItem("更改警衔",
        [playerid, targetid]() {
            DialogManager::get().push<PlayerSetPoliceRankDialog>(
                playerid, targetid
            );
        });
    }
}

PlayerSendMessageDialog::PlayerSendMessageDialog(int playerid, int target) :
    InputDialog(playerid, "发送密语"), mTargetPlayer(target)
{
}

void PlayerSendMessageDialog::build()
{
    setMessage("请输入你要发送的消息");
}

bool PlayerSendMessageDialog::handleCallback(
    bool response, int /* listitem */, const std::string &inputtext)
{
    if(!response)
    {
        return true;
    }
    auto target = PlayerManager::get().getPlayer(mTargetPlayer);
    auto p = PlayerManager::get().getPlayer(mPlayerId);
    if(target == nullptr || p == nullptr)
    {
        return true;
    }
    if(!IsPlayerConnected(mTargetPlayer))
    {
        SendClientMessage(mPlayerId, 0xFFFFFFFF, "未找到该玩家.");
        return true;
    }
    std::stringstream info;
    info << "消息发送给 " << target->getNickname()
        << "{FFFFFF}(" << mTargetPlayer << "): " << inputtext;
    SendClientMessage(mPlayerId, 0xFFFFFFFF, info.str().c_str());
    std::stringstream msg;
    msg << "消息来自 " << p->getNickname()
        << "{FFFFFF}(" << mPlayerId << "): " << inputtext;
    SendClientMessage(mTargetPlayer, 0xFFFFFFFF, msg.str().c_str());
    LOG(INFO) << "Private message sent from " << p->getLogName()
        << " to " << target->getLogName() << ": " << inputtext;
    return false;
}

PlayerSetAdminLevelDialog::PlayerSetAdminLevelDialog(int playerid,
    int target) : RadioListDialog<int>(playerid, "更改管理员等级"),
    mTargetPlayer(target)
{
}

void PlayerSetAdminLevelDialog::build()
{
    auto target = PlayerManager::get().getPlayer(mTargetPlayer);
    auto p = PlayerManager::get().getPlayer(mPlayerId);
    if(target == nullptr || p == nullptr)
    {
        return;
    }
    addItem(0, "无管理权限", target->getAdminLevel() == 0);
    addItem(1, "1级 调解员", target->getAdminLevel() == 1);
    addItem(2, "2级 管理员", target->getAdminLevel() == 2);
    addItem(3, "3级 领袖", target->getAdminLevel() == 3);
}

bool PlayerSetAdminLevelDialog::process(int adminlevel)
{
    auto target = PlayerManager::get().getPlayer(mTargetPlayer);
    if(target != nullptr)
    {
        return target->setAdminLevel(adminlevel);
    }
    return false;
}

PlayerSetPoliceRankDialog::PlayerSetPoliceRankDialog(int playerid,
    int target) : RadioListDialog<PoliceRank>(playerid, "设置警衔"),
    mTargetPlayer(target)
{
}

void PlayerSetPoliceRankDialog::build()
{
    auto target = PlayerManager::get().getPlayer(mTargetPlayer);
    if(target == nullptr)
    {
        return;
    }
    addItem(CIVILIAN, PoliceRankString[CIVILIAN],
        target->getPoliceRank() == CIVILIAN);
    addItem(POLICE_OFFICER, PoliceRankString[POLICE_OFFICER],
        target->getPoliceRank() == POLICE_OFFICER);
    addItem(POLICE_DEPUTY_CHIEF, PoliceRankString[POLICE_DEPUTY_CHIEF],
        target->getPoliceRank() == POLICE_DEPUTY_CHIEF);
    addItem(CHIEF_OF_POLICE, PoliceRankString[CHIEF_OF_POLICE],
        target->getPoliceRank() == CHIEF_OF_POLICE);
}

bool PlayerSetPoliceRankDialog::process(PoliceRank rank)
{
    auto target = PlayerManager::get().getPlayer(mTargetPlayer);
    if(target != nullptr)
    {
        return target->setPoliceRank(rank);
    }
    return false;
}

PlayerControlPanelDialog::PlayerControlPanelDialog(int playerid) :
    MenuDialog(playerid, "控制面板")
{
}

void PlayerControlPanelDialog::build()
{
    auto p = PlayerManager::get().getPlayer(mPlayerId);
    if(p == nullptr)
    {
        return;
    }
    int playerid = mPlayerId;
    // General Functions
    // * Weapon Shop
    addItem("武器商店", [playerid]() {
        DialogManager::get().push<WeaponShopDialog>(playerid);
    });
    // * Create Teleport
    addItem("在此创建传送点", [playerid]() {
        DialogManager::get().push<CreateTeleportDialog>(playerid);
    });
    // * Profile
    addItem("我的资料", [playerid]() {
        DialogManager::get().push<PlayerEditProfileDialog>(playerid);
    });
    // Admin Level 3
    // * Map Manager
    if(p->getAdminLevel() >= 3)
    {
        addItem("地图管理", [playerid]() {
            DialogManager::get().push<MapManagerDialog>(playerid);
        });
        addItem("重新载入帮派地盘", []() {
            GangZoneManager::get().loadAll();
            GangZoneManager::get().showAll();
        });
    }
    // Police System
    if(p->getWantedLevel() > 0)
    {
        addItem("自首", [playerid]() {
            DialogManager::get().push<ArrestSurrenderDialog>(playerid);
        });
    }
}

PlayerSetWantedLevelDialog::PlayerSetWantedLevelDialog(int playerid,
    int target, int minlevel, int maxlevel) :
    RadioListDialog<int>(playerid, "设置通缉等级"),
    mTargetPlayer(target), mMin(minlevel < 0 ? 0 : minlevel),
    mMax(maxlevel > 6 ? 6 : maxlevel)
{
}

void PlayerSetWantedLevelDialog::build()
{
    auto target = PlayerManager::get().getPlayer(mTargetPlayer);
    auto p = PlayerManager::get().getPlayer(mPlayerId);
    if(target == nullptr || p == nullptr)
    {
        return;
    }
    if(0 >= mMin && 0 <= mMax)
        addItem(0, "不通缉", target->getWantedLevel() == 0);
    if(1 >= mMin && 1 <= mMax)
        addItem(1, "一星通缉", target->getWantedLevel() == 1);
    if(2 >= mMin && 2 <= mMax)
        addItem(2, "二星通缉", target->getWantedLevel() == 2);
    if(3 >= mMin && 3 <= mMax)
        addItem(3, "三星通缉", target->getWantedLevel() == 3);
    if(4 >= mMin && 4 <= mMax)
        addItem(4, "四星通缉", target->getWantedLevel() == 4);
    if(5 >= mMin && 5 <= mMax)
        addItem(5, "五星通缉", target->getWantedLevel() == 5);
    if(6 >= mMin && 6 <= mMax)
        addItem(6, "六星通缉", target->getWantedLevel() == 6);
}

bool PlayerSetWantedLevelDialog::process(int level)
{
    auto target = PlayerManager::get().getPlayer(mTargetPlayer);
    auto p = PlayerManager::get().getPlayer(mPlayerId);
    if(target == nullptr || p == nullptr)
    {
        return false;
    }
    // You can't down set wanted level if it's beyond your authority.
    if(target->getWantedLevel() > level && target->getWantedLevel() > mMax)
    {
        SendClientMessage(mPlayerId, 0xFFFFFFFF,
            "你没有权限降低他的通缉等级.");
        return false;
    }
    target->setWantedLevel(level);
    if(level > 0)
    {
        DialogManager::get().push<ArrestSurrenderDialog>(mTargetPlayer);
    }
    return true;
}

ArrestSurrenderDialog::ArrestSurrenderDialog(int playerid) :
    ConfirmDialog(playerid, "通缉")
{
}

void ArrestSurrenderDialog::build()
{
    setMessage("你被通缉了, 你可以自首(只关押两分钟)或逃跑. 你要自首吗?");
}

bool ArrestSurrenderDialog::handleCallback(
    bool response, int /* listitem */, const std::string& /* inputtext */)
{
    auto p = PlayerManager::get().getPlayer(mPlayerId);
    if(p == nullptr) return true;
    if(response && p->getWantedLevel() > 0)
    {
        p->putIntoPrison(60 * 2);
    }
    return true;
}

ArrestDialog::ArrestDialog(int playerid, int target) :
    MenuDialog(playerid, "逮捕"),
    mTarget(target)
{
}

void ArrestDialog::build()
{
    int targetid = mTarget;
    auto p = PlayerManager::get().getPlayer(mPlayerId);
    if(p == nullptr) return;
    addItem("关押5分钟", [targetid]() {
        auto target = PlayerManager::get().getPlayer(targetid);
        if(target != nullptr && target->getWantedLevel() > 0)
            target->putIntoPrison(60 * 5);
    });
    addItem("关押10分钟", [targetid]() {
        auto target = PlayerManager::get().getPlayer(targetid);
        if(target != nullptr && target->getWantedLevel() > 0)
            target->putIntoPrison(60 * 10);
    });
    addItem("关押15分钟", [targetid]() {
        auto target = PlayerManager::get().getPlayer(targetid);
        if(target != nullptr && target->getWantedLevel() > 0)
            target->putIntoPrison(60 * 15);
    });
}

bool ArrestDialog::handleCallback(
    bool response, int listitem, const std::string &inputtext)
{
    MenuDialog::handleCallback(response, listitem, inputtext);
    return true;
}

CreateTeleportDialog::CreateTeleportDialog(int playerid) :
    InputDialog(playerid, "创建传送点")
{
}

void CreateTeleportDialog::build()
{
    setMessage("请给传送点命名");
}

bool CreateTeleportDialog::handleCallback(
    bool response, int /* listitem */, const std::string &inputtext)
{
    if(!response) return true;
    auto p = PlayerManager::get().getPlayer(mPlayerId);
    if(p == nullptr) return false;
    return p->createTeleport(inputtext);
}

}
