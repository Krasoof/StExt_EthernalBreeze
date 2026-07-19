#include <StonedExtension.h>

namespace Gothic_II_Addon
{
	const zCOLOR ItemRankColor[] =
	{
		zCOLOR(235, 235, 235, 255),		// rank 0
		zCOLOR(0, 128, 255, 255),		// rank 1
		zCOLOR(128, 255, 0, 255),		// rank 2
		zCOLOR(255, 128, 64, 255),		// rank 3
		zCOLOR(255, 128, 255, 255),		// rank 4
		zCOLOR(128, 0, 255, 255),		// rank >=5
	};

	const zCOLOR ItemTextColor_Default = zCOLOR(235, 235, 235, 255);
	const zCOLOR ItemTextColor_Default_Faded = zCOLOR(200, 200, 200, 255);
	const zCOLOR ItemTextColor_Gold = zCOLOR(235, 235, 0, 255);
	const zCOLOR ItemTextColor_Orange = zCOLOR(235, 180, 235, 255);
	const zCOLOR ItemTextColor_Red = zCOLOR(235, 0, 0, 255);
	const zCOLOR ItemTextColor_Green = zCOLOR(0, 220, 0, 255);
	const zCOLOR ItemTextColor_Blue = zCOLOR(0, 125, 235, 255);
	const zCOLOR ItemTextColor_Violet = zCOLOR(235, 0, 235, 255);
	const zCOLOR ItemTextColor_Pink = zCOLOR(235, 120, 235, 255);


	ItemInfoLine::ItemInfoLine(const zSTRING& txt, const zCOLOR& color, const UiContentAlignEnum alignMode)
	{
		Text = txt;
		TextColor = color;
		TextAllignMode = alignMode;
	}

	ItemInfoPanel::ItemInfoPanel()
	{
		Parent = Null;
		DisplayItem = Null;
		Content = Array<ItemInfoLine>();
	}

	void ItemInfoPanel::Init()
	{
		Parent = Null;
		DisplayItem = Null;
		Content = Array<ItemInfoLine>();
		ContentWidthIndex = 0;
		ContentHeightIndex = 0;
		BaseUiElement::Init();
		IsHiden = true;
	}


	inline void ItemInfoPanel::CreateContentLine(const zSTRING& txt, const  zCOLOR& color, const UiContentAlignEnum alignMode)	{ Content.InsertEnd(ItemInfoLine(txt, color, alignMode)); }
	inline void ItemInfoPanel::CreateContentSeparatorLine() { Content.InsertEnd(ItemInfoLine("", TextColor_Regular_Default)); }
	inline void ItemInfoPanel::CreateContentStatLine(const int statId, const int statValue, const int statDuration, const zCOLOR& color)
	{
		const ExtraStatData* statData = GetExtraStatDataById(statId);
		if (!statData) return;

		zSTRING valueStr = "";
		const UiValueDisplayType displayType = (UiValueDisplayType)statData->ValueType;
		switch (displayType)
		{
			case UiValueDisplayType::Bool: ConvertValueToYesNo(valueStr, statValue); break;
			case UiValueDisplayType::Permille: ConvertValueToPermille(valueStr, statValue); break;
			case UiValueDisplayType::Percent: valueStr = zSTRING(statValue) + "%"; break;
			case UiValueDisplayType::DeciPercent: valueStr = zSTRING(statValue * 10) + "%"; break;
			case UiValueDisplayType::Default:
			default: valueStr = zSTRING(statValue); break;
		}

		zSTRING result = GetExtraStatNameById(statId) + " " + valueStr;
		if (statDuration > 0)
			result += " | " + zSTRING(statDuration) + SecondsSuffixString;
		CreateContentLine(result, color);
	}
	inline void ItemInfoPanel::CreateContentAbilityLine(const int abilityId, const int abilityValue, const int abilityDuration, const int abilityChance, const zCOLOR& color)
	{
		const ItemAbility* abilityData = GetItemAbility(abilityId);
		if (!abilityData) return;

		//ToDo: compleate ability string
		CreateContentLine(abilityData->ScriptInstance.Name, color);
	}


	inline int ItemInfoPanel::GetArtifactType()
	{
		if (!DisplayItem) return Invalid;
		zSTRING name = GetItemInstanceName(DisplayItem);
		if (name.CompareI("itut_stext_magicbook")) return 1;
		if (name.CompareI("itut_stext_magicdagger")) return 2;		
		return Invalid;
	}

	inline const zCOLOR& ItemInfoPanel::GetRankColor(const int rank)
	{
		if (rank < 0) return ItemRankColor[0];
		else if (rank >= 5) return ItemRankColor[5];
		else return ItemRankColor[rank];
	}

	inline const zSTRING& ItemInfoPanel::GetItemRankString(const int rank)
	{
		if (IsIndexInBounds(rank, ItemsGeneratorConfigs.ItemRankMax))
			return parser->GetSymbol("StExt_Str_ItemRank")->stringdata[rank];
		return zString_Unknown;
	}

	inline const zSTRING& ReadItemClassString(Map<byte, zSTRING>& nameMap, const byte index)
	{
		const auto pair = nameMap.GetSafePair(index);
		return pair ? pair->GetValue() : zString_Unknown;
	}

	inline zSTRING ItemInfoPanel::GetItemFullClassString(const ItemExtension* itemExtension)
	{
		byte itemType;
		byte itemClass;
		byte itemSubClass;

		if (itemExtension)
		{
			itemType = itemExtension->Type;
			itemClass = itemExtension->Class;
			itemSubClass = itemExtension->SubClass;
		}
		else
		{
			const ItemClassKey itemClassKey = GetItemClassKey(DisplayItem);
			ItemClassKey_Unpack(itemClassKey, itemType, itemClass, itemSubClass);
		}

		switch (static_cast<ItemType>(itemType))
		{
			case ItemType::Armor:
			case ItemType::Weapon: 
				return Z(ReadItemClassString(ItemClassesText, itemClass) + ", " + ReadItemClassString(ItemSubClassesText, itemSubClass));
				break;

			case ItemType::Jewelry:
			case ItemType::Consumable: 
				return Z(ReadItemClassString(ItemTypesText, itemType) + ", " + ReadItemClassString(ItemClassesText, itemClass));
				break;

			case ItemType::Munition:
			case ItemType::Inventory:
			case ItemType::Craft: 
			case ItemType::Other: 
				return Z(ReadItemClassString(ItemTypesText, itemType)); 
				break;

			case ItemType::Unknown:
			default: 
				return zString_Unknown; 
				break;
		}
		return zString_Unknown;
	}

	inline zSTRING ItemInfoPanel::GetExtraDamageTypeString(const int damageTypes)
	{
		zCPar_Symbol* namesArray = parser->GetSymbol("StExt_Str_ExtraDamageType");
		zSTRING result;
		for (uint i = 0; i < namesArray->ele; ++i)
		{
			const int flag = 1 << i;
			if (HasFlag(damageTypes, flag))
				AppendTag(result, namesArray->stringdata[i]);			
		}
		return result;
	}

	inline zSTRING ItemInfoPanel::GetExtraDamageFlagString(const int damageFlags)
	{
		zCPar_Symbol* namesArray = parser->GetSymbol("StExt_Str_ExtraDamageFlags");
		zSTRING result;
		for (uint i = 0; i < namesArray->ele; ++i)
		{
			const int flag = 1 << i;
			if (HasFlag(damageFlags, flag))
				AppendTag(result, namesArray->stringdata[i]);
		}
		return result;
	}


	inline void ItemInfoPanel::BuildItemNameDescription(const ItemExtension* itemExtension)
	{
		if (!DisplayItem) return;

#if DebugEnabled && DebugItemInstanceNameEnabled
		if (itemExtension && parser->GetSymbol("StExt_Config_DebugEnabled")->single_intdata)
			CreateContentLine("[" + itemExtension->InstanceName + "]");
#endif

		if (!itemExtension)
		{
			CreateContentLine(DisplayItem->name);
			return;
		}

		const zCOLOR& nameColor = GetRankColor(itemExtension->Rank);
		if (!itemExtension->OwnName.IsEmpty())
			CreateContentLine(itemExtension->OwnName, nameColor);
		CreateContentLine(DisplayItem->name, nameColor);
		CreateContentLine("[" + GetItemRankString(itemExtension->Rank) + "]", nameColor);
	}

	inline void ItemInfoPanel::BuildItemPropertiesDescription(const ItemExtension* itemExtension)
	{
		CreateContentLine(GetItemFullClassString(itemExtension), ItemTextColor_Default_Faded);
		CreateContentLine(parser->GetSymbol("StExt_Str_ItemLevel")->stringdata + Z(itemExtension->Level), ItemTextColor_Default_Faded);
		CreateContentLine(parser->GetSymbol("StExt_Str_ItemQuality")->stringdata + Z(itemExtension->Quality) + "%", ItemTextColor_Default_Faded);
		
		ItemExtension* itemExtensionUnsafe = const_cast<ItemExtension*>(itemExtension);
		const int maxSockets = itemExtensionUnsafe->GetProperty((int)ItemProperty::SocketsMax);
		const int usedSockets = itemExtensionUnsafe->GetProperty((int)ItemProperty::SocketsUsed);
		if (maxSockets > 0)
			CreateContentLine(parser->GetSymbol("StExt_Str_ItemSockets")->stringdata + Z(usedSockets) + "/" + Z(maxSockets), ItemTextColor_Default_Faded);

		// --- Legendary catalog bonus (prop 28): visible on the item card so
		// the rolled power is inspectable and clearly saved with the item ---
		{
			const int lgb = itemExtensionUnsafe->GetProperty(28);
			const char* lgbName = Null;
			switch (lgb)
			{
				case 1:  lgbName = "Krwawy Osad: +25% obrazen ponizej 30% HP wroga"; break;
				case 2:  lgbName = "Egzekutor: dobija wrogow ponizej 10% HP"; break;
				case 3:  lgbName = "Kiel Rozkladu: zatruwa trafionych"; break;
				case 4:  lgbName = "Ostrze Rzeznika: rozcina - krwawienie"; break;
				case 5:  lgbName = "Piesc Tytana: +10% obrazen"; break;
				case 6:  lgbName = "Glod Ostrza: 3% obrazen jako zycie"; break;
				case 21: lgbName = "Zelazna Wola: -10% obrazen wrecz"; break;
				case 22: lgbName = "Tarcza Ducha: -15% obrazen od magii"; break;
				case 23: lgbName = "Plaszcz Cierni: odbija 10% obrazen"; break;
				case 41: lgbName = "Piesn Krwi: 3% obrazen jako zycie"; break;
				case 42: lgbName = "Kolia Egzekutora: +25% obrazen ponizej 25% HP wroga"; break;
				case 43: lgbName = "Pierscien Cierni: odbija 15% obrazen wrecz"; break;
				case 44: lgbName = "Sygnet Hazardzisty: losowo do +100% obrazen"; break;
				case 45: lgbName = "Pierscien Pijawki: 2% obrazen jako mana"; break;
				case 46: lgbName = "Wezel Czasu: 10% szansy na ogluszenie"; break;
				case 47: lgbName = "Pas Berserkera: +20% obrazen ponizej 30% HP"; break;
				case 48: lgbName = "Zelazna Skora: -10% obrazen wrecz"; break;
			}
			if (lgbName)
			{
				CreateContentSeparatorLine();
				CreateContentLine(zSTRING("Moc legendarna - ") + lgbName, GetRankColor(5));
			}
		}

		// --- StExt weapon element (perk) + seal: names + COMPUTED damage ---
		// WEAPONS ONLY (mainflag 2|4): jewelry/armor also carry the old perk
		// prop from the retired aura system, and rendering weapon-style
		// "+X obrazen/cios / Atak H" on an amulet was pure nonsense.
		if (DisplayItem && HasFlag(DisplayItem->mainflag, 2 | 4))
		{
			static const int ElFlag[8]   = { 16, 32, 64, 128, 256, 512, 1024, 2048 };
			static const char* ElName[8] = { "Ogien", "Lod", "Blyskawice", "Wiatr", "Ziemia", "Swiatlo", "Mrok", "Smierc" };

			const int perkSpell = itemExtensionUnsafe->GetProperty((int)ItemProperty::SpellId);
			const int perkPower = itemExtensionUnsafe->GetProperty((int)ItemProperty::SpellPower);
			const int sealSpell = itemExtensionUnsafe->GetProperty((int)ItemProperty::SpellCost);
			const int sealPower = itemExtensionUnsafe->GetProperty((int)ItemProperty::SpellCharges);
			const int sealLevel = itemExtensionUnsafe->GetProperty(26);

			zCPar_Symbol* flagsSym = parser->GetSymbol("StExt_SpellDamageFlags");

			int perkEl = -1, sealEl = -1;
			if (flagsSym)
			{
				if (perkSpell > 0 && perkSpell < flagsSym->ele)
					for (int e = 0; e < 8; e++) if (HasFlag(flagsSym->intdata[perkSpell], ElFlag[e])) { perkEl = e; break; }
				if (sealSpell > 0 && sealSpell < flagsSym->ele)
					for (int e = 0; e < 8; e++) if (HasFlag(flagsSym->intdata[sealSpell], ElFlag[e])) { sealEl = e; break; }
			}

			if (perkSpell > 0 || sealSpell > 0)
				CreateContentSeparatorLine();

			// Tooltip liczy DOKLADNIE te funkcje skryptowe co runtime
			// (StExt_Tooltip_* -> wspolne rdzenie w ItemAbilitiesController.d).
			// Koniec z recznymi lustrami wzorow w C++ - rozjezdzaly sie z gra
			// ("dziwne liczby na broni": inne skalowanie masterii, inny interwal).
			static const int fElementPerHit  = parser->GetIndex("StExt_Tooltip_ElementPerHit");
			static const int fBurstPower     = parser->GetIndex("StExt_Tooltip_BurstPower");
			static const int fSealInterval   = parser->GetIndex("StExt_Tooltip_SealProcInterval");
			static const int fSealProcDamage = parser->GetIndex("StExt_Tooltip_SealProcDamageL");	// wariant z poziomem pieczeci (+0.5%/lvl)
			static const int fBleedTick      = parser->GetIndex("StExt_Tooltip_BleedTick");
			static const int fPiercePermille = parser->GetIndex("StExt_Tooltip_PiercePermille");
			const bool tooltipFuncsOk = (fElementPerHit != Invalid) && (fBurstPower != Invalid) && (fSealInterval != Invalid)
				&& (fSealProcDamage != Invalid) && (fBleedTick != Invalid) && (fPiercePermille != Invalid);

			const int weaponDmg = DisplayItem ? DisplayItem->damageTotal : 0;
			const int weaponFlags = DisplayItem ? DisplayItem->flags : 0;
			// przyblizenie WeaponSkillUsesMana dla focusowanego itemu (miecz
			// magiczny/kostur nosi spell) - runtime liczy to doklaniej z instancji
			const int usesMana = (DisplayItem && DisplayItem->spell > 0) ? 1 : 0;

			// Realny cios per postac + rozbicie na kanaly (sieczne/obuchowe/...)
			// - te same skrypty co runtime (StExt_Tooltip_RealHit), proporcje
			// kanalow wg damage[] broni. Szacunek: bez crita i pancerza celu.
			static const int fRealHit = parser->GetIndex("StExt_Tooltip_RealHit");
			if (fRealHit != Invalid && DisplayItem && DisplayItem->damageTotal > 0)
			{
				const int realHit = *(int*)parser->CallFunc(fRealHit, weaponDmg, weaponFlags, usesMana);
				CreateContentSeparatorLine();
				CreateContentLine(zSTRING("Realny cios: ~") + Z(realHit) + " (twoja postac, przed pancerzem)", ItemTextColor_Green);
				static const char* DmgName[8] = { "Bariera", "Obuchowe", "Sieczne", "Ogien", "Podmuch", "Magia", "Klute", "Upadek" };
				zSTRING split = "";
				for (int di = 0; di < 8; ++di)
				{
					if (DisplayItem->damage[di] <= 0) continue;
					const int part = (int)((__int64)realHit * DisplayItem->damage[di] / DisplayItem->damageTotal);
					if (!split.IsEmpty()) split += "  |  ";
					split += zSTRING(DmgName[di]) + " ~" + Z(part);
				}
				if (!split.IsEmpty()) CreateContentLine(zSTRING("  w tym: ") + split, ItemTextColor_Green);
			}

			if (perkSpell > 0 && perkEl >= 0 && tooltipFuncsOk)
			{
				const int flat  = *(int*)parser->CallFunc(fElementPerHit, perkSpell, perkPower, weaponDmg, usesMana);
				const int burst = *(int*)parser->CallFunc(fBurstPower, perkSpell, perkPower, weaponDmg, weaponFlags, usesMana);
				CreateContentLine(zSTRING("Zywiol: ") + ElName[perkEl], ItemTextColor_Green);
				CreateContentLine(zSTRING("  +") + Z(flat) + " obrazen/cios  |  Atak H: " + Z(burst) + " obrazen", ItemTextColor_Green);
			}

			if (sealSpell >= 9000)
			{
				if (sealSpell == 9001 && tooltipFuncsOk)
				{
					const int dot = *(int*)parser->CallFunc(fBleedTick, sealPower, sealLevel);
					CreateContentLine(zSTRING("Pieczec: Krwawienie [poz. ") + Z(sealLevel) + "]  |  +" + Z(dot) + " obrazen/s + % maxHP celu", ItemTextColor_Green);
				}
				else if (sealSpell == 9002 && tooltipFuncsOk)
				{
					const int permille = *(int*)parser->CallFunc(fPiercePermille, sealPower, sealLevel);
					CreateContentLine(zSTRING("Pieczec: Przebicie [poz. ") + Z(sealLevel) + "]  |  " + Z(permille / 10) + "% ciosu ignoruje pancerz", ItemTextColor_Green);
				}
			}
			else if (sealSpell > 0 && sealEl >= 0 && tooltipFuncsOk)
			{
				const int interval = *(int*)parser->CallFunc(fSealInterval, sealSpell, sealPower);
				const int procDmg  = *(int*)parser->CallFunc(fSealProcDamage, sealSpell, sealPower, sealLevel, weaponDmg, weaponFlags, usesMana);
				CreateContentLine(zSTRING("Pieczec: ") + ElName[sealEl] + " [poz. " + Z(sealLevel) + "]", ItemTextColor_Green);
				CreateContentLine(zSTRING("  czar co ") + Z(interval) + " cios(y)  |  ~" + Z(procDmg) + " obrazen", ItemTextColor_Green);
			}
		}

		// ToDo: organize other options...

		// Special damage: WEAPONS only; special protection: ARMOR only.
		// Jewelry used to render both green lines ("Specjalna Ochrona: Mrok"
		// on a belt...) - legacy noise, gone per user call.
		if ((itemExtension->SpecialDamageMin + itemExtension->SpecialDamageMax) > 0
			&& DisplayItem && HasFlag(DisplayItem->mainflag, 2 | 4))
		{
			CreateContentSeparatorLine();
			CreateContentLine(parser->GetSymbol("StExt_Str_ItemSpecialDamage")->stringdata +
				Z(itemExtension->SpecialDamageMin) + " - " + Z(itemExtension->SpecialDamageMax) + " | " + GetExtraDamageTypeString(itemExtension->SpecialDamageTypes), ItemTextColor_Green);
		}

		if (itemExtension->SpecialProtection > 0
			&& DisplayItem && HasFlag(DisplayItem->mainflag, 64))
		{
			CreateContentSeparatorLine();
			CreateContentLine(parser->GetSymbol("StExt_Str_ItemSpecialProtection")->stringdata +
				Z(itemExtension->SpecialProtection) + " | " + GetExtraDamageTypeString(itemExtension->SpecialProtectionTypes), ItemTextColor_Green);
		}

		// ToDo: organize craft data info
		if (itemExtensionUnsafe->GetTag((int)ItemTags::Infused) > 0)
		{
			CreateContentSeparatorLine();

		}
	}


	void ItemInfoPanel::BuildArtifactDescription(const int mode)
	{
		BuildItemNameDescription(Null);
		CreateContentLine(GetItemFullClassString(Null), ItemTextColor_Default_Faded);
		CreateContentSeparatorLine();
		
		int expNow, expNext;
		int level;
		int lp;
		zCPar_Symbol* statsArray = Null;

		// Grimoir
		if (mode == 1) 
		{
			expNow = parser->GetSymbol("StExt_Grimoir_ExpNow")->single_intdata;
			expNext = parser->GetSymbol("StExt_Grimoir_ExpNext")->single_intdata;
			level = parser->GetSymbol("StExt_Grimoir_Level")->single_intdata;
			lp = parser->GetSymbol("StExt_Grimoir_Lp")->single_intdata;
			statsArray = parser->GetSymbol("StExt_PcStats_Grimoir");
		}
		// Dagger
		else if (mode == 2)         
		{
			expNow = parser->GetSymbol("StExt_Dagger_ExpNow")->single_intdata;
			expNext = parser->GetSymbol("StExt_Dagger_ExpNext")->single_intdata;
			level = parser->GetSymbol("StExt_Dagger_Level")->single_intdata;
			lp = parser->GetSymbol("StExt_Dagger_Lp")->single_intdata;
			statsArray = parser->GetSymbol("StExt_PcStats_Dagger");
		}
		else
		{
			DEBUG_MSG("BuildArtifactDescription - invalid mode: " + Z(mode) + "!");
			return;
		}

		CreateContentLine(parser->GetSymbol("StExt_Str_Level")->stringdata + Z(level), TextColor_Regular_Faded);
		CreateContentLine(parser->GetSymbol("StExt_Str_Exp")->stringdata + Z(expNow) + "/" + Z(expNext), TextColor_Regular_Faded);
		CreateContentLine(parser->GetSymbol("StExt_Str_Lp")->stringdata + Z(lp), TextColor_Regular_Faded);
		CreateContentSeparatorLine();

		for (int i = 0; i < MaxStatId; ++i)
		{
			if (statsArray->intdata[i] == 0) continue;
			CreateContentStatLine(i, statsArray->intdata[i], Invalid, ItemTextColor_Blue);
		}
	}

	void ItemInfoPanel::BuildSpellDescription(const ItemExtension* itemExtension)
	{
		BuildItemNameDescription(itemExtension);
		CreateContentLine(GetItemFullClassString(Null), ItemTextColor_Default_Faded);
		CreateContentSeparatorLine();

		const int damageTypes = *(int*)parser->CallFunc(GetSpellDamageFlagsFunc, DisplayItem->spell);
		const int damageFlags = *(int*)parser->CallFunc(GetSpellEffectFlagsFunc, DisplayItem->spell);
		zSTRING damageTypeStr = damageTypes != Invalid ? GetExtraDamageTypeString(damageTypes) : zString_Unknown;
		zSTRING damageFlagsStr = damageFlags != Invalid ? GetExtraDamageTypeString(damageFlags) : zString_Unknown;

		CreateContentLine(parser->GetSymbol("StExt_Str_ExtraDamageType_String")->stringdata);
		CreateContentLine(damageTypeStr);
		CreateContentSeparatorLine();
		CreateContentLine(parser->GetSymbol("StExt_Str_ExtraDamageFlags_String")->stringdata);
		CreateContentLine(damageFlagsStr);
	}

	void ItemInfoPanel::BuildExtensionDescription(const ItemExtension* itemExtension)
	{
		BuildItemNameDescription(itemExtension);
		CreateContentSeparatorLine();
		BuildItemPropertiesDescription(itemExtension);
		CreateContentSeparatorLine();

		ItemExtension* itemExtensionUnsafe = const_cast<ItemExtension*>(itemExtension);
		if (itemExtensionUnsafe->GetTag((int)ItemTags::Unidentified))
		{
			CreateContentLine(parser->GetSymbol("StExt_Str_Undefined")->stringdata, ItemTextColor_Red);
			return;
		}

		// ToDo: write infusions and attached souls
		bool hasOwnStatsPropsSeparator = false;
		if (!itemExtension->AttachedSoulName.IsEmpty())
		{

			hasOwnStatsPropsSeparator = true;
		}
		if (itemExtensionUnsafe->GetTag((int)ItemTags::Infused) > 0)
		{
			
			hasOwnStatsPropsSeparator = true;
		}		
		if (hasOwnStatsPropsSeparator) CreateContentSeparatorLine();		

		hasOwnStatsPropsSeparator = false;
		for (int i = 0; i < ItemExtension_OwnStats_Max; ++i)
		{
			if (!IsIndexInBounds(itemExtension->OwnStatId[i], MaxStatId)) continue;
			if (!hasOwnStatsPropsSeparator) hasOwnStatsPropsSeparator = true;
			CreateContentStatLine(itemExtension->OwnStatId[i], itemExtension->OwnStatValue[i], Invalid, ItemTextColor_Gold);
		}
		if (hasOwnStatsPropsSeparator) CreateContentSeparatorLine();

		for (int i = 0; i < ItemExtension_Stats_Max; ++i)
		{
			if (!IsIndexInBounds(itemExtension->StatId[i], MaxStatId)) continue;
			CreateContentStatLine(itemExtension->StatId[i], itemExtension->StatValue[i], itemExtension->StatDuration[i], ItemTextColor_Blue);
		}
	}


	void ItemInfoPanel::BuildItemDescription()
	{
		Content.Clear();
		ContentWidthIndex = 0;
		ContentHeightIndex = 0;

		if (!DisplayItem) return;		
		const ItemExtension* itemExtension = GetItemExtension(DisplayItem);
		const int artifactMode = GetArtifactType();

		if (artifactMode == 1 || artifactMode == 2)
			BuildArtifactDescription(artifactMode);
		else if (HasFlag(DisplayItem->mainflag, item_kat_rune))
			BuildSpellDescription(itemExtension);
		else if (itemExtension)
		{
			BuildExtensionDescription(itemExtension);
			UpdateItemDescriptionText(const_cast<oCItem*>(DisplayItem), itemExtension);
		}
		else return;

		for (uint i = 0; i < Content.GetNum(); ++i)
		{
			const int width = screen->FontSize(Content[i].Text);
			if (width > ContentWidthIndex) ContentWidthIndex = width;
			++ContentHeightIndex;
		}
		Resize();
	}

	void ItemInfoPanel::SetDisplayItem(const oCItem* item)
	{
		ItemSwitched = DisplayItem != item || !item;
		DisplayItem = item;
	}


	void ItemInfoPanel::Resize()
	{
		PosX = ExtraItemInfoPanel_PosX;
		PosY = ExtraItemInfoPanel_PosY;
		SizeX = SizeY = 0.001f;

		LocalPosX = static_cast<int>(PosX * ScreenVBufferSize);
		LocalPosY = static_cast<int>(PosY * ScreenVBufferSize);
		LocalSizeX = static_cast<int>(SizeX * ScreenVBufferSize);
		LocalSizeY = static_cast<int>(SizeY * ScreenVBufferSize);

		if (View)
		{
			const int contentHeightFix = Content.IsEmpty() ? 1 : 2;
			LocalSizeY = screen->FontY() * (ContentHeightIndex + contentHeightFix);
			SizeY = static_cast<float>(LocalSizeY * ScreenToRelativePixDelta);

			const float contentWidthFix = Content.IsEmpty() ? 0.00f : 0.02f;
			LocalSizeX = ContentWidthIndex + static_cast<int>(contentWidthFix * ScreenVBufferSize);
			SizeX = static_cast<float>(LocalSizeX * ScreenToRelativePixDelta);

			View->SetSize(LocalSizeX, LocalSizeY);
			View->SetPos(LocalPosX, LocalPosY);
		}

		GlobalPosX = LocalPosX;
		GlobalPosY = LocalPosY;
		GlobalSizeX = LocalSizeX;
		GlobalSizeY = LocalSizeY;
	}

	void ItemInfoPanel::Draw()
	{
		if (!screen) return;
		if (!View)
		{
			View = zNEW(zCView)(0, 0, ScreenVBufferSize, ScreenVBufferSize);			
			View->InsertBack(BgTexture);
			View->SetAlphaBlendFunc(zRND_ALPHA_FUNC_BLEND);
			View->SetTransparency(240);
			Resize();
		}

		View->ClrPrintwin();
		if (!IsVisible || IsHiden)
		{
			if(screen->childs.IsIn(View))
				screen->RemoveItem(View);
			return;
		}

		const uint contentSize = Content.GetNum();
		const int yOffset = View->FontY();
		const int startX = static_cast<int>(ScreenVBufferSize * 0.01f);
		int y = yOffset, x = 0;
		for (uint i = 0; i < Content.GetNum(); ++i)
		{
			auto& contentLine = Content[i];
			if (contentLine.Text.IsEmpty())
			{
				y += yOffset;
				continue;
			}

			const int textWidth = View->FontSize(contentLine.Text);
			View->SetFontColor(contentLine.TextColor);

			if (contentLine.TextAllignMode == UiContentAlignEnum::Begin) x = startX;
			else if (contentLine.TextAllignMode == UiContentAlignEnum::End) x = ScreenVBufferSize - textWidth;
			else x = static_cast<int>(ScreenHalfVBufferSize - (textWidth * 0.5f));

			View->Print(x, y, contentLine.Text);
			y += yOffset;
		}

		if (!screen->childs.IsIn(View)) 
			screen->InsertItem(View);
		if (OnDraw) OnDraw(this);
	}

	void ItemInfoPanel::Update()
	{
		if (ItemSwitched)
		{
			BuildItemDescription();
			ItemSwitched = false;
		}
		BaseUiElement::Update();
		IsHiden = !DisplayItem || Content.IsEmpty();
	}

	ItemInfoPanel::~ItemInfoPanel() 
	{
		Content.Clear();
		DisplayItem = Null; 
		BaseUiElement::~BaseUiElement();
	}
}