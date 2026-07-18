#include <StonedExtension.h>

namespace Gothic_II_Addon
{
    ItemExtensionDataStorage* ItemsExtensionData;

    ItemsGeneratorConfig ItemsGeneratorConfigs;
    ItemExtensionsCoreState ItemExtensionsState;
    ItemClassDescriptor BaseItemClassDescriptor;

    Array<zSTRING> ItemPreffixesList;
    Array<zSTRING> ItemAffixesList;
    Array<zSTRING> ItemSuffixesList;
    Array<zSTRING> ItemNamesList;
    Array<zSTRING> ItemRanksList;
    Array<zSTRING> ItemDamageTypesList;
    Array<zSTRING> ItemProtectionsTypesList;

    Map<int, zSTRING> ItemConditionsText;
    Map<byte, zSTRING> ItemTypesText;
    Map<byte, zSTRING> ItemClassesText;
    Map<byte, zSTRING> ItemSubClassesText;

    Map<ItemClassKey, ItemClassDescriptor> ItemsClassData;
    StringMap<ItemClassKey> ItemsClassDataIndexer_InstanceName(1024);

    zSTRING ItemNameValueString;
    zSTRING ItemDamageString;
    zSTRING ItemConditionString;
    zSTRING ItemRangeString;
    zSTRING ItemOrcWeaponTagString;
    zSTRING ItemProtectionString;
    zSTRING ItemWeightString;
    int ItemCondSpecialSeparator;

    inline void UpdateItemExtensionsCoreState()
    {
        ItemExtensionsState.ItemExtensionsCount = ItemsExtensionData->ItemsCount;
        ItemExtensionsState.NextItemExtensionUId = GetNextItemUId();
    }

    inline void ResetItemExtensionsCoreState()
    {
        ItemExtensionsState = ItemExtensionsCoreState();
        ItemExtensionsState.ItemExtensionsCount = 0U;
        ItemExtensionsState.NextItemExtensionUId = 0U;
    }

    inline ItemClassKey ItemClassKey_Create(byte itemType, byte itemClass, byte itemSubClass) {
        return (static_cast<ItemClassKey>(itemType) << 16) | (static_cast<ItemClassKey>(itemClass) << 8) | (static_cast<ItemClassKey>(itemSubClass));
    }
    inline byte ItemClassKey_GetType(ItemClassKey key) { return static_cast<byte>((key >> 16) & 0xFF); }
    inline byte ItemClassKey_GetClass(ItemClassKey key) { return static_cast<byte>((key >> 8) & 0xFF); }
    inline byte ItemClassKey_GetSubClass(ItemClassKey key) { return static_cast<byte>(key & 0xFF); }
    inline void ItemClassKey_Unpack(ItemClassKey key, byte& outType, byte& outClass, byte& outSubClass) 
    { 
        outType = static_cast<byte>((key >> 16) & 0xFF);        
        outClass = static_cast<byte>((key >> 8) & 0xFF);
        outSubClass = static_cast<byte>(key & 0xFF);
    }

    inline bool IsExtendedItem(const oCItem* item) { return item ? GetItemInstanceName(item).StartWith(GenerateItemPrefix) : false; }
    inline bool IsExtendedItem(const zSTRING& instanceName) { return !instanceName.IsEmpty() && instanceName.StartWith(GenerateItemPrefix); }

    inline ItemExtension* GetItemExtension(const unsigned int key) { return (ItemsExtensionData != Null) ? ItemsExtensionData->Get(key) : Null; }
    inline ItemExtension* GetItemExtension(const zSTRING& instanceName) { return (ItemsExtensionData != Null) ? ItemsExtensionData->Get(instanceName) : Null; }
    inline ItemExtension* GetItemExtension(const oCItem* item) { return item ? GetItemExtension(GetItemInstanceName(item)) : Null; }

    ItemClassKey GetItemClassKey(const oCItem* item)
    {
        if (!item) return ItemClassKey_Error;

        zSTRING instanceName = GetItemInstanceName(item);
        if (IsExtendedItem(instanceName))
        {
            ItemExtension* itmExt = GetItemExtension(instanceName);
            if (itmExt)
                return itmExt->ItemClassID;
        }

        auto pair = ItemsClassDataIndexer_InstanceName.Find(instanceName);
        if (pair) return *pair;

        const ItemClassKey key = ParseItemClassKey(item);
        ItemsClassDataIndexer_InstanceName.Insert(instanceName, key);
        return key;
    }
    ItemClassKey GetItemClassKey(const zSTRING& instanceName)
    {
        if (instanceName.IsEmpty()) return ItemClassKey_Empty;

        if (IsExtendedItem(instanceName))
        {
            ItemExtension* itmExt = GetItemExtension(instanceName);
            if (itmExt) return itmExt->ItemClassID;
        }

        auto pair = ItemsClassDataIndexer_InstanceName.Find(instanceName);
        if (pair) return *pair;

        ItemClassKey key = ItemClassKey_Empty;
        oCItem* pItem = new oCItem();
        if (parser->CreateInstance(parser->GetIndex(instanceName), pItem))
        {
            key = ParseItemClassKey(pItem);
            ItemsClassDataIndexer_InstanceName.Insert(instanceName, key);
        }
        SAFE_DELETE(pItem);
        return key;
    }

    inline const ItemClassDescriptor* GetItemClassDescriptor(const ItemClassKey key)
    {
        auto pair = ItemsClassData.GetSafePair(key);
        if (!pair)
        {
            DEBUG_MSG("GetItemClassDescriptor: item class descriptor with key: " + Z(static_cast<int>(key) + " not found!"));
            return Null;
        }
        return &pair->GetValue();
    }
    inline const ItemClassDescriptor* GetItemClassDescriptor(const zSTRING& instanceName)
    {
        const ItemClassKey key = GetItemClassKey(instanceName);
        return GetItemClassDescriptor(key);
    }
    inline const ItemClassDescriptor* GetItemClassDescriptor(oCItem* item)
    {
        const ItemClassKey key = GetItemClassKey(item);
        return GetItemClassDescriptor(key);
    }

    inline bool RegisterItemExtension(ItemExtension* itemExtension) 
    { 
        if (ItemsExtensionData->Insert(itemExtension))
        {
            UpdateItemExtensionsCoreState();
            return true;
        }
        return false;
    }

    inline void ClearGeneratedItemsData() 
    { 
        ItemsExtensionData->Clear();
        ClearActiveItemAbilities();
        UpdateItemExtensionsCoreState();
    }


    //-------------------------------------------------------------------
    //						    Items logic
    //-------------------------------------------------------------------

    void BuildItemRequirementsString(zSTRING& conditionString, const oCItem* item)
    {
        conditionString = ItemConditionString;
        zSTRING condLine = zSTRING();

        int totalConditions = 0;
        for (int i = 0; i < ItemExtension_Conditions_Max; ++i)
        {
            if (item->cond_atr[i] == 0 || item->cond_atr[i] == Invalid) continue;

            const auto& condTextData = ItemConditionsText.GetSafePair(item->cond_atr[i]);
            if (condTextData)
            {
                const int value = item->cond_atr[i] == 92 ? item->cond_value[i] * 10 : item->cond_value[i];
                if (item->cond_atr[i] == 90) 
                { 
                    AppendTag(condLine, condTextData->GetValue()); 
                    continue;
                }
                AppendTag(condLine, Z(value) + " " + condTextData->GetValue());
            }
            else { AppendTag(condLine, item->cond_value[i] + " ??? (" + Z item->cond_atr[i] + ")"); }
            ++totalConditions;
        }
        conditionString += (totalConditions > 0) ? 
            condLine : parser->GetSymbol("StExt_Str_No")->stringdata;
    }
    
    inline void BuildItemDescriptionText_Armor(oCItem* item, const ItemExtension* extension)
    {
        zSTRING tagLine = zSTRING();
        auto ConcatProtectionLine = [&](const int index) { AppendTag(tagLine, Z(item->protection[index]) + " " + ItemDamageTypesList[index]); };
        
        tagLine.Clear();
        ConcatProtectionLine(1);
        ConcatProtectionLine(2);
        ConcatProtectionLine(6);
        item->text[0] = ItemProtectionString + tagLine;

        tagLine.Clear();
        ConcatProtectionLine(3);
        ConcatProtectionLine(5);
        item->text[1] = ItemProtectionString + tagLine;

        tagLine.Clear();
        ConcatProtectionLine(4);
        ConcatProtectionLine(7);
        item->text[2] = ItemProtectionString + tagLine;

        item->text[3] = ItemWeightString + Z(item->weight);
        BuildItemRequirementsString(item->text[4], item);        
    }

    inline void BuildItemDescriptionText_Weapon(oCItem* item, const ItemExtension* extension)
    {
        zSTRING damageString = ItemDamageString + Z(item->damageTotal);
        zSTRING itemTypeString = zSTRING();
        zSTRING protectionString = zSTRING();

        zSTRING damageTags = new zSTRING();
        for (int i = 0; i < oEDamageIndex_MAX; ++i)
        {
            if (item->damage[i] > 0)
                AppendTag(damageTags, Z(item->damage[i]) + " " + ItemDamageTypesList[i]);
        }
        if (!damageTags.IsEmpty())damageString += " (" + damageTags + ")";

        auto classPair = ItemClassesText.GetSafePair(extension->Class);
        auto subClassPair = ItemSubClassesText.GetSafePair(extension->SubClass);

        if (classPair && subClassPair) itemTypeString = classPair->GetValue() + ", " + subClassPair->GetValue();
        else if (subClassPair) itemTypeString = subClassPair->GetValue();
        else if (classPair) itemTypeString = classPair->GetValue();
        else itemTypeString = "";

        if (HasFlag(item->hitp, bit_item_orc_weapon) || extension->SubClass == (byte)ItemSubClass::OrcWeapon) AppendTag(itemTypeString, ItemOrcWeaponTagString);
        if (extension->Class == (byte)ItemClass::MeeleWeapon) AppendTag(itemTypeString, ItemRangeString + Z(item->range));

        zSTRING protectionTags = new zSTRING();
        for (int i = 0; i < oEDamageIndex_MAX; ++i)
        {
            if (item->protection[i] > 0)
                AppendTag(protectionTags, Z(item->protection[i]) + " " + ItemDamageTypesList[i]);
        }
        if (!protectionTags.IsEmpty()) protectionString = ItemProtectionString + protectionTags;

        item->text[0] = "";
        item->text[1] = damageString;
        item->text[2] = itemTypeString;
        item->text[3] = protectionString;
        BuildItemRequirementsString(item->text[4], item);
    }

    inline void BuildItemDescriptionText_Jewelry(oCItem* item, const ItemExtension* extension)
    {
        switch (static_cast<ItemClass>(extension->Class))
        {
            case ItemClass::Amulet:
            case ItemClass::Ring:
            case ItemClass::Belt: BuildItemRequirementsString(item->text[0], item); break;

            case ItemClass::Braclets: break;
            case ItemClass::Earing: break;
            case ItemClass::Collar: break;
            case ItemClass::Trophy: break;
            default: break;
        }
    }

    inline void BuildItemDescriptionText_Consumable(oCItem* item, const ItemExtension* extension) { }
    inline void BuildItemDescriptionText_Munition(oCItem* item, const ItemExtension* extension) { }
    inline void BuildItemDescriptionText_Inventory(oCItem* item, const ItemExtension* extension) { }
    inline void BuildItemDescriptionText_Craft(oCItem* item, const ItemExtension* extension) { }
    inline void BuildItemDescriptionText_Other(oCItem* item, const ItemExtension* extension) { }

    void UpdateItemDescriptionText(oCItem* item, const ItemExtension* extension)
    {
        if (!item || !extension)
        {
            DEBUG_MSG_IF(!item, "UpdateItemDescriptionText: item instance is null!");
            DEBUG_MSG_IF(!extension, "UpdateItemDescriptionText: item extension is null");
            return;
        }

        item->count[5] = item->value;
        item->text[5] = ItemRanksList[ValidateValue(extension->Rank, 0, ItemsGeneratorConfigs.ItemMaxRank)] + " | " + ItemNameValueString;

        switch (static_cast<ItemType>(extension->Type))
        {
            case ItemType::Unknown: break;
            case ItemType::Armor: BuildItemDescriptionText_Armor(item, extension); break;
            case ItemType::Weapon: BuildItemDescriptionText_Weapon(item, extension); break;
            case ItemType::Jewelry: BuildItemDescriptionText_Jewelry(item, extension); break;
            case ItemType::Consumable: BuildItemDescriptionText_Consumable(item, extension); break;
            case ItemType::Munition: BuildItemDescriptionText_Munition(item, extension); break;
            case ItemType::Inventory: BuildItemDescriptionText_Inventory(item, extension); break;
            case ItemType::Craft: BuildItemDescriptionText_Craft(item, extension); break;
            case ItemType::Other: BuildItemDescriptionText_Other(item, extension); break;
            default: break;
        }
    }

    bool ApplyItemExtension(oCItem* item, const ItemExtension* extension)
    {
        if (!item || !extension) 
        {
            DEBUG_MSG_IF(!item, "ApplyItemExtension: item instance is null!");
            DEBUG_MSG_IF(!extension, "ApplyItemExtension: item extension is null");
            return false; 
        }
        //DEBUG_MSG("ApplyItemExtension: apply item extension for '" + item->GetInstanceName() + "' ...");

        zSTRING affix = (extension->Affix.IsEmpty() ? "" : " - ") + extension->Affix;
        zSTRING preffix = extension->Preffix + (extension->Preffix.IsEmpty() ? "" : " ");
        zSTRING suffix = (extension->Suffix.IsEmpty() ? "" : " ") + extension->Suffix;

        item->description = extension->OwnName.IsEmpty() ? preffix + item->description + affix + suffix : extension->OwnName;
        item->name = item->description;

        item->max_hitp = static_cast<int>(extension->UId);
        item->flags |= extension->ExtraFlags_Base;
        item->hitp |= extension->ExtraFlags_Additional;

        if (extension->Cost > 0) item->value = extension->Cost;
        if (item->value <= 0) item->value = 1;
        // ItemClassData bywa NULL - gdy klasa itemu nie ma zarejestrowanego
        // deskryptora (ItemExtension::Initialize ustawia go z GetItemClassDescriptor,
        // ktore moze zwrocic NULL - patrz DEBUG_MSG "ItemClassData not found"). Autor
        // guarduje to WSZEDZIE (UpgradeDamage/Change*/AddSockets: "if (!ItemClassData) return"),
        // ale TU i w UpdatePrice przeoczyl. To byl crash: bron bossa trafiala na klase
        // bez deskryptora, a InitByScript -> ApplyItemExtension derefowal NULL->ModWeight,
        // gdy plecak gracza zaczytywal ja po podniesieniu. Fallback: wlasna waga itemu.
        item->weight = (extension->ItemClassData && extension->ItemClassData->ModWeight && extension->Weight != Invalid) ? extension->Weight : item->weight;

        for (int i = 0; i < oEDamageIndex_MAX; ++i)
        {
            item->protection[i] = extension->Protection[i];
            item->damage[i] = extension->Damage[i];
        }
        item->damageTotal = extension->DamageTotal;
        item->damageTypes = extension->DamageTypes;
        item->range = extension->Range;

        for (int i = 0; i < ItemExtension_Conditions_Max; ++i)
        {
            item->cond_atr[i] = extension->CondAtr[i];
            item->cond_value[i] = extension->CondValue[i];
        }
        UpdateItemDescriptionText(item, extension);
        return true;
    }
    
    bool AddItemExtensionEffects(oCItem* item, const ItemExtension* extension)
    {
        if (!item || !extension) { return false; }

        const ItemType itemType = static_cast<ItemType>(extension->Type);
        if ((itemType != ItemType::Armor) && (itemType != ItemType::Weapon) && (itemType != ItemType::Jewelry)) return true;

        zCPar_Symbol* statsArray = parser->GetSymbol("StExt_PcStats_Items");
        if (!statsArray)
        {
            DEBUG_MSG("AddItemExtensionEffects: symbol 'StExt_PcStats_Items' not found!");
            return false;
        }

        auto addStat = [&](const int statId, const int statBonus)
        {
            statsArray->SetValue(statsArray->intdata[statId] + statBonus, statId);
            parser->CallFunc(HandlePcStatChangeFunc, statId, statBonus);
        };

        for (int i = 0; i < ItemExtension_OwnStats_Max; ++i)
        {
            if (IsIndexInBounds(extension->OwnStatId[i], MaxStatId))
                addStat(extension->OwnStatId[i], extension->OwnStatValue[i]);
        }

        for (int i = 0; i < ItemExtension_Stats_Max; ++i)
        {
            if (IsIndexInBounds(extension->StatId[i], MaxStatId))
                addStat(extension->StatId[i], extension->StatValue[i]);
        }

        ApplyItemAbilities(item, extension);
        return true;
    }

    bool RemoveItemExtensionEffects(oCItem* item, const ItemExtension* extension)
    {
        if (!item || !extension) { return false; }

        const ItemType itemType = static_cast<ItemType>(extension->Type);
        if ((itemType != ItemType::Armor) && (itemType != ItemType::Weapon) && (itemType != ItemType::Jewelry)) return true;

        zCPar_Symbol* statsArray = parser->GetSymbol("StExt_PcStats_Items");
        if (!statsArray)
        {
            DEBUG_MSG("RemoveItemExtensionEffects: symbol 'StExt_PcStats_Items' not found!");
            return false;
        }

        auto removeStat = [&](const int statId, const int statBonus)
        {
            statsArray->SetValue(ValidateValueMin(statsArray->intdata[statId] - statBonus, 0), statId);
            parser->CallFunc(HandlePcStatChangeFunc, statId, -statBonus);
        };

        for (int i = 0; i < ItemExtension_OwnStats_Max; ++i)
        {
            if (IsIndexInBounds(extension->OwnStatId[i], MaxStatId))
                removeStat(extension->OwnStatId[i], extension->OwnStatValue[i]);
        }

        for (int i = 0; i < ItemExtension_Stats_Max; ++i)
        {
            if (IsIndexInBounds(extension->StatId[i], MaxStatId))
                removeStat(extension->StatId[i], extension->StatValue[i]);
        }

        RemoveItemAbilities(item, extension);
        return true;
    }


    //-------------------------------------------------------------------
    //						 Serialization logic
    //-------------------------------------------------------------------

    string GetGeneratedItemsArchivePath()
    {
        int slotID = SaveLoadGameInfo.slotID;
        string savesDir = zoptions->GetDirString(zTOptionPaths::DIR_SAVEGAMES);
        string slotDir = GetSaveSlotNameByID(slotID);
        string archivePath = string::Combine("%s\\%s\\StExt_GeneratedItems.sav", savesDir, slotDir);
        return archivePath;
    }

    inline void ArchiveItemExtensionsCoreState(zCArchiver& ar) 
    { 
        UpdateItemExtensionsCoreState(); 
        ar.WriteRaw("ExtensionsState_ItemExtensionsCount", &ItemExtensionsState.ItemExtensionsCount, sizeof(uint)); 
        ar.WriteRaw("ExtensionsState_NextItemExtensionUId", &ItemExtensionsState.NextItemExtensionUId, sizeof(uint)); 
    }
    inline void UnArchiveItemExtensionsCoreState(zCArchiver& ar) 
    {
        ResetItemExtensionsCoreState(); 
        ar.ReadRaw("ExtensionsState_ItemExtensionsCount", &ItemExtensionsState.ItemExtensionsCount, sizeof(uint));
        ar.ReadRaw("ExtensionsState_NextItemExtensionUId", &ItemExtensionsState.NextItemExtensionUId, sizeof(uint));
    }

    void SaveGeneratedItems()
    {
        DEBUG_MSG("SaveGeneratedItems: save items...");

        string archiveName = GetGeneratedItemsArchivePath();
        zCArchiver* ar = zarcFactory->CreateArchiverWrite(archiveName, zARC_MODE_BINARY_SAFE, 0, 0);
        if (!ar)
        {
            DEBUG_MSG("SaveGeneratedItems: fail to create writer for: " + archiveName);
            Message::Error((string)"SaveGeneratedItems - archiver is Null!", "Critical error!");
            gameMan->ExitGame();
            return;
        }

        if (!ItemsExtensionData)
        {
            DEBUG_MSG("SaveGeneratedItems: ItemsExtensionData is null!");
            return;
        }

        ArchiveItemExtensionsCoreState(*ar);
        ItemsExtensionData->Archive(*ar);

        ar->Close();
        ar->Release();
        DEBUG_MSG("SaveGeneratedItems: saved " + Z(static_cast<int>(ItemsExtensionData->ItemsCount)) + " items.");
    }

    void LoadGeneratedItems()
    {
        DEBUG_MSG("LoadGeneratedItems: load items...");

        const zSTRING archiveName = GetGeneratedItemsArchivePath();
        zCArchiver* ar = zarcFactory->CreateArchiverRead(archiveName, 0);
        if (!ar)
        {
            DEBUG_MSG("LoadGeneratedItems: fail to create reader for: " + archiveName);
            Message::Error((string)"LoadGeneratedItems - archiver is Null!", "Critical error!");
            gameMan->ExitGame();
            return;
        }

        ClearGeneratedItemsData();
        if (!ItemsExtensionData)
            ItemsExtensionData = new ItemExtensionDataStorage();

        UnArchiveItemExtensionsCoreState(*ar);
        ItemsExtensionData->UnArchive(*ar);

        ar->Close();
        ar->Release();
        DEBUG_MSG_IF(ItemsExtensionData->ItemsCount != ItemExtensionsState.ItemExtensionsCount, "LoadGeneratedItems: not all expected items seems loaded!");
        DEBUG_MSG("LoadGeneratedItems: loaded " + Z(static_cast<int>(ItemsExtensionData->ItemsCount)) + "/" + Z(static_cast<int>(ItemExtensionsState.ItemExtensionsCount)) + " items.");
        UpdateItemExtensionsCoreState();
    }


    //-------------------------------------------------------------------
    //						        API
    //-------------------------------------------------------------------

    void FindPlayerItems(const int itemClassId, Array<const oCItem*>& foundItems)
    {
        if (!player || !player->inventory2.GetContents())
        {
            DEBUG_MSG("FindPlayerItems - player (or it's inventory) seems not initialized!?");
            return;
        }

        //zCListSort<oCItem>* it = player->inventory2.GetContents()->GetNextInList();
        zCListSort<oCItem>* it = player->inventory2.GetContents();
        while (it)
        {
            const oCItem* itm = it->GetData();
            if (!itm) {
                it = it->GetNextInList();
                continue;
            }
            if (HasFlag(itm->flags, ITM_FLAG_ACTIVE) || HasFlag(itm->hitp, bit_item_questitem)) {
                it = it->GetNextInList();
                continue;
            }

            if (itemClassId == GetItemClassKey(itm)) 
                foundItems.Insert(itm);
            it = it->GetNextInList();
        }
    }

    void FindPlayerItems(const Array<int>& itemClassIds, Array<const oCItem*>& foundItems)
    {
        if (!player || !player->inventory2.GetContents())
        {
            DEBUG_MSG("FindPlayerItems - player (or it's inventory) seems not initialized!?");
            return;
        }

        zCListSort<oCItem>* it = player->inventory2.GetContents();
        while (it)
        {
            const oCItem* itm = it->GetData();
            if (!itm) {
                it = it->GetNextInList();
                continue;
            }
            if (HasFlag(itm->flags, ITM_FLAG_ACTIVE) || HasFlag(itm->hitp, bit_item_questitem)) {
                it = it->GetNextInList();
                continue;
            }

            const int itmClassId = GetItemClassKey(itm);
            for (uint i = 0; i < itemClassIds.GetNum(); ++i)
            {
                if (itemClassIds[i] == itmClassId)
                {
                    foundItems.Insert(itm);
                    break;
                }
            }
            it = it->GetNextInList();
        }
    }

    void FindPlayerUndefinedItems(Array<const oCItem*>& foundItems)
    {
        if (!player || !player->inventory2.GetContents())
        {
            DEBUG_MSG("FindPlayerUndefinedItems - player (or it's inventory) seems not initialized!?");
            return;
        }

        //zCListSort<oCItem>* it = player->inventory2.GetContents()->GetNextInList();
        zCListSort<oCItem>* it = player->inventory2.GetContents();
        while (it)
        {
            const oCItem* itm = it->GetData();
            if (!itm)
            {
                it = it->GetNextInList();
                continue;
            }
            const ItemExtension* ext = GetItemExtension(itm);
            if (ext && ext->Tags[(int)ItemTags::Unidentified])
                foundItems.Insert(itm);
            it = it->GetNextInList();
        }
    }

    inline const int FindGeneratedItem(const ItemClassKey itemClassKey, const int power, const int rank)
    {
        byte type = 0, cls = 0, subClass = 0;
        ItemClassKey_Unpack(itemClassKey, type, cls, subClass);

        const uint lookUpBufferSize = ItemsGeneratorConfigs.LookupGeneratedItemCountBufferSize <= 32U ? 32U : 
            static_cast<unsigned int>(ItemsGeneratorConfigs.LookupGeneratedItemCountBufferSize);
        Array<ItemExtension*> lookUpBuffer = Array<ItemExtension*>();
        ItemsExtensionData->Get(lookUpBuffer, lookUpBufferSize, type, cls, subClass, 0ULL, Invalid, rank, power, 0, static_cast<int>(power * 0.05));

        const uint realBufferSize = lookUpBuffer.GetNum();
        if (realBufferSize <= 1U) return Invalid;

        const uint selectedIndex = StExt_Rand::Index(realBufferSize);
        ItemExtension* extension = lookUpBuffer[selectedIndex];
        if (!extension) 
            return Invalid;

        const int itemInstnceId = parser->GetIndex(extension->InstanceName);
        if (itemInstnceId == Invalid)
        {
            DEBUG_MSG("FindGeneratedItem: item virtual instance not in table! Instance name: " + extension->InstanceName);
            return Invalid;
        }
        return itemInstnceId;
    }

    inline const int FinalizeItemGeneration(ItemExtension* extension)
    {
        if (!extension)
        {
            DEBUG_MSG("GenerateNewItem: generation failed!");
            return Invalid;
        }

        if (!RegisterItemExtension(extension))
        {
            DEBUG_MSG("GenerateNewItem: extension registration failed!");
            SAFE_DELETE(extension);
            return Invalid;
        }

        const int itemInstnceId = parser->GetIndex(extension->InstanceName);
        if (itemInstnceId == Invalid)
        {
            DEBUG_MSG("GenerateNewItem: item virtual instance not in table! Instance name: " + extension->InstanceName);
            return Invalid;
        }
        return itemInstnceId;
    }

    const int GenerateNewMagicItem(const int itemClassId, const int power)
    {
        if (itemClassId <= 0) 
            return Invalid;

        const ItemClassKey itemClass = static_cast<ItemClassKey>(itemClassId);
        int itemInstnceId = Invalid;

        if (StExt_ForceItemRank < 0 &&
            (ItemsExtensionData->ItemsCount > static_cast<uint>(ItemsGeneratorConfigs.LookupGeneratedItemCountThreshold)) &&
            StExt_Rand::Permille(ItemsGeneratorConfigs.LookupGeneratedItemChance))
        {
            itemInstnceId = FindGeneratedItem(itemClass, power, ItemsGeneratorConfigs.ItemMaxRank);
            if(itemInstnceId != Invalid) return itemInstnceId;
        }

        DEBUG_MSG("GenerateNewMagicItem: start generation [classId: " + Z(itemClassId) + "] ...");
        ItemExtension* extension = RollSpecificMagicItem(power, itemClass);
        itemInstnceId = FinalizeItemGeneration(extension);
        return itemInstnceId;
    }

    const int GenerateNewRegularItem(const int itemClassId, const int power)
    {
        if (itemClassId <= 0)
            return Invalid;

        const ItemClassKey itemClass = static_cast<ItemClassKey>(itemClassId);
        int itemInstnceId = Invalid;

        if (StExt_Rand::Permille(ItemsGeneratorConfigs.PlainPrototypeItemGenerateChance))
        {
            const ItemClassDescriptor* classDesc = GetItemClassDescriptor(itemClass);
            if (classDesc)
            {
                const zSTRING instanceName = RollPrototypeInstanceName(CalcItemTier(power, classDesc), classDesc);
                itemInstnceId = parser->GetIndex(instanceName);
                if (itemInstnceId != Invalid) 
                    return itemInstnceId;
            }
        }

        if ((ItemsExtensionData->ItemsCount > static_cast<uint>(ItemsGeneratorConfigs.LookupGeneratedItemCountThreshold)) && 
            StExt_Rand::Permille(ItemsGeneratorConfigs.LookupGeneratedItemChance))
        {
            itemInstnceId = FindGeneratedItem(itemClass, power, 0);
            if (itemInstnceId != Invalid) return itemInstnceId;
        }

        DEBUG_MSG("GenerateNewRegularItem: start generation [classId: " + Z(itemClassId) + "] ...");
        ItemExtension* extension = RollSpecificSimpleItem(power, itemClass);
        itemInstnceId = FinalizeItemGeneration(extension);
        return itemInstnceId;
    }

    // Enchant the PLAYER'S actual item in place: same base item (name,
    // visuals, damage/protection) upgraded with a rolled magic extension.
    // Returns the new dynamic instance id, or Invalid.
    const int EnchantItemInPlace(oCItem* item, const int power)
    {
        if (!item) return Invalid;
        if (GetItemExtension(item)) return Invalid; // already magic

        DEBUG_MSG("EnchantItemInPlace: enchanting '" + item->GetInstanceName() + "' [power: " + Z(power) + "] ...");
        ItemExtension* extension = RollInPlaceMagicItem(item, power <= 0 ? 1 : power);
        return FinalizeItemGeneration(extension);
    }

    // Reroll ("stat mix") for an already-magic item: fresh full roll bound
    // to the SAME base item (name/visuals/base stats via BaseInstanceName).
    // Returns the new dynamic instance id; the old extension stays orphaned
    // in storage (harmless) while the script swaps the inventory item.
    const int RerollItemInPlace(oCItem* item, const int power)
    {
        if (!item) return Invalid;
        ItemExtension* oldExt = GetItemExtension(item);
        if (!oldExt) return Invalid; // plain item -> use EnchantItemInPlace

        zSTRING baseName = oldExt->BaseInstanceName;
        const int baseId = parser->GetIndex(baseName);
        if (baseId == Invalid)
        {
            DEBUG_MSG("RerollItemInPlace: base instance '" + baseName + "' not found!");
            return Invalid;
        }

        DEBUG_MSG("RerollItemInPlace: rerolling '" + item->GetInstanceName() + "' from base '" + baseName + "' [power: " + Z(power) + "] ...");
        oCItem* baseItem = new oCItem();
        int result = Invalid;
        if (parser->CreateInstance(baseId, baseItem))
        {
            ItemExtension* fresh = RollInPlaceMagicItem(baseItem, power <= 0 ? 1 : power);
            result = FinalizeItemGeneration(fresh);
        }
        if (baseItem) baseItem->Release();
        return result;
    }

    void IdentifyItem(const oCItem* item)
    {
        if (!item) return;

        ItemExtension* ext = GetItemExtension(item);
        if (ext && ext->Tags[(int)ItemTags::Unidentified])
            ext->Tags[(int)ItemTags::Unidentified] = 0;        
    }

    void ItemExtensions_DefineExternals()
    {
    }

    //-------------------------------------------------------------------
    //						        Hooks
    //-------------------------------------------------------------------

    HOOK Hook_oCItem_InitByScript PATCH(&oCItem::InitByScript, &oCItem::InitByScript_StExt);
    void oCItem::InitByScript_StExt(int instance, int bInSaveGame)
    {
        // TEMP DIAG: crash przy PODNOSZENIU broni bossa. Trace konczyl sie czysto
        // na "<< ItemInfoPanel OK" (item=NULL = item znika z listy trupa), czyli
        // gra pada w samym PRZENIESIENIU - przed AddItemEffects. Bron bossa to
        // dynamiczna instancja (STEXT_GENERATED_*), a symptom "0 obrazen" sugeruje,
        // ze jest juz uszkodzona zanim ja dotkniesz. Tu silnik ja odtwarza.
        StExt_Trace(zSTRING(">> InitByScript inst=") + Z(instance) + " inSave=" + Z(bInSaveGame));
        THISCALL(Hook_oCItem_InitByScript)(instance, bInSaveGame);
        StExt_Trace(zSTRING("   .. oryginal OK, name='") + GetItemInstanceName(this) + "'");
        if ((instance != Invalid) && this)
        {
            const ItemExtension* extension = GetItemExtension(this);
            StExt_Trace(zSTRING("   .. ext=") + Z((int)extension));
            if (extension)
            {
                if (!ApplyItemExtension(this, extension)) {
                    StExt_Trace("   .. ApplyItemExtension ZAWIODL!");
                    DEBUG_MSG("oCItem::InitByScript_StExt: fail apply item extension for '" + this->GetInstanceName() + "'!");
                }
                StExt_Trace("   .. ApplyItemExtension OK");
            }

            // Global price update
            float price = static_cast<float>(this->value);
            if ((price > 0) && (ItemBasePriceMult >= 0.1f))
            {
                price *= ItemBasePriceMult;
                if (price <= 0.0f) price = 1.0f;
            }
            this->value = static_cast<int>(price);
        }
        StExt_Trace("<< InitByScript OK");
    }

    inline bool IsItemPriceValid(const int price) { return (price >= 0) && (price <= ItemExtension_MaxPrice); }

    HOOK Hook_oCItem_GetValue PATCH(&oCItem::GetValue, &oCItem::GetValue_StExt);
    int oCItem::GetValue_StExt()
    {
        // TEMP DIAG: podnoszenie itemu wstawia go do ekwipunku, a sortowanie
        // ekwipunku wola GetValue na swiezym itemie - kandydat na miejsce
        // crashu przy zbieraniu generowanej broni. Log tylko przy ZMIANIE
        // itemu (inaczej zalaloby plik przy kazdej klatce w handlu).
        static const oCItem* lastValTraced = Null;
        const bool trace = (this != lastValTraced);
        if (trace) { lastValTraced = this; StExt_Trace(zSTRING(">> GetValue inst='") + GetItemInstanceName(this) + "'"); }
        int result = THISCALL(Hook_oCItem_GetValue)();
        if (trace) StExt_Trace("   .. oryginal OK");

        const ItemExtension* extension = GetItemExtension(this);
        if (trace) StExt_Trace(zSTRING("<< GetValue ext=") + Z((int)extension));
        if (extension) 
            result = IsItemPriceValid(extension->Cost) ? extension->Cost : IsItemPriceValid(this->value) ? this->value : 10;
        else if (!IsItemPriceValid(result))
            result = IsItemPriceValid(this->value) ? this->value : 10;
        
        return ValidateValue(result, 10, ItemExtension_MaxPrice);
    }

    // GROUND PICKUP path (crash: podnoszenie broni bossa Z ZIEMI). Bron trzymana
    // przez bossa lada na ziemi po smierci; podnoszona z ziemi idzie przez
    // DoTakeVob -> PutInInv i NIE odpala InitByScript, wiec ApplyItemExtension
    // (z guardem na ItemClassData) nigdy sie nie wykonuje - inaczej niz loot z
    // okna trupa (kontener -> InitByScript). Dlatego zbroja z okna dziala, a bron
    // z ziemi crashuje. Ten hook loguje CALA sciezke z ziemi (dotad nieobserwowana)
    // i defensywnie re-aplikuje rozszerzenie po podniesieniu, zeby item na ziemi
    // dostal to samo co przez okno.
    HOOK Hook_oCNpc_DoTakeVob PATCH(&oCNpc::DoTakeVob, &oCNpc::DoTakeVob_StExt);
    int oCNpc::DoTakeVob_StExt(zCVob* vob)
    {
        oCItem* item = dynamic_cast<oCItem*>(vob);
        const bool trace = item && this && this->IsSelfPlayer();
        if (trace)
        {
            const ItemExtension* ext = GetItemExtension(item);
            StExt_Trace(zSTRING(">> DoTakeVob inst='") + GetItemInstanceName(item)
                + "' flags=" + Z(item->flags)
                + " ext=" + Z((int)ext)
                + (ext ? (zSTRING(" classData=") + Z((int)ext->ItemClassData)) : zSTRING("")));
        }

        const int result = THISCALL(Hook_oCNpc_DoTakeVob)(vob);
        if (trace) StExt_Trace("   .. oryginal PutInInv OK");

        // Item juz w plecaku gracza. Sciezka z ziemi NIE przeszla przez InitByScript,
        // wiec rozszerzenie moglo nie zostac domkniete. KLUCZOWE: wymuszamy
        // Initialize() na rozszerzeniu - to re-linkuje ItemClassData (ext->ItemClassData
        // = GetItemClassDescriptor(ItemClassID)). Wlasnie ten wskaznik bywal NULL i
        // zabijal kazdy pozniejszy odczyt broni bossa. Re-link naprawia ZRODLO dla
        // dowolnego deferred-czytacza, nie tylko dla ApplyItemExtension. Potem
        // domykamy staty tak jak sciezka z okna.
        if (item && this && this->IsSelfPlayer() && IsExtendedItem(item))
        {
            ItemExtension* extension = GetItemExtension(item);
            if (extension)
            {
                if (trace) StExt_Trace(zSTRING("   .. Initialize (re-link ItemClassData) classData_przed=") + Z((int)extension->ItemClassData));
                extension->Initialize();
                if (trace) StExt_Trace(zSTRING("   .. classData_po=") + Z((int)extension->ItemClassData));
                ApplyItemExtension(item, extension);
                if (trace) StExt_Trace("   .. re-ApplyItemExtension OK");
            }
        }
        if (trace) StExt_Trace("<< DoTakeVob OK");
        return result;
    }

    HOOK Hook_oCNpc_AddItemEffects PATCH(&oCNpc::AddItemEffects, &oCNpc::AddItemEffects_StExt);
    void oCNpc::AddItemEffects_StExt(oCItem* item)
    {
        // TEMP DIAG: crash zglaszany DOKLADNIE przy podniesieniu broni bossa,
        // a trace urywal sie tuz po doliczeniu statow itemu - czyli tutaj.
        // Kazdy krok osobno: ostatnia linia w stext_trace.log wskaze winnego.
        const bool trace = item && IsSelfPlayer();
        if (trace) StExt_Trace(zSTRING(">> AddItemEffects inst='") + GetItemInstanceName(item) + "'");

        THISCALL(Hook_oCNpc_AddItemEffects)(item);
        if (trace) StExt_Trace("   .. oryginal OK");

        if (!item || !IsSelfPlayer() || !IsExtendedItem(item))
        {
            if (trace) StExt_Trace("<< AddItemEffects: nie nasz item, koniec");
            return;
        }

        if (trace) StExt_Trace("   .. GetItemExtension");
        const ItemExtension* extension = GetItemExtension(item);
        if (trace) StExt_Trace(zSTRING("   .. AddItemExtensionEffects ext=") + Z((int)extension));
        if (extension && !AddItemExtensionEffects(item, extension)) {
            DEBUG_MSG("oCNpc::AddItemEffects_StExt: fail add item effects from '" + item->GetInstanceName() + "'!");
        }
        if (trace) StExt_Trace("<< AddItemEffects OK");
    }

    HOOK Hook_oCNpc_RemoveItemEffects PATCH(&oCNpc::RemoveItemEffects, &oCNpc::RemoveItemEffects_StExt);
    void oCNpc::RemoveItemEffects_StExt(oCItem* item)
    {
        // TEMP DIAG: crash przy ZBIERANIU zalozonej broni z trupa bossa - to
        // jest jedna z dwoch sciezek, ktore odpalaja sie przy zdejmowaniu.
        StExt_Trace(zSTRING(">> RemoveItemEffects inst='") + GetItemInstanceName(item) + "'");
        THISCALL(Hook_oCNpc_RemoveItemEffects)(item);
        StExt_Trace("<< RemoveItemEffects OK");
        if (!item || !IsSelfPlayer() || !IsExtendedItem(item)) return;

        const ItemExtension* extension = GetItemExtension(item);
        if (extension && !RemoveItemExtensionEffects(item, extension)) {
            DEBUG_MSG("oCNpc::AddItemEffects_StExt: fail remove item effects from '" + item->GetInstanceName() + "'!");
        }
    }

    HOOK Hook_oCNpc_CanUse PATCH(&oCNpc::CanUse, &oCNpc::CanUse_StExt);
    int oCNpc::CanUse_StExt(oCItem* item)
    {
        if (!item || !this) return False;

        const bool isPlayer = this->IsAPlayer();
        const bool hasSpecialCond = (item->cond_atr[0] > ItemCondSpecialSeparator) ||
            (item->cond_atr[1] > ItemCondSpecialSeparator) || (item->cond_atr[2] > ItemCondSpecialSeparator);

        if (IsExtendedItem(item) || (isPlayer && hasSpecialCond))
        {
            if (!isPlayer) return True;

            int skill = GetTalentSkill(Gothic_II_Addon::oCNpcTalent::oTEnumNpcTalent::NPC_TAL_MAGE);
            if (skill < item->mag_circle)
            {
                parser->SetInstance("SELF", this);
                parser->SetInstance("ITEM", item);
                int index = parser->GetIndex("G_CANNOTCAST");
                parser->CallFunc(index, IsAPlayer(), item->mag_circle, skill);
                return False;
            }

            int canUse = True;
            for (int i = 0; i < ItemExtension_Conditions_Max; ++i)
            {
                if (item->cond_atr[i] == 0 || item->cond_atr[i] == Invalid) continue;
                int funcResult = *(int*)parser->CallFunc(StExt_CheckConditionStatFunc, item->cond_atr[i], item->cond_value[i]);
                if (!funcResult) { canUse = False; break; }
            }
            return canUse;
        }

        if (!isPlayer) return True;
        return THISCALL(Hook_oCNpc_CanUse)(item);
    }
}