#include <StonedExtension.h>
#include <cstdio>

namespace Gothic_II_Addon
{
	// TEMP DIAG (remove when the numbers are trusted): one line per hero hit into
	// stext_damage.log - raw -> weapon-class bonus -> what we asked the engine for.
	// 'applied' is filled in after the original ran, i.e. the number AFTER armour.
	static int   StExtDiag_LastWanted = 0;
	static int   StExtDiag_LastBefore = 0;
	static bool  StExtDiag_Pending = false;

	static void StExt_DamageDiagLog(int weaponFlags, int rawBefore, int flat, int perc, int wanted, int dmgType)
	{
		if (!StExt_TraceOn()) return;
		const char* kind = "other";
		if (weaponFlags & ((1 << 14) | (1 << 16)))      kind = "SWORD";
		else if (weaponFlags & ((1 << 15) | (1 << 17))) kind = "AXE";
		else if (weaponFlags & (1 << 13))               kind = "DAGGER";

		FILE* f = fopen("stext_damage.log", "a");
		if (f)
		{
			fprintf(f, "HIT %-6s | raw=%-5d flat=+%-4d perc=+%d.%d%% | wanted=%-5d | dmgType=%d\n",
				kind, rawBefore, flat, perc / 10, perc % 10, wanted, dmgType);
			fclose(f);
		}
		StExtDiag_LastWanted = wanted;
		StExtDiag_LastBefore = rawBefore;
		StExtDiag_Pending = true;
	}

	// Called after the original OnDamage: how much HP the target actually lost.
	static void StExt_DamageDiagApplied(int hpLost)
	{
		if (!StExtDiag_Pending) return;
		StExtDiag_Pending = false;
		FILE* f = fopen("stext_damage.log", "a");
		if (f)
		{
			fprintf(f, "    -> applied(after armour)=%d  (raw was %d, we asked for %d)\n",
				hpLost, StExtDiag_LastBefore, StExtDiag_LastWanted);
			fclose(f);
		}
	}

	// TEMP DIAG: every hit that lands on the HERO -> stext_combat.log.
	// Written for the "boss cannot finish me off, I sit at 1 HP" report. The
	// interesting column is ATK: damage arriving with attacker=NONE goes down
	// the DontKill fall-through, which treats every peaceful human - the hero
	// included - as protected and clamps the blow to leave exactly 1 HP.
	static void StExt_CombatDiagLog(const zSTRING& line)
	{
		if (!StExt_TraceOn()) return;
		FILE* f = fopen("stext_combat.log", "a");
		if (f) { fputs(line.ToChar(), f); fputc('\n', f); fclose(f); }
	}

	// Base-game oCItem weapon-type flags (item->flags bits).
	const int ITEM_FLAG_DAG     = 1 << 13;   // 8192   dagger / light blade
	const int ITEM_FLAG_SWD     = 1 << 14;   // 16384  1h sword
	const int ITEM_FLAG_AXE     = 1 << 15;   // 32768  1h axe
	const int ITEM_FLAG_2HD_SWD = 1 << 16;   // 65536  2h sword
	const int ITEM_FLAG_2HD_AXE = 1 << 17;   // 131072 2h axe

	const int SPELLEFFECT_STORE_TIME = 3600;

	struct DamageMeta
	{
		oCNpc* Target;
		oCNpc* Attacker;
		oCItem* Weapon;
		oCNpc::oSDamageDescriptor* Desc;

		bool IsExtraDamage;
		bool IsDotDamage;
		bool IsAoeDamage;
		bool IsReflectDamage;
		bool IsAbility;
		bool IsOverlayDamage;
		bool IsInitialDamage;
		bool TargetIsImmortal;
		bool SpellFxInDict;

		// Pierce amounts for THIS packet, carried from ExtraDamageInfo via
		// DamageExtraParams (side-band DoExtraDamage -> PushDamageMeta) and
		// consumed post-protection in ProcessExtraDamage.
		int PierceDamage[8];

		DamageInfo DamageInfo;
	};

	struct DamageInfoMeta
	{
		bool IsPending;

		oCNpc* Target;
		oCNpc* Attacker;
		oCItem* Weapon;

		DamageInfo DamageInfo;
	};

	struct ExtraDamageParams
	{
		bool HasPendingExtraParams;
		int DamageType;
		int DamageFlags;
		int PierceDamage[8];
	};

	struct PendingDamage
	{
		oCNpc* Target;
		oCNpc* Attacker;

		bool IsMock;
		uint Flags;
		int FrameTime;
		int DamageSelectorFunc;		
		ExtraDamageInfo DamageInfo;
	};

	ExtraDamageInfo ExtraDamage;
	ExtraDamageInfo DotDamage;
	ExtraDamageInfo ReflectDamage;

	ExtraDamageParams DamageExtraParams;
	DamageInfoMeta DamageInfoMetaData;
	Array<DamageMeta> DamageMetaData;
	Array<PendingDamage> PendingDamageData;
	Map<oCVisualFX*, PendingDamage> PendingEffectDamageData;

	int MaxSpellId;
	int StExt_AbilityPrefix;
	StringMap<int> SpellFxNames;

	
#if DebugEnabled 
	#define DEBUG_MSG_DAMDESC(desc, msg, target) PrintDamageDescriptorDebug(desc, msg, target)
	#define DEBUG_MSG_DAMMETA(meta, msg) PrintDamageMetaDebug(meta, msg)
	#define DEBUG_MSG_DAMINFO(info, msg) PrintDamageInfoDebug(info, msg)
	#define DEBUG_MSG_DAMINCOMINFO(info, msg) PrintIncomingDamageInfoDebug(info, msg)
#else
	#define DEBUG_MSG_DAMDESC(desc, msg, target) ((void)0)
	#define DEBUG_MSG_DAMMETA(desc, msg) ((void)0)
	#define DEBUG_MSG_DAMINFO(desc, msg) ((void)0)
	#define DEBUG_MSG_DAMINCOMINFO(info, msg) ((void)0)
#endif

	//-----------------------------------------------------------------
	//						DEBUG FUNCTIONS
	//-----------------------------------------------------------------

	inline zSTRING SafeNpcName(oCNpc* npc) { return npc ? (npc->IsSelfPlayer() ? "Hero" : npc->name[0]) : "<Null>"; }
	inline zSTRING SafeItemName(oCItem* item) { return (!item) ? "<Null>" : item->name; }
	inline zSTRING GetNpcHpEs(oCNpc* npc)
	{
		if (!npc) return "HP: ???/??? | ES: ???/???";

		oCNpcEx* npcEx = dynamic_cast<oCNpcEx*>(npc);
		if (!npcEx) return "HP: " + Z(npc->attribute[0]) + "/" + Z(npc->attribute[1]) + " | ES: ???/???";

		int esCur; GetNpcExtensionVar(npcEx->m_pVARS[StExt_AiVar_Uid], StExt_AiVar_EsCur, esCur);
		int esMax; GetNpcExtensionVar(npcEx->m_pVARS[StExt_AiVar_Uid], StExt_AiVar_EsMax, esMax);
		return "HP: " + Z(npc->attribute[0]) + "/" + Z(npc->attribute[1]) + " | ES: " + Z(esCur) + "/" + Z(esMax);
	}

	inline void PrintDamageDescriptorDebug(oCNpc::oSDamageDescriptor& desc, zSTRING msg, oCNpc* target)
	{
		DEBUG_MSG("-------------------------------------------------------------");
		DEBUG_MSG_DAM(" <<< DAMAGEDESCRIPTOR >>> ", msg, desc.pNpcAttacker, target);
		DEBUG_MSG("");

		DEBUG_MSG("DescriptorFlags: " + Z(desc.dwFieldsValid) + " | DamageFlags: " + Z(desc.enuModeDamage) + " | WeaponFlags: " + Z(desc.enuModeWeapon));
		DEBUG_MSG("DamageReal: " + Z((int)desc.fDamageTotal) + " | DamageTotal: " + Z((int)desc.fDamageReal) + " | DamageEffective: " + Z((int)desc.fDamageEffective));

		bool bFinished = desc.bFinished ? true : false;
		bool bIsDead = desc.bIsDead ? true : false;
		bool bIsUnconscious = desc.bIsUnconscious ? true : false;
		DEBUG_MSG("IsFinished: " + Z((int)bFinished) + " | IsDead: " + Z((int)bIsDead) + " | IsUnconscious: " + Z((int)bIsUnconscious));

		zSTRING damage = "Damage[] = ";
		zSTRING effectiveDamage = Z"EffectiveDamage[] = ";
		for (int i = 0; i < oEDamageIndex_MAX; ++i)
		{
			damage += Z desc.aryDamage[i] + Z" | ";
			effectiveDamage += Z desc.aryDamageEffective[i] + Z" | ";
		}
		DEBUG_MSG(damage);
		DEBUG_MSG(effectiveDamage);
		if (desc.pFXHit)
			DEBUG_MSG(Z"FxName: " + desc.pFXHit->fxName + " | SpellId: " + Z((int)desc.nSpellID) + " | SpellLevel: " + Z((int)desc.nSpellLevel) + " | SpellCat: " + Z((int)desc.nSpellCat));
		DEBUG_MSG("-------------------------------------------------------------");
	}
	inline void PrintDamageInfoDebug(const DamageInfo& info, zSTRING title)
	{
		DEBUG_MSG("-------------------------------------------------------------");
		DEBUG_MSG(" <<< DAMAGEINFO: " + title + " >>> ");
		DEBUG_MSG("");

		zSTRING dmgArray = Z"Damage[]: ";
		for (int i = 0; i < 8; ++i)
			dmgArray += Z(info.Damage[i]) + " | ";
		DEBUG_MSG(dmgArray);
		DEBUG_MSG("TotalDamage: " + Z(info.TotalDamage) + " | RealDamage: " + Z(info.RealDamage) + " | SpellId: " + Z(info.SpellId));
		DEBUG_MSG("DamageEnum: " + Z(info.DamageEnum) +	" | WeaponEnum: " + Z(info.WeaponEnum) + " | DamageType: " + Z(info.DamageType) + " | DamageFlags: " + Z(info.DamageFlags));
		DEBUG_MSG("BlockDamage: " + Z(info.BlockDamage) + " | StopProcess: " + Z(info.StopProcess) + " | IsInitial: " + Z(info.IsInitial));
		DEBUG_MSG("-------------------------------------------------------------");
	}
	inline void PrintDamageMetaDebug(const DamageMeta& meta, zSTRING title)
	{
		DEBUG_MSG("=============================================================");
		DEBUG_MSG(" <<< DAMAGEMETA: " + title + " >>> ");
		DEBUG_MSG("");

		DEBUG_MSG("Target: " + SafeNpcName(meta.Target) + " [" + Z((int)meta.Target) + "] | " + 
			"Attacker: " + SafeNpcName(meta.Attacker) + " [" + Z((int)meta.Attacker) + "] | " + 
			"Weapon: " + SafeItemName(meta.Weapon) + " [" + Z((int)meta.Weapon) + "]");
		DEBUG_MSG("IsExtraDamage: " + Z(meta.IsExtraDamage) + " | IsDotDamage: " + Z(meta.IsDotDamage) + " | IsAoeDamage: " + Z(meta.IsAoeDamage) + " | IsReflectDamage: " + Z(meta.IsReflectDamage));
		DEBUG_MSG("IsAbility: " + Z(meta.IsAbility) + " | IsOverlayDamage: " + Z(meta.IsOverlayDamage) + " | IsInitialDamage: " + Z(meta.IsInitialDamage) + " | TargetIsImmortal: " + Z(meta.TargetIsImmortal));
		DEBUG_MSG("=============================================================");
	}
	inline void PrintIncomingDamageInfoDebug(const IncomingDamageInfo& info, zSTRING title)
	{
		DEBUG_MSG("-------------------------------------------------------------");
		DEBUG_MSG(" <<< INCOMINGDAMAGEINFO: " + title + " >>> ");
		DEBUG_MSG("");

		zSTRING dmgArray = Z"Damage[]: ";
		for (int i = 0; i < 8; ++i)
			dmgArray += Z(info.Damage[i]) + " | ";
		DEBUG_MSG(dmgArray);
		DEBUG_MSG("DamageTotal: " + Z(info.DamageTotal) + " | SpellId: " + Z(info.SpellId));
		DEBUG_MSG("Flags: " + Z(info.Flags) + " | DamageType: " + Z(info.DamageType) + " | DamageFlags: " + Z(info.DamageFlags));
		DEBUG_MSG("-------------------------------------------------------------");
	}

	//-----------------------------------------------------------------
	//						UTIL FUNCTIONS
	//-----------------------------------------------------------------

	inline int GetCurNpcEs(oCNpc* npc)
	{
		if (!npc) return 0;
		oCNpcEx* npcEx = dynamic_cast<oCNpcEx*>(npc);
		if (!npcEx) return 0;

		int esCur; GetNpcExtensionVar(npcEx->m_pVARS[StExt_AiVar_Uid], StExt_AiVar_EsCur, esCur);
		return esCur;
	}

	inline bool IsDamageDescriptorSane(oCNpc::oSDamageDescriptor& desc)
	{
		const int MAX_REASONABLE_DAMAGE = 100000;
		const int MAX_REASONABLE_TOTAL = 500000;

		if (desc.fDamageTotal < 0 || desc.fDamageTotal > MAX_REASONABLE_TOTAL) return false;
		if (desc.fDamageReal < 0 || desc.fDamageReal > MAX_REASONABLE_TOTAL) return false;
		if (desc.fDamageEffective < 0 || desc.fDamageEffective > MAX_REASONABLE_TOTAL) return false;

		ulong damageSum = 0;
		for (int i = 0; i < oEDamageIndex_MAX; ++i)
		{
			ulong dam = desc.aryDamage[i];
			if (dam > MAX_REASONABLE_DAMAGE)
				return false;
			damageSum += dam;
		}
		if (damageSum > MAX_REASONABLE_TOTAL) return false;
		if (desc.pNpcAttacker && (uintptr_t)desc.pNpcAttacker < 0x10000) return false;
		return true;
	}

	template<typename D>
	inline void ApplyDamagesGeneric(const ulong type, D* damage, int& total)
	{
		if (total <= 0)
		{
			DEBUG_MSG("ApplyDamagesGeneric - total damage is less then 0!");
			return;
		}

		float damTypesCount = 0.0f;
		for (ulong i = 0; i < oEDamageIndex_MAX; ++i) {
			if (type & (1 << i))
				++damTypesCount; 
		}
		if ((type == 0UL) || (damTypesCount <= 0.5f)) { damage[dam_index_barrier] = static_cast<D>(total); return; }
		
		float dam = (float)total / (damTypesCount + 0.5f);
		if (dam < 1.0f) dam = 1.0f;
		for (ulong i = 0; i < oEDamageIndex_MAX; ++i) 
			damage[i] = type & (1 << i) ? static_cast<D>(dam) : D{};
	}
	void ApplyDamages(ulong type, ulong* damage, int& total) { ApplyDamagesGeneric(type, damage, total); }
	void ApplyDamages(int type, int* damage, int& total) { ApplyDamagesGeneric(static_cast<ulong>(type), damage, total); }

	inline int GetFxTrueId(oCNpc::oSDamageDescriptor* desc, bool& isAbility)
	{
		if (!desc || !desc->pFXHit)
		{
			isAbility = false;
			return Invalid;
		}

		int spellId = static_cast<int>(desc->nSpellID);
		if (spellId > 0)
		{
			isAbility = false;
			return spellId;
		}

		zSTRING fxNameKey = desc->pFXHit->fxName;
		if (SpellFxNames.IsEmpty() || fxNameKey.IsEmpty())
		{
			DEBUG_MSG_IF(SpellFxNames.IsEmpty(), "GetFxSpellId - SpellFxNames is null!");
			DEBUG_MSG_IF(fxNameKey.IsEmpty(), "GetFxSpellId - fxName is empty!");
			isAbility = false;
			return Invalid;
		}

		fxNameKey.Upper().Replace("SPELLFX_", "");
		auto pair = SpellFxNames.Find(fxNameKey);
		if (pair) 
		{
			spellId = *pair;
			isAbility = true;
			return spellId + StExt_AbilityPrefix;
		}

		pair = SpellFxNames.FindApprox(fxNameKey);
		if (pair)
		{
			spellId = *pair;
			isAbility = true;
			return spellId + StExt_AbilityPrefix;
		}
		isAbility = true;
		return Invalid;
	}

	inline void SetScriptDamageActors(oCNpc* atk, oCNpc* target, oCItem* weap)
	{
		parser->SetInstance(StExt_TargetNpc_SymId, target);
		parser->SetInstance(StExt_AttackNpc_SymId, atk);
		parser->SetInstance(StExt_AttackWeapon_SymId, weap);
	}

	inline void SetScriptActors(oCNpc* atk, oCNpc* target)
	{
		parser->SetInstance(StExt_Self_SymId, target);
		parser->SetInstance(StExt_Other_SymId, atk);
	}

	inline void BuildDescriptor(oCNpc::oSDamageDescriptor& desc)
	{
		desc.bOnce = True;
		desc.bFinished = False;
		desc.bIsDead = False;
		desc.bIsUnconscious = False;
		desc.bDamageDontKill = False;

		for (int i = 0; i < oEDamageIndex_MAX; ++i) {
			desc.aryDamage[i] = 0UL;
			desc.aryDamageEffective[i] = 0UL;
		}

		desc.nSpellID = 0UL;
		desc.nSpellCat = 0UL;
		desc.nSpellLevel = 0UL;

		desc.fTimeDuration = 0.0f;
		desc.fTimeInterval = 0.0f;
		desc.fDamagePerInterval = 0.0f;

		desc.fAzimuth = 0.0f;
		desc.fElevation = 0.0f;
		desc.fTimeCurrent = 0.0f;
		desc.vecLocationHit = zVEC3();
		desc.vecDirectionFly = zVEC3();

		desc.pVobParticleFX = Null;
		desc.pParticleFX = Null;
		desc.pVisualFX = Null;
		desc.pFXHit = Null;
		desc.pItemWeapon = Null;
		desc.strVisualFX = zSTRING();
	}

	inline void BuildDescriptor(oCNpc::oSDamageDescriptor& desc, oCNpc* atk, oCNpc* target, const ExtraDamageInfo& damStruct, const ulong flags)
	{
		BuildDescriptor(desc);

		desc.dwFieldsValid = oCNpc::oEDamageDescFlag_Attacker | oCNpc::oEDamageDescFlag_Npc | 
			oCNpc::oEDamageDescFlag_Inflictor | oCNpc::oEDamageDescFlag_DamageType | oCNpc::oEDamageDescFlag_Damage;

		desc.dwFieldsValid |= flags;
		if (HasFlag(damStruct.DamageFlags, StExt_DamageFlag_Aoe) || HasFlag(damStruct.DamageFlags, StExt_DamageFlag_Chain)) desc.dwFieldsValid |= DamageDescFlag_AoeDamage;

		desc.enuModeDamage = 0UL;
		desc.enuModeWeapon = oETypeWeapon_Special;

		desc.pNpcAttacker = atk;
		desc.pVobAttacker = atk;
		desc.pVobHit = target;

		int totalDamage = 0;
		for (int i = 0; i < oEDamageIndex_MAX; ++i)
		{
			desc.aryDamage[i] = 0UL;
			desc.aryDamageEffective[i] = 0UL;
			if (damStruct.Damage[i] <= 0) continue;

			totalDamage += damStruct.Damage[i];
			desc.aryDamage[i] = static_cast<ulong>(damStruct.Damage[i]);
			desc.enuModeDamage |= static_cast<ulong>(1 << i);
		}

		desc.fDamageTotal = static_cast<float>(totalDamage);
		desc.fDamageReal = static_cast<float>(totalDamage);
		desc.fDamageEffective = static_cast<float>(totalDamage);
		desc.fDamageMultiplier = 1.0f;

		if (target)
		{
			desc.vecDirectionFly = target->GetPositionWorld();
			desc.vecLocationHit = target->GetPositionWorld();
		}
	}

	inline void BuildDescriptor(oCNpc::oSDamageDescriptor& desc, oCNpc* atk, oCNpc* target, int damType, int damTotal)
	{
		BuildDescriptor(desc);

		desc.dwFieldsValid = oCNpc::oEDamageDescFlag_Attacker | oCNpc::oEDamageDescFlag_Npc |
			oCNpc::oEDamageDescFlag_Inflictor | oCNpc::oEDamageDescFlag_DamageType | oCNpc::oEDamageDescFlag_Damage;

		desc.enuModeWeapon = oETypeWeapon_Special;
		desc.pNpcAttacker = atk;
		desc.pVobAttacker = atk;
		desc.pVobHit = target;

		for (int i = 0; i < oEDamageIndex_MAX; ++i)
		{
			desc.aryDamage[i] = 0UL;
			desc.aryDamageEffective[i] = 0UL;
		}
		desc.enuModeDamage = static_cast<ulong>(damType);
		ApplyDamages(desc.enuModeDamage, desc.aryDamage, damTotal);

		desc.fDamageTotal = static_cast<float>(damTotal);
		desc.fDamageReal = static_cast<float>(damTotal);
		desc.fDamageEffective = static_cast<float>(damTotal);
		desc.fDamageMultiplier = 1.0f;

		if (target)
		{
			desc.vecDirectionFly = target->GetPositionWorld();
			desc.vecLocationHit = target->GetPositionWorld();
		}
	}

	inline void UpdateDamageInfo(DamageInfo& damageInfo, oCNpc::oSDamageDescriptor& desc)
	{
		int total = 0;
		int real = 0;
		ulong damageEnum = 0;

		if (!IsDamageDescriptorSane(desc))
		{
			DEBUG_MSG("UpdateDamageInfo - damage descriptor is corrupted!");
			return;
		}

		for (int i = 0; i < oEDamageIndex_MAX; ++i)
		{
			int baseDam = static_cast<int>(desc.aryDamage[i]);
			int effDam = static_cast<int>(desc.aryDamageEffective[i]);
			int finalDam = (effDam > 0) ? effDam : 
				(baseDam > 0) ? baseDam : 0;

			damageInfo.Damage[i] = finalDam;
			if (finalDam > 0) damageEnum |= (1 << i);
			total += baseDam;
			real += finalDam;
		}

		damageInfo.TotalDamage = static_cast<int>(desc.fDamageTotal > 0.0f ? desc.fDamageTotal : total);
		damageInfo.RealDamage = static_cast<int>(desc.fDamageReal > 0.0f ? desc.fDamageReal : real);
		if (damageInfo.TotalDamage <= 0) damageInfo.TotalDamage = 0;
		if (damageInfo.RealDamage <= 0) damageInfo.RealDamage = 0;

		damageInfo.DamageEnum = static_cast<int>(damageEnum);
		damageInfo.WeaponEnum = static_cast<int>(desc.enuModeWeapon);
	}
	inline void BuildDamageInfo(DamageInfo& damageInfo, oCNpc::oSDamageDescriptor& desc)
	{
		memset(&damageInfo, 0, sizeof(DamageInfo));
		damageInfo.DamageType = 0;
		damageInfo.DamageFlags = 0;
		damageInfo.BlockDamage = 0;
		damageInfo.StopProcess = false;
		damageInfo.IsInitial = false;
		UpdateDamageInfo(damageInfo, desc);
	}

	inline void CreateIncomingDamage(IncomingDamageInfo& incDamInfo, DamageInfo& damInfo)
	{
		incDamInfo = IncomingDamageInfo{};

		incDamInfo.Flags = 0;
		incDamInfo.DamageType = damInfo.DamageType;
		incDamInfo.DamageFlags = damInfo.DamageFlags;
		incDamInfo.SpellId = damInfo.SpellId;
		for (int i = 0; i < oEDamageIndex_MAX; ++i)
			incDamInfo.Damage[i] = damInfo.Damage[i];
		incDamInfo.DamageTotal = damInfo.RealDamage;

		if (HasFlag(damInfo.DamageFlags, StExt_DamageFlag_Dot)) incDamInfo.Flags |= StExt_IncomingDamageFlag_Index_DotDamage;
		if (HasFlag(damInfo.DamageFlags, StExt_DamageFlag_Aoe)) incDamInfo.Flags |= StExt_IncomingDamageFlag_Index_AoeDamage;
		if (HasFlag(damInfo.DamageFlags, StExt_DamageFlag_Reflect)) incDamInfo.Flags |= StExt_IncomingDamageFlag_Index_ReflectDamage;
	}

	inline void CreateIncomingDamage(IncomingDamageInfo& damageInfo, DamageMeta* damageMeta)
	{
		damageInfo = IncomingDamageInfo{};

		damageInfo.Flags = 0;
		damageInfo.DamageType = damageMeta->DamageInfo.DamageType;
		damageInfo.DamageFlags = damageMeta->DamageInfo.DamageFlags;
		damageInfo.SpellId = damageMeta->DamageInfo.SpellId;
		for (int i = 0; i < oEDamageIndex_MAX; ++i)
			damageInfo.Damage[i] = damageMeta->DamageInfo.Damage[i];
		damageInfo.DamageTotal = damageMeta->DamageInfo.RealDamage;
		
		if (damageMeta->IsDotDamage) damageInfo.Flags |= StExt_IncomingDamageFlag_Index_DotDamage;
		if (damageMeta->IsReflectDamage) damageInfo.Flags |= StExt_IncomingDamageFlag_Index_ReflectDamage;
		if (damageMeta->IsExtraDamage) damageInfo.Flags |= StExt_IncomingDamageFlag_Index_ExtraDamage;
		if (damageMeta->IsAoeDamage) damageInfo.Flags |= StExt_IncomingDamageFlag_Index_AoeDamage;
		if (damageMeta->Attacker) damageInfo.Flags |= StExt_IncomingDamageFlag_Index_HasAttacker;
		if (damageMeta->Weapon) damageInfo.Flags |= StExt_IncomingDamageFlag_Index_HasWeapon;
	}
	inline void CreateIncomingDamage(IncomingDamageInfo& damageInfo, const int damage)
	{
		damageInfo = IncomingDamageInfo{};
		damageInfo.Flags = StExt_IncomingDamageFlag_Index_Contextual;
		damageInfo.DamageType = 0;
		damageInfo.DamageFlags = 0;
		damageInfo.SpellId = 0;
		for (int i = 0; i < oEDamageIndex_MAX; ++i)
			damageInfo.Damage[i] = 0;

		damageInfo.Damage[0] = damage;
		damageInfo.DamageTotal = damage;
	}


	inline DamageMeta& PushDamageMeta(oCNpc* target, oCNpc::oSDamageDescriptor& desc)
	{
		DamageMeta damageMeta = DamageMeta{};

		damageMeta.Target = target;
		damageMeta.Attacker = IsNpcPointerValid(desc.pNpcAttacker) ? zDYNAMIC_CAST<oCNpc>(desc.pNpcAttacker) : Null;
		damageMeta.Weapon = desc.pItemWeapon;
		SetScriptDamageActors(desc.pNpcAttacker, target, desc.pItemWeapon);

		damageMeta.Desc = &desc;
		const ulong descriptorFlags = desc.dwFieldsValid;

		damageMeta.IsExtraDamage = HasFlag(descriptorFlags, (ulong)DamageDescFlag_ExtraDamage);
		damageMeta.IsDotDamage = HasFlag(descriptorFlags, (ulong)DamageDescFlag_DotDamage);
		damageMeta.IsAoeDamage = HasFlag(descriptorFlags, (ulong)DamageDescFlag_AoeDamage);
		damageMeta.IsReflectDamage = HasFlag(descriptorFlags, (ulong)DamageDescFlag_ReflectDamage);
		damageMeta.IsInitialDamage = !damageMeta.IsExtraDamage && !damageMeta.IsDotDamage && !damageMeta.IsAoeDamage && !damageMeta.IsReflectDamage;
		damageMeta.IsOverlayDamage = HasFlag(descriptorFlags, (ulong)oCNpc::oEDamageDescFlag_OverlayActivate) || HasFlag(descriptorFlags, (ulong)oCNpc::oEDamageDescFlag_OverlayInterval) ||
			HasFlag(descriptorFlags, (ulong)oCNpc::oEDamageDescFlag_OverlayDuration) || HasFlag(descriptorFlags, (ulong)oCNpc::oEDamageDescFlag_OverlayDamage);
		damageMeta.TargetIsImmortal = *(int*)parser->CallFunc(IsNpcImmortalFunc);
		damageMeta.SpellFxInDict = false;

		BuildDamageInfo(damageMeta.DamageInfo, desc);
		damageMeta.DamageInfo.SpellId = GetFxTrueId(&desc, damageMeta.IsAbility);
		damageMeta.DamageInfo.IsInitial = damageMeta.IsInitialDamage;

		if (DamageExtraParams.HasPendingExtraParams)
		{
			damageMeta.DamageInfo.DamageType |= DamageExtraParams.DamageType;
			damageMeta.DamageInfo.DamageFlags |= DamageExtraParams.DamageFlags;
			memcpy(damageMeta.PierceDamage, DamageExtraParams.PierceDamage, sizeof(damageMeta.PierceDamage));
			memset(DamageExtraParams.PierceDamage, 0, sizeof(DamageExtraParams.PierceDamage));
			DamageExtraParams.HasPendingExtraParams = false;
		}

		DamageMeta& ref = DamageMetaData.InsertEnd(damageMeta);
		DEBUG_MSG("PushDamageMeta: damage meta was PUSHED to stack! Stack level: " + Z((int)DamageMetaData.GetNum()) + " | descPtr: " + Z((int)&desc));

		parser->SetInstance(StExt_DamageInfo_SymId, &ref.DamageInfo);
		DEBUG_MSG_DAMMETA(ref, "PushDamageMeta");
		DEBUG_MSG_DAMINFO(ref.DamageInfo, "PushDamageMeta");
		return ref;
	}

	inline void PopDamageMeta()
	{
		if (DamageMetaData.IsEmpty())
		{
			DEBUG_MSG("PopDamageMeta: Stack is empty!");
			SetScriptDamageActors(Null, Null, Null);
			parser->SetInstance(StExt_DamageInfo_SymId, Null);
			DamageInfoMetaData.IsPending = false;
			return;
		}

		DamageMeta& damageMeta = DamageMetaData.GetLast();

		DEBUG_MSG_IF(DamageInfoMetaData.IsPending, "PopDamageMeta: DamageInfoMetaData is pending!?");
		DamageInfoMetaData.IsPending = true;
		DamageInfoMetaData.Attacker = damageMeta.Attacker;
		DamageInfoMetaData.Target = damageMeta.Target;
		DamageInfoMetaData.Weapon = damageMeta.Weapon;
		DamageInfoMetaData.DamageInfo = damageMeta.DamageInfo;

		DamageMetaData.RemoveAt(DamageMetaData.GetNum() - 1U);

		DEBUG_MSG("PopDamageMeta: damage meta was POPED from stack! Stack level: " + Z((int)DamageMetaData.GetNum()));
		if (!DamageMetaData.IsEmpty())
		{
			DamageMeta& damageMeta = DamageMetaData.GetLast();
			DEBUG_MSG_DAMMETA(damageMeta, "PopDamageMeta");
			DEBUG_MSG_DAMINFO(damageMeta.DamageInfo, "PopDamageMeta");
			parser->SetInstance(StExt_DamageInfo_SymId, &damageMeta.DamageInfo);
		}
		else
			parser->SetInstance(StExt_DamageInfo_SymId, Null);
	}

	inline DamageMeta* GetDamageMeta(bool isOverlay = false)
	{
		if (DamageMetaData.IsEmpty())
		{
			DEBUG_MSG("GetDamageMeta: Stack is empty!");
			return Null;
		}
		return &DamageMetaData.GetLast();
	}

	inline void ClearDamageMeta()
	{
		DEBUG_MSG_IF(DamageMetaData.GetNum() > 0, "ClearDamageMeta: there is a damageMeta left in stack!");
		DamageExtraParams = ExtraDamageParams{};
		DamageInfoMetaData.IsPending = false;
		DamageMetaData.Clear();
		PendingEffectDamageData.Clear();
	}

	int FindTargets(Array<oCNpc*>& targetsList, oCNpc* atk, oCNpc* target, ExtraDamageInfo& damageStruct, int damageSelectorFunc = Invalid)
	{
		bool isAoe = HasFlag(damageStruct.DamageFlags, StExt_DamageFlag_Aoe);
		bool isChain = HasFlag(damageStruct.DamageFlags, StExt_DamageFlag_Chain);
		bool inMassDamage = (isAoe || isChain) && (damageStruct.MaxTargets > 0);

		if (!inMassDamage) return Invalid;

		oCNpc* center = Null;
		if (target && (HasFlag(damageStruct.DamageFlags, StExt_DamageFlag_Target) ||
			HasFlag(damageStruct.DamageFlags, StExt_DamageFlag_Single))) center = target;
		else if (atk) center = atk;
		else if (target) center = target;
		else
		{
			DEBUG_MSG("FindTargets: center npc is null! Skipped!");
			return Invalid;
		}

		int foundNpcs = 0;
		if (damageSelectorFunc == Invalid)
			damageSelectorFunc = StExt_IsEnemyFuncIndex;
		const float radius = static_cast<float>(damageStruct.Radius < 100 ? 100 : damageStruct.Radius);
		center->ClearVobList();
		center->CreateVobList(radius);

		zCVob* pVob = Null;
		oCNpc* npc = Null;
		void* oldOther = parser->GetSymbol(StExt_ModOther_SymId)->GetInstanceAdr();
		void* oldSelf = parser->GetSymbol(StExt_ModSelf_SymId)->GetInstanceAdr();

		for (int i = 0; i < center->vobList.GetNum(); ++i)
		{
			if (i > 8192 || foundNpcs > damageStruct.MaxTargets) break;
			pVob = center->vobList.GetSafe(i);
			if (!pVob) continue;

			npc = zDYNAMIC_CAST<oCNpc>(pVob);
			if (!npc) continue;
			if (npc->IsDead() || npc->IsUnconscious()) continue;

			parser->SetInstance(StExt_ModSelf_SymId, atk);
			parser->SetInstance(StExt_ModOther_SymId, npc);
			bool isEnemy = *(int*)parser->CallFunc(damageSelectorFunc);
			if (isEnemy) {
				targetsList.Insert(npc);
				++foundNpcs;
			}
		}

		if (center && player && (center != player))
		{
			float dist = center->GetDistanceToVob(*player);
			if ((dist <= radius) && !targetsList.HasEqual(player))
			{
				parser->SetInstance(StExt_ModSelf_SymId, atk);
				parser->SetInstance(StExt_ModOther_SymId, player);
				bool isEnemy = *(int*)parser->CallFunc(damageSelectorFunc);
				if (isEnemy) {
					targetsList.InsertFront(player);
					++foundNpcs;
				}
			}
		}

		parser->SetInstance(StExt_ModSelf_SymId, oldSelf);
		parser->SetInstance(StExt_ModOther_SymId, oldOther);
		return foundNpcs;
	}

	inline void CheckExtraDamages(bool& hasDamage, bool& hasDotDamage, ExtraDamageInfo& damageStruct)
	{
		int totalDamage = 0;
		int totalDotDamage = 0;
		for (int i = 0; i < oEDamageIndex_MAX; ++i)
		{
			totalDamage += damageStruct.Damage[i];
			totalDamage += damageStruct.PierceDamage[i];	// hit czysto zywiolowy (tylko pierce) tez musi przejsc przez OnDamage
			totalDotDamage += damageStruct.DotDamage[i];
		}
		hasDamage = (totalDamage > 0);
		hasDotDamage = (totalDotDamage > 0);
	}


	//-----------------------------------------------------------------
	//					DAMAGE PROCESSING FUNCTIONS
	//-----------------------------------------------------------------

	inline void AddDotDamage(oCNpc* atk, oCNpc* target, ExtraDamageInfo& extraDam)
	{
		static int dotDamageFuncIndex = parser->GetIndex("StExt_OnDotDamage");
		static int extraDamagePtrIndex = parser->GetIndex("StExt_ExtraDamageInfoPtr");

		if (!target) {
			DEBUG_MSG("AddDotDamage: target npc is null! Skipped!");
			return;
		}
		if (target->IsDead()) return;

		parser->SetInstance(StExt_ModSelf_SymId, target);
		parser->SetInstance(StExt_ModOther_SymId, atk);
		parser->SetInstance(extraDamagePtrIndex, &extraDam);
		parser->CallFunc(dotDamageFuncIndex);
		parser->SetInstance(extraDamagePtrIndex, Null);
	}

	inline void AddPendingDamage(oCNpc* atk, oCNpc* target, const ExtraDamageInfo& extraDam, const ulong flags, int damageSelectorFunc = Invalid)
	{
		PendingDamage pendingDamage = PendingDamage{};
		pendingDamage.IsMock = false;
		pendingDamage.Attacker = atk;
		pendingDamage.Target = target;
		pendingDamage.Flags = flags;
		pendingDamage.FrameTime = Invalid;
		pendingDamage.DamageSelectorFunc = damageSelectorFunc;
		pendingDamage.DamageInfo = extraDam;
		PendingDamageData.Insert(pendingDamage);
	}

	inline void AddPendingSpellDamage(oCNpc* atk, oCNpc* target, oCVisualFX* hitFx, const ExtraDamageInfo& extraDam, const ulong flags, int damageSelectorFunc)
	{
		if (!hitFx) return;

		PendingDamage pendingDamage = PendingDamage{};
		pendingDamage.IsMock = false;
		pendingDamage.Attacker = atk;
		pendingDamage.Target = target;
		pendingDamage.Flags = flags;
		pendingDamage.FrameTime = SPELLEFFECT_STORE_TIME;
		pendingDamage.DamageSelectorFunc = damageSelectorFunc;
		pendingDamage.DamageInfo = extraDam;
		PendingEffectDamageData.Insert(hitFx, pendingDamage);
	}

	inline void DoExtraDamage(oCNpc* atk, oCNpc* target, ExtraDamageInfo& extraDam, const ulong flags, const bool hasDamage, const bool hasDotDamage)
	{
		if (!target || !IsNpcPointerValid(target)) {
			DEBUG_MSG("DoExtraDamage: target npc is null or corrupted! Skipped!");
			return;
		}
		if (target->IsDead()) return;

		if (!IsNpcPointerValid(atk)) atk = Null;
		if (hasDotDamage)
			AddDotDamage(atk, target, extraDam);

		if (hasDamage)
		{
			oCNpc::oSDamageDescriptor desc;
			BuildDescriptor(desc, atk, target, extraDam, flags);

			DamageExtraParams.HasPendingExtraParams = true;
			DamageExtraParams.DamageType = extraDam.DamageType;
			DamageExtraParams.DamageFlags = extraDam.DamageFlags;
			memcpy(DamageExtraParams.PierceDamage, extraDam.PierceDamage, sizeof(DamageExtraParams.PierceDamage));
			if (HasFlag(flags, (ulong)DamageDescFlag_DotDamage)) DamageExtraParams.DamageFlags |= StExt_DamageFlag_Dot;
			if (HasFlag(flags, (ulong)DamageDescFlag_ReflectDamage)) DamageExtraParams.DamageFlags |= StExt_DamageFlag_Reflect;

			target->OnDamage(desc);
		}
	}

	void ApplyExtraDamage_Generic(oCNpc* atk, oCNpc* target, ExtraDamageInfo& damageStruct, const ulong flags, int damageSelectorFunc = Invalid)
	{ 
		bool hasDamage = false, hasDotDamage = false;
		CheckExtraDamages(hasDamage, hasDotDamage, damageStruct);
		if (!hasDamage && !hasDotDamage) {
			DEBUG_MSG("ApplyExtraDamage_Generic: total damage is less than 0! Skipped!");
			return;
		}

		Array<oCNpc*> targetsList;
		if (target)
			targetsList.Insert(target);

		FindTargets(targetsList, atk, target, damageStruct, damageSelectorFunc);
		if (targetsList.IsEmpty()) {
			DEBUG_MSG("ApplyExtraDamage_Generic: no targets to damage! Skipped!");
			return;
		}

		for (uint i = 0; i < targetsList.GetNum(); ++i)
			DoExtraDamage(atk, targetsList[i], damageStruct, flags, hasDamage, hasDotDamage);
	}

	void ApplyExtraDamage(oCNpc* atk, oCNpc* target) { ApplyExtraDamage_Generic(atk, target, ExtraDamage, DamageDescFlag_ExtraDamage); }
	
	void ApplyScriptDamage(oCNpc* atk, oCNpc* target, ExtraDamageInfo& damStruct, const int damageSelectorFuncIndex) { 
		ApplyExtraDamage_Generic(atk, target, damStruct, DamageDescFlag_ExtraDamage, damageSelectorFuncIndex);
	}

	void ApplyDotDamage(oCNpc* atk, oCNpc* target)
	{
		if (!target) {
			DEBUG_MSG("ApplyDotDamage: Target is null!");
			return;
		}

		bool hasDamage = false, hasDotDamage = false;
		CheckExtraDamages(hasDamage, hasDotDamage, DotDamage);
		if (!hasDamage) {
			DEBUG_MSG("ApplyDotDamage: total damage is less than 0! Skipped!");
			return;
		}
		DoExtraDamage(atk, target, DotDamage, DamageDescFlag_DotDamage, hasDamage, false);
	}

	void ApplyReflectDamage(oCNpc* atk, oCNpc* target) { AddPendingDamage(atk, target, ReflectDamage, DamageDescFlag_ReflectDamage); }

	void ApplyScriptPendingDamage(oCNpc* atk, oCNpc* target, ExtraDamageInfo& damStruct, const int damageSelectorFuncIndex) {
		AddPendingDamage(atk, target, damStruct, DamageDescFlag_ExtraDamage, damageSelectorFuncIndex);
	}

	void ProcessPendingDamages()
	{
		if (IsLevelChanging || IsLoading)
		{
			DEBUG_MSG("ProcessPendingDamages: BREAK: can't process while level is changing!");
			PendingDamageData.Clear();
			PendingEffectDamageData.Clear();
			return;
		}

		if (!PendingDamageData.IsEmpty())
		{
			for (uint i = 0U; i < PendingDamageData.GetNum(); ++i)
			{
				PendingDamage& pendingDamage = PendingDamageData[i];
				ApplyExtraDamage_Generic(pendingDamage.Attacker, pendingDamage.Target, pendingDamage.DamageInfo, pendingDamage.Flags, pendingDamage.DamageSelectorFunc);
			}
			PendingDamageData.Clear();
		}

		if (PendingEffectDamageData.IsEmpty()) return;
		auto* pairs = PendingEffectDamageData.begin();
		for (int i = PendingEffectDamageData.GetNum() - 1; i >= 0; --i)
		{
			auto& pair = pairs[i];
			if (--pair.GetValue().FrameTime <= 0)
				PendingEffectDamageData.Remove(pair.GetKey());
		}
	}


	void ProcessExtraDamage(oCNpc::oSDamageDescriptor& desc, oCNpc* target)
	{
		DEBUG_MSG_DAM("ProcessExtraDamage", "ENTER", desc.pNpcAttacker, target);
		static int dontKillcheckFuncIndex = parser->GetIndex("StExt_DontKillByExtraDamage_Engine");

		if (!target) {
			DEBUG_MSG_FUNC("ProcessExtraDamage", "target is null!");
			return;
		}

		if (target->IsDead() || target->IsUnconscious()){
			DEBUG_MSG_IF(target->IsDead(), "ProcessExtraDamage: target is dead!");
			DEBUG_MSG_IF(target->IsUnconscious(), "ProcessExtraDamage: target is unconscious!");
			return;
		}

		DamageMeta* currentDamageMeta = GetDamageMeta();
		DEBUG_MSG_IF(!currentDamageMeta, "ProcessExtraDamage: fail to get currentDamageMeta!");
		DEBUG_MSG_IF(currentDamageMeta && currentDamageMeta->Target != target, "ProcessExtraDamage: use DamageStackMeta for incorrect character!");

		int damageTotal = 0;
		int damageReal = 0;
		for (int i = 0; i < oEDamageIndex_MAX; ++i)
		{
			int dam = static_cast<int>(desc.aryDamage[i]);
			if (dam <= 0) continue;

			damageTotal += dam;
			if (target->protection[i] < 0) continue;

			dam = dam - target->protection[i];
			dam = dam <= 0 ? 1 : dam;
			damageReal += dam;
			desc.aryDamageEffective[i] = static_cast<ulong>(dam);
		}

		// Pierce channels (element damage): applied with NO protection
		// subtraction (user call 2026-07-18); immunity (protection < 0)
		// still skips the channel entirely.
		if (currentDamageMeta)
		{
			for (int i = 0; i < oEDamageIndex_MAX; ++i)
			{
				int pierce = currentDamageMeta->PierceDamage[i];
				if (pierce <= 0) continue;
				if (target->protection[i] < 0) continue;

				damageTotal += pierce;
				damageReal += pierce;
				desc.aryDamageEffective[i] += static_cast<ulong>(pierce);
			}
		}

		if (damageReal <= 0) damageReal = 1;
		desc.fDamageTotal = static_cast<float>(damageTotal);
		desc.fDamageEffective = desc.fDamageReal = static_cast<float>(damageReal);

		int totalHp = target->attribute[0] + GetCurNpcEs(target);
		if(currentDamageMeta)
			UpdateDamageInfo(currentDamageMeta->DamageInfo, desc);

		target->ChangeAttribute(NPC_ATR_HITPOINTS, -damageReal);
		damageReal = totalHp - (target->attribute[0] + GetCurNpcEs(target));

		target->OnDamage_Condition(desc);		
		if ((target->attribute[0] <= 1) && target->IsHuman())
		{
			int dontKill = *(int*)parser->CallFunc(dontKillcheckFuncIndex);
			if (dontKill) target->attribute[0] = 1;
		}

		desc.bIsDead = (target->attribute[0] <= 0);
		desc.bIsUnconscious = target->IsHuman() ? (target->attribute[0] == 1) : False;
		target->OnDamage_Events(desc);

		int damMsgType = StExt_DamageMessageType_Default;
		if (HasFlag(desc.dwFieldsValid, (ulong)DamageDescFlag_DotDamage)) damMsgType |= StExt_DamageMessageType_Dot;
		if (HasFlag(desc.dwFieldsValid, (ulong)DamageDescFlag_ReflectDamage)) damMsgType |= StExt_DamageMessageType_Reflect;
		parser->CallFunc(PrintDamageFunc, damageReal, damMsgType);

		DEBUG_MSG_DAM("ProcessExtraDamage", "EXIT. RealDamage: " + Z(damageReal), desc.pNpcAttacker, target);
	}

	HOOK Hook_oCNpc_EV_DamagePerFrame PATCH(&oCNpc::EV_DamagePerFrame, &oCNpc::EV_DamagePerFrame_StExt);
	int oCNpc::EV_DamagePerFrame_StExt(oCMsgDamage* msg)
	{
		bool hasMeta = false;
		try 
		{ 
			if (msg && this && !msg->deleted)
			{
				oSDamageDescriptor& desc = msg->descDamage;
				if (IsDamageDescriptorSane(desc) && IsNpcPointerValid(this))
				{
					DamageMeta& damageMeta = PushDamageMeta(this, desc);
					damageMeta.DamageInfo.DamageFlags |= StExt_DamageFlag_Dot;
					hasMeta = true;
				}
			}

			int result = THISCALL(Hook_oCNpc_EV_DamagePerFrame)(msg);
			if (hasMeta) PopDamageMeta();
			return result;
		}
		catch (const std::exception& e) { DEBUG_MSG("EV_DamagePerFrame_StExt - EXCEPTION: " + Z(e.what()) + "!"); }
		catch (...) { DEBUG_MSG("EV_DamagePerFrame_StExt - UNKNOWN EXCEPTION!"); }

		if (hasMeta) PopDamageMeta();
		return True;		
	}
	
	HOOK Hook_oCNpc_OnDamage PATCH (&oCNpc::OnDamage, &oCNpc::OnDamage_StExt);
	void oCNpc::OnDamage_StExt(oSDamageDescriptor& desc)
	{
		if (IsLevelChanging || IsLoading || !this) 
		{ 
			DEBUG_MSG_IF(!this, "OnDamage_StExt: BREAK: npc is Null!");
			DEBUG_MSG_IF(IsLevelChanging || IsLoading, "OnDamage_StExt: BREAK: can't process while level is changing!");
			return; 
		}

		if (!IsNpcPointerValid(this))
		{
			DEBUG_MSG("OnDamage_StExt: BREAK: target is corrupted!?");
			return;
		}

		try 
		{ 
			if (!IsDamageDescriptorSane(desc)) {
				DEBUG_MSG("OnDamage_StExt: BREAK: damage descriptor is corrupted!?");
				return;
			}
		}
		catch (const std::exception& e) { DEBUG_MSG("OnDamage_StExt - EXCEPTION! (damage descriptor is corrupted!): " + Z(e.what()) + "!"); return; }
		catch (...) { DEBUG_MSG("OnDamage_StExt - UNKNOWN EXCEPTION! (damage descriptor is corrupted!)"); return; }

		if (desc.pNpcAttacker && !IsNpcPointerValid(desc.pNpcAttacker)) desc.pNpcAttacker = Null;

		DEBUG_MSG_DAM("OnDamage_StExt", "ENTER", desc.pNpcAttacker, this);
		DEBUG_MSG_DAMDESC(desc, "OnDamage_StExt - ENTER", this);		
		DamageMeta& damageMeta = PushDamageMeta(this, desc);

		if (desc.pFXHit)
		{
			if (damageMeta.Attacker && !damageMeta.Attacker->IsSelfPlayer())
			{
				int applyFxDamage = *(int*)parser->CallFunc(FxDamageCanBeAppliedFunc);
				if (!applyFxDamage)
				{
					DEBUG_MSG_DAM("OnDamage_StExt", "EXIT. Reason: pFxHit is blocked (friendly fier)", desc.pNpcAttacker, this);
					PopDamageMeta();
					OnDamage_Effects_End(desc);
					return;
				}
			}

			auto fxDamagePair = PendingEffectDamageData.GetSafePair(desc.pFXHit);
			if (fxDamagePair)
			{
				damageMeta.SpellFxInDict = true;
				PendingDamage& pendingDamage = fxDamagePair->GetValue();
				if (!pendingDamage.IsMock)
				{
					int totalDamage = 0;
					int totalDotDamage = 0;
					for (int i = 0; i < oEDamageIndex_MAX; ++i)
					{
						totalDotDamage += pendingDamage.DamageInfo.DotDamage[i];
						if (pendingDamage.DamageInfo.Damage[i] <= 0) continue;

						totalDamage += pendingDamage.DamageInfo.Damage[i];
						desc.aryDamage[i] += static_cast<ulong>(pendingDamage.DamageInfo.Damage[i]);
						desc.enuModeDamage |= static_cast<ulong>(1 << i);
					}
					desc.fDamageReal += static_cast<float>(totalDamage);
					desc.dwFieldsValid |= pendingDamage.Flags;

					UpdateDamageInfo(damageMeta.DamageInfo, desc);
					damageMeta.DamageInfo.DamageType = pendingDamage.DamageInfo.DamageType;
					damageMeta.DamageInfo.DamageFlags = pendingDamage.DamageInfo.DamageFlags;

					if (totalDotDamage > 0)
						AddDotDamage(desc.pNpcAttacker, this, pendingDamage.DamageInfo);
				}
				else damageMeta.DamageInfo.StopAoe = true;
			}
		}

		if (damageMeta.IsOverlayDamage)
		{
			DEBUG_MSG_DAM("OnDamage_StExt", "EXIT. Reason: Handle overlay damage...", desc.pNpcAttacker, this);
			damageMeta.DamageInfo.DamageFlags |= StExt_DamageFlag_Dot;

			try { THISCALL(Hook_oCNpc_OnDamage)(desc); }
			catch (const std::exception& e) { DEBUG_MSG("OnDamage (overlay) - EXCEPTION: " + Z(e.what()) + "!"); }
			catch (...) { DEBUG_MSG("OnDamage (overlay) - UNKNOWN EXCEPTION!"); }

			PopDamageMeta();
			return;
		}

		// Incoming damage is from my mod. Handle it separately and leave.
		if (!damageMeta.IsInitialDamage)
		{
			ProcessExtraDamage(desc, this);
			DEBUG_MSG_DAM("OnDamage_StExt", "EXIT. Reason: Extra damage handled.", desc.pNpcAttacker, this);
			PopDamageMeta();
			return;
		}

		// Call script damage pre-process
		const int realBeforeBegin = damageMeta.DamageInfo.RealDamage;
		if (damageMeta.Attacker)
		{
			parser->CallFunc(OnDamageBeginFunc);
			if ((damageMeta.DamageInfo.BlockDamage > 0) || damageMeta.DamageInfo.StopProcess)
			{
				DEBUG_MSG_DAM("OnDamage_StExt", "EXIT. Reason: damage was blocked.", desc.pNpcAttacker, this);
				PopDamageMeta();
				return;
			}
		}

		// Weapon-class per-type bonuses -> MAIN HIT, applied HERE in the DLL.
		// The script (StExt_Hero_BeforeOffenceHandler) cannot reliably read
		// StExt_PcStats[311+] under zParserExtender - it reads 0 while the engine
		// holds the correct value - so the sword/axe/light-blade flat+% bonuses
		// are applied natively where the read is trustworthy. Flat first, then
		// permille (cap 150) of the boosted value, mirroring the old script math.
		// Feeds RealDamage; the writeback below scales the raw channels to match.
		if (damageMeta.Attacker && damageMeta.Attacker->IsSelfPlayer()
			&& damageMeta.Weapon && (damageMeta.DamageInfo.DamageType & StExt_DamageType_Melee))
		{
			static zCPar_Symbol* pcStatsSym = parser->GetSymbol("StExt_PcStats");
			if (pcStatsSym && pcStatsSym->intdata && (int)pcStatsSym->ele > 324)
			{
				const int wf = damageMeta.Weapon->flags;
				int flat = 0, perc = 0;
				if (wf & (ITEM_FLAG_SWD | ITEM_FLAG_2HD_SWD))      { flat = pcStatsSym->intdata[319]; perc = pcStatsSym->intdata[320]; }
				else if (wf & (ITEM_FLAG_AXE | ITEM_FLAG_2HD_AXE)) { flat = pcStatsSym->intdata[321]; perc = pcStatsSym->intdata[322]; }
				else if (wf & ITEM_FLAG_DAG)                       { flat = pcStatsSym->intdata[323]; perc = pcStatsSym->intdata[324]; }

				if (flat != 0 || perc != 0)
				{
					int rd = damageMeta.DamageInfo.RealDamage + flat;
					if (perc < 0) perc = 0; else if (perc > 150) perc = 150;
					rd += (int)((long long)rd * perc / 1000);
					damageMeta.DamageInfo.RealDamage = rd < 0 ? 0 : rd;
				}

				// TEMP DIAG: the whole chain of one hero hit -> stext_damage.log
				StExt_DamageDiagLog(wf, realBeforeBegin, flat, perc,
					damageMeta.DamageInfo.RealDamage, damageMeta.DamageInfo.DamageType);
			}
		}

		// Land Before-handler RealDamage edits on the MAIN hit. The original
		// handler reads desc (not DamageInfo), and UpdateDamageInfo below
		// overwrites RealDamage back from desc - so EVERY script edit to
		// DamageInfo.RealDamage in a Before handler was silently discarded.
		// This revives: hero OFFENCE bonuses (weapon-class jewelry, Zar embers)
		// AND hero DEFENCE modifiers (Pancerz Dusz, legendary armor Zelazna
		// Wola / Tarcza Ducha, jewelry Zelazna Skora, Zar embers fragility).
		// Scale the raw damage channels by the intended ratio (both directions);
		// full mitigation zeroes the hit. Gated to hero-involved hits only, so
		// NPC-vs-NPC damage is untouched. Never underflows (scale, not subtract).
		if (realBeforeBegin > 0 && damageMeta.DamageInfo.RealDamage != realBeforeBegin
			&& ((damageMeta.Attacker && damageMeta.Attacker->IsSelfPlayer()) || this->IsSelfPlayer())
			&& IsDamageDescriptorSane(desc))
		{
			const int realWanted = damageMeta.DamageInfo.RealDamage;
			if (realWanted <= 0)
			{
				for (int i = 0; i < oEDamageIndex_MAX; ++i) desc.aryDamage[i] = 0UL;
			}
			else
			{
				const float mainRatio = (float)realWanted / (float)realBeforeBegin;
				for (int i = 0; i < oEDamageIndex_MAX; ++i)
					desc.aryDamage[i] = (unsigned long)((float)desc.aryDamage[i] * mainRatio);
			}
		}

		// Original damage handler
		const int diagHpBefore = (this && IsNpcPointerValid(this)) ? this->attribute[NPC_ATR_HITPOINTS] : 0;
		DEBUG_MSG_DAM("OnDamage", "ENTER.", desc.pNpcAttacker, this);

		// Real parry detection: the engine decides the parade INSIDE the
		// original OnDamage and sets defender->didParade. Consume stale
		// state before the call, check right after - this is the only spot
		// where the flag is guaranteed fresh (the AI clears it later in
		// ProcessNpc, which is why the per-frame hook never saw it).
		const bool watchParade = this->IsSelfPlayer() && StExt_OnPlayerParadeSuccessFunc != Invalid;
		if (watchParade) this->didParade = 0;

		bool descStillSane = false;
		try
		{
			THISCALL(Hook_oCNpc_OnDamage)(desc);
			descStillSane = IsDamageDescriptorSane(desc);
		}
		catch (const std::exception& e)
		{
			DEBUG_MSG("OnDamage - EXCEPTION: " + Z(e.what()) + "!");
			PopDamageMeta();
			return;
		}
		catch (...)
		{
			DEBUG_MSG("OnDamage - UNKNOWN EXCEPTION!");
			PopDamageMeta();
			return;
		}

		if (damageMeta.Attacker && !IsNpcPointerValid(damageMeta.Attacker))
		{
			DEBUG_MSG("OnDamage - BEFORE EXIT. Atk is corrupted!");
			damageMeta.Attacker = Null;
			if (descStillSane) desc.pNpcAttacker = Null;
		}
		if (descStillSane && desc.pNpcAttacker && !IsNpcPointerValid(desc.pNpcAttacker)) desc.pNpcAttacker = Null;

		// Freshly set inside the original OnDamage = the player just parried
		// this very hit. Notify scripts (perfect vs held-block resolved there).
		if (watchParade && this->didParade)
			parser->CallFunc(StExt_OnPlayerParadeSuccessFunc);

		// TEMP DIAG: what the target actually lost, i.e. the number after armour
		if (this && IsNpcPointerValid(this))
			StExt_DamageDiagApplied(diagHpBefore - this->attribute[NPC_ATR_HITPOINTS]);


		UpdateDamageInfo(damageMeta.DamageInfo, desc);
		SetScriptDamageActors(damageMeta.Attacker, damageMeta.Target, damageMeta.Weapon);
		parser->SetInstance(StExt_DamageInfo_SymId, &damageMeta.DamageInfo);

		DEBUG_MSG_DAM("OnDamage", "EXIT.", descStillSane ? desc.pNpcAttacker : Null, this);
		if (descStillSane)
			DEBUG_MSG_DAMDESC(desc, "OnDamage AFTER original OnDamage", this);
		DEBUG_MSG_DAMMETA(damageMeta, "OnDamage");
		DEBUG_MSG_DAMINFO(damageMeta.DamageInfo, "OnDamage");

		// Call script damage post-process
		if (damageMeta.Attacker)
			parser->CallFunc(OnDamageAfterFunc);

		// Floating damage number for what the HERO just dealt to an enemy.
		if (damageMeta.Attacker && damageMeta.Attacker->IsSelfPlayer()
			&& !this->IsSelfPlayer() && damageMeta.DamageInfo.RealDamage > 0)
			StExt_SpawnFloatingDamage(this, damageMeta.DamageInfo.RealDamage, false);

		// prevent multi-aoe damage from spells
		// mark spellFx as processed
		if (descStillSane && desc.pFXHit && !damageMeta.SpellFxInDict)
		{
			PendingDamage mockDamage = PendingDamage{};
			mockDamage.IsMock = true;
			mockDamage.FrameTime = SPELLEFFECT_STORE_TIME;
			PendingEffectDamageData.Insert(desc.pFXHit, mockDamage);
		}

		PopDamageMeta();
		if (descStillSane)
			DEBUG_MSG_DAMDESC(desc, "OnDamage_StExt - EXIT", this);
		DEBUG_MSG_DAM("OnDamage_StExt", "EXIT.", descStillSane ? desc.pNpcAttacker : Null, this);
	}


	HOOK ivk_oCNpc_ChangeAttribute PATCH(&oCNpc::ChangeAttribute, &oCNpc::ChangeAttribute_StExt);


	void oCNpc::ChangeAttribute_StExt(int attrIndex, int value)
	{
		static int dontKillcheckFuncIndex = parser->GetIndex("StExt_DontKillByExtraDamage_Engine");

		if ((attrIndex == NPC_ATR_HITPOINTS) && (value < 0))
		{
			if (!this || !IsNpcPointerValid(this)) 
			{
				// Try do it anyway. (fix dead npc raising)
				if (this)
				{
					try 
					{ 
						DEBUG_MSG("ChangeAttribute_StExt: apply Hp change to corrupted npc...");
						THISCALL(ivk_oCNpc_ChangeAttribute)(attrIndex, value); 
					}
					catch (const std::exception& e) { DEBUG_MSG("ChangeAttribute_StExt - EXCEPTION: " + Z(e.what()) + "!"); return; }
					catch (...) { DEBUG_MSG("ChangeAttribute_StExt - UNKNOWN EXCEPTION!"); return; }
				}
				DEBUG_MSG("ChangeAttribute_StExt: EXIT. Reason: npc is corrupted!");
				return;
			}

			if (this->IsDead()) {
				DEBUG_MSG("ChangeAttribute_StExt: EXIT. Reason: npc already dead.");
				return;
			}

			int damage = value * (-1);
			IncomingDamageInfo currentDamageInfo;
			DamageMeta* currentDamageMeta = GetDamageMeta();
			oCNpc* attaker = Null;
			oCItem* weapon = Null;

			if (currentDamageMeta && currentDamageMeta->Target == this && IsDamageDescriptorSane(*currentDamageMeta->Desc))
			{
				attaker = currentDamageMeta->Attacker;
				weapon = currentDamageMeta->Weapon;
				UpdateDamageInfo(currentDamageMeta->DamageInfo, *currentDamageMeta->Desc);
				CreateIncomingDamage(currentDamageInfo, currentDamageMeta);
				DEBUG_MSG("ChangeAttribute_StExt: used descPtr: " + Z((int)currentDamageMeta->Desc));
			}
			else
			{
				DEBUG_MSG_IF(currentDamageMeta && currentDamageMeta->Target != this, "ChangeAttribute_StExt: use DamageStackMeta for incorrect character!");
				DEBUG_MSG_IF(DamageInfoMetaData.IsPending && DamageInfoMetaData.Target != this, "ChangeAttribute_StExt: use DamageInfoMetaData for incorrect character!");

				// try retrieve last damage info
				if (DamageInfoMetaData.IsPending && DamageInfoMetaData.Target == this)
				{
					attaker = DamageInfoMetaData.Attacker;
					weapon = DamageInfoMetaData.Weapon;
					CreateIncomingDamage(currentDamageInfo, DamageInfoMetaData.DamageInfo);
					DamageInfoMetaData.IsPending = false;
				}
				else
				{
					DEBUG_MSG("ChangeAttribute_StExt: create empty IncomingDamageInfo!");
					CreateIncomingDamage(currentDamageInfo, damage);
				}
			}

			if (!IsNpcPointerValid(attaker)) attaker = Null;
			if (currentDamageInfo.DamageTotal != damage)
			{
				DEBUG_MSG("ChangeAttribute_StExt: actual damage and IncomingDamageInfo damages was different! Was: " + Z(currentDamageInfo.DamageTotal));
				currentDamageInfo.DamageTotal = damage;
			}
			if (attaker) currentDamageInfo.Flags |= StExt_IncomingDamageFlag_Index_HasAttacker;
			if (weapon) currentDamageInfo.Flags |= StExt_IncomingDamageFlag_Index_HasWeapon;

			DEBUG_MSG_DAM("ChangeAttribute_StExt", "ENTER. " + GetNpcHpEs(this) + " | Damage: " + Z(damage), attaker, this);
			DEBUG_MSG_DAMINCOMINFO(currentDamageInfo, "ChangeAttribute");

			SetScriptDamageActors(attaker, this, weapon);
			parser->SetInstance(StExt_IcomingDamageInfo_SymId, &currentDamageInfo);
			value += *(int*)parser->CallFunc(ProcessHpDamageFunc);
			DEBUG_MSG_DAM("ChangeAttribute_StExt", "AFTER SCRIPT. " + GetNpcHpEs(this) + " | Value: " + Z(value), attaker, this);

			if (value >= 0) 
			{
				DEBUG_MSG_FUNC("ChangeAttribute_StExt", "Damage to '" + SafeNpcName(this) + "' was absorbed! (value now: " + Z(value) + ")");
				parser->CallFunc(OnPostDamageFunc);
				parser->SetInstance(StExt_IcomingDamageInfo_SymId, Null);
				DamageInfoMetaData.IsPending = false;
				return;
			}

			damage = value * (-1);
			bool mustKill = false;
			// TEMP DIAG state for the hero-combat log below.
			const bool diagHero = this->IsSelfPlayer();
			int diagDontKill = -1;		// -1 = guard not consulted
			bool diagClamped = false;
			if ((damage >= this->attribute[0]) && (this->attribute[0] > 1) && this->IsHuman())
			{
				int dontKill = *(int*)parser->CallFunc(dontKillcheckFuncIndex);
				bool saveLife = HasFlag(currentDamageInfo.Flags, StExt_IncomingDamageFlag_Index_DontKill) || dontKill;
				diagDontKill = dontKill;
				diagClamped = saveLife;
				if (saveLife)
					value = -(this->attribute[0] - 1);
				else mustKill = true;
			}

			if (diagHero)
			{
				zSTRING atkName = attaker ? SafeNpcName(attaker) : zSTRING("NONE(!)");
				StExt_CombatDiagLog(zSTRING("HERO HIT | atk=") + atkName
					+ " | dmg=" + zSTRING(damage)
					+ " | hp=" + zSTRING(this->attribute[0]) + "/" + zSTRING(this->attribute[1])
					+ " | dontKillGuard=" + zSTRING(diagDontKill)
					+ (diagClamped ? " | CLAMPED->1HP (nie zginiesz)" : "")
					+ " | dmgType=" + zSTRING(currentDamageInfo.DamageType)
					+ " | flags=" + zSTRING(currentDamageInfo.Flags));
			}

			DEBUG_MSG_FUNC("ChangeAttribute_StExt", "Apply " + Z(damage) + " damage to '" + SafeNpcName(this) + "' ...");
			int hpBefore = this->attribute[0];
			THISCALL(ivk_oCNpc_ChangeAttribute)(attrIndex, value);
			DEBUG_MSG_DAM("ChangeAttribute_StExt", "After original hp handler. " + GetNpcHpEs(this) + " | Value: " + Z(value), attaker, this);

			SetScriptDamageActors(attaker, this, weapon);
			parser->SetInstance(StExt_IcomingDamageInfo_SymId, &currentDamageInfo);
			parser->CallFunc(OnPostDamageFunc);

			parser->SetInstance(StExt_IcomingDamageInfo_SymId, Null);
			DamageInfoMetaData.IsPending = false;

			if (mustKill && (this->attribute[0] > 0)) this->attribute[0] = 0;
			else if (hpBefore == this->attribute[0])
			{
				DEBUG_MSG("ChangeAttribute_StExt: Damage wasn't applied?! Hp now: " + Z(this->attribute[0]) + " | Hp was: " + Z(hpBefore) + " | Damage: " + Z(damage));
				this->attribute[0] += value;
				if (this->attribute[0] < 0) this->attribute[0] = 0;
			}
			return;
		}
		THISCALL(ivk_oCNpc_ChangeAttribute)(attrIndex, value);
	}

	HOOK Hook_oCNpc_DoDie PATCH(&oCNpc::DoDie, &oCNpc::DoDie_StExt);
	void oCNpc::DoDie_StExt(oCNpc* atk)
	{
		if (this->attribute[0] > 0)
		{
			DEBUG_MSG("DoDie_StExt: target not dead... yet?!");
			return;
		}

		if (atk && atk->IsSelfPlayer())
		{
			oCNpc* trueAtk = Null;
			oCNpcEx* npcEx = dynamic_cast<oCNpcEx*>(this);
			int trueAtkPtr = 0;
			if (npcEx && GetNpcExtensionVar(npcEx->m_pVARS[StExt_AiVar_Uid], StExt_AiVar_LastAtkPtr, trueAtkPtr))
			{
				trueAtk = (oCNpc*)trueAtkPtr;
				if (IsNpcPointerValid(trueAtk)) atk = trueAtk;
			}
		}
		THISCALL(Hook_oCNpc_DoDie)(atk);
	}
}