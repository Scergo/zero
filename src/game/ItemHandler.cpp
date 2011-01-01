/*
 * Copyright (C) 2005-2011 MaNGOS <http://getmangos.com/>
 * Copyright (C) 2009-2011 MaNGOSZero <http://github.com/mangoszero/mangoszero/>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "Common.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "Opcodes.h"
#include "Log.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "Item.h"
#include "UpdateData.h"
#include "Chat.h"

void WorldSession::HandleSplitItemOpcode( WorldPacket & recv_data )
{
    //DEBUG_LOG("WORLD: CMSG_SPLIT_ITEM");
    uint8 srcbag, srcslot, dstbag, dstslot, count;

    recv_data >> srcbag >> srcslot >> dstbag >> dstslot >> count;
    //DEBUG_LOG("STORAGE: receive srcbag = %u, srcslot = %u, dstbag = %u, dstslot = %u, count = %u", srcbag, srcslot, dstbag, dstslot, count);

    uint16 src = ( (srcbag << 8) | srcslot );
    uint16 dst = ( (dstbag << 8) | dstslot );

    if(src == dst)
        return;

    if (count == 0)
        return;                                             //check count - if zero it's fake packet

    if(!_player->IsValidPos(srcbag, srcslot, true))
    {
        _player->SendEquipError( EQUIP_ERR_ITEM_NOT_FOUND, NULL, NULL );
        return;
    }

    if(!_player->IsValidPos(dstbag, dstslot, false))        // can be autostore pos
    {
        _player->SendEquipError( EQUIP_ERR_ITEM_DOESNT_GO_TO_SLOT, NULL, NULL );
        return;
    }

    _player->SplitItem( src, dst, count );
}

void WorldSession::HandleSwapInvItemOpcode( WorldPacket & recv_data )
{
    //DEBUG_LOG("WORLD: CMSG_SWAP_INV_ITEM");
    uint8 srcslot, dstslot;

    recv_data >> srcslot >> dstslot;
    //DEBUG_LOG("STORAGE: receive srcslot = %u, dstslot = %u", srcslot, dstslot);

    // prevent attempt swap same item to current position generated by client at special cheating sequence
    if(srcslot == dstslot)
        return;

    if(!_player->IsValidPos(INVENTORY_SLOT_BAG_0, srcslot, true))
    {
        _player->SendEquipError( EQUIP_ERR_ITEM_NOT_FOUND, NULL, NULL );
        return;
    }

    if(!_player->IsValidPos(INVENTORY_SLOT_BAG_0, dstslot, true))
    {
        _player->SendEquipError( EQUIP_ERR_ITEM_DOESNT_GO_TO_SLOT, NULL, NULL );
        return;
    }

    uint16 src = ( (INVENTORY_SLOT_BAG_0 << 8) | srcslot );
    uint16 dst = ( (INVENTORY_SLOT_BAG_0 << 8) | dstslot );

    _player->SwapItem( src, dst );
}

void WorldSession::HandleAutoEquipItemSlotOpcode( WorldPacket & recv_data )
{
    uint64 itemguid;
    uint8 dstslot;
    recv_data >> itemguid >> dstslot;

    // cheating attempt, client should never send opcode in that case
    if(!Player::IsEquipmentPos(INVENTORY_SLOT_BAG_0, dstslot))
        return;

    Item* item = _player->GetItemByGuid(itemguid);
    uint16 dstpos = dstslot | (INVENTORY_SLOT_BAG_0 << 8);

    if(!item || item->GetPos() == dstpos)
        return;

    _player->SwapItem(item->GetPos(), dstpos);
}

void WorldSession::HandleSwapItem( WorldPacket & recv_data )
{
    //DEBUG_LOG("WORLD: CMSG_SWAP_ITEM");
    uint8 dstbag, dstslot, srcbag, srcslot;

    recv_data >> dstbag >> dstslot >> srcbag >> srcslot ;
    //DEBUG_LOG("STORAGE: receive srcbag = %u, srcslot = %u, dstbag = %u, dstslot = %u", srcbag, srcslot, dstbag, dstslot);

    uint16 src = ( (srcbag << 8) | srcslot );
    uint16 dst = ( (dstbag << 8) | dstslot );

    // prevent attempt swap same item to current position generated by client at special cheating sequence
    if(src == dst)
        return;

    if(!_player->IsValidPos(srcbag, srcslot, true))
    {
        _player->SendEquipError( EQUIP_ERR_ITEM_NOT_FOUND, NULL, NULL );
        return;
    }

    if(!_player->IsValidPos(dstbag, dstslot, true))
    {
        _player->SendEquipError( EQUIP_ERR_ITEM_DOESNT_GO_TO_SLOT, NULL, NULL );
        return;
    }

    _player->SwapItem( src, dst );
}

void WorldSession::HandleAutoEquipItemOpcode( WorldPacket & recv_data )
{
    //DEBUG_LOG("WORLD: CMSG_AUTOEQUIP_ITEM");
    uint8 srcbag, srcslot;

    recv_data >> srcbag >> srcslot;
    //DEBUG_LOG("STORAGE: receive srcbag = %u, srcslot = %u", srcbag, srcslot);

    Item *pSrcItem  = _player->GetItemByPos( srcbag, srcslot );
    if( !pSrcItem )
        return;                                             // only at cheat

    uint16 dest;
    uint8 msg = _player->CanEquipItem( NULL_SLOT, dest, pSrcItem, !pSrcItem->IsBag() );
    if( msg != EQUIP_ERR_OK )
    {
        _player->SendEquipError( msg, pSrcItem, NULL );
        return;
    }

    uint16 src = pSrcItem->GetPos();
    if(dest == src)                                         // prevent equip in same slot, only at cheat
        return;

    Item *pDstItem = _player->GetItemByPos( dest );
    if( !pDstItem )                                         // empty slot, simple case
    {
        _player->RemoveItem( srcbag, srcslot, true );
        _player->EquipItem( dest, pSrcItem, true );
        _player->AutoUnequipOffhandIfNeed();
    }
    else                                                    // have currently equipped item, not simple case
    {
        uint8 dstbag = pDstItem->GetBagSlot();
        uint8 dstslot = pDstItem->GetSlot();

        msg = _player->CanUnequipItem( dest, !pSrcItem->IsBag() );
        if( msg != EQUIP_ERR_OK )
        {
            _player->SendEquipError( msg, pDstItem, NULL );
            return;
        }

        // check dest->src move possibility
        ItemPosCountVec sSrc;
        uint16 eSrc = 0;
        if( _player->IsInventoryPos( src ) )
        {
            msg = _player->CanStoreItem( srcbag, srcslot, sSrc, pDstItem, true );
            if( msg != EQUIP_ERR_OK )
                msg = _player->CanStoreItem( srcbag, NULL_SLOT, sSrc, pDstItem, true );
            if( msg != EQUIP_ERR_OK )
                msg = _player->CanStoreItem( NULL_BAG, NULL_SLOT, sSrc, pDstItem, true );
        }
        else if( _player->IsBankPos( src ) )
        {
            msg = _player->CanBankItem( srcbag, srcslot, sSrc, pDstItem, true );
            if( msg != EQUIP_ERR_OK )
                msg = _player->CanBankItem( srcbag, NULL_SLOT, sSrc, pDstItem, true );
            if( msg != EQUIP_ERR_OK )
                msg = _player->CanBankItem( NULL_BAG, NULL_SLOT, sSrc, pDstItem, true );
        }
        else if( _player->IsEquipmentPos( src ) )
        {
            msg = _player->CanEquipItem( srcslot, eSrc, pDstItem, true);
            if( msg == EQUIP_ERR_OK )
                msg = _player->CanUnequipItem( eSrc, true);
        }

        if( msg != EQUIP_ERR_OK )
        {
            _player->SendEquipError( msg, pDstItem, pSrcItem );
            return;
        }

        // now do moves, remove...
        _player->RemoveItem(dstbag, dstslot, false);
        _player->RemoveItem(srcbag, srcslot, false);

        // add to dest
        _player->EquipItem(dest, pSrcItem, true);

        // add to src
        if( _player->IsInventoryPos( src ) )
            _player->StoreItem(sSrc, pDstItem, true);
        else if( _player->IsBankPos( src ) )
            _player->BankItem(sSrc, pDstItem, true);
        else if( _player->IsEquipmentPos( src ) )
            _player->EquipItem(eSrc, pDstItem, true);

        _player->AutoUnequipOffhandIfNeed();
    }
}

void WorldSession::HandleDestroyItemOpcode( WorldPacket & recv_data )
{
    //DEBUG_LOG("WORLD: CMSG_DESTROYITEM");
    uint8 bag, slot, count, data1, data2, data3;

    recv_data >> bag >> slot >> count >> data1 >> data2 >> data3;
    //DEBUG_LOG("STORAGE: receive bag = %u, slot = %u, count = %u", bag, slot, count);

    uint16 pos = (bag << 8) | slot;

    // prevent drop unequipable items (in combat, for example) and non-empty bags
    if(_player->IsEquipmentPos(pos) || _player->IsBagPos(pos))
    {
        uint8 msg = _player->CanUnequipItem( pos, false );
        if( msg != EQUIP_ERR_OK )
        {
            _player->SendEquipError( msg, _player->GetItemByPos(pos), NULL );
            return;
        }
    }

    Item *pItem  = _player->GetItemByPos( bag, slot );
    if(!pItem)
    {
        _player->SendEquipError( EQUIP_ERR_ITEM_NOT_FOUND, NULL, NULL );
        return;
    }

    // checked at client side and not have server side appropriate error output
    if (pItem->GetProto()->Flags & ITEM_FLAG_INDESTRUCTIBLE)
    {
        _player->SendEquipError( EQUIP_ERR_CANT_DROP_SOULBOUND, NULL, NULL );
        return;
    }

    if(count)
    {
        uint32 i_count = count;
        _player->DestroyItemCount( pItem, i_count, true );
    }
    else
        _player->DestroyItem( bag, slot, true );
}

// Only _static_ data send in this packet !!!
void WorldSession::HandleItemQuerySingleOpcode( WorldPacket & recv_data )
{
    //DEBUG_LOG("WORLD: CMSG_ITEM_QUERY_SINGLE");
    uint32 item;
    recv_data >> item;
    recv_data.read_skip<uint64>();                          // guid

    DETAIL_LOG("STORAGE: Item Query = %u", item);

    ItemPrototype const *pProto = ObjectMgr::GetItemPrototype( item );
    if( pProto )
    {
        std::string Name        = pProto->Name1;
        std::string Description = pProto->Description;

        int loc_idx = GetSessionDbLocaleIndex();
        if ( loc_idx >= 0 )
        {
            ItemLocale const *il = sObjectMgr.GetItemLocale(pProto->ItemId);
            if (il)
            {
                if (il->Name.size() > size_t(loc_idx) && !il->Name[loc_idx].empty())
                    Name = il->Name[loc_idx];
                if (il->Description.size() > size_t(loc_idx) && !il->Description[loc_idx].empty())
                    Description = il->Description[loc_idx];
            }
        }
                                                            // guess size
        WorldPacket data( SMSG_ITEM_QUERY_SINGLE_RESPONSE, 600);
        data << pProto->ItemId;
        data << pProto->Class;
        // client known only 0 subclass (and 1-2 obsolute subclasses)
        data << (pProto->Class==ITEM_CLASS_CONSUMABLE ? uint32(0) : pProto->SubClass);
        data << Name;
        data << uint8(0x00);                                //pProto->Name2; // blizz not send name there, just uint8(0x00); <-- \0 = empty string = empty name...
        data << uint8(0x00);                                //pProto->Name3; // blizz not send name there, just uint8(0x00);
        data << uint8(0x00);                                //pProto->Name4; // blizz not send name there, just uint8(0x00);
        data << pProto->DisplayInfoID;
        data << pProto->Quality;
        data << pProto->Flags;
        data << pProto->BuyPrice;
        data << pProto->SellPrice;
        data << pProto->InventoryType;
        data << pProto->AllowableClass;
        data << pProto->AllowableRace;
        data << pProto->ItemLevel;
        data << pProto->RequiredLevel;
        data << pProto->RequiredSkill;
        data << pProto->RequiredSkillRank;
        data << pProto->RequiredSpell;
        data << pProto->RequiredHonorRank;
        data << pProto->RequiredCityRank;
        data << pProto->RequiredReputationFaction;
        data << (pProto->RequiredReputationFaction > 0  ? pProto->RequiredReputationRank : 0 );  // send value only if reputation faction id setted ( needed for some items)
        data << pProto->MaxCount;
        data << pProto->Stackable;
        data << pProto->ContainerSlots;
        for(int i = 0; i < MAX_ITEM_PROTO_STATS; ++i)
        {
            data << pProto->ItemStat[i].ItemStatType;
            data << pProto->ItemStat[i].ItemStatValue;
        }
        for(int i = 0; i < MAX_ITEM_PROTO_DAMAGES; ++i)
        {
            data << pProto->Damage[i].DamageMin;
            data << pProto->Damage[i].DamageMax;
            data << pProto->Damage[i].DamageType;
        }

        // resistances (7)
        data << pProto->Armor;
        data << pProto->HolyRes;
        data << pProto->FireRes;
        data << pProto->NatureRes;
        data << pProto->FrostRes;
        data << pProto->ShadowRes;
        data << pProto->ArcaneRes;

        data << pProto->Delay;
        data << pProto->AmmoType;
        data << (float)pProto->RangedModRange;

        for(int s = 0; s < MAX_ITEM_PROTO_SPELLS; ++s)
        {
            // send DBC data for cooldowns in same way as it used in Spell::SendSpellCooldown
            // use `item_template` or if not set then only use spell cooldowns
            SpellEntry const* spell = sSpellStore.LookupEntry(pProto->Spells[s].SpellId);
            if(spell)
            {
                bool db_data = pProto->Spells[s].SpellCooldown >= 0 || pProto->Spells[s].SpellCategoryCooldown >= 0;

                data << pProto->Spells[s].SpellId;
                data << pProto->Spells[s].SpellTrigger;
                data << uint32(-abs(pProto->Spells[s].SpellCharges));

                if(db_data)
                {
                    data << uint32(pProto->Spells[s].SpellCooldown);
                    data << uint32(pProto->Spells[s].SpellCategory);
                    data << uint32(pProto->Spells[s].SpellCategoryCooldown);
                }
                else
                {
                    data << uint32(spell->RecoveryTime);
                    data << uint32(spell->Category);
                    data << uint32(spell->CategoryRecoveryTime);
                }
            }
            else
            {
                data << uint32(0);
                data << uint32(0);
                data << uint32(0);
                data << uint32(-1);
                data << uint32(0);
                data << uint32(-1);
            }
        }
        data << pProto->Bonding;
        data << Description;
        data << pProto->PageText;
        data << pProto->LanguageID;
        data << pProto->PageMaterial;
        data << pProto->StartQuest;
        data << pProto->LockID;
        data << pProto->Material;
        data << pProto->Sheath;
        data << pProto->RandomProperty;
        data << pProto->Block;
        data << pProto->ItemSet;
        data << pProto->MaxDurability;
        data << pProto->Area;
        data << pProto->Map;                                // Added in 1.12.x & 2.0.1 client branch
        data << pProto->BagFamily;
        SendPacket( &data );
    }
    else
    {
        DEBUG_LOG( "WORLD: CMSG_ITEM_QUERY_SINGLE - NO item INFO! (ENTRY: %u)", item );
        WorldPacket data( SMSG_ITEM_QUERY_SINGLE_RESPONSE, 4);
        data << uint32(item | 0x80000000);
        SendPacket( &data );
    }
}

void WorldSession::HandleReadItemOpcode( WorldPacket & recv_data )
{
    //DEBUG_LOG( "WORLD: CMSG_READ_ITEM");

    uint8 bag, slot;
    recv_data >> bag >> slot;

    //sLog.outDetail("STORAGE: Read bag = %u, slot = %u", bag, slot);
    Item *pItem = _player->GetItemByPos( bag, slot );

    if( pItem && pItem->GetProto()->PageText )
    {
        WorldPacket data;

        uint8 msg = _player->CanUseItem( pItem );
        if( msg == EQUIP_ERR_OK )
        {
            data.Initialize (SMSG_READ_ITEM_OK, 8);
            DETAIL_LOG("STORAGE: Item page sent");
        }
        else
        {
            data.Initialize( SMSG_READ_ITEM_FAILED, 8 );
            DETAIL_LOG("STORAGE: Unable to read item");
            _player->SendEquipError( msg, pItem, NULL );
        }
        data << pItem->GetGUID();
        SendPacket(&data);
    }
    else
        _player->SendEquipError( EQUIP_ERR_ITEM_NOT_FOUND, NULL, NULL );
}

void WorldSession::HandlePageQuerySkippedOpcode( WorldPacket & recv_data )
{
    DEBUG_LOG( "WORLD: Received CMSG_PAGE_TEXT_QUERY" );

    uint32 itemid;
    ObjectGuid guid;

    recv_data >> itemid >> guid;

    DETAIL_LOG("Packet Info: itemid: %u guid: %s", itemid, guid.GetString().c_str());
}

void WorldSession::HandleSellItemOpcode( WorldPacket & recv_data )
{
    DEBUG_LOG(  "WORLD: Received CMSG_SELL_ITEM" );

    ObjectGuid vendorGuid;
    uint64 itemguid;
    uint8 _count;

    recv_data >> vendorGuid;
    recv_data >> itemguid;
    recv_data >> _count;

    // prevent possible overflow, as mangos uses uint32 for item count
    uint32 count = _count;

    if(!itemguid)
        return;

    Creature *pCreature = GetPlayer()->GetNPCIfCanInteractWith(vendorGuid, UNIT_NPC_FLAG_VENDOR);
    if (!pCreature)
    {
        DEBUG_LOG("WORLD: HandleSellItemOpcode - %s not found or you can't interact with him.", vendorGuid.GetString().c_str());
        _player->SendSellError( SELL_ERR_CANT_FIND_VENDOR, NULL, itemguid, 0);
        return;
    }

    // remove fake death
    if(GetPlayer()->hasUnitState(UNIT_STAT_DIED))
        GetPlayer()->RemoveSpellsCausingAura(SPELL_AURA_FEIGN_DEATH);

    Item *pItem = _player->GetItemByGuid( itemguid );
    if( pItem )
    {
        // prevent sell not owner item
        if(_player->GetGUID() != pItem->GetOwnerGUID())
        {
            _player->SendSellError( SELL_ERR_CANT_SELL_ITEM, pCreature, itemguid, 0);
            return;
        }

        // prevent sell non empty bag by drag-and-drop at vendor's item list
        if(pItem->IsBag() && !((Bag*)pItem)->IsEmpty())
        {
            _player->SendSellError( SELL_ERR_CANT_SELL_ITEM, pCreature, itemguid, 0);
            return;
        }

        // prevent sell currently looted item
        if(_player->GetLootGUID() == pItem->GetGUID())
        {
            _player->SendSellError( SELL_ERR_CANT_SELL_ITEM, pCreature, itemguid, 0);
            return;
        }

        // special case at auto sell (sell all)
        if(count == 0)
        {
            count = pItem->GetCount();
        }
        else
        {
            // prevent sell more items that exist in stack (possible only not from client)
            if(count > pItem->GetCount())
            {
                _player->SendSellError( SELL_ERR_CANT_SELL_ITEM, pCreature, itemguid, 0);
                return;
            }
        }

        ItemPrototype const *pProto = pItem->GetProto();
        if( pProto )
        {
            if( pProto->SellPrice > 0 )
            {
                if(count < pItem->GetCount())               // need split items
                {
                    Item *pNewItem = pItem->CloneItem( count, _player );
                    if (!pNewItem)
                    {
                        sLog.outError("WORLD: HandleSellItemOpcode - could not create clone of item %u; count = %u", pItem->GetEntry(), count );
                        _player->SendSellError( SELL_ERR_CANT_SELL_ITEM, pCreature, itemguid, 0);
                        return;
                    }

                    pItem->SetCount( pItem->GetCount() - count );
                    _player->ItemRemovedQuestCheck( pItem->GetEntry(), count );
                    if( _player->IsInWorld() )
                        pItem->SendCreateUpdateToPlayer( _player );
                    pItem->SetState(ITEM_CHANGED, _player);

                    _player->AddItemToBuyBackSlot( pNewItem );
                    if( _player->IsInWorld() )
                        pNewItem->SendCreateUpdateToPlayer( _player );
                }
                else
                {
                    _player->ItemRemovedQuestCheck( pItem->GetEntry(), pItem->GetCount());
                    _player->RemoveItem( pItem->GetBagSlot(), pItem->GetSlot(), true);
                    pItem->RemoveFromUpdateQueueOf(_player);
                    _player->AddItemToBuyBackSlot( pItem );
                }

                _player->ModifyMoney( pProto->SellPrice * count );
            }
            else
                _player->SendSellError( SELL_ERR_CANT_SELL_ITEM, pCreature, itemguid, 0);
            return;
        }
    }
    _player->SendSellError( SELL_ERR_CANT_FIND_ITEM, pCreature, itemguid, 0);
    return;
}

void WorldSession::HandleBuybackItem(WorldPacket & recv_data)
{
    DEBUG_LOG( "WORLD: Received CMSG_BUYBACK_ITEM" );
    ObjectGuid vendorGuid;
    uint32 slot;

    recv_data >> vendorGuid >> slot;

    Creature *pCreature = GetPlayer()->GetNPCIfCanInteractWith(vendorGuid, UNIT_NPC_FLAG_VENDOR);
    if (!pCreature)
    {
        DEBUG_LOG("WORLD: HandleBuybackItem - %s not found or you can't interact with him.", vendorGuid.GetString().c_str());
        _player->SendSellError( SELL_ERR_CANT_FIND_VENDOR, NULL, 0, 0);
        return;
    }

    // remove fake death
    if(GetPlayer()->hasUnitState(UNIT_STAT_DIED))
        GetPlayer()->RemoveSpellsCausingAura(SPELL_AURA_FEIGN_DEATH);

    Item *pItem = _player->GetItemFromBuyBackSlot( slot );
    if( pItem )
    {
        uint32 price = _player->GetUInt32Value( PLAYER_FIELD_BUYBACK_PRICE_1 + slot - BUYBACK_SLOT_START );
        if( _player->GetMoney() < price )
        {
            _player->SendBuyError( BUY_ERR_NOT_ENOUGHT_MONEY, pCreature, pItem->GetEntry(), 0);
            return;
        }

        ItemPosCountVec dest;
        uint8 msg = _player->CanStoreItem( NULL_BAG, NULL_SLOT, dest, pItem, false );
        if( msg == EQUIP_ERR_OK )
        {
            _player->ModifyMoney( -(int32)price );
            _player->RemoveItemFromBuyBackSlot( slot, false );
            _player->ItemAddedQuestCheck( pItem->GetEntry(), pItem->GetCount());
            _player->StoreItem( dest, pItem, true );
        }
        else
            _player->SendEquipError( msg, pItem, NULL );
        return;
    }
    else
        _player->SendBuyError( BUY_ERR_CANT_FIND_ITEM, pCreature, 0, 0);
}

void WorldSession::HandleBuyItemInSlotOpcode( WorldPacket & recv_data )
{
    DEBUG_LOG( "WORLD: Received CMSG_BUY_ITEM_IN_SLOT" );
    uint64 vendorguid, bagguid;
    uint32 item;
    uint8 bagslot, count;

    recv_data >> vendorguid >> item >> bagguid >> bagslot >> count;

    uint8 bag = NULL_BAG;                                   // init for case invalid bagGUID

    // find bag slot by bag guid
    if (bagguid == _player->GetGUID())
        bag = INVENTORY_SLOT_BAG_0;
    else
    {
        for (int i = INVENTORY_SLOT_BAG_START; i < INVENTORY_SLOT_BAG_END;++i)
        {
            if (Bag *pBag = (Bag*)_player->GetItemByPos(INVENTORY_SLOT_BAG_0,i))
            {
                if (bagguid == pBag->GetGUID())
                {
                    bag = i;
                    break;
                }
            }
        }
    }

    // bag not found, cheating?
    if (bag == NULL_BAG)
        return;

    GetPlayer()->BuyItemFromVendor(vendorguid,item,count,bag,bagslot);
}

void WorldSession::HandleBuyItemOpcode( WorldPacket & recv_data )
{
    DEBUG_LOG( "WORLD: Received CMSG_BUY_ITEM" );
    uint64 vendorguid;
    uint32 item;
    uint8 count, unk1;

    recv_data >> vendorguid >> item >> count >> unk1;

    GetPlayer()->BuyItemFromVendor(vendorguid,item,count,NULL_BAG,NULL_SLOT);
}

void WorldSession::HandleListInventoryOpcode( WorldPacket & recv_data )
{
    ObjectGuid guid;

    recv_data >> guid;

    if (!GetPlayer()->isAlive())
        return;

    DEBUG_LOG("WORLD: Recvd CMSG_LIST_INVENTORY");

    SendListInventory( guid );
}

void WorldSession::SendListInventory(ObjectGuid vendorguid)
{
    DEBUG_LOG("WORLD: Sent SMSG_LIST_INVENTORY");

    Creature *pCreature = GetPlayer()->GetNPCIfCanInteractWith(vendorguid, UNIT_NPC_FLAG_VENDOR);
    if (!pCreature)
    {
        DEBUG_LOG("WORLD: SendListInventory - %s not found or you can't interact with him.", vendorguid.GetString().c_str());
        _player->SendSellError(SELL_ERR_CANT_FIND_VENDOR, NULL, 0, 0);
        return;
    }

    // remove fake death
    if(GetPlayer()->hasUnitState(UNIT_STAT_DIED))
        GetPlayer()->RemoveSpellsCausingAura(SPELL_AURA_FEIGN_DEATH);

    // Stop the npc if moving
    pCreature->StopMoving();

    VendorItemData const* vItems = pCreature->GetVendorItems();
    VendorItemData const* tItems = pCreature->GetVendorTemplateItems();

    if (!vItems && !tItems)
    {
        WorldPacket data( SMSG_LIST_INVENTORY, (8+1+1) );
        data << ObjectGuid(vendorguid);
        data << uint8(0);                                   // count==0, next will be error code
        data << uint8(0);                                   // "Vendor has no inventory"
        SendPacket(&data);
        return;
    }

    uint8 customitems = vItems ? vItems->GetItemCount() : 0;
    uint8 numitems = customitems + (tItems ? tItems->GetItemCount() : 0);

    uint8 count = 0;

    WorldPacket data( SMSG_LIST_INVENTORY, (8+1+numitems*7*4) );
    data << ObjectGuid(vendorguid);

    size_t count_pos = data.wpos();
    data << uint8(count);

    float discountMod = _player->GetReputationPriceDiscount(pCreature);

    for(int i = 0; i < numitems; ++i )
    {
        VendorItem const* crItem = i < customitems ? vItems->GetItem(i) : tItems->GetItem(i - customitems);

        if (crItem)
        {
            if (ItemPrototype const *pProto = ObjectMgr::GetItemPrototype(crItem->item))
            {
                if (!_player->isGameMaster())
                {
                    // class wrong item skip only for bindable case
                    if ((pProto->AllowableClass & _player->getClassMask()) == 0 && pProto->Bonding == BIND_WHEN_PICKED_UP)
                        continue;

                    // race wrong item skip always
                    if ((pProto->AllowableRace & _player->getRaceMask()) == 0)
                        continue;

                    // when no faction required but rank > 0 will be used faction id from the vendor faction template to compare the rank
                    if (!pProto->RequiredReputationFaction && pProto->RequiredReputationRank > 0 &&
                        ReputationRank(pProto->RequiredReputationRank) > _player->GetReputationRank(pCreature->getFactionTemplateEntry()->faction))
                        continue;
                }

                ++count;

                // reputation discount
                uint32 price = uint32(floor(pProto->BuyPrice * discountMod));

                data << uint32(count);
                data << uint32(crItem->item);
                data << uint32(pProto->DisplayInfoID);
                data << uint32(crItem->maxcount <= 0 ? 0xFFFFFFFF : pCreature->GetVendorItemCurrentCount(crItem));
                data << uint32(price);
                data << uint32(pProto->MaxDurability);
                data << uint32(pProto->BuyCount);
            }
        }
    }

    if (count == 0)
    {
        data << uint8(0);                                   // "Vendor has no inventory"
        SendPacket(&data);
        return;
    }

    data.put<uint8>(count_pos, count);
    SendPacket(&data);
}

void WorldSession::HandleAutoStoreBagItemOpcode( WorldPacket & recv_data )
{
    //DEBUG_LOG("WORLD: CMSG_AUTOSTORE_BAG_ITEM");
    uint8 srcbag, srcslot, dstbag;

    recv_data >> srcbag >> srcslot >> dstbag;
    //DEBUG_LOG("STORAGE: receive srcbag = %u, srcslot = %u, dstbag = %u", srcbag, srcslot, dstbag);

    Item *pItem = _player->GetItemByPos( srcbag, srcslot );
    if( !pItem )
        return;

    if(!_player->IsValidPos(dstbag, NULL_SLOT, false))      // can be autostore pos
    {
        _player->SendEquipError( EQUIP_ERR_ITEM_DOESNT_GO_TO_SLOT, NULL, NULL );
        return;
    }

    uint16 src = pItem->GetPos();

    // check unequip potability for equipped items and bank bags
    if(_player->IsEquipmentPos ( src ) || _player->IsBagPos ( src ))
    {
        uint8 msg = _player->CanUnequipItem( src, !_player->IsBagPos ( src ));
        if(msg != EQUIP_ERR_OK)
        {
            _player->SendEquipError( msg, pItem, NULL );
            return;
        }
    }

    ItemPosCountVec dest;
    uint8 msg = _player->CanStoreItem( dstbag, NULL_SLOT, dest, pItem, false );
    if( msg != EQUIP_ERR_OK )
    {
        _player->SendEquipError( msg, pItem, NULL );
        return;
    }

    // no-op: placed in same slot
    if(dest.size() == 1 && dest[0].pos == src)
    {
        // just remove gray item state
        _player->SendEquipError( EQUIP_ERR_NONE, pItem, NULL );
        return;
    }

    _player->RemoveItem(srcbag, srcslot, true );
    _player->StoreItem( dest, pItem, true );
}


bool WorldSession::CheckBanker(ObjectGuid guid)
{
    // GM case
    if (guid == GetPlayer()->GetObjectGuid())
    {
        // command case will return only if player have real access to command
        if (!ChatHandler(GetPlayer()).FindCommand("bank"))
        {
            DEBUG_LOG("%s attempt open bank in cheating way.", guid.GetString().c_str());
            return false;
        }
    }
    // banker case
    else
    {
        if (!GetPlayer()->GetNPCIfCanInteractWith(guid, UNIT_NPC_FLAG_BANKER))
        {
            DEBUG_LOG("Banker %s not found or you can't interact with him.", guid.GetString().c_str());
            return false;
        }
    }

    return true;
}

void WorldSession::HandleBuyBankSlotOpcode(WorldPacket& recvPacket)
{
    DEBUG_LOG("WORLD: CMSG_BUY_BANK_SLOT");

    ObjectGuid guid;
    recvPacket >> guid;

    if (!CheckBanker(guid))
        return;

    uint32 slot = _player->GetBankBagSlotCount();

    // next slot
    ++slot;

    DETAIL_LOG("PLAYER: Buy bank bag slot, slot number = %u", slot);

    BankBagSlotPricesEntry const* slotEntry = sBankBagSlotPricesStore.LookupEntry(slot);

    WorldPacket data(SMSG_BUY_BANK_SLOT_RESULT, 4);

    if(!slotEntry)
    {
        data << uint32(ERR_BANKSLOT_FAILED_TOO_MANY);
        SendPacket(&data);
        return;
    }

    uint32 price = slotEntry->price;

    if (_player->GetMoney() < price)
    {
        data << uint32(ERR_BANKSLOT_INSUFFICIENT_FUNDS);
        SendPacket(&data);
        return;
    }

    _player->SetBankBagSlotCount(slot);
    _player->ModifyMoney(-int32(price));

     data << uint32(ERR_BANKSLOT_OK);
     SendPacket(&data);
}

void WorldSession::HandleAutoBankItemOpcode(WorldPacket& recvPacket)
{
    DEBUG_LOG("WORLD: CMSG_AUTOBANK_ITEM");
    uint8 srcbag, srcslot;

    recvPacket >> srcbag >> srcslot;
    DEBUG_LOG("STORAGE: receive srcbag = %u, srcslot = %u", srcbag, srcslot);

    Item *pItem = _player->GetItemByPos( srcbag, srcslot );
    if( !pItem )
        return;

    ItemPosCountVec dest;
    uint8 msg = _player->CanBankItem( NULL_BAG, NULL_SLOT, dest, pItem, false );
    if( msg != EQUIP_ERR_OK )
    {
        _player->SendEquipError( msg, pItem, NULL );
        return;
    }

    // no-op: placed in same slot
    if(dest.size() == 1 && dest[0].pos == pItem->GetPos())
    {
        // just remove gray item state
        _player->SendEquipError( EQUIP_ERR_NONE, pItem, NULL );
        return;
    }

    _player->RemoveItem(srcbag, srcslot, true);
    _player->BankItem( dest, pItem, true );
}

void WorldSession::HandleAutoStoreBankItemOpcode(WorldPacket& recvPacket)
{
    DEBUG_LOG("WORLD: CMSG_AUTOSTORE_BANK_ITEM");
    uint8 srcbag, srcslot;

    recvPacket >> srcbag >> srcslot;
    DEBUG_LOG("STORAGE: receive srcbag = %u, srcslot = %u", srcbag, srcslot);

    Item *pItem = _player->GetItemByPos( srcbag, srcslot );
    if( !pItem )
        return;

    if(_player->IsBankPos(srcbag, srcslot))                 // moving from bank to inventory
    {
        ItemPosCountVec dest;
        uint8 msg = _player->CanStoreItem( NULL_BAG, NULL_SLOT, dest, pItem, false );
        if( msg != EQUIP_ERR_OK )
        {
            _player->SendEquipError( msg, pItem, NULL );
            return;
        }

        _player->RemoveItem(srcbag, srcslot, true);
        _player->StoreItem( dest, pItem, true );
    }
    else                                                    // moving from inventory to bank
    {
        ItemPosCountVec dest;
        uint8 msg = _player->CanBankItem( NULL_BAG, NULL_SLOT, dest, pItem, false );
        if( msg != EQUIP_ERR_OK )
        {
            _player->SendEquipError( msg, pItem, NULL );
            return;
        }

        _player->RemoveItem(srcbag, srcslot, true);
        _player->BankItem( dest, pItem, true );
    }
}

void WorldSession::HandleSetAmmoOpcode(WorldPacket & recv_data)
{
    if(!GetPlayer()->isAlive())
    {
        GetPlayer()->SendEquipError( EQUIP_ERR_YOU_ARE_DEAD, NULL, NULL );
        return;
    }

    DEBUG_LOG("WORLD: CMSG_SET_AMMO");
    uint32 item;

    recv_data >> item;

    if(!item)
        GetPlayer()->RemoveAmmo();
    else
        GetPlayer()->SetAmmo(item);
}

void WorldSession::SendEnchantmentLog(uint64 Target, uint64 Caster,uint32 ItemID,uint32 SpellID)
{
    WorldPacket data(SMSG_ENCHANTMENTLOG, (8+8+4+4+1));     // last check 2.0.10
    data << uint64(Target);
    data << uint64(Caster);
    data << uint32(ItemID);
    data << uint32(SpellID);
    data << uint8(0);
    SendPacket(&data);
}

void WorldSession::SendItemEnchantTimeUpdate(uint64 Playerguid, uint64 Itemguid,uint32 slot,uint32 Duration)
{
                                                            // last check 2.0.10
    WorldPacket data(SMSG_ITEM_ENCHANT_TIME_UPDATE, (8+4+4+8));
    data << uint64(Itemguid);
    data << uint32(slot);
    data << uint32(Duration);
    data << uint64(Playerguid);
    SendPacket(&data);
}

void WorldSession::HandleItemNameQueryOpcode(WorldPacket & recv_data)
{
    uint32 itemid;
    recv_data >> itemid;
    recv_data.read_skip<uint64>();                          // guid

    DEBUG_LOG("WORLD: CMSG_ITEM_NAME_QUERY %u", itemid);
    ItemPrototype const *pProto = ObjectMgr::GetItemPrototype( itemid );
    if( pProto )
    {
        std::string Name;
        Name = pProto->Name1;

        int loc_idx = GetSessionDbLocaleIndex();
        if (loc_idx >= 0)
        {
            ItemLocale const *il = sObjectMgr.GetItemLocale(pProto->ItemId);
            if (il)
            {
                if (il->Name.size() > size_t(loc_idx) && !il->Name[loc_idx].empty())
                    Name = il->Name[loc_idx];
            }
        }
                                                            // guess size
        WorldPacket data(SMSG_ITEM_NAME_QUERY_RESPONSE, (4+10));
        data << uint32(pProto->ItemId);
        data << Name;
        data << uint32(pProto->InventoryType);
        SendPacket(&data);
        return;
    }
    else
        sLog.outError("WORLD: CMSG_ITEM_NAME_QUERY for item %u failed (unknown item)", itemid);
}

void WorldSession::HandleWrapItemOpcode(WorldPacket& recv_data)
{
    DEBUG_LOG("Received opcode CMSG_WRAP_ITEM");

    uint8 gift_bag, gift_slot, item_bag, item_slot;
    //recv_data.hexlike();

    recv_data >> gift_bag >> gift_slot;                     // paper
    recv_data >> item_bag >> item_slot;                     // item

    DEBUG_LOG("WRAP: receive gift_bag = %u, gift_slot = %u, item_bag = %u, item_slot = %u", gift_bag, gift_slot, item_bag, item_slot);

    Item *gift = _player->GetItemByPos( gift_bag, gift_slot );
    if (!gift)
    {
        _player->SendEquipError( EQUIP_ERR_ITEM_NOT_FOUND, gift, NULL );
        return;
    }

    // cheating: non-wrapper wrapper (all empty wrappers is stackable)
    if (!(gift->GetProto()->Flags & ITEM_FLAG_WRAPPER) || gift->GetMaxStackCount() == 1)
    {
        _player->SendEquipError( EQUIP_ERR_ITEM_NOT_FOUND, gift, NULL );
        return;
    }

    Item *item = _player->GetItemByPos( item_bag, item_slot );

    if (!item)
    {
        _player->SendEquipError( EQUIP_ERR_ITEM_NOT_FOUND, item, NULL );
        return;
    }

    if (item == gift)                                       // not possible with packet from real client
    {
        _player->SendEquipError( EQUIP_ERR_WRAPPED_CANT_BE_WRAPPED, item, NULL );
        return;
    }

    if (item->IsEquipped())
    {
        _player->SendEquipError( EQUIP_ERR_EQUIPPED_CANT_BE_WRAPPED, item, NULL );
        return;
    }

    if (!item->GetGuidValue(ITEM_FIELD_GIFTCREATOR).IsEmpty())// HasFlag(ITEM_FIELD_FLAGS, ITEM_DYNFLAG_WRAPPED);
    {
        _player->SendEquipError( EQUIP_ERR_WRAPPED_CANT_BE_WRAPPED, item, NULL );
        return;
    }

    if (item->IsBag())
    {
        _player->SendEquipError( EQUIP_ERR_BAGS_CANT_BE_WRAPPED, item, NULL );
        return;
    }

    if (item->IsSoulBound())
    {
        _player->SendEquipError( EQUIP_ERR_BOUND_CANT_BE_WRAPPED, item, NULL );
        return;
    }

    if (item->GetMaxStackCount() != 1)
    {
        _player->SendEquipError( EQUIP_ERR_STACKABLE_CANT_BE_WRAPPED, item, NULL );
        return;
    }

    // maybe not correct check  (it is better than nothing)
    if (item->GetProto()->MaxCount > 0)
    {
        _player->SendEquipError( EQUIP_ERR_UNIQUE_CANT_BE_WRAPPED, item, NULL );
        return;
    }

    CharacterDatabase.BeginTransaction();
    CharacterDatabase.PExecute("INSERT INTO character_gifts VALUES ('%u', '%u', '%u', '%u')", GUID_LOPART(item->GetOwnerGUID()), item->GetGUIDLow(), item->GetEntry(), item->GetUInt32Value(ITEM_FIELD_FLAGS));
    item->SetEntry(gift->GetEntry());

    switch (item->GetEntry())
    {
        case 5042:  item->SetEntry( 5043); break;
        case 5048:  item->SetEntry( 5044); break;
        case 17303: item->SetEntry(17302); break;
        case 17304: item->SetEntry(17305); break;
        case 17307: item->SetEntry(17308); break;
        case 21830: item->SetEntry(21831); break;
    }
    item->SetGuidValue(ITEM_FIELD_GIFTCREATOR, _player->GetObjectGuid());
    item->SetUInt32Value(ITEM_FIELD_FLAGS, ITEM_DYNFLAG_WRAPPED);
    item->SetState(ITEM_CHANGED, _player);

    if(item->GetState() == ITEM_NEW)                        // save new item, to have alway for `character_gifts` record in `item_instance`
    {
        // after save it will be impossible to remove the item from the queue
        item->RemoveFromUpdateQueueOf(_player);
        item->SaveToDB();                                   // item gave inventory record unchanged and can be save standalone
    }
    CharacterDatabase.CommitTransaction();

    uint32 count = 1;
    _player->DestroyItemCount(gift, count, true);
}

void WorldSession::HandleCancelTempEnchantmentOpcode(WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: CMSG_CANCEL_TEMP_ENCHANTMENT");

    uint32 eslot;

    recv_data >> eslot;

    // apply only to equipped item
    if(!Player::IsEquipmentPos(INVENTORY_SLOT_BAG_0, eslot))
        return;

    Item* item = GetPlayer()->GetItemByPos(INVENTORY_SLOT_BAG_0, eslot);

    if(!item)
        return;

    if(!item->GetEnchantmentId(TEMP_ENCHANTMENT_SLOT))
        return;

    GetPlayer()->ApplyEnchantment(item, TEMP_ENCHANTMENT_SLOT, false);
    item->ClearEnchantment(TEMP_ENCHANTMENT_SLOT);
}
