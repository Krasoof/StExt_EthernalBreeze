#include <StonedExtension.h>

namespace Gothic_II_Addon
{
	StatsWindow* Instance = Null;

	const int StatsSectionOrderMax = 7;
	const int StatsSectionOrder[] =
	{
		StatGroup_Characteristics,
		StatGroup_Attack,
		StatGroup_Protection,
		StatGroup_Skills,
		StatGroup_Auras,
		StatGroup_Summons,
		StatGroup_Other
	};

	struct SkillPanelArgs
	{
		int SkillId;
		int MasteryId;
		int MasteryTreeIndex;
		float PosY;
		bool IsCorruptionMastery;
		bool IsGenericMastery;
		zSTRING NamePostfix;
		zSTRING TitleSymbol;
		zSTRING DescSymbol;
		zSTRING ValueSymbol;
	};

	struct SkillLearnArgs
	{
		int SkillId;
		int MasteryTreeIndex;
	};

	struct SkillHeaderPanelArgs
	{
		int MasteryId;
		float PosY;
		bool IsCorruptionMastery;
		bool IsCorruptionTouchMastery;
		zSTRING NamePostfix;
		zSTRING TitleSymbol;
		zSTRING DescSymbol;
		zSTRING LevelSymbol;
		zSTRING RankSymbol;
		zSTRING ExpSymbol;
		zSTRING NextExpSymbol;
		zSTRING LpSymbol;
	};

	struct BonusPanelSectionArgs
	{
		uint Id;
		uint NameIndex;
		bool HasDuration;
		int ValueSymArrayIndex;
		int TimeSymArrayIndex;
	};

	//----------------------------------------------------------------------
	//								HANDLERS
	//----------------------------------------------------------------------

	bool StatsWindow_OnTabButtonClick(BaseMenuElement* item, const UiMouseEventArgs& args)
	{
		if (!item || !Instance) return false;
		if (args.Action != UiMouseEnum::LeftClick) return false;

		MenuItem* itm = static_cast<MenuItem*>(item);
		if (itm)
		{
			int index = *itm->GetData<int>();
			if (index < 0) index = 0;
			if (index >= Instance->TabIndexMax) index = Instance->TabIndexMax - 1;
			Instance->TabIndex = index;
		}
		return true;
	}

	bool StatsWindow_OnMasteryItemClick(BaseMenuElement* item, const UiMouseEventArgs& args)
	{
		if (!item || !Instance) return false;
		if (args.Action != UiMouseEnum::LeftClick) return false;

		MenuItem* itm = static_cast<MenuItem*>(item);
		if (itm)
		{
			int index = *itm->GetData<int>();
			if (index < 0) index = 0;
			if (index >= Instance->MasteryIndexMax) index = Instance->MasteryIndexMax - 1;
			Instance->MasteryIndex = index;
		}
		return true;
	}

	void StatsWindow_OnEsPanelUpdate(BaseUiElement* panel)
	{
		if (!panel || !Instance) return;

		MenuItem* itm = static_cast<MenuItem*>(panel);
		if (itm)
		{
			oCNpcEx* heroEx = dynamic_cast<oCNpcEx*>(player);
			if (heroEx)
			{
				int esCur; GetNpcExtensionVar(heroEx->m_pVARS[StExt_AiVar_Uid], StExt_AiVar_EsCur, esCur);
				int esMax; GetNpcExtensionVar(heroEx->m_pVARS[StExt_AiVar_Uid], StExt_AiVar_EsMax, esMax);
				itm->Text = Z(esCur) + "/" + Z(esMax);
			}
			else itm->Text = "???/???";
		}
	}

	void StatsWindow_OnCorruptionPanelUpdate(BaseUiElement* panel)
	{
		if (!panel || !Instance) return;

		int corruptionTypeTmp = parser->GetSymbol("StExt_CorruptionPath")->single_intdata;
		const int corruptionTypeMax = parser->GetSymbol("StExt_Corruption_Max")->single_intdata;
		const int corruptionType = (corruptionTypeTmp >= corruptionTypeMax) || (corruptionTypeTmp < 0) ? 0 : corruptionTypeTmp;

		BaseMenuPanel* itm = static_cast<BaseMenuPanel*>(panel);
		if (itm)
		{
			MenuItem* titleItem = Null;
			MenuTextItem* titleDescItem = Null;
			for (auto childItm : itm->Items)
			{
				if (!titleItem && childItm->Name == "GenericCorruptionPanel_Title")
					titleItem = static_cast<MenuItem*>(childItm);

				if (!titleDescItem && childItm->Name == "GenericCorruptionPanel_TitleDescription")
					titleDescItem = static_cast<MenuTextItem*>(childItm);

				if (titleItem && titleDescItem) break;
			}

			if (titleItem)
				titleItem->Text = parser->GetSymbol("StExt_Str_CorruptionName")->stringdata[corruptionType];
			if (titleDescItem)
				titleDescItem->Text = parser->GetSymbol("StExt_Str_CorruptionDesc")->stringdata[corruptionType];
		}
	}


	inline void StatsWindow_OnBonusPanelUpdate_BuildSectionContent(MenuScrollPanel* panel, const BonusPanelSectionArgs& args, float& yOffset)
	{
		static const int statsMax = parser->GetSymbol("StExt_PcStats_Index_Max")->single_intdata;
		static const zSTRING secStr = parser->GetSymbol("StExt_Str_Seconds")->stringdata;
		static const int statDescSymArrayIndex = parser->GetIndex("StExt_PcStats_Desc");
		static const int noContentPlaceholderSymIndex = parser->GetIndex("StExt_Str_No");

		uint elementsCount = 0;
		zCPar_Symbol* descArray = parser->GetSymbol(statDescSymArrayIndex);
		zCPar_Symbol* valueArray = (args.ValueSymArrayIndex != Invalid) ? parser->GetSymbol(args.ValueSymArrayIndex) : Null;
		zCPar_Symbol* durationArray = (args.HasDuration && (args.TimeSymArrayIndex != Invalid)) ? parser->GetSymbol(args.TimeSymArrayIndex) : Null;

		if (valueArray) 
		{
			for (int i = 0; i < statsMax; ++i)
			{
				if (valueArray->intdata[i] == 0) continue;
				if (durationArray && (durationArray->intdata[i] <= 0)) continue;

				const float posY = yOffset;
				zSTRING contentText = descArray->stringdata[i] + " " + Z(valueArray->intdata[i]);
				if (durationArray) contentText += " (" + Z(durationArray->intdata[i]) + secStr + ")";

				MenuItem* contentItem = new MenuItem();
				contentItem->OnInit = [args, posY, i, contentText](BaseUiElement* item)
				{
					item->Name = "BonusTabPanel_SectionContent_" + Z((int)args.Id) + "_" + Z(i);
					item->SizeX = 0.98f;
					item->SizeY = 0.03f;
					item->PosX = 0.01f;
					item->PosY = posY;

					if (auto* itm = static_cast<MenuItem*>(item))
					{
						itm->Text = contentText;
						itm->BehaviorFlags = UiElementBehaviorFlags::Hoverable | UiElementBehaviorFlags::Interactable;
						itm->HorizontalAlign = UiContentAlignEnum::Center;
						itm->VerticalAlign = UiContentAlignEnum::Center;
					}
				};

				panel->AddItem(contentItem);
				yOffset += 0.035f;
				++elementsCount;
			}
		}

		if (elementsCount == 0) 
		{
			const float posY = yOffset;
			MenuItem* contentItem = new MenuItem();
			contentItem->OnInit = [args, posY](BaseUiElement* item)
			{
				item->Name = "BonusTabPanel_SectionContent_" + Z((int)args.Id) + "_Empty";
				item->SizeX = 0.98f;
				item->SizeY = 0.03f;
				item->PosX = 0.01f;
				item->PosY = posY;

				if (auto* itm = static_cast<MenuItem*>(item))
				{
					itm->Text = parser->GetSymbol(noContentPlaceholderSymIndex)->stringdata;
					itm->BehaviorFlags = UiElementBehaviorFlags::Hoverable | UiElementBehaviorFlags::Interactable;
					itm->HorizontalAlign = UiContentAlignEnum::Center;
					itm->VerticalAlign = UiContentAlignEnum::Center;
				}
			};

			panel->AddItem(contentItem);
			yOffset += 0.035f;
		}
	}

	inline void StatsWindow_OnBonusPanelUpdate_BuildSection(MenuScrollPanel* panel, const BonusPanelSectionArgs& args, float& yOffset)
	{
		static int headerNameSymIndex = parser->GetIndex("StExt_BonusStats_Desc");

		const float posY = yOffset;
		MenuItem* titleItem = new MenuItem();
		titleItem->OnInit = [args, posY](BaseUiElement* item)
		{
			item->Name = "BonusTabPanel_SectionTitle_" + Z((int)args.Id);
			item->SizeX = 0.98f;
			item->SizeY = 0.08f;
			item->PosX = 0.01f;
			item->PosY = posY;

			if (auto* itm = static_cast<MenuItem*>(item))
			{
				itm->Text = parser->GetSymbol(headerNameSymIndex)->stringdata[args.NameIndex];
				itm->TextColor_Default = TextColor_Header_Default;
				itm->TextColor_Hovered = TextColor_Header_Hovered;
				itm->TextColor_Selected = TextColor_Header_Selected;
				itm->TextColor_Disabled = TextColor_Header_Disabled;
				itm->Font = MenuFont_Header;
				itm->BehaviorFlags = UiElementBehaviorFlags::Hoverable | UiElementBehaviorFlags::Interactable;
				itm->HorizontalAlign = UiContentAlignEnum::Center;
				itm->VerticalAlign = UiContentAlignEnum::Center;
			}
		};
		panel->AddItem(titleItem);
		yOffset += 0.09f;

		StatsWindow_OnBonusPanelUpdate_BuildSectionContent(panel, args, yOffset);
	}

	inline void StatsWindow_OnBonusPanelUpdate_InitializeSections(Array<BonusPanelSectionArgs>& sectionArgs)
	{
		sectionArgs.Clear();

		// Alchemy
		sectionArgs.Insert(BonusPanelSectionArgs { 0U, 2U, true, parser->GetIndex("StExt_PcStats_Alchemy"), parser->GetIndex("StExt_PcStats_Alchemy_Cooldown") });
		// Effects
		sectionArgs.Insert(BonusPanelSectionArgs { 1U, 3U, true, parser->GetIndex("StExt_PcStats_Buffs"), parser->GetIndex("StExt_PcStats_Buffs_Cooldown") });
		// Auras
		sectionArgs.Insert(BonusPanelSectionArgs { 2U, 4U, false, parser->GetIndex("StExt_PcStats_Auras"), Invalid });
		// Artifacts
		sectionArgs.Insert(BonusPanelSectionArgs { 3U, 1U, false, parser->GetIndex("StExt_PcStats_Grimoir"), Invalid });
		sectionArgs.Insert(BonusPanelSectionArgs { 4U, 1U, false, parser->GetIndex("StExt_PcStats_Dagger"), Invalid });
		sectionArgs.Insert(BonusPanelSectionArgs { 5U, 1U, false, Invalid, Invalid });
		// Items
		sectionArgs.Insert(BonusPanelSectionArgs { 6U, 0U, false, parser->GetIndex("StExt_PcStats_Items"), Invalid });
	}

	void StatsWindow_OnBonusPanelUpdate(BaseUiElement* panel)
	{
		if (!panel || !Instance) return;
		static int artEquippedSymIndex = parser->GetIndex("StExt_ArtifactEquipped");

		static int updateCounter = 0;
		if (++updateCounter <= 25) return;
		updateCounter = 0;

		MenuScrollPanel* container = static_cast<MenuScrollPanel*>(panel);
		if (!container) return;

		static Array<BonusPanelSectionArgs> sectionArgs;
		if (sectionArgs.IsEmpty())
			StatsWindow_OnBonusPanelUpdate_InitializeSections(sectionArgs);

		const int artType = parser->GetSymbol(artEquippedSymIndex)->single_intdata;
		const bool hasArt = (artType == StExt_ArtifactIndex_Grimoir) || (artType == StExt_ArtifactIndex_Dagger);
		float yOffset = 0.01f;
		int scrollOffset, scrollOffsetBefore;
		container->GetScrollOffset(scrollOffset, scrollOffsetBefore);

		container->ClearItems();
		const uint sectionsCount = sectionArgs.GetNum();
		for (auto& args : sectionArgs) 
		{
			if ((args.Id == 3U) && (artType != StExt_ArtifactIndex_Grimoir)) continue;
			if ((args.Id == 4U) && (artType != StExt_ArtifactIndex_Dagger)) continue;
			if ((args.Id == 5U) && hasArt) continue;

			StatsWindow_OnBonusPanelUpdate_BuildSection(container, args, yOffset);
			yOffset += 0.05f;
		}

		container->Init();
		container->SetScrollOffset(scrollOffset, Invalid);
	}


	inline bool OnLearnSkillButton_CanLearnPerk(const SkillLearnArgs& learnArgs, const ExtraMasteryData& masteryData)
	{
		if (masteryData.IsCorruption) return *(int*)parser->CallFunc(masteryData.PerkLearnCheckFuncIndex, masteryData.MasteryId, learnArgs.SkillId, True);
		else if (masteryData.IsGeneric) return *(int*)parser->CallFunc(masteryData.PerkLearnCheckFuncIndex, learnArgs.SkillId, True);
		else return *(int*)parser->CallFunc(masteryData.PerkLearnCheckFuncIndex, masteryData.MasteryId, learnArgs.SkillId);
	}

	inline bool OnLearnSkillButton_IsPerkLearned(const SkillLearnArgs& learnArgs, const ExtraMasteryData& masteryData)
	{
		if (masteryData.IsGeneric) return *(int*)parser->CallFunc(masteryData.PerkIsLearnedCheckFuncIndex, learnArgs.SkillId);
		else return *(int*)parser->CallFunc(masteryData.PerkIsLearnedCheckFuncIndex, masteryData.MasteryId, learnArgs.SkillId);
	}

	bool StatsWindow_OnLearnSkillButtonClick(BaseMenuElement* item, const UiMouseEventArgs& args)
	{
		if (!item || !Instance) return false;
		if (args.Action != UiMouseEnum::LeftClick) return false;

		MenuItem* itm = static_cast<MenuItem*>(item);
		if (itm && itm->Data)
		{
			const SkillLearnArgs& learnArgs = *(itm->GetData<SkillLearnArgs>());
			const ExtraMasteryData& masteryData = ExtraMasteriesData[learnArgs.MasteryTreeIndex];
			if (OnLearnSkillButton_CanLearnPerk(learnArgs, masteryData))
			{
				if (masteryData.IsCorruption) parser->CallFunc(masteryData.PerkLearnFuncIndex, masteryData.MasteryId, learnArgs.SkillId);
				else if (masteryData.IsGeneric) parser->CallFunc(masteryData.PerkLearnFuncIndex, learnArgs.SkillId);
				else parser->CallFunc(masteryData.PerkLearnFuncIndex, masteryData.MasteryId, learnArgs.SkillId);
			}
		}
		return true;
	}

	void StatsWindow_OnLearnSkillButtonUpdate(BaseUiElement* button)
	{
		if (!button || !Instance) return;

		static const zSTRING learnStr = parser->GetSymbol("StExt_Str_DoSkillLearn")->stringdata;
		static const zSTRING learnLpStr = parser->GetSymbol("StExt_Str_Required_Lp")->stringdata;
		static const zSTRING learnGoldStr = parser->GetSymbol("StExt_Str_Required_Gold")->stringdata;
		static const int getCorruptionPerkCostFuncIndex = parser->GetIndex("STEXT_GETCORRUPTIONPERKCOST");
		static const int getGenericPerkCostFuncIndex = parser->GetIndex("STEXT_GETGENERICPERKLPCOST");
		static const int getGenericPerkGoldCostFuncIndex = parser->GetIndex("STEXT_GETGENERICPERKCOST");

		MenuItem* itm = static_cast<MenuItem*>(button);
		if (itm && itm->Data)
		{
			const SkillLearnArgs& learnArgs = *(itm->GetData<SkillLearnArgs>());
			const ExtraMasteryData& masteryData = ExtraMasteriesData[learnArgs.MasteryTreeIndex];

			if ((masteryData.IsGeneric || masteryData.IsCorruption) && !itm->IsHiden)
			{
				const int requiredLp = masteryData.IsGeneric ?
					*(int*)parser->CallFunc(getGenericPerkCostFuncIndex, learnArgs.SkillId) :
					*(int*)parser->CallFunc(getCorruptionPerkCostFuncIndex, masteryData.MasteryId, learnArgs.SkillId);

				itm->Text = learnStr + " (" + Z(requiredLp) + learnLpStr;
				if (masteryData.IsGeneric)
				{
					const int requiredGold = *(int*)parser->CallFunc(getGenericPerkGoldCostFuncIndex, learnArgs.SkillId);
					itm->Text += ", " + Z(requiredGold) + " " + learnGoldStr;
				}
				itm->Text += ")";
			}

			itm->IsHiden = OnLearnSkillButton_IsPerkLearned(learnArgs, masteryData);
			itm->IsEnabled = OnLearnSkillButton_CanLearnPerk(learnArgs, masteryData);
		}
	}

	inline bool OnLearnCorruptionTouchStatButton_CanLearnStat(const int statId)
	{
		static const int statArrayIndex = parser->GetIndex("STEXT_PCSTATS_OTHER");
		static const int lpSymIndex = parser->GetIndex("STEXT_CORRUPTEDPERK_DAMAGEPOINTS");

		const int statMax = CorruptionTouchStatsData[statId].GetValue().StatMax;
		const int statNow = parser->GetSymbol(statArrayIndex)->intdata[statId];
		const int lpNow = parser->GetSymbol(lpSymIndex)->single_intdata;
		return (statNow < statMax) && (lpNow > 0);
	}

	inline bool OnLearnCorruptionTouchStatButton_IsStatLearned(const int statId)
	{
		static const int statArrayIndex = parser->GetIndex("STEXT_PCSTATS_OTHER");
		const int statMax = CorruptionTouchStatsData[statId].GetValue().StatMax;
		const int statNow = parser->GetSymbol(statArrayIndex)->intdata[statId];
		return statNow >= statMax;
	}

	bool CorruptionTouch_OnLearnStatButtonClick(BaseMenuElement* item, const UiMouseEventArgs& args)
	{
		static const int addFucnIndex = parser->GetIndex("STEXT_ADDCORRUPTTOUCHSTATENGINE");

		if (!item || !Instance) return false;
		if (args.Action != UiMouseEnum::LeftClick) return false;

		MenuItem* itm = static_cast<MenuItem*>(item);
		if (itm && itm->Data)
		{
			const int statId = *(itm->GetData<int>());
			if (OnLearnCorruptionTouchStatButton_CanLearnStat(statId))
			{
				const int statValue = CorruptionTouchStatsData[statId].GetValue().StatsPerPoint;
				parser->CallFunc(addFucnIndex, statId, statValue);
			}
		}
		return true;
	}

	void CorruptionTouch_OnLearnStatButtonUpdate(BaseUiElement* button)
	{
		if (!button || !Instance) return;

		MenuItem* itm = static_cast<MenuItem*>(button);
		if (itm && itm->Data)
		{
			const int statId = *(itm->GetData<int>());
			itm->IsHiden = OnLearnCorruptionTouchStatButton_IsStatLearned(statId);
			itm->IsEnabled = OnLearnCorruptionTouchStatButton_CanLearnStat(statId);
		}
	}

	bool StatsWindow_OnLearnSkillForgetButtonClick(BaseMenuElement* item, const UiMouseEventArgs& args)
	{
		if (!item || !Instance) return false;
		if (args.Action != UiMouseEnum::LeftClick) return false;

		MenuItem* itm = static_cast<MenuItem*>(item);
		if (itm && itm->Data)
		{
			const int treeIndex = *(itm->GetData<int>());
			const ExtraMasteryData& masteryData = ExtraMasteriesData[treeIndex];
			if (!masteryData.CanResetPerks) return true;

			if (masteryData.IsGeneric || masteryData.IsCorruption) parser->CallFunc(masteryData.ResetPerksFuncIndex);
			else parser->CallFunc(masteryData.ResetPerksFuncIndex, masteryData.MasteryId);
		}
		return true;
	}
	
	//----------------------------------------------------------------------
	//								TABS PANEL
	//----------------------------------------------------------------------

	void StatsWindow::InitTabs()
	{
		TabPanel = new BaseMenuPanel();
		TabPanel->OnInit = [](BaseUiElement* item) 
		{
			item->Name = "TabsPanel";
			item->SizeX = 0.98f;
			item->SizeY = 0.08f;
			item->PosX = 0.01f;
			item->PosY = 0.01f;
			item->BgTexture = UiElement_ButtonNoBorderTexture;
		};
		AddItem(TabPanel);

		zCPar_Symbol* tabNameArraySym = parser->GetSymbol("StExt_StatsMenu_TabName");
		float totalWidth = 1.0f;
		float margin = 0.025f;
		float gap = 0.01f;

		const int tabCount = tabNameArraySym->ele;
		const float availableWidth = totalWidth - (margin * 2) - (gap * (tabCount - 1));
		const float tabWidth = availableWidth / tabCount;

		TabIndexMax = tabCount;
		float x = margin;
		for (int i = 0; i < tabCount; ++i)
		{
			MenuItem* tabItem = new MenuItem();
			tabItem->OnInit = [i, x, tabWidth, margin](BaseUiElement* item)
			{
				item->Name = "Tab_" + Z(i);
				item->SizeX = tabWidth;
				item->SizeY = 1.0f - (margin * 2.0f);
				item->PosX = x;
				item->PosY = margin;
				item->BgTexture = UiElement_ButtonTexture;

				if (auto* itm = static_cast<MenuItem*>(item))
				{
					itm->IsSelected = i == 0;
					itm->IsEnabled = true;
					itm->Text = parser->GetSymbol("StExt_StatsMenu_TabName")->stringdata[i];
					itm->Font = MenuFont_Header;
					itm->TextColor_Default = TextColor_HeaderTab_Default;
					itm->TextColor_Hovered = TextColor_HeaderTab_Hovered;
					itm->TextColor_Selected = TextColor_HeaderTab_Selected;
					itm->TextColor_Disabled = TextColor_HeaderTab_Disabled;
					itm->BehaviorFlags = UiElementBehaviorFlags::Hoverable | UiElementBehaviorFlags::Selectable | UiElementBehaviorFlags::Interactable | UiElementBehaviorFlags::Checkable;
					itm->SetOwnedData<int>(i);
					itm->OnMouseEvent = StatsWindow_OnTabButtonClick;
				}
			};

			TabPanel->AddItem(tabItem);
			x += tabWidth + gap;
		}

		MenuItem* tabPanelSeparator = BuildHSeparator(0.09f);
		AddItem(tabPanelSeparator);

		TabPanel->Init();
		tabPanelSeparator->Init();
	}

	inline void BuildSkillPostfixName(zSTRING& postfix, const int masteryId, const int skillId, const bool isCorruption, const bool isGeneric, const int masteryIndex = 0) { postfix = Z((int)isCorruption) + Z((int)isGeneric) + Z(masteryIndex) + "_" + Z(masteryId) + "_" + Z(skillId); }


	//----------------------------------------------------------------------
	//								GENERAL PANEL
	//----------------------------------------------------------------------

	inline BaseMenuPanel* BuildGenericStatPanel(const int statId, const float posY, const float sizeY)
	{
		const ExtraStatData* statData = GetExtraStatDataById(statId);
		if (!statData)
		{
			DEBUG_MSG("StExt Mod Ui - Fail to build generic stats panel. StatId = " + Z(statId));
			return Null;
		}
		zSTRING statTitle = GetExtraStatNameById(statId);

		BaseMenuPanel* panel = new BaseMenuPanel();
		panel->OnInit = [statId, posY, sizeY](BaseUiElement* item)
		{
			item->Name = "GenericStatPanel_" + Z(statId);
			item->SizeX = 0.98f;
			item->SizeY = sizeY;
			item->PosX = 0.01f;
			item->PosY = posY;
			if (auto* itm = static_cast<BaseMenuPanel*>(item)) {
				itm->BehaviorFlags = UiElementBehaviorFlags::Hoverable | UiElementBehaviorFlags::Interactable;
			}
		};

		MenuItem* titleItem = new MenuItem();
		titleItem->OnInit = [statId, statTitle](BaseUiElement* item)
		{
			item->Name = "GenericStatPanel_Title_" + Z(statId);
			item->SizeX = 0.80f;
			item->SizeY = 0.95f;
			item->PosX = 0.025f;
			item->PosY = 0.025f;

			if (auto* itm = static_cast<MenuItem*>(item))
			{
				itm->Text = statTitle;
				itm->BehaviorFlags = UiElementBehaviorFlags::Hoverable | UiElementBehaviorFlags::Interactable;
				itm->HorizontalAlign = UiContentAlignEnum::Begin;
				itm->SetOwnedData<int>(statId);
			}
		};

		MenuValueItem* valueItem = new MenuValueItem();
		UiValueDisplayType displayType = (UiValueDisplayType)statData->ValueType;
		valueItem->OnInit = [statId, displayType](BaseUiElement* item)
		{
			item->Name = "GenericStatPanel_Value_" + Z(statId);
			item->SizeX = 0.15f;
			item->SizeY = 0.95f;
			item->PosX = 0.825f;
			item->PosY = 0.25f;

			if (auto* itm = static_cast<MenuValueItem*>(item))
			{
				itm->IsArray = true;
				itm->PrimaryValueArrayIndex = statId;
				itm->PrimaryValueName = "StExt_PcStats";
				itm->PrimaryValueDisplayType = displayType;
				itm->BehaviorFlags = UiElementBehaviorFlags::Hoverable | UiElementBehaviorFlags::Interactable;
				itm->SetOwnedData<int>(statId);
			}
		};

		panel->AddItem(titleItem);
		panel->AddItem(valueItem);
		return panel;
	}

	inline BaseMenuPanel* BuildGenericDamagePanel(const zSTRING damageSym, const zSTRING damageHeaderSym, const float posY, const float sizeY)
	{
		BaseMenuPanel* panel = new BaseMenuPanel();
		panel->OnInit = [damageSym, posY, sizeY](BaseUiElement* item)
		{
			item->Name = "GenericDamagePanel_" + damageSym;
			item->SizeX = 0.98f;
			item->SizeY = sizeY;
			item->PosX = 0.01f;
			item->PosY = posY;
			if (auto* itm = static_cast<BaseMenuPanel*>(item)) {
				itm->BehaviorFlags = UiElementBehaviorFlags::Hoverable | UiElementBehaviorFlags::Interactable;
			}
		};

		MenuItem* titleItem = new MenuItem();
		titleItem->OnInit = [damageHeaderSym, damageSym](BaseUiElement* item)
		{
			item->Name = "GenericDamagePanel_Title_" + damageSym;
			item->SizeX = 0.80f;
			item->SizeY = 0.95f;
			item->PosX = 0.025f;
			item->PosY = 0.025f;

			if (auto* itm = static_cast<MenuItem*>(item))
			{
				itm->Text = parser->GetSymbol(damageHeaderSym)->stringdata;
				itm->BehaviorFlags = UiElementBehaviorFlags::Hoverable | UiElementBehaviorFlags::Interactable;
				itm->HorizontalAlign = UiContentAlignEnum::Begin;
			}
		};

		MenuValueItem* valueItem = new MenuValueItem();
		valueItem->OnInit = [damageSym](BaseUiElement* item)
		{
			item->Name = "GenericDamagePanel_Value_" + damageSym;
			item->SizeX = 0.15f;
			item->SizeY = 0.95f;
			item->PosX = 0.825f;
			item->PosY = 0.25f;

			if (auto* itm = static_cast<MenuValueItem*>(item))
			{
				itm->IsApproximal = true;
				itm->PrimaryValueName = damageSym;
				itm->PrimaryValueDisplayType = UiValueDisplayType::Default;
				itm->BehaviorFlags = UiElementBehaviorFlags::Hoverable | UiElementBehaviorFlags::Interactable;
			}
		};

		panel->AddItem(titleItem);
		panel->AddItem(valueItem);
		return panel;
	}

	inline BaseMenuPanel* BuildGenericEsPanel(const float posY, const float sizeY)
	{
		BaseMenuPanel* panel = new BaseMenuPanel();
		panel->OnInit = [posY, sizeY](BaseUiElement* item)
		{
			item->Name = "GenericEsPanel";
			item->SizeX = 0.98f;
			item->SizeY = sizeY;
			item->PosX = 0.01f;
			item->PosY = posY;
			if (auto* itm = static_cast<BaseMenuPanel*>(item)) {
				itm->BehaviorFlags = UiElementBehaviorFlags::Hoverable | UiElementBehaviorFlags::Interactable;
			}
		};

		MenuItem* titleItem = new MenuItem();
		titleItem->OnInit = [](BaseUiElement* item)
		{
			item->Name = "GenericEsPanel_Title";
			item->SizeX = 0.80f;
			item->SizeY = 0.95f;
			item->PosX = 0.025f;
			item->PosY = 0.025f;

			if (auto* itm = static_cast<MenuItem*>(item))
			{
				itm->Text = parser->GetSymbol("StExt_EsText")->stringdata;
				itm->BehaviorFlags = UiElementBehaviorFlags::Hoverable | UiElementBehaviorFlags::Interactable;
				itm->HorizontalAlign = UiContentAlignEnum::Begin;
			}
		};

		MenuItem* valueItem = new MenuItem();
		valueItem->OnInit = [](BaseUiElement* item)
		{
			item->Name = "GenericEsPanel_Value";
			item->SizeX = 0.15f;
			item->SizeY = 0.95f;
			item->PosX = 0.825f;
			item->PosY = 0.25f;
			item->OnUpdate = StatsWindow_OnEsPanelUpdate;

			if (auto* itm = static_cast<MenuItem*>(item))
			{
				itm->BehaviorFlags = UiElementBehaviorFlags::Hoverable | UiElementBehaviorFlags::Interactable;
			}
		};

		panel->AddItem(titleItem);
		panel->AddItem(valueItem);
		return panel;
	}

	inline BaseMenuPanel* BuildGenericAuraPanel(const float posY, const float sizeY)
	{
		BaseMenuPanel* panel = new BaseMenuPanel();
		panel->OnInit = [posY, sizeY](BaseUiElement* item)
		{
			item->Name = "GenericAuraPanel";
			item->SizeX = 0.98f;
			item->SizeY = sizeY;
			item->PosX = 0.01f;
			item->PosY = posY;
			if (auto* itm = static_cast<BaseMenuPanel*>(item)) {
				itm->BehaviorFlags = UiElementBehaviorFlags::Hoverable | UiElementBehaviorFlags::Interactable;
			}
		};

		MenuItem* titleItem = new MenuItem();
		titleItem->OnInit = [](BaseUiElement* item)
		{
			item->Name = "GenericAuraPanel_Title";
			item->SizeX = 0.80f;
			item->SizeY = 0.95f;
			item->PosX = 0.025f;
			item->PosY = 0.025f;

			if (auto* itm = static_cast<MenuItem*>(item))
			{
				itm->Text = parser->GetSymbol("StExt_Str_Auras")->stringdata;
				itm->BehaviorFlags = UiElementBehaviorFlags::Hoverable | UiElementBehaviorFlags::Interactable;
				itm->HorizontalAlign = UiContentAlignEnum::Begin;
			}
		};

		MenuValueItem* valueItem = new MenuValueItem();
		valueItem->OnInit = [](BaseUiElement* item)
		{
			item->Name = "GenericAuraPanel_Value";
			item->SizeX = 0.15f;
			item->SizeY = 0.95f;
			item->PosX = 0.825f;
			item->PosY = 0.25f;

			if (auto* itm = static_cast<MenuValueItem*>(item))
			{
				itm->IsRanged = true;
				itm->PrimaryValueName = "StExt_HeroActiveAuras";
				itm->SecondaryValueName = "StExt_HeroActiveAurasMax";
				itm->PrimaryValueDisplayType = UiValueDisplayType::Default;
				itm->SecondaryValueDisplayType = UiValueDisplayType::Default;
				itm->BehaviorFlags = UiElementBehaviorFlags::Hoverable | UiElementBehaviorFlags::Interactable;
			}
		};

		panel->AddItem(titleItem);
		panel->AddItem(valueItem);
		return panel;
	}

	inline BaseMenuPanel* BuildGenericAuraListPanel(const float posY, const float sizeY)
	{
		BaseMenuPanel* panel = new BaseMenuPanel();
		panel->OnInit = [posY, sizeY](BaseUiElement* item)
		{
			item->Name = "GenericAuraPanel";
			item->SizeX = 0.98f;
			item->SizeY = sizeY;
			item->PosX = 0.01f;
			item->PosY = posY;
			if (auto* itm = static_cast<BaseMenuPanel*>(item)) 
			{
				itm->BehaviorFlags = UiElementBehaviorFlags::Hoverable | UiElementBehaviorFlags::Interactable;
			}
		};

		MenuValueItem* valueItem = new MenuValueItem();
		valueItem->OnInit = [](BaseUiElement* item)
		{
			item->Name = "GenericAuraPanel_Value";
			item->SizeX = 0.98f;
			item->SizeY = 0.98f;
			item->PosX = 0.01f;
			item->PosY = 0.01f;

			if (auto* itm = static_cast<MenuValueItem*>(item))
			{
				itm->PrimaryValueName = "StExt_ActiveAurasNames";
				itm->VerticalAlign = UiContentAlignEnum::Center;
				itm->HorizontalAlign = UiContentAlignEnum::Center;
				itm->BehaviorFlags = UiElementBehaviorFlags::Hoverable | UiElementBehaviorFlags::Interactable;
			}
		};

		panel->AddItem(valueItem);
		return panel;
	}

	inline BaseMenuPanel* BuildGenericSummonsPanel(const float posY, const float sizeY)
	{
		BaseMenuPanel* panel = new BaseMenuPanel();
		panel->OnInit = [posY, sizeY](BaseUiElement* item)
		{
			item->Name = "GenericSummonsPanel";
			item->SizeX = 0.98f;
			item->SizeY = sizeY;
			item->PosX = 0.01f;
			item->PosY = posY;
			if (auto* itm = static_cast<BaseMenuPanel*>(item)) {
				itm->BehaviorFlags = UiElementBehaviorFlags::Hoverable | UiElementBehaviorFlags::Interactable;
			}
		};

		MenuItem* titleItem = new MenuItem();
		titleItem->OnInit = [](BaseUiElement* item)
		{
			item->Name = "GenericSummonsPanel_Title";
			item->SizeX = 0.80f;
			item->SizeY = 0.95f;
			item->PosX = 0.025f;
			item->PosY = 0.025f;

			if (auto* itm = static_cast<MenuItem*>(item))
			{
				itm->Text = parser->GetSymbol("StExt_Str_SummonsCountGlobalStat")->stringdata;
				itm->BehaviorFlags = UiElementBehaviorFlags::Hoverable | UiElementBehaviorFlags::Interactable;
				itm->HorizontalAlign = UiContentAlignEnum::Begin;
			}
		};

		MenuValueItem* valueItem = new MenuValueItem();
		valueItem->OnInit = [](BaseUiElement* item)
		{
			item->Name = "GenericSummonsPanel_Value";
			item->SizeX = 0.15f;
			item->SizeY = 0.95f;
			item->PosX = 0.825f;
			item->PosY = 0.25f;

			if (auto* itm = static_cast<MenuValueItem*>(item))
			{
				itm->IsRanged = true;
				itm->PrimaryValueName = "rx_summoncount";
				itm->SecondaryValueName = "rx_summoncountmax";
				itm->PrimaryValueDisplayType = UiValueDisplayType::Default;
				itm->SecondaryValueDisplayType = UiValueDisplayType::Default;
				itm->BehaviorFlags = UiElementBehaviorFlags::Hoverable | UiElementBehaviorFlags::Interactable;
			}
		};

		panel->AddItem(titleItem);
		panel->AddItem(valueItem);
		return panel;
	}

	inline BaseMenuPanel* BuildGenericMasteriesPanel(const float posY, const float sizeY)
	{
		BaseMenuPanel* panel = new BaseMenuPanel();
		panel->OnInit = [posY, sizeY](BaseUiElement* item)
		{
			item->Name = "GenericMasteriesPanel";
			item->SizeX = 0.98f;
			item->SizeY = sizeY;
			item->PosX = 0.01f;
			item->PosY = posY;
			if (auto* itm = static_cast<BaseMenuPanel*>(item)) {
				itm->BehaviorFlags = UiElementBehaviorFlags::Hoverable | UiElementBehaviorFlags::Interactable;
			}
		};

		MenuItem* titleItem = new MenuItem();
		titleItem->OnInit = [](BaseUiElement* item)
		{
			item->Name = "GenericMasteriesPanel_Title";
			item->SizeX = 0.80f;
			item->SizeY = 0.95f;
			item->PosX = 0.025f;
			item->PosY = 0.025f;

			if (auto* itm = static_cast<MenuItem*>(item))
			{
				itm->Text = parser->GetSymbol("StExt_Str_Masteries")->stringdata;
				itm->BehaviorFlags = UiElementBehaviorFlags::Hoverable | UiElementBehaviorFlags::Interactable;
				itm->HorizontalAlign = UiContentAlignEnum::Begin;
			}
		};

		MenuValueItem* valueItem = new MenuValueItem();
		valueItem->OnInit = [](BaseUiElement* item)
		{
			item->Name = "GenericMasteriesPanel_Value";
			item->SizeX = 0.15f;
			item->SizeY = 0.95f;
			item->PosX = 0.825f;
			item->PosY = 0.25f;

			if (auto* itm = static_cast<MenuValueItem*>(item))
			{
				itm->IsRanged = true;
				itm->PrimaryValueName = "StExt_HeroMasteriesLearned";
				itm->SecondaryValueName = "StExt_HeroMasteriesMax";
				itm->PrimaryValueDisplayType = UiValueDisplayType::Default;
				itm->SecondaryValueDisplayType = UiValueDisplayType::Default;
				itm->BehaviorFlags = UiElementBehaviorFlags::Hoverable | UiElementBehaviorFlags::Interactable;
			}
		};

		panel->AddItem(titleItem);
		panel->AddItem(valueItem);
		return panel;
	}

	inline BaseMenuPanel* BuildGenericCorruptionPanel(const float posY)
	{
		BaseMenuPanel* panel = new BaseMenuPanel();
		panel->OnInit = [posY](BaseUiElement* item)
		{
			item->Name = "GenericCorruptionPanel";
			item->SizeX = 0.98f;
			item->SizeY = 0.45f * ModMenuItem_EmdedPanelsScaleMult;
			item->PosX = 0.01f;
			item->PosY = posY;
			item->OnUpdate = StatsWindow_OnCorruptionPanelUpdate;
			if (auto* itm = static_cast<BaseMenuPanel*>(item)) {
				itm->BehaviorFlags = UiElementBehaviorFlags::Hoverable | UiElementBehaviorFlags::Interactable;
			}
		};

		MenuItem* titleItem = new MenuItem();
		titleItem->OnInit = [](BaseUiElement* item)
		{
			item->Name = "GenericCorruptionPanel_Title";
			item->SizeX = 0.80f;
			item->SizeY = 0.15f;
			item->PosX = 0.10f;
			item->PosY = 0.02f;

			if (auto* itm = static_cast<MenuItem*>(item))
			{
				itm->Text = "";
				itm->TextColor_Default = zCOLOR(130, 80, 250);
				itm->Font = MenuFont_Header;
				itm->BehaviorFlags = UiElementBehaviorFlags::Hoverable | UiElementBehaviorFlags::Interactable;
				itm->HorizontalAlign = UiContentAlignEnum::Center;
				itm->VerticalAlign = UiContentAlignEnum::Center;
			}
		};
		MenuTextItem* titleDescItem = new MenuTextItem();
		titleDescItem->OnInit = [](BaseUiElement* item)
		{
			item->Name = "GenericCorruptionPanel_TitleDescription";
			item->SizeX = 0.80f;
			item->SizeY = 0.20f;
			item->PosX = 0.10f;
			item->PosY = 0.18f;

			if (auto* itm = static_cast<MenuTextItem*>(item))
			{
				itm->Text = "";
				itm->BehaviorFlags = UiElementBehaviorFlags::Hoverable | UiElementBehaviorFlags::Interactable;
				itm->HorizontalAlign = UiContentAlignEnum::Center;
				itm->VerticalAlign = UiContentAlignEnum::Center;
			}
		};
		panel->AddItem(titleItem);
		panel->AddItem(titleDescItem);

		//0.38
		MenuItem* levelTitleItem = new MenuItem();
		levelTitleItem->OnInit = [](BaseUiElement* item)
		{
			item->Name = "GenericCorruptionPanel_LevelTitle";
			item->SizeX = 0.40f;
			item->SizeY = 0.05f;
			item->PosX = 0.10f;
			item->PosY = 0.38f;

			if (auto* itm = static_cast<MenuItem*>(item))
			{
				itm->Text = parser->GetSymbol("StExt_Str_Level")->stringdata;
				itm->BehaviorFlags = UiElementBehaviorFlags::Hoverable | UiElementBehaviorFlags::Interactable;
				itm->HorizontalAlign = UiContentAlignEnum::Center;
				itm->VerticalAlign = UiContentAlignEnum::Center;
			}
		};
		MenuValueItem* levelValueItem = new MenuValueItem();
		levelValueItem->OnInit = [](BaseUiElement* item)
		{
			item->Name = "GenericCorruptionPanel_LevelValue";
			item->SizeX = 0.40f;
			item->SizeY = 0.05f;
			item->PosX = 0.50f;
			item->PosY = 0.38f;

			if (auto* itm = static_cast<MenuValueItem*>(item))
			{
				itm->PrimaryValueName = "StExt_CorruptionPath_Level";
				itm->PrimaryValueDisplayType = UiValueDisplayType::Default;
				itm->BehaviorFlags = UiElementBehaviorFlags::Hoverable | UiElementBehaviorFlags::Interactable;
			}
		};
		panel->AddItem(levelTitleItem);
		panel->AddItem(levelValueItem);

		//0.43
		MenuItem* expTitleItem = new MenuItem();
		expTitleItem->OnInit = [](BaseUiElement* item)
		{
			item->Name = "GenericCorruptionPanel_ExpTitle";
			item->SizeX = 0.40f;
			item->SizeY = 0.05f;
			item->PosX = 0.10f;
			item->PosY = 0.43f;

			if (auto* itm = static_cast<MenuItem*>(item))
			{
				itm->Text = parser->GetSymbol("StExt_Str_Exp")->stringdata;
				itm->BehaviorFlags = UiElementBehaviorFlags::Hoverable | UiElementBehaviorFlags::Interactable;
				itm->HorizontalAlign = UiContentAlignEnum::Center;
				itm->VerticalAlign = UiContentAlignEnum::Center;
			}
		};
		MenuValueItem* expValueItem = new MenuValueItem();
		expValueItem->OnInit = [](BaseUiElement* item)
		{
			item->Name = "GenericCorruptionPanel_ExpValue";
			item->SizeX = 0.40f;
			item->SizeY = 0.05f;
			item->PosX = 0.50f;
			item->PosY = 0.43f;

			if (auto* itm = static_cast<MenuValueItem*>(item))
			{
				itm->IsRanged = true;
				itm->PrimaryValueName = "StExt_CorruptionPath_ExpNow";
				itm->SecondaryValueName = "StExt_CorruptionPath_ExpNext";
				itm->PrimaryValueDisplayType = UiValueDisplayType::Default;
				itm->SecondaryValueDisplayType = UiValueDisplayType::Default;
				itm->BehaviorFlags = UiElementBehaviorFlags::Hoverable | UiElementBehaviorFlags::Interactable;
			}
		};
		panel->AddItem(expTitleItem);
		panel->AddItem(expValueItem);

		//0.48
		MenuItem* lpTitleItem = new MenuItem();
		lpTitleItem->OnInit = [](BaseUiElement* item)
		{
			item->Name = "GenericCorruptionPanel_LpTitle";
			item->SizeX = 0.40f;
			item->SizeY = 0.05f;
			item->PosX = 0.10f;
			item->PosY = 0.48f;

			if (auto* itm = static_cast<MenuItem*>(item))
			{
				itm->Text = parser->GetSymbol("StExt_Str_Lp")->stringdata;
				itm->BehaviorFlags = UiElementBehaviorFlags::Hoverable | UiElementBehaviorFlags::Interactable;
				itm->HorizontalAlign = UiContentAlignEnum::Center;
				itm->VerticalAlign = UiContentAlignEnum::Center;
			}
		};
		MenuValueItem* lpValueItem = new MenuValueItem();
		lpValueItem->OnInit = [](BaseUiElement* item)
		{
			item->Name = "GenericCorruptionPanel_LpValue";
			item->SizeX = 0.40f;
			item->SizeY = 0.05f;
			item->PosX = 0.50f;
			item->PosY = 0.48f;

			if (auto* itm = static_cast<MenuValueItem*>(item))
			{
				itm->PrimaryValueName = "StExt_CorruptionPath_SkillPoints";
				itm->PrimaryValueDisplayType = UiValueDisplayType::Default;
				itm->BehaviorFlags = UiElementBehaviorFlags::Hoverable | UiElementBehaviorFlags::Interactable;
			}
		};
		panel->AddItem(lpTitleItem);
		panel->AddItem(lpValueItem);

		//0.55
		MenuItem* bonusTitleItem = new MenuItem();
		bonusTitleItem->OnInit = [](BaseUiElement* item)
		{
			item->Name = "GenericCorruptionPanel_BonusesTitle";
			item->SizeX = 0.80f;
			item->SizeY = 0.05f;
			item->PosX = 0.10f;
			item->PosY = 0.55f;

			if (auto* itm = static_cast<MenuItem*>(item))
			{
				itm->Text = parser->GetSymbol("StExt_Str_Corruption_BonusHeader")->stringdata;
				itm->BehaviorFlags = UiElementBehaviorFlags::Hoverable | UiElementBehaviorFlags::Interactable;
				itm->HorizontalAlign = UiContentAlignEnum::Center;
				itm->VerticalAlign = UiContentAlignEnum::Center;
			}
		};
		panel->AddItem(bonusTitleItem);

		//0.61
		const int corruptionBonusMax = parser->GetSymbol("StExt_CorruptionBonus_Max")->single_intdata;
		float yOffset = 0.61f;
		const float ySize = 0.05f;
		for (int i = 0; i < corruptionBonusMax; i++)
		{
			MenuItem* bonusTitleItem = new MenuItem();
			bonusTitleItem->OnInit = [ySize, yOffset, i](BaseUiElement* item)
			{
				item->Name = "GenericCorruptionPanel_BonusTitle_" + Z(i);
				item->SizeX = 0.40f;
				item->SizeY = ySize;
				item->PosX = 0.10f;
				item->PosY = yOffset;

				if (auto* itm = static_cast<MenuItem*>(item))
				{
					itm->Text = parser->GetSymbol("StExt_Str_Corruption_BonusStat")->stringdata[i];
					itm->BehaviorFlags = UiElementBehaviorFlags::Hoverable | UiElementBehaviorFlags::Interactable;
					itm->HorizontalAlign = UiContentAlignEnum::Center;
					itm->VerticalAlign = UiContentAlignEnum::Center;
				}
			};
			MenuValueItem* bonusValueItem = new MenuValueItem();
			bonusValueItem->OnInit = [ySize, yOffset, i](BaseUiElement* item)
			{
				item->Name = "GenericCorruptionPanel_BonusValue_" + Z(i);
				item->SizeX = 0.40f;
				item->SizeY = ySize;
				item->PosX = 0.50f;
				item->PosY = yOffset;

				if (auto* itm = static_cast<MenuValueItem*>(item))
				{
					itm->IsArray = true;
					itm->IsRanged = true;
					itm->RangeValueSeparator = " | ";
					itm->PrimaryValueArrayIndex = i;
					itm->SecondaryValueArrayIndex = i;
					itm->PrimaryValueName = "StExt_Corruption_BonusCount";
					itm->SecondaryValueName = "StExt_Corruption_BonusNext";
					itm->PrimaryValueDisplayType = (i == 5) ? UiValueDisplayType::Permille : UiValueDisplayType::Default;
					itm->SecondaryValueDisplayType = (i == 5) ? UiValueDisplayType::Permille : UiValueDisplayType::Default;
					itm->BehaviorFlags = UiElementBehaviorFlags::Hoverable | UiElementBehaviorFlags::Interactable;
				}
			};

			panel->AddItem(bonusTitleItem);
			panel->AddItem(bonusValueItem);
			yOffset += ySize;
		}
		return panel;
	}

	inline BaseMenuPanel* BuildGenericValuePanel(const zSTRING valueSym, const zSTRING valueHeaderSym, const float posY, const float sizeY)
	{
		BaseMenuPanel* panel = new BaseMenuPanel();
		panel->OnInit = [valueSym, posY, sizeY](BaseUiElement* item)
		{
			item->Name = "GenericDamagePanel_" + valueSym;
			item->SizeX = 0.98f;
			item->SizeY = sizeY;
			item->PosX = 0.01f;
			item->PosY = posY;
			if (auto* itm = static_cast<BaseMenuPanel*>(item)) {
				itm->BehaviorFlags = UiElementBehaviorFlags::Hoverable | UiElementBehaviorFlags::Interactable;
			}
		};

		MenuItem* titleItem = new MenuItem();
		titleItem->OnInit = [valueHeaderSym, valueSym](BaseUiElement* item)
		{
			item->Name = "GenericDamagePanel_Title_" + valueSym;
			item->SizeX = 0.80f;
			item->SizeY = 0.95f;
			item->PosX = 0.025f;
			item->PosY = 0.025f;

			if (auto* itm = static_cast<MenuItem*>(item))
			{
				itm->Text = parser->GetSymbol(valueHeaderSym)->stringdata;
				itm->BehaviorFlags = UiElementBehaviorFlags::Hoverable | UiElementBehaviorFlags::Interactable;
				itm->HorizontalAlign = UiContentAlignEnum::Begin;
			}
		};

		MenuValueItem* valueItem = new MenuValueItem();
		valueItem->OnInit = [valueSym](BaseUiElement* item)
		{
			item->Name = "GenericDamagePanel_Value_" + valueSym;
			item->SizeX = 0.15f;
			item->SizeY = 0.95f;
			item->PosX = 0.825f;
			item->PosY = 0.25f;

			if (auto* itm = static_cast<MenuValueItem*>(item))
			{
				itm->PrimaryValueName = valueSym;
				itm->PrimaryValueDisplayType = UiValueDisplayType::Default;
				itm->BehaviorFlags = UiElementBehaviorFlags::Hoverable | UiElementBehaviorFlags::Interactable;
			}
		};

		panel->AddItem(titleItem);
		panel->AddItem(valueItem);
		return panel;
	}

	inline BaseMenuPanel* BuildSkillGenericHeaderPanel(const SkillHeaderPanelArgs& args, const float posY, const float sizeY)
	{
		BaseMenuPanel* panel = new BaseMenuPanel();
		panel->OnInit = [args, posY, sizeY](BaseUiElement* item)
		{
			item->Name = "SkillGenericHeaderPanel_" + args.NamePostfix;
			item->SizeX = 0.98f;
			item->SizeY = sizeY;
			item->PosX = 0.01f;
			item->PosY = posY;
			if (auto* itm = static_cast<BaseMenuPanel*>(item)) {
				itm->BehaviorFlags = UiElementBehaviorFlags::Hoverable | UiElementBehaviorFlags::Interactable;
			}
		};

		MenuItem* titleItem = new MenuItem();
		MenuValueItem* levelValueItem = new MenuValueItem();
		MenuValueItem* expValueItem = new MenuValueItem();

		titleItem->OnInit = [args](BaseUiElement* item)
		{
			item->Name = "SkillGenericHeaderPanel_Title_" + args.NamePostfix;
			item->SizeX = 0.80f;
			item->SizeY = 0.95f;
			item->PosX = 0.025f;
			item->PosY = 0.025f;

			if (auto* itm = static_cast<MenuItem*>(item))
			{
				itm->Text = parser->GetSymbol(args.TitleSymbol)->stringdata[args.MasteryId];
				itm->BehaviorFlags = UiElementBehaviorFlags::Hoverable | UiElementBehaviorFlags::Interactable;
				itm->HorizontalAlign = UiContentAlignEnum::Begin;
			}
		};

		levelValueItem->OnInit = [args](BaseUiElement* item)
		{
			item->Name = "SkillGenericHeaderPanel_LevelValue_" + args.NamePostfix;
			item->SizeX = 0.15f;
			item->SizeY = 0.95f;
			item->PosX = 0.825f;
			item->PosY = 0.025f;

			if (auto* itm = static_cast<MenuValueItem*>(item))
			{
				itm->IsArray = true;
				itm->IsRanged = true;
				itm->RangeValueSeparator = " | ";
				itm->HorizontalAlign = UiContentAlignEnum::Center;
				itm->BehaviorFlags = UiElementBehaviorFlags::Hoverable | UiElementBehaviorFlags::Interactable;

				itm->PrimaryValueName = args.LevelSymbol;
				itm->PrimaryValueArrayIndex = args.MasteryId;
				itm->PrimaryValueDisplayType = UiValueDisplayType::Default;

				itm->SecondaryValueName = args.RankSymbol;
				itm->SecondaryValueArrayIndex = args.MasteryId;
				itm->SecondaryValueDisplayType = UiValueDisplayType::Bool;
				itm->SecondarySpecialValueFormatter = ConvertValueToMasteryRank;
			}
		};

		panel->AddItem(titleItem);
		panel->AddItem(levelValueItem);
		return panel;
	}


#define STEXT_GENERALTABPANEL_ADDITEM(itm, offset)  GeneralTabPanel->AddItem(itm); yOffset += offset;
	void StatsWindow::InitGeneralTab()
	{
		GeneralTabPanel = new MenuScrollPanel();
		GeneralTabPanel->OnInit = [](BaseUiElement* item)
		{
			item->Name = "GeneralTabPanel";
			item->SizeX = 0.98f;
			item->SizeY = 0.88f;
			item->PosX = 0.01f;
			item->PosY = 0.11f;
			item->BgTexture = UiElement_PanelNoBorderTexture;
		};
		AddItem(GeneralTabPanel);

		const float statPanelSizeY = 0.035f;
		const float statHeaderSizeY = 0.08f;
		const float yOffsetSmall = 0.03f;
		const float yOffsetBig = 0.05f;
		float yOffset = yOffsetBig;

		STEXT_GENERALTABPANEL_ADDITEM(BuildGenericCorruptionPanel(yOffset), statPanelSizeY);
		yOffset += (0.45f * ModMenuItem_EmdedPanelsScaleMult) + yOffsetBig;

		// masteries
		zSTRING namePostfix = "";
		for (auto& masteryData : ExtraMasteriesData)
		{
			if (masteryData.IsGeneric || masteryData.IsCorruption || masteryData.IsCorruptionTouch) continue;

			SkillHeaderPanelArgs skillsHeaderPanelArgs = SkillHeaderPanelArgs();
			BuildSkillPostfixName(namePostfix, masteryData.MasteryId, 0, masteryData.IsCorruption, masteryData.IsGeneric);

			skillsHeaderPanelArgs.MasteryId = masteryData.MasteryId;
			skillsHeaderPanelArgs.PosY = yOffset;
			skillsHeaderPanelArgs.IsCorruptionMastery = masteryData.IsCorruption;
			skillsHeaderPanelArgs.NamePostfix = namePostfix;

			skillsHeaderPanelArgs.TitleSymbol = masteryData.TitleSymbol;
			skillsHeaderPanelArgs.DescSymbol = masteryData.DescSymbol;
			skillsHeaderPanelArgs.LevelSymbol = masteryData.LevelSymbol;
			skillsHeaderPanelArgs.RankSymbol = masteryData.RankSymbol;
			skillsHeaderPanelArgs.ExpSymbol = masteryData.ExpSymbol;
			skillsHeaderPanelArgs.NextExpSymbol = masteryData.NextExpSymbol;
			skillsHeaderPanelArgs.LpSymbol = masteryData.LpSymbol;

			BaseMenuPanel* skillsHeaderPanel = BuildSkillGenericHeaderPanel(skillsHeaderPanelArgs, yOffset, statPanelSizeY);
			STEXT_GENERALTABPANEL_ADDITEM(skillsHeaderPanel, statPanelSizeY);
		}
		yOffset += yOffsetBig;

		// Karma
		STEXT_GENERALTABPANEL_ADDITEM(BuildGenericValuePanel("StExt_InnosKarma", "StExt_Str_Karma_Innos", yOffset, statPanelSizeY), statPanelSizeY);
		STEXT_GENERALTABPANEL_ADDITEM(BuildGenericValuePanel("StExt_BeliarKarma", "StExt_Str_Karma_Beliar", yOffset, statPanelSizeY), statPanelSizeY);
		STEXT_GENERALTABPANEL_ADDITEM(BuildGenericValuePanel("StExt_AdanosKarma", "StExt_Str_Karma_Adanos", yOffset, statPanelSizeY), statPanelSizeY);
		yOffset += yOffsetSmall;

		// Caps
		STEXT_GENERALTABPANEL_ADDITEM(BuildGenericMasteriesPanel(yOffset, statPanelSizeY), statPanelSizeY);
		STEXT_GENERALTABPANEL_ADDITEM(BuildGenericSummonsPanel(yOffset, statPanelSizeY), statPanelSizeY);
		STEXT_GENERALTABPANEL_ADDITEM(BuildGenericAuraPanel(yOffset, statPanelSizeY), statPanelSizeY);
		STEXT_GENERALTABPANEL_ADDITEM(BuildGenericStatPanel(159, yOffset, statPanelSizeY), statPanelSizeY);
		STEXT_GENERALTABPANEL_ADDITEM(BuildGenericAuraListPanel(yOffset, statPanelSizeY), statPanelSizeY);
		yOffset += yOffsetSmall;

		// Damage
		STEXT_GENERALTABPANEL_ADDITEM(BuildGenericEsPanel(yOffset, statPanelSizeY), statPanelSizeY);
		STEXT_GENERALTABPANEL_ADDITEM(BuildGenericStatPanel(27, yOffset, statPanelSizeY), statPanelSizeY);
		STEXT_GENERALTABPANEL_ADDITEM(BuildGenericStatPanel(246, yOffset, statPanelSizeY), statPanelSizeY);
		yOffset += yOffsetSmall;
		STEXT_GENERALTABPANEL_ADDITEM(BuildGenericDamagePanel("StExt_MeeleDamageInfo", "StExt_Str_AverageWeapDamageInfo_Meele", yOffset, statPanelSizeY), statPanelSizeY);
		STEXT_GENERALTABPANEL_ADDITEM(BuildGenericDamagePanel("StExt_RangeDamageInfo", "StExt_Str_AverageWeapDamageInfo_Range", yOffset, statPanelSizeY), statPanelSizeY);
		STEXT_GENERALTABPANEL_ADDITEM(BuildGenericDamagePanel("StExt_RuneDamageInfo", "StExt_Str_AverageWeapDamageInfo_Magic", yOffset, statPanelSizeY), statPanelSizeY);
		yOffset += yOffsetSmall;

		STEXT_GENERALTABPANEL_ADDITEM(BuildGenericStatPanel(146, yOffset, statPanelSizeY), statPanelSizeY);
		STEXT_GENERALTABPANEL_ADDITEM(BuildGenericStatPanel(147, yOffset, statPanelSizeY), statPanelSizeY);
		STEXT_GENERALTABPANEL_ADDITEM(BuildGenericStatPanel(148, yOffset, statPanelSizeY), statPanelSizeY);
		STEXT_GENERALTABPANEL_ADDITEM(BuildGenericStatPanel(149, yOffset, statPanelSizeY), statPanelSizeY);
		STEXT_GENERALTABPANEL_ADDITEM(BuildGenericStatPanel(150, yOffset, statPanelSizeY), statPanelSizeY);
		STEXT_GENERALTABPANEL_ADDITEM(BuildGenericStatPanel(151, yOffset, statPanelSizeY), statPanelSizeY);
		STEXT_GENERALTABPANEL_ADDITEM(BuildGenericStatPanel(152, yOffset, statPanelSizeY), statPanelSizeY);
		STEXT_GENERALTABPANEL_ADDITEM(BuildGenericStatPanel(153, yOffset, statPanelSizeY), statPanelSizeY);
		STEXT_GENERALTABPANEL_ADDITEM(BuildGenericStatPanel(154, yOffset, statPanelSizeY), statPanelSizeY);
		STEXT_GENERALTABPANEL_ADDITEM(BuildGenericStatPanel(155, yOffset, statPanelSizeY), statPanelSizeY);
		STEXT_GENERALTABPANEL_ADDITEM(BuildGenericStatPanel(156, yOffset, statPanelSizeY), statPanelSizeY);
		STEXT_GENERALTABPANEL_ADDITEM(BuildGenericStatPanel(157, yOffset, statPanelSizeY), statPanelSizeY);
		STEXT_GENERALTABPANEL_ADDITEM(BuildGenericStatPanel(249, yOffset, statPanelSizeY), statPanelSizeY);
		yOffset += yOffsetSmall;

		// Protection
		STEXT_GENERALTABPANEL_ADDITEM(BuildGenericStatPanel(40, yOffset, statPanelSizeY), statPanelSizeY);
		STEXT_GENERALTABPANEL_ADDITEM(BuildGenericStatPanel(41, yOffset, statPanelSizeY), statPanelSizeY);
		STEXT_GENERALTABPANEL_ADDITEM(BuildGenericStatPanel(42, yOffset, statPanelSizeY), statPanelSizeY);
		STEXT_GENERALTABPANEL_ADDITEM(BuildGenericStatPanel(43, yOffset, statPanelSizeY), statPanelSizeY);
		STEXT_GENERALTABPANEL_ADDITEM(BuildGenericStatPanel(44, yOffset, statPanelSizeY), statPanelSizeY);
		STEXT_GENERALTABPANEL_ADDITEM(BuildGenericStatPanel(45, yOffset, statPanelSizeY), statPanelSizeY);
		STEXT_GENERALTABPANEL_ADDITEM(BuildGenericStatPanel(46, yOffset, statPanelSizeY), statPanelSizeY);
		STEXT_GENERALTABPANEL_ADDITEM(BuildGenericStatPanel(47, yOffset, statPanelSizeY), statPanelSizeY);
		STEXT_GENERALTABPANEL_ADDITEM(BuildGenericStatPanel(274, yOffset, statPanelSizeY), statPanelSizeY);
		STEXT_GENERALTABPANEL_ADDITEM(BuildGenericStatPanel(275, yOffset, statPanelSizeY), statPanelSizeY);
		yOffset += yOffsetSmall;

		// Luck
		STEXT_GENERALTABPANEL_ADDITEM(BuildGenericStatPanel(239, yOffset, statPanelSizeY), statPanelSizeY);
		STEXT_GENERALTABPANEL_ADDITEM(BuildGenericStatPanel(240, yOffset, statPanelSizeY), statPanelSizeY);
		STEXT_GENERALTABPANEL_ADDITEM(BuildGenericStatPanel(241, yOffset, statPanelSizeY), statPanelSizeY);
		STEXT_GENERALTABPANEL_ADDITEM(BuildGenericStatPanel(242, yOffset, statPanelSizeY), statPanelSizeY);
		yOffset += yOffsetSmall;
		GeneralTabPanel->Init();
	}

	void StatsWindow::InitProfesionTab()
	{
		ProfesionTabPanel = new MenuScrollPanel();
		ProfesionTabPanel->OnInit = [](BaseUiElement* item)
		{
			item->Name = "GeneralTabPanel";
			item->SizeX = 0.98f;
			item->SizeY = 0.88f;
			item->PosX = 0.01f;
			item->PosY = 0.11f;
			item->BgTexture = UiElement_PanelNoBorderTexture;
		};
		AddItem(ProfesionTabPanel);

		// ToDo

		ProfesionTabPanel->Init();
	}

	void StatsWindow::InitBonusTab()
	{
		BonusTabPanel = new MenuScrollPanel();
		BonusTabPanel->OnInit = [](BaseUiElement* item)
		{
			item->Name = "BonusTabPanel";
			item->SizeX = 0.98f;
			item->SizeY = 0.88f;
			item->PosX = 0.01f;
			item->PosY = 0.11f;
			item->BgTexture = UiElement_PanelNoBorderTexture;
			item->OnUpdate = StatsWindow_OnBonusPanelUpdate;
		};
		AddItem(BonusTabPanel);
		BonusTabPanel->Init();
	}

	//----------------------------------------------------------------------
	//								STATS PANEL
	//----------------------------------------------------------------------

	inline int GetStatGroupOrder(int statGroup) 
	{
		for (int i = 0; i < StatsSectionOrderMax; ++i) 
		{
			if (StatsSectionOrder[i] == statGroup)
				return static_cast<int>(i);
		}
		return INT_MAX;
	}

	inline void SortStats(std::vector<int>& ids)
	{
		ids.reserve(ExtraStatsData.GetNum());
		for (const auto& kv : ExtraStatsData)
			ids.push_back(kv.GetKey());

		std::sort(ids.begin(), ids.end(), [](int a, int b)
		{
			auto pA = ExtraStatsData.GetSafePair(a);
			auto pB = ExtraStatsData.GetSafePair(b);

			if (!pA || !pB) return false;

			const auto& ea = pA->GetValue();
			const auto& eb = pB->GetValue();

			int ga = GetStatGroupOrder(ea.StatGroup);
			int gb = GetStatGroupOrder(eb.StatGroup);
			if (ga != gb)
				return ga < gb;

			int sa = (ea.SortIndex == Invalid ? INT_MAX : ea.SortIndex);
			int sb = (eb.SortIndex == Invalid ? INT_MAX : eb.SortIndex);
			if (sa != sb)
				return sa < sb;

			return ea.Id < eb.Id;
		});
	}

	inline BaseMenuPanel* BuildStatPanel(const int statId, const float posY, const float sizeY)
	{
		const ExtraStatData* statData = GetExtraStatDataById(statId);
		if (!statData)
		{
			DEBUG_MSG("StExt Mod Ui - Fail to build stats panel. StatId = " + Z(statId));
			return Null;
		}
		zSTRING statTitle = GetExtraStatNameById(statId);

		BaseMenuPanel* panel = new BaseMenuPanel();
		panel->OnInit = [statId, posY, sizeY](BaseUiElement* item)
		{
			item->Name = "StatPanel_" + Z(statId);
			item->SizeX = 0.98f;
			item->SizeY = sizeY;
			item->PosX = 0.01f;
			item->PosY = posY;
			if (auto* itm = static_cast<BaseMenuPanel*>(item)) {
				itm->BehaviorFlags = UiElementBehaviorFlags::Hoverable | UiElementBehaviorFlags::Interactable;
			}
		};

		MenuItem* titleItem = new MenuItem();
		titleItem->OnInit = [statId, statTitle](BaseUiElement* item)
		{
			item->Name = "StatPanel_Title_" + Z(statId);
			item->SizeX = 0.80f;
			item->SizeY = 0.95f;
			item->PosX = 0.025f;
			item->PosY = 0.025f;

			if (auto* itm = static_cast<MenuItem*>(item))
			{
				itm->Text = statTitle;
				itm->BehaviorFlags = UiElementBehaviorFlags::Hoverable | UiElementBehaviorFlags::Interactable;
				itm->HorizontalAlign = UiContentAlignEnum::Begin;
				itm->SetOwnedData<int>(statId);
			}
		};

		MenuValueItem* valueItem = new MenuValueItem();
		UiValueDisplayType displayType = (UiValueDisplayType)statData->ValueType;
		valueItem->OnInit = [statId, displayType](BaseUiElement* item)
		{
			item->Name = "StatPanel_Value_" + Z(statId);
			item->SizeX = 0.15f;
			item->SizeY = 0.95f;
			item->PosX = 0.825f;
			item->PosY = 0.25f;

			if (auto* itm = static_cast<MenuValueItem*>(item))
			{
				itm->IsArray = true;
				itm->PrimaryValueArrayIndex = statId;
				itm->PrimaryValueName = "StExt_PcStats";
				itm->PrimaryValueDisplayType = displayType;
				itm->BehaviorFlags = UiElementBehaviorFlags::Hoverable | UiElementBehaviorFlags::Interactable;
				itm->SetOwnedData<int>(statId);
			}
		};

		panel->AddItem(titleItem);
		panel->AddItem(valueItem);
		return panel;
	}

	inline MenuItem* BuildStatHeader(const int groupId, const float posY, const float sizeY)
	{
		MenuItem* headerItem = new MenuItem();
		headerItem->OnInit = [groupId, posY, sizeY](BaseUiElement* item)
		{
			item->Name = "StatPanel_Header_" + Z(groupId);
			item->SizeX = 0.95f;
			item->SizeY = sizeY;
			item->PosX = 0.025f;
			item->PosY = posY;
			item->BgTexture = UiElement_ButtonNoBorderTexture;

			if (auto* itm = static_cast<MenuItem*>(item))
			{
				itm->Text = parser->GetSymbol("StExt_PcStats_SectionDesc")->stringdata[groupId];
				itm->TextColor_Default = TextColor_Header_Default;
			}
		};
		return headerItem;
	}

	void StatsWindow::InitStatsTab()
	{
		StatsTabPanel = new MenuScrollPanel();
		StatsTabPanel->OnInit = [](BaseUiElement* item)
		{
			item->Name = "StatsTabPanel";
			item->SizeX = 0.98f;
			item->SizeY = 0.88f;
			item->PosX = 0.01f;
			item->PosY = 0.11f;
			item->BgTexture = UiElement_PanelNoBorderTexture;
		};
		AddItem(StatsTabPanel);

		std::vector<int> ids;
		SortStats(ids);

		const float statPanelSizeY = 0.035f;
		const float statHeaderSizeY = 0.08f;
		const float itemMargin = 0.005f;
		const float groupMargin = 0.05f;

		float yOffset = 0.03f;
		int currentGroup = Invalid;
		for (int statId : ids)
		{
			ExtraStatData statData;
			auto pair = ExtraStatsData.GetSafePair(statId);

			if (!pair)
			{
				DEBUG_MSG("StExt Mod Ui StatsWindow::InitStatsTab() - Fail to find stat StatId = " + Z(statId));
				continue;
			}
			statData = pair->GetValue();

			if (statData.StatGroup != currentGroup)
			{
				yOffset += groupMargin;
				currentGroup = statData.StatGroup;
				MenuItem* headerItem = BuildStatHeader(currentGroup, yOffset, statHeaderSizeY);				
				StatsTabPanel->AddItem(headerItem);
				yOffset += statHeaderSizeY + (itemMargin * 2.5f);
			}

			BaseMenuPanel* statPanel = BuildStatPanel(statId, yOffset, statPanelSizeY);
			if (statPanel == Null)
			{
				DEBUG_MSG("StExt Mod Ui StatsWindow::InitStatsTab() - Fail to build stat panel! StatId = " + Z(statId));
				continue;
			}
			StatsTabPanel->AddItem(statPanel);
			yOffset += statPanelSizeY + itemMargin;
		}
		StatsTabPanel->Init();
	}

	//----------------------------------------------------------------------
	//								SKILLS PANEL
	//----------------------------------------------------------------------

	inline BaseMenuPanel* BuildSkillPanel(const SkillPanelArgs& args)
	{
		BaseMenuPanel* panel = new BaseMenuPanel();
		panel->OnInit = [args](BaseUiElement* item)
		{
			item->Name = "SkillPanel_" + args.NamePostfix;
			item->SizeX = 0.98f;
			item->SizeY = 0.125f * ModMenuItem_EmdedPanelsScaleMult;
			item->PosX = 0.01f;
			item->PosY = args.PosY;
			if (auto* itm = static_cast<BaseMenuPanel*>(item)) {
				itm->BehaviorFlags = UiElementBehaviorFlags::Hoverable | UiElementBehaviorFlags::Interactable;
			}
		};
		panel->SizeY = 0.125f * ModMenuItem_EmdedPanelsScaleMult;

		MenuItem* titleItem = new MenuItem();
		titleItem->OnInit = [args](BaseUiElement* item)
		{
			item->Name = "SkillPanel_Title_" + args.NamePostfix;
			item->SizeX = 0.98f;
			item->SizeY = 0.175f;
			item->PosX = 0.01f;
			item->PosY = 0.025f;

			if (auto* itm = static_cast<MenuItem*>(item))
			{
				itm->Text = parser->GetSymbol(args.TitleSymbol)->stringdata[args.SkillId];
				itm->TextColor_Default = TextColor_Header_Default;
				itm->TextColor_Hovered = TextColor_Regular_Selected;
				itm->TextColor_Selected = TextColor_Regular_Selected;
				itm->TextColor_Disabled = TextColor_Header_Disabled;
				itm->BehaviorFlags = UiElementBehaviorFlags::Hoverable | UiElementBehaviorFlags::Interactable;
			}
		};

		MenuTextItem* descriptionItem = new MenuTextItem();
		descriptionItem->OnInit = [args](BaseUiElement* item)
		{
			item->Name = "SkillPanel_Description_" + args.NamePostfix;
			item->SizeX = 0.80f;
			item->SizeY = 0.76f;
			item->PosX = 0.01f;
			item->PosY = 0.23f;

			if (auto* itm = static_cast<MenuTextItem*>(item))
			{
				itm->HorizontalAlign = UiContentAlignEnum::Begin;
				itm->Text = parser->GetSymbol(args.DescSymbol)->stringdata[args.SkillId];
				itm->BehaviorFlags = UiElementBehaviorFlags::Hoverable | UiElementBehaviorFlags::Interactable;
			}
		};

		MenuValueItem* valueItem = new MenuValueItem();
		valueItem->OnInit = [args](BaseUiElement* item)
		{
			item->Name = "SkillPanel_Value_" + args.NamePostfix;
			item->SizeX = 0.17f;
			item->SizeY = 0.76f;
			item->PosX = 0.82f;
			item->PosY = 0.23f;

			if (auto* itm = static_cast<MenuValueItem*>(item))
			{
				itm->IsArray = true;
				itm->PrimaryValueArrayIndex = args.SkillId;
				itm->PrimaryValueName = args.ValueSymbol;
				itm->PrimaryValueDisplayType = UiValueDisplayType::Bool;
				itm->PrimarySpecialValueFormatter = ConvertValueToSkill;
				itm->SpecialColorFormatter = BoolValueColorFormatter;
				itm->BehaviorFlags = UiElementBehaviorFlags::Hoverable | UiElementBehaviorFlags::Interactable;
			}
		};

		MenuItem* learnButton = new MenuItem();
		learnButton->OnInit = [args](BaseUiElement* item)
		{
			item->Name = "SkillPanel_LearnButton_" + args.NamePostfix;
			item->SizeX = 0.17f;
			item->SizeY = 0.23f;
			item->PosX = 0.82f;
			item->PosY = 0.01f;
			item->BgTexture = UiElement_ButtonTexture;

			if (auto* itm = static_cast<MenuItem*>(item))
			{
				itm->Text = parser->GetSymbol("StExt_Str_DoSkillLearn")->stringdata;
				itm->Font = MenuFont_Sys_Default;
				itm->TextColor_Default = TextColor_HeaderTab_Default;
				itm->TextColor_Hovered = TextColor_Yes;
				itm->TextColor_Selected = TextColor_Yes;
				itm->TextColor_Disabled = TextColor_HeaderTab_Disabled;
				itm->BehaviorFlags = UiElementBehaviorFlags::Hoverable | UiElementBehaviorFlags::Selectable | UiElementBehaviorFlags::Interactable;

				SkillLearnArgs learnArgs = SkillLearnArgs{};
				learnArgs.SkillId = args.SkillId;
				learnArgs.MasteryTreeIndex = args.MasteryTreeIndex;
				itm->SetOwnedData<SkillLearnArgs>(learnArgs);
				itm->OnMouseEvent = StatsWindow_OnLearnSkillButtonClick;
				itm->OnUpdate = StatsWindow_OnLearnSkillButtonUpdate;
			}
		};

		panel->AddItem(learnButton);
		panel->AddItem(titleItem);
		panel->AddItem(descriptionItem);
		panel->AddItem(valueItem);
		return panel;
	}

	inline BaseMenuPanel* BuildSkillHeaderPanel(const SkillHeaderPanelArgs& args)
	{
		BaseMenuPanel* panel = new BaseMenuPanel();
		panel->OnInit = [args](BaseUiElement* item)
		{
			item->Name = "SkillHeaderPanel_" + args.NamePostfix;
			item->SizeX = 0.98f;
			item->SizeY = 0.25f * ModMenuItem_EmdedPanelsScaleMult;
			item->PosX = 0.01f;
			item->PosY = args.PosY;
			item->BgTexture = UiElement_ButtonTexture;
		};
		panel->SizeY = 0.25f * ModMenuItem_EmdedPanelsScaleMult;

		MenuItem* titleItem = new MenuItem();
		titleItem->OnInit = [args](BaseUiElement* item)
		{
			item->Name = "SkillHeaderPanel_Title_" + args.NamePostfix;
			item->SizeX = 0.98f;
			item->SizeY = 0.14f;
			item->PosX = 0.01f;
			item->PosY = 0.05f;

			if (auto* itm = static_cast<MenuItem*>(item))
			{
				itm->Font = MenuFont_Header;
				itm->Text = parser->GetSymbol(args.TitleSymbol)->stringdata[args.MasteryId];
				itm->TextColor_Default = TextColor_Header_Default;
				itm->TextColor_Disabled = TextColor_Header_Disabled;
			}
		};

		MenuItem* levelTitleItem = new MenuItem();
		MenuItem* expTitleItem = new MenuItem();
		MenuItem* lpTitleItem = new MenuItem();

		levelTitleItem->OnInit = [args](BaseUiElement* item)
		{
			item->Name = "SkillHeaderPanel_LevelTitle_" + args.NamePostfix;
			item->SizeX = 0.35f;
			item->SizeY = 0.10f;
			item->PosX = 0.10f;
			item->PosY = 0.21f;

			if (auto* itm = static_cast<MenuItem*>(item)) {
				itm->Text = parser->GetSymbol("StExt_Str_Talents_Progress")->stringdata;
				itm->HorizontalAlign = UiContentAlignEnum::End;
			}
		};
		expTitleItem->OnInit = [args](BaseUiElement* item)
		{
			item->Name = "SkillHeaderPanel_ExpTitle_" + args.NamePostfix;
			item->SizeX = 0.35f;
			item->SizeY = 0.10f;
			item->PosX = 0.10f;
			item->PosY = 0.32f;

			if (auto* itm = static_cast<MenuItem*>(item)) {
				itm->Text = args.IsCorruptionTouchMastery ? parser->GetSymbol("StExt_Str_CorruptedPerk_DamageExp")->stringdata : parser->GetSymbol("StExt_Str_Talents_Exp")->stringdata;
				itm->HorizontalAlign = UiContentAlignEnum::End;
			}
		};
		lpTitleItem->OnInit = [args](BaseUiElement* item)
		{
			item->Name = "SkillHeaderPanel_LpTitle_" + args.NamePostfix;
			item->SizeX = 0.35f;
			item->SizeY = 0.10f;
			item->PosX = 0.10f;
			item->PosY = 0.43f;

			if (auto* itm = static_cast<MenuItem*>(item)) {
				itm->Text = args.IsCorruptionTouchMastery ? parser->GetSymbol("StExt_Str_CorruptedPerk_DamagePoints")->stringdata : parser->GetSymbol("StExt_Str_Talents_Lp")->stringdata;
				itm->HorizontalAlign = UiContentAlignEnum::End;
			}
		};

		MenuValueItem* levelValueItem = new MenuValueItem();
		MenuValueItem* expValueItem = new MenuValueItem();
		MenuValueItem* lpValueItem = new MenuValueItem();

		levelValueItem->OnInit = [args](BaseUiElement* item)
		{
			item->Name = "SkillHeaderPanel_LevelValue_" + args.NamePostfix;
			item->SizeX = 0.35f;
			item->SizeY = 0.10f;
			item->PosX = 0.55f;
			item->PosY = 0.21f;

			if (auto* itm = static_cast<MenuValueItem*>(item))
			{
				itm->IsArray = !args.IsCorruptionMastery && !args.IsCorruptionTouchMastery;
				itm->HorizontalAlign = UiContentAlignEnum::Begin;				

				itm->PrimaryValueName = args.LevelSymbol;
				itm->PrimaryValueArrayIndex = args.MasteryId;
				itm->PrimaryValueDisplayType = UiValueDisplayType::Default;

				if (!args.IsCorruptionMastery && !args.IsCorruptionTouchMastery)
				{
					itm->IsRanged = true;
					itm->RangeValueSeparator = " | ";
					itm->SecondaryValueName = args.RankSymbol;
					itm->SecondaryValueArrayIndex = args.MasteryId;
					itm->SecondaryValueDisplayType = UiValueDisplayType::Bool;
					itm->SecondarySpecialValueFormatter = ConvertValueToMasteryRank;
				}
			}
		};

		expValueItem->OnInit = [args](BaseUiElement* item)
		{
			item->Name = "SkillHeaderPanel_ExpValue_" + args.NamePostfix;
			item->SizeX = 0.35f;
			item->SizeY = 0.10f;
			item->PosX = 0.55f;
			item->PosY = 0.32f;

			if (auto* itm = static_cast<MenuValueItem*>(item))
			{
				itm->IsArray = !args.IsCorruptionMastery && !args.IsCorruptionTouchMastery;
				itm->IsRanged = true;
				itm->HorizontalAlign = UiContentAlignEnum::Begin;

				itm->PrimaryValueName = args.ExpSymbol;
				itm->PrimaryValueArrayIndex = args.MasteryId;
				itm->PrimaryValueDisplayType = UiValueDisplayType::Default;

				itm->SecondaryValueName = args.NextExpSymbol;
				itm->SecondaryValueArrayIndex = args.MasteryId;
				itm->SecondaryValueDisplayType = UiValueDisplayType::Default;
			}
		};

		lpValueItem->OnInit = [args](BaseUiElement* item)
		{
			item->Name = "SkillHeaderPanel_LpValue_" + args.NamePostfix;
			item->SizeX = 0.35f;
			item->SizeY = 0.10f;
			item->PosX = 0.55f;
			item->PosY = 0.43f;

			if (auto* itm = static_cast<MenuValueItem*>(item))
			{
				itm->IsArray = !args.IsCorruptionMastery && !args.IsCorruptionTouchMastery;
				itm->HorizontalAlign = UiContentAlignEnum::Begin;

				itm->PrimaryValueName = args.LpSymbol;
				itm->PrimaryValueArrayIndex = args.MasteryId;
				itm->PrimaryValueDisplayType = UiValueDisplayType::Default;
			}
		};

		MenuTextItem* descriptionItem = new MenuTextItem();
		descriptionItem->OnInit = [args](BaseUiElement* item)
		{
			item->Name = "SkillHeaderPanel_Description_" + args.NamePostfix;
			item->SizeX = 0.98f;
			item->SizeY = 0.54f;
			item->PosX = 0.01f;
			item->PosY = 0.45f;

			if (auto* itm = static_cast<MenuTextItem*>(item)) {
				itm->Text = parser->GetSymbol(args.DescSymbol)->stringdata[args.MasteryId];
			}
		};

		panel->AddItem(titleItem);
		panel->AddItem(levelTitleItem);
		panel->AddItem(expTitleItem);
		panel->AddItem(lpTitleItem);
		panel->AddItem(levelValueItem);
		panel->AddItem(expValueItem);
		panel->AddItem(lpValueItem);
		panel->AddItem(descriptionItem);
		
		return panel;
	}

	inline BaseMenuPanel* BuildCorruptionTouchStatPanel(const int statId, const float posY, const float sizeY)
	{
		const ExtraStatData* statData = GetExtraStatDataById(statId);
		if (!statData)
		{
			DEBUG_MSG("StExt Mod Ui - Fail to build stats panel (CorruptionTouch). StatId = " + Z(statId));
			return Null;
		}

		BaseMenuPanel* panel = new BaseMenuPanel();
		panel->OnInit = [statId, posY, sizeY](BaseUiElement* item)
		{
			item->Name = "CorruptionTouch_StatPanel_" + Z(statId);
			item->SizeX = 0.98f;
			item->SizeY = sizeY;
			item->PosX = 0.01f;
			item->PosY = posY;
			if (auto* itm = static_cast<BaseMenuPanel*>(item)) {
				itm->BehaviorFlags = UiElementBehaviorFlags::Hoverable | UiElementBehaviorFlags::Interactable;
			}
		};

		MenuItem* titleItem = new MenuItem();
		titleItem->OnInit = [statId](BaseUiElement* item)
		{
			item->Name = "CorruptionTouch_StatPanel_Title_" + Z(statId);
			item->SizeX = 0.75f;
			item->SizeY = 0.95f;
			item->PosX = 0.025f;
			item->PosY = 0.025f;

			if (auto* itm = static_cast<MenuItem*>(item))
			{
				itm->Text = GetExtraStatNameById(statId);
				itm->BehaviorFlags = UiElementBehaviorFlags::Hoverable | UiElementBehaviorFlags::Interactable;
				itm->HorizontalAlign = UiContentAlignEnum::Begin;
				itm->SetOwnedData<int>(statId);
			}
		};

		MenuValueItem* valueItem = new MenuValueItem();
		UiValueDisplayType displayType = (UiValueDisplayType)statData->ValueType;
		valueItem->OnInit = [statId, displayType](BaseUiElement* item)
		{
			item->Name = "CorruptionTouch_StatPanel_Value_" + Z(statId);
			item->SizeX = 0.115f;
			item->SizeY = 0.95f;
			item->PosX = 0.785f;
			item->PosY = 0.025f;

			if (auto* itm = static_cast<MenuValueItem*>(item))
			{
				itm->IsArray = true;
				itm->PrimaryValueArrayIndex = statId;
				itm->PrimaryValueName = "StExt_PcStats_Other";
				itm->PrimaryValueDisplayType = displayType;
				itm->BehaviorFlags = UiElementBehaviorFlags::Hoverable | UiElementBehaviorFlags::Interactable;
				itm->SetOwnedData<int>(statId);
			}
		};

		MenuItem* learnButton = new MenuItem();
		learnButton->OnInit = [statId](BaseUiElement* item)
		{
			item->Name = "CorruptionTouch_StatPanel_LearnButton_" + Z(statId);
			item->SizeX = 0.065f;
			item->SizeY = 0.95f;
			item->PosX = 0.91f;
			item->PosY = 0.025f;
			item->BgTexture = UiElement_ButtonTexture;

			if (auto* itm = static_cast<MenuItem*>(item))
			{
				itm->Text = "+";
				itm->Font = MenuFont_Sys_Default;
				itm->TextColor_Default = TextColor_HeaderTab_Default;
				itm->TextColor_Hovered = TextColor_Yes;
				itm->TextColor_Selected = TextColor_Yes;
				itm->TextColor_Disabled = TextColor_HeaderTab_Disabled;
				itm->BehaviorFlags = UiElementBehaviorFlags::Hoverable | UiElementBehaviorFlags::Selectable | UiElementBehaviorFlags::Interactable;

				itm->SetOwnedData<int>(statId);
				itm->OnMouseEvent = CorruptionTouch_OnLearnStatButtonClick;
				itm->OnUpdate = CorruptionTouch_OnLearnStatButtonUpdate;
			}
		};

		panel->AddItem(titleItem);
		panel->AddItem(valueItem);
		panel->AddItem(learnButton);
		return panel;
	}

	inline MenuItem* BuildSkillsForgetButtom(const int index, const float posY)
	{
		MenuItem* button = new MenuItem();
		button->OnInit = [index, posY](BaseUiElement* item)
		{
			item->Name = "SkillPanel_ForgetButtom_" + Z(index);
			item->SizeX = 0.70f;
			item->SizeY = 0.10f;
			item->PosX = 0.15f;
			item->PosY = posY;
			item->BgTexture = UiElement_ButtonTexture;

			if (auto* itm = static_cast<MenuItem*>(item))
			{
				itm->Text = parser->GetSymbol("StExt_Str_ForgetPerks")->stringdata;
				itm->Font = MenuFont_Header;
				itm->TextColor_Default = TextColor_No;
				itm->TextColor_Hovered = TextColor_HeaderTab_Hovered;
				itm->TextColor_Selected = TextColor_HeaderTab_Selected;
				itm->TextColor_Disabled = TextColor_HeaderTab_Disabled;
				itm->BehaviorFlags = UiElementBehaviorFlags::Hoverable | UiElementBehaviorFlags::Selectable | UiElementBehaviorFlags::Interactable;

				itm->SetOwnedData<int>(index);
				itm->OnMouseEvent = StatsWindow_OnLearnSkillForgetButtonClick;
			}
		};
		return button;
	}

	void StatsWindow::InitSkillsTab()
	{
		SkillsTabPanel = new BaseMenuPanel();
		SkillsTabPanel->OnInit = [](BaseUiElement* item)
		{
			item->Name = "SkillsTabPanel";
			item->SizeX = 0.98f;
			item->SizeY = 0.88f;
			item->PosX = 0.01f;
			item->PosY = 0.11f;
			item->BgTexture = UiElement_PanelNoBorderTexture;
		};
		AddItem(SkillsTabPanel);

		MenuListView* masteriesListView = new MenuListView();
		masteriesListView->OnInit = [](BaseUiElement* item)
		{
			item->Name = "MasteriesListView";
			item->SizeX = 0.18f;
			item->SizeY = 0.98f;
			item->PosX = 0.01f;
			item->PosY = 0.01f;
			item->BgTexture = UiElement_ListBorderTexture;
			if (auto* itm = static_cast<MenuListView*>(item))
			{
				itm->ItemHeight = 0.03f;
				itm->ItemMargin = 0.02f;
			}
		};
		SkillsTabPanel->AddItem(masteriesListView);

		MasteryIndexMax = ExtraMasteriesData.GetNum();
		for (int index = 0; index < MasteryIndexMax; ++index)
		{
			auto& masteryData = ExtraMasteriesData[index];
			MenuItem* masteriesListItem = new MenuItem();
			masteriesListItem->OnInit = [index, masteryData](BaseUiElement* item)
			{
				item->Name = "MasteriesListViewItem_" + Z(index);
				item->SizeX = 0.98f;
				item->SizeY = 0.05f;
				item->PosX = 0.01f;
				item->PosY = 0.01f;

				if (auto* itm = static_cast<MenuItem*>(item))
				{
					itm->IsSelected = index == 0;
					itm->Text = parser->GetSymbol(masteryData.TitleSymbol)->stringdata[masteryData.MasteryId];
					itm->BehaviorFlags = UiElementBehaviorFlags::Hoverable | UiElementBehaviorFlags::Selectable | UiElementBehaviorFlags::Interactable | UiElementBehaviorFlags::Checkable;
					itm->SetOwnedData<int>(index);
					itm->OnMouseEvent = StatsWindow_OnMasteryItemClick;
				}
			};
			masteriesListView->AddItem(masteriesListItem);

			MenuScrollPanel* masteriesPanel = new MenuScrollPanel();
			masteriesPanel->OnInit = [index](BaseUiElement* item)
			{
				item->Name = "MasteriesPanel_" + Z(index);
				item->SizeX = 0.79f;
				item->SizeY = 0.98f;
				item->PosX = 0.20f;
				item->PosY = 0.01f;
				item->IsHiden = true;
			};
			SkillPanels.InsertEnd(masteriesPanel);
			SkillsTabPanel->AddItem(masteriesPanel);

			float yOffset = 0.01f;
			zSTRING namePostfix = "";
			if (!masteryData.IsGeneric)
			{
				SkillHeaderPanelArgs skillsHeaderPanelArgs = SkillHeaderPanelArgs();
				BuildSkillPostfixName(namePostfix, masteryData.MasteryId, 0, masteryData.IsCorruption, masteryData.IsGeneric, index);
				
				skillsHeaderPanelArgs.MasteryId = masteryData.MasteryId;
				skillsHeaderPanelArgs.PosY = yOffset;
				skillsHeaderPanelArgs.IsCorruptionMastery = masteryData.IsCorruption;
				skillsHeaderPanelArgs.IsCorruptionTouchMastery = masteryData.IsCorruptionTouch;
				skillsHeaderPanelArgs.NamePostfix = namePostfix;

				skillsHeaderPanelArgs.TitleSymbol = masteryData.TitleSymbol;
				skillsHeaderPanelArgs.DescSymbol = masteryData.DescSymbol;
				skillsHeaderPanelArgs.LevelSymbol = masteryData.LevelSymbol;
				skillsHeaderPanelArgs.RankSymbol = masteryData.RankSymbol;
				skillsHeaderPanelArgs.ExpSymbol = masteryData.ExpSymbol;
				skillsHeaderPanelArgs.NextExpSymbol = masteryData.NextExpSymbol;
				skillsHeaderPanelArgs.LpSymbol = masteryData.LpSymbol;

				BaseMenuPanel* skillsHeaderPanel = BuildSkillHeaderPanel(skillsHeaderPanelArgs);
				masteriesPanel->AddItem(skillsHeaderPanel);
				yOffset += skillsHeaderPanel->SizeY + 0.05f;
			}

			if (!masteryData.IsCorruptionTouch)
			{
				for (int perkId = 0; perkId < masteryData.PerksCount; ++perkId)
				{
					SkillPanelArgs skillPanelArgs = SkillPanelArgs();
					BuildSkillPostfixName(namePostfix, masteryData.MasteryId, perkId, masteryData.IsCorruption, masteryData.IsGeneric, index);

					skillPanelArgs.SkillId = perkId;
					skillPanelArgs.MasteryId = masteryData.MasteryId;
					skillPanelArgs.MasteryTreeIndex = index;
					skillPanelArgs.PosY = yOffset;
					skillPanelArgs.IsCorruptionMastery = masteryData.IsCorruption;
					skillPanelArgs.IsGenericMastery = masteryData.IsGeneric;
					skillPanelArgs.NamePostfix = namePostfix;
					skillPanelArgs.TitleSymbol = masteryData.PerkNameSymbol;
					skillPanelArgs.DescSymbol = masteryData.PerkDescSymbol;
					skillPanelArgs.ValueSymbol = masteryData.PerkValueSymbol;

					BaseMenuPanel* skillPanel = BuildSkillPanel(skillPanelArgs);
					yOffset += skillPanel->SizeY + 0.01f;
					MenuItem* skillSeparator = BuildHSeparator(yOffset);
					yOffset += skillSeparator->SizeY + 0.03f;

					masteriesPanel->AddItem(skillPanel);
					masteriesPanel->AddItem(skillSeparator);
				}
			}
			else
			{
				for (auto& pair : CorruptionTouchStatsData)
				{
					CorruptionTouchStatData& statData = pair.GetValue();
					BaseMenuPanel* statPanel = BuildCorruptionTouchStatPanel(statData.StatId, yOffset, 0.035f);
					yOffset += 0.045f;
					masteriesPanel->AddItem(statPanel);
				}
			}

			if (masteryData.CanResetPerks)
			{
				yOffset += 0.1f;
				masteriesPanel->AddItem(BuildSkillsForgetButtom(index, yOffset));
				yOffset += 0.15f;
			}
		}

		SkillsTabPanel->Init();
	}

	//----------------------------------------------------------------------
	//								WINDOW
	//----------------------------------------------------------------------

	inline BaseMenuPanel* StatsWindow::GetTabPanel(const int index)
	{
		switch (index)
		{
			case 0: return GeneralTabPanel;
			case 1: return StatsTabPanel;
			case 2: return SkillsTabPanel;
			case 3: return ProfesionTabPanel;
			case 4: return BonusTabPanel;
		}
		return Null;
	}

	StatsWindow::StatsWindow() 
	{
		SkillPanels = Array<MenuScrollPanel*>();
		TabPanel = Null;
		SkillsTabPanel = Null;
		GeneralTabPanel = Null;
		StatsTabPanel = Null;
		ProfesionTabPanel = Null;
		BonusTabPanel = Null;
	}

	void StatsWindow::Resize() { MenuWindow::Resize(); }

	void StatsWindow::Init()
	{
		OnInit = [](BaseUiElement* item) {
			item->Name = "StatsWindow";
			item->BgTexture = UiElement_PanelTexture;
			item->Resize();
		};
		MenuWindow::Init();

		InitTabs();
		InitGeneralTab();
		InitStatsTab();
		InitSkillsTab();
		InitProfesionTab();
		InitBonusTab();

		Instance = this;
	}

	void StatsWindow::Draw() { MenuWindow::Draw(); }

	void StatsWindow::Update() 
	{
		MenuWindow::Update();

		for (int i = 0; i < TabIndexMax; ++i)
		{
			BaseMenuPanel* panel = GetTabPanel(i);
			if (panel == Null) continue;
			panel->IsHiden = TabIndex != i;
		}

		BaseMenuPanel* currentTabPanel = GetTabPanel(TabIndex);
		if (!currentTabPanel) return;

		if (currentTabPanel == SkillsTabPanel)
		{
			for (int i = 0; i < MasteryIndexMax; ++i)
			{
				BaseMenuPanel* panel = SkillPanels[i];
				if (panel == Null) continue;
				panel->IsHiden = MasteryIndex != i;
			}
		}
	}

	StatsWindow::~StatsWindow() 
	{
		SkillPanels.Clear();
		TabPanel = Null;
		SkillsTabPanel = Null;
		GeneralTabPanel = Null;
		StatsTabPanel = Null;
		ProfesionTabPanel = Null;
		BonusTabPanel = Null;
	}
}