#include <StonedExtension.h>

namespace Gothic_II_Addon
{
    PlayerActionEventArgs PlayerActionEventData;
    Map<int, NpcExtension*> NpcExtensionData;
    uint NpcExtensionCount;
    int LastNpcUid;

    inline bool IsNpcPointerValid(oCNpc* npc)
    {
        if (!npc) return false;

        zCWorld* world = ogame->GetGameWorld();
        if (!world) return false;
        if (world->SearchVob((zCVob*)npc, Null) == Null) return false;

        oCNpc* realNpc = zDYNAMIC_CAST<oCNpc>((zCObject*)npc);
        if (!realNpc) return false;

        if (realNpc->GetInstanceName().IsEmpty()) return false;
        if (!realNpc->homeWorld) return false;

        return true;
    }

    inline bool IsUidRegistered(const int npcUid) { return NpcExtensionData.GetSafePair(npcUid) != Null; }
    inline bool IsUidValid(const int npcUid) { return (npcUid != 0 && npcUid != Invalid); }
    inline NpcExtension* CreateNpcExtension(const int npcUid, const zSTRING& instanceName, oCNpc* npc)
    {
        NpcExtension* npcExt = new NpcExtension();
        npcExt->NpcPtr = npc;
        npcExt->NpcUid = npcUid;
        npcExt->InstanceName = instanceName;
        memset(&npcExt->Stats, 0, sizeof(npcExt->Stats));
        return npcExt;
    }

    inline void AddNewNpcRecord(const int npcUid, const zSTRING& instanceName, oCNpc* npc)
    {
        NpcExtension* npcExt = CreateNpcExtension(npcUid, instanceName, npc);
        NpcExtensionData.Insert(npcUid, npcExt);
    }

    inline void ClearNpcExtensionData()
    {
        for (auto& pair : NpcExtensionData)
        {
            NpcExtension* npcExt = pair.GetValue();
            SAFE_DELETE(npcExt);
        }
        NpcExtensionData.Clear();
        NpcExtensionCount = NpcExtensionData.GetNum();
    }

    inline void ClearNpcExtensionPointers()
    {
        for (auto& pair : NpcExtensionData) 
        {
            NpcExtension* ext = pair.GetValue();
            if (ext) ext->NpcPtr = Null;
        }
    }

    inline int GetNpcUid(oCNpc* npc)
    {
        oCNpcEx* npcEx = dynamic_cast<oCNpcEx*>(npc);
        return npcEx ? npcEx->m_pVARS[StExt_AiVar_Uid] : Invalid;
    }

    inline int GetNextNpcUid()
    {
        int result = LastNpcUid + 1;
        while (IsUidRegistered(result))
        {
            ++result;
            if (result == 0 || result == Invalid) ++result;
        }
        LastNpcUid = result;
        return result;
    }

    void RegisterNpc(oCNpc* npc, int& npcUid)
    {
        if (!npc)
        {
            DEBUG_MSG("RegisterNpc - Try register Null or corrupted npc!");
            return;
        }

        oCNpcEx* npcEx = dynamic_cast<oCNpcEx*>(npc);
        if (!npcEx) { DEBUG_MSG("RegisterNpc - fail convert '" + npc->name[0] + "' to oCNpcEx!"); return; }

        if (npc->IsSelfPlayer() || npc == player)
        {
            npcUid = 0;
            npcEx->m_pVARS[StExt_AiVar_Uid] = 0;
            auto pair = NpcExtensionData.GetSafePair(0);
            if (!pair)
            {
                AddNewNpcRecord(npcUid, player->GetInstanceName(), player);
                NpcExtensionCount = NpcExtensionData.GetNum();
            }
            else
            {
                NpcExtension* ext = pair->GetValue();
                if (!ext) {
                    pair->GetValue() = CreateNpcExtension(npcUid, player->GetInstanceName(), player);
                    ext = pair->GetValue();
                }

                ext->NpcPtr = player;
                ext->NpcUid = npcUid;
                ext->InstanceName = player->GetInstanceName();
            }
            return;
        }

        const zSTRING incomingNpcInstanceName = npc->GetInstanceName();
        if (incomingNpcInstanceName.IsEmpty())
        {
            DEBUG_MSG("RegisterNpc - Try register npc without instance!? Instance name: '" + Z(incomingNpcInstanceName) + "' | Object name: '" +
                Z(npc->GetObjectName()) + "' | VobInfo: " + Z(npc->GetVobInfo()));            
            return;
        }

        const int incomingNpcUid = npcEx->m_pVARS[StExt_AiVar_Uid];
        if (IsUidValid(incomingNpcUid))
        {
            auto pair = NpcExtensionData.GetSafePair(incomingNpcUid);
            if (pair)
            {
                NpcExtension* ext = pair->GetValue();
                if (ext)
                {
                    if (ext->InstanceName == incomingNpcInstanceName)
                    {
                        ext->NpcPtr = npc;
                        npcUid = incomingNpcUid;
                        npcEx->m_pVARS[StExt_AiVar_Uid] = npcUid;                        
                        return;
                    }

                    // Collision
                    DEBUG_MSG("RegisterNpc - UId collision! '" + 
                        incomingNpcInstanceName + " [" + npcEx->name[0] + "]' tried to use UID " + Z(incomingNpcUid) + ", already owned by '" + ext->InstanceName + "'");
                }
            }
            else
            {
                npcUid = incomingNpcUid;
                npcEx->m_pVARS[StExt_AiVar_Uid] = npcUid;
                AddNewNpcRecord(npcUid, incomingNpcInstanceName, npc);
                NpcExtensionCount = NpcExtensionData.GetNum();
                return;
            }
        }

        const int newUid = GetNextNpcUid();
        npcUid = npc->IsSelfPlayer() ? 0 : newUid;
        npcEx->m_pVARS[StExt_AiVar_Uid] = newUid;
        AddNewNpcRecord(newUid, incomingNpcInstanceName, npc);
        NpcExtensionCount = NpcExtensionData.GetNum();
        DEBUG_MSG("RegisterNpc - Assigned NEW UId " + Z(newUid) + " to '" + Z(incomingNpcInstanceName) + " [" + npc->name[0] + "]'");
    }

    inline oCNpc* GetNpcByUid(const int npcUid)
    {
        auto pair = NpcExtensionData.GetSafePair(npcUid);
        if (!pair)
        {
            if (player && (npcUid == 0))
            {
                int tmpUid = 0;
                RegisterNpc(player, tmpUid);
                return player;
            }
            return Null;
        }

        NpcExtension* npcExt = pair->GetValue();
        if (!npcExt)
        {
            NpcExtensionData.Remove(npcUid);
            return Null;
        }

        if (!IsNpcPointerValid(npcExt->NpcPtr))
        {
            DEBUG_MSG("GetNpcByUid - Fail retrieve Npc with UId: " + Z(npcUid) + " - pointer is not valid!");
            return Null;
        }
        return npcExt->NpcPtr;
    }
        

    void RegisterWorldNpcs()
    {
        DEBUG_MSG("RegisterWorldNpcs - start search npcs...");
        oCWorld* world = ogame->GetGameWorld();
        if (!world)
        {
            DEBUG_MSG("RegisterWorldNpcs - world not initialized!");
            return;
        }

        int foundNpcs = 0;
        int npcUid = Invalid;
        auto it = world->voblist_npcs;
        while (it)
        {
            oCNpc* npc = it->GetData();
            if (npc)
            {
                int npcUid = Invalid;
                RegisterNpc(npc, npcUid);
                ++foundNpcs;
            }
            it = it->GetNextInList();
        }

        if (player)
        {
            int npcUid = 0;
            RegisterNpc(player, npcUid);
        }
        DEBUG_MSG("RegisterWorldNpcs - done! Found: " + Z(foundNpcs) + "/" + Z(static_cast<int>(NpcExtensionCount)));
    }


    //-----------------------------------------------------------------
    //						        API
    //-----------------------------------------------------------------

    inline bool SetNpcExtensionVar(const int uid, const int index, const int value)
    {
        auto pair = NpcExtensionData.GetSafePair(uid);
        if (!pair) return false;
        NpcExtension* npcExt = pair->GetValue();
        if (npcExt && IsIndexInBounds(index, NpcExtension_StatsMax))
        {
            npcExt->Stats[index] = value;
            return true;
        }
        return false;
    }

    inline bool GetNpcExtensionVar(const int uid, const int index, int& value)
    {
        auto pair = NpcExtensionData.GetSafePair(uid);
        if (!pair) return false;
        NpcExtension* npcExt = pair->GetValue();
        if (npcExt && IsIndexInBounds(index, NpcExtension_StatsMax))
        {
            value = npcExt->Stats[index];
            return true;
        }
        return false;
    }

    inline bool GetNpcExtensionArr(const int uid, int*& arr)
    {
        auto pair = NpcExtensionData.GetSafePair(uid);
        if (!pair) return false;
        NpcExtension* npcExt = pair->GetValue();
        if (npcExt) {
            arr = npcExt->Stats;
            return true;
        }
        return false;
    }

    //-----------------------------------------------------------------
    //						    SERIALIZATION
    //-----------------------------------------------------------------

    string GetNpcExtensionsArchivePath()
    {
        int slotID = SaveLoadGameInfo.slotID;
        string savesDir = zoptions->GetDirString(zTOptionPaths::DIR_SAVEGAMES);
        string slotDir = GetSaveSlotNameByID(slotID);
        string archivePath = string::Combine("%s\\%s\\StExt_NpcData.sav", savesDir, slotDir);
        return archivePath;
    }

    void SaveNpcExtensions()
    {
        DEBUG_MSG("SaveNpcExtensions: save npcs...");

        string archiveName = GetNpcExtensionsArchivePath();
        zCArchiver* ar = zarcFactory->CreateArchiverWrite(archiveName, zARC_MODE_BINARY_SAFE, 0, 0);
        if (!ar)
        {
            DEBUG_MSG("SaveNpcExtensions: fail to create writer for: " + archiveName);
            Message::Error((string)"SaveNpcExtensions - archiver is Null!", "Critical error!");
            gameMan->ExitGame();
            return;
        }

        uint savedNpcExtension = 0;
        NpcExtensionCount = NpcExtensionData.GetNum();
        ar->WriteRaw("NpcExtensionCount", &NpcExtensionCount, sizeof(NpcExtensionCount));
        ar->WriteInt("LastNpcUid", LastNpcUid);
        for (auto& pair : NpcExtensionData)
        {
            NpcExtension* npcExt = pair.GetValue();
            if (npcExt)
            {
                ar->WriteInt("NpcUid", npcExt->NpcUid);
                ar->WriteString("InstanceName", npcExt->InstanceName);
                ar->WriteRaw("NpcStats", &npcExt->Stats, sizeof(npcExt->Stats));
                ++savedNpcExtension;
            }
        }

        ar->Close();
        ar->Release();
        NpcExtensionCount = NpcExtensionData.GetNum();
        DEBUG_MSG("SaveNpcExtensions: saved " + Z(static_cast<int>(savedNpcExtension)) + "/" + Z(static_cast<int>(NpcExtensionCount)) + " npcs.");
    }

    void LoadNpcExtensions()
    {
        DEBUG_MSG("LoadNpcExtensions: load npcs...");

        const zSTRING archiveName = GetNpcExtensionsArchivePath();
        zCArchiver* ar = zarcFactory->CreateArchiverRead(archiveName, 0);
        if (!ar)
        {
            DEBUG_MSG("LoadNpcExtensions: fail to create reader for: " + archiveName);
            Message::Error((string)"LoadNpcExtensions - archiver is Null!", "Critical error!");
            gameMan->ExitGame();
            return;
        }

        ClearNpcExtensionData();
        uint loadedNpcExtension = 0;
        ar->ReadRaw("NpcExtensionCount", &NpcExtensionCount, sizeof(NpcExtensionCount));
        ar->ReadInt("LastNpcUid", LastNpcUid);
        const uint extensionToLoad = NpcExtensionCount;
        for (uint i = 0; i < extensionToLoad; ++i)
        {
            NpcExtension* npcExt = new NpcExtension();
            ar->ReadInt("NpcUid", npcExt->NpcUid);
            ar->ReadString("InstanceName", npcExt->InstanceName);
            ar->ReadRaw("NpcStats", &npcExt->Stats, sizeof(npcExt->Stats));
            npcExt->NpcPtr = Null;

            if (npcExt->InstanceName.IsEmpty())
            {
                DEBUG_MSG("LoadNpcExtensions: fail to load npc without instance! UId: " + Z(npcExt->NpcUid));
                continue;
            }
            NpcExtensionData.Insert(npcExt->NpcUid, npcExt);
            ++loadedNpcExtension;
        }

        ar->Close();
        ar->Release();        
        DEBUG_MSG("LoadNpcExtensions: loaded " + Z(static_cast<int>(loadedNpcExtension)) + "/" + Z(static_cast<int>(extensionToLoad)) + " npcs.");

        NpcExtensionCount = NpcExtensionData.GetNum();
        RegisterWorldNpcs();
    }

    
    //-----------------------------------------------------------------
    //							   HOOKS
    //-----------------------------------------------------------------

    HOOK Hook_oCNpc_InitByScript PATCH(&oCNpc::InitByScript, &oCNpc::InitByScript_StExt);
    void oCNpc::InitByScript_StExt(int instance, int inSaveGame)
    {
        THISCALL(Hook_oCNpc_InitByScript)(instance, inSaveGame);
        if (IsLoading || IsLevelChanging) return;
        //if (!IsNpcPointerValid(this)) return;

        oCNpcEx* npcEx = dynamic_cast<oCNpcEx*>(this);
        if (!npcEx)
        {
            DEBUG_MSG("InitByScript_StExt - fail convert '" + this->name[0] + "' to oCNpcEx!");
            return;
        }

        int uid = npcEx->m_pVARS[StExt_AiVar_Uid];
        if (this->IsAPlayer()) int uid = 0;
        RegisterNpc(this, uid);      
    }

    /*
    HOOK Hook_oCNpc_Archive PATCH(&oCNpc::Archive, &oCNpc::Archive_StExt);
    void oCNpc::Archive_StExt(zCArchiver& ar)
    {
        THISCALL(Hook_oCNpc_Archive)(ar);
        if (!IsLevelChanging) RegisterNpc(this);
    }
    */

    HOOK Hook_oCNpc_Unarchive PATCH(&oCNpc::Unarchive, &oCNpc::Unarchive_StExt);
    void oCNpc::Unarchive_StExt(zCArchiver& ar)
    {
        THISCALL(Hook_oCNpc_Unarchive)(ar);
        UnArchiveAdditionalArmors(this);
    }

    // Zapadka HP celow krucjaty (definicja: Damage.cpp). -1 = brak wpisu.
    int StExt_WarRatchetGetFloor(void* npc);
    // Jednorazowa zgoda na egzekucje celu wojny (podloga 0, DoDie jeszcze nie byl).
    bool StExt_WarRatchetExecuteDue(void* npc);

    HOOK Hook_oCNpc_ProcessNpc PATCH(&oCNpc::ProcessNpc, &oCNpc::ProcessNpc_StExt);
    void oCNpc::ProcessNpc_StExt()
    {
        THISCALL(Hook_oCNpc_ProcessNpc)();

        // KRUCJATA BELIARA: framework odnawia HP celow wojny CO KLATKE
        // (log DH-WARHIT: lost=-2184 - w trakcie lancucha oddal dokladnie
        // tyle, ile cios przed chwila zabral; pasek HP zawsze pelny).
        // Zapadka z Damage.cpp musi wiec domykac sie tez per klatka, nie
        // tylko przy ciosie - inaczej postep wojny jest niewidoczny.
        // W tablicy zapadki sa WYLACZNIE cele wojny, wiec trafiony wpis
        // jest jedyna potrzebna bramka; skan 10 wskaznikow = zero narzutu.
        if (this)
        {
            const int warFloor = StExt_WarRatchetGetFloor(this);
            if ((warFloor >= 0) && (this->attribute[NPC_ATR_HITPOINTS] > warFloor))
                this->attribute[NPC_ATR_HITPOINTS] = warFloor;

            // EGZEKUCJA poza kontekstem obrazen (jak konsolowy kill - jedyna
            // sciezka smierci potwierdzona u chronionych NPC frameworka).
            // Wewnatrz OnDamage framework wycisza kolejne eventy po pierwszym
            // "nielegalnym" spadku HP, wiec smierc wykonana tam nie domyka sie.
            if ((warFloor == 0) && StExt_WarRatchetExecuteDue(this))
            {
                this->attribute[NPC_ATR_HITPOINTS] = 0;
                StExt_Trace(zSTRING("DH-DODIE egzekucja celu wojny (ProcessNpc)"));
                this->DoDie(oCNpc::player);
            }
        }

        if (this && !this->IsDead())
        {
            // (parry detection moved to Damage.cpp OnDamage_StExt - the AI
            // clears didParade inside ProcessNpc, so this spot never saw it)
            parser->SetInstance(StExt_ModSelf_SymId, this);
            parser->CallFunc(StExt_OnAiStateFunc);
            parser->SetInstance(StExt_ModSelf_SymId, Null);
        }
    }

    HOOK Hook_oCNpc_OpenInventory PATCH(&oCNpc::OpenInventory, &oCNpc::OpenInventory_StExt);
    void oCNpc::OpenInventory_StExt(int mode)
    {
        if (this && this->HasBodyStateModifier(BS_MOD_BURNING)) {
            this->ModifyBodyState(0, BS_MOD_BURNING);
        }
        THISCALL(Hook_oCNpc_OpenInventory)(mode);
    }
    
    HOOK Hook_oCNpc_OnMessage PATCH(&oCNpc::OnMessage, &oCNpc::OnMessage_StExt);
    void oCNpc::OnMessage_StExt(zCEventMessage* msg, zCVob* vob)
    {
        try { THISCALL(Hook_oCNpc_OnMessage)(msg, vob); }
        catch (const std::exception& e) { DEBUG_MSG("OnMessage_StExt - EXCEPTION: " + Z(e.what()) + "!"); }
        catch (...) { DEBUG_MSG("OnMessage_StExt - UNKNOWN EXCEPTION!"); }
    }

    // Player actions events system

    inline void ProcessPlayerEvent(oCNpc* npc, const int eventType, const int eventResult)
    {
        static int OnPlayerEventFuncIndex = parser->GetIndex("STEXT_ONPLAYERACTION");
        static int OnPlayerEventArgsIndex = parser->GetIndex("STEXT_ONPLAYERACTION_EVENTARGS");
        static int EventArgsTargetIndex = parser->GetIndex("STEXT_EVENTARGS_TARGET");

        if (!npc || npc != player || !npc->IsSelfPlayer()) return;
        memset(&PlayerActionEventData, 0, sizeof(PlayerActionEventData));

        PlayerActionEventData.Type = eventType;
        PlayerActionEventData.Flags = StExt_PcActionFlag_None;
        PlayerActionEventData.Result = eventResult;
        PlayerActionEventData.IsHandled = False;

        PlayerActionEventData.WeaponMode = npc->GetWeaponMode();
        PlayerActionEventData.BodyState = npc->GetBodyState();
        PlayerActionEventData.CanDoAni = True;
        PlayerActionEvent_CollectPcInputData(PlayerActionEventData);

        if ((eventType == StExt_PcActionType_OnInput) && 
            !PlayerActionEventData.MouseLButtonClicked && !PlayerActionEventData.MouseRButtonClicked) return;

        parser->SetInstance(EventArgsTargetIndex, npc->GetFocusNpc());
        parser->SetInstance(OnPlayerEventArgsIndex, &PlayerActionEventData);
        parser->CallFunc(OnPlayerEventFuncIndex);
        parser->SetInstance(OnPlayerEventArgsIndex, Null);
        parser->SetInstance(EventArgsTargetIndex, Null);
    }


    HOOK Hook_oCNpc_EV_AttackForward PATCH(&oCNpc::EV_AttackForward, &oCNpc::EV_AttackForward_StExt);
    int oCNpc::EV_AttackForward_StExt(oCMsgAttack* msg)
    {
        if (!this) return True;

        const int result = THISCALL(Hook_oCNpc_EV_AttackForward)(msg);
        if (this->IsSelfPlayer() && result) {
            ProcessPlayerEvent(this, StExt_PcActionType_OnAttackForward, result);
        }
        return result;
    }

    HOOK Hook_oCNpc_EV_AttackLeft PATCH(&oCNpc::EV_AttackLeft, &oCNpc::EV_AttackLeft_StExt);
    int oCNpc::EV_AttackLeft_StExt(oCMsgAttack* msg)
    {
        if (!this) return True;

        const int result = THISCALL(Hook_oCNpc_EV_AttackLeft)(msg);
        if (this->IsSelfPlayer() && result) {
            ProcessPlayerEvent(this, StExt_PcActionType_OnAttackLeft, result);
        }
        return result;
    }

    HOOK Hook_oCNpc_EV_AttackRight PATCH(&oCNpc::EV_AttackRight, &oCNpc::EV_AttackRight_StExt);
    int oCNpc::EV_AttackRight_StExt(oCMsgAttack* msg)
    {
        if (!this) return True;

        const int result = THISCALL(Hook_oCNpc_EV_AttackRight)(msg);
        if (this->IsSelfPlayer() && result) {
            ProcessPlayerEvent(this, StExt_PcActionType_OnAttackRight, result);
        }
        return result;
    }

    HOOK Hook_oCNpc_EV_AttackRun PATCH(&oCNpc::EV_AttackRun, &oCNpc::EV_AttackRun_StExt);
    int oCNpc::EV_AttackRun_StExt(oCMsgAttack* msg)
    {
        if (!this) return True;

        const int result = THISCALL(Hook_oCNpc_EV_AttackRun)(msg);
        if (this->IsSelfPlayer() && result) {
            ProcessPlayerEvent(this, StExt_PcActionType_OnAttackRun, result);
        }
        return result;
    }

    HOOK Hook_oCNpc_EV_AttackFinish PATCH(&oCNpc::EV_AttackFinish, &oCNpc::EV_AttackFinish_StExt);
    int oCNpc::EV_AttackFinish_StExt(oCMsgAttack* msg)
    {
        if (!this) return True;

        const int result = THISCALL(Hook_oCNpc_EV_AttackFinish)(msg);
        if (this->IsSelfPlayer() && result) {
            ProcessPlayerEvent(this, StExt_PcActionType_OnAttackFinish, result);
        }
        return result;
    }

    HOOK Hook_oCNpc_EV_Parade PATCH(&oCNpc::EV_Parade, &oCNpc::EV_Parade_StExt);
    int oCNpc::EV_Parade_StExt(oCMsgAttack* msg)
    {
        if (!this) return True;

        const int result = THISCALL(Hook_oCNpc_EV_Parade)(msg);
        if (this->IsSelfPlayer() && result) {
            ProcessPlayerEvent(this, StExt_PcActionType_OnParade, result);
        }
        return result;
    }

    HOOK Hook_oCNpc_EV_ShootAt PATCH(&oCNpc::EV_ShootAt, &oCNpc::EV_ShootAt_StExt);
    int oCNpc::EV_ShootAt_StExt(oCMsgAttack* msg)
    {
        if (!this) return True;

        const int result = THISCALL(Hook_oCNpc_EV_ShootAt)(msg);
        if (this->IsSelfPlayer() && result) {
            ProcessPlayerEvent(this, StExt_PcActionType_OnShootAt, result);
        }
        return result;
    }

    HOOK Hook_oCNpc_EV_Defend PATCH(&oCNpc::EV_Defend, &oCNpc::EV_Defend_StExt);
    int oCNpc::EV_Defend_StExt(oCMsgAttack* msg)
    {
        if (!this) return True;

        const int result = THISCALL(Hook_oCNpc_EV_Defend)(msg);
        if (this->IsSelfPlayer() && result) {
            ProcessPlayerEvent(this, StExt_PcActionType_OnDefend, result);
        }
        return result;
    }
/*

// WEAPON MODE
#define	zWEAPON_NONE				0
#define	zWEAPON_FIST				1
#define	zWEAPON_KNIFE				2
#define	zWEAPON_1H					3
#define	zWEAPON_2H					4
#define	zWEAPON_BOW					5
#define	zWEAPON_CROSSBOW			6
#define	zWEAPON_MAGIC				7

//WEAPON TYPE
#define zWEAPON_TYPE_FIST			1
#define zWEAPON_TYPE_NF				2
#define zWEAPON_TYPE_FF				4
#define zWEAPON_TYPE_MAGIC			8

// BodyStates
#define zBS_STAND					0
#define zBS_WALK					1
#define zBS_SNEAK					2
#define zBS_RUN						3
#define zBS_SPRINT					4
#define zBS_SWIM					5
#define zBS_CRAWL					6
#define zBS_DIVE					7
#define zBS_JUMP					8
#define zBS_CLIMB					9
#define zBS_FALL					10
#define zBS_SIT						11
#define zBS_LIE						12
#define zBS_INVENTORY				13
#define zBS_ITEMINTERACT			14
#define zBS_MOBINTERACT				15
#define zBS_MOBINTERACT_INTERRUPT	16
#define zBS_TAKEITEM				17
#define zBS_DROPITEM				18
#define zBS_THROWITEM				19
#define zBS_PICKPOCKET				20
#define zBS_STUMBLE					21
#define zBS_UNCONSCIOUS				22
#define zBS_DEAD					23
#define zBS_AIMNEAR					24
#define zBS_AIMFAR					25
#define zBS_HIT						26
#define zBS_PARADE					27
#define zBS_CASTING					28
#define zBS_PETRIFIED				29
#define zBS_CONTROLLING				30
#define zBS_MAX						31
*/
}