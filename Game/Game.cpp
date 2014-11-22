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

#include "Game.hpp"

namespace swcu {

bool Game::addPlayer(int playerid)
{
    return mPlayers.insert(std::make_pair(playerid,
        std::move(std::unique_ptr<Player>(new Player(playerid))))).second;
}

bool Game::removePlayer(int playerid)
{
    return mPlayers.erase(playerid) > 0;
}

bool Game::hasPlayer(int playerid)
{
    return mPlayers.count(playerid) > 0;
}

Player* Game::getPlayer(int playerid)
{
    auto iter = mPlayers.find(playerid);
    if(iter == mPlayers.end())
    {
        return nullptr;
    }
    else
    {
        return iter->second.get();
    }
}

}
