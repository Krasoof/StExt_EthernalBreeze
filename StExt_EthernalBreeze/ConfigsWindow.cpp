#include <StonedExtension.h>

namespace Gothic_II_Addon
{
	ConfigsWindow* Instance = Null;
	zSTRING ThxList[] = 
	{
		"Piranha Bytes",
		"Liker (New Balance mod)",
		"Haart (New Balance mod)",
		"Gratt (zParserExtender)",
		"Gokaq",
		"DanilaDNL",
		"And all artists, whose resources were used in this mod",
		"",
		"Icefist",
		"ToXaL1",
		"lorddemonik",
		"Shiva",
		"Junes",
		"Khellzhao",
		"Gothicmap",
		"Dezomorphin",
		"lev4enko",
		"Denis Bagretsov",
		"ZauronixRus"
	};

	int ConfigsExportButtonCooldown = 0;

	struct ConfigPanelArgs
	{
		float PosY;
		int DisplayType;
		bool IsEditable;
		zSTRING TitleSymbol;
		zSTRING ValueSymbol;
	};

	//----------------------------------------------------------------------
	//								HANDLERS
	//----------------------------------------------------------------------

	bool ConfigsWindow_OnTabButtonClick(BaseMenuElement* item, const UiMouseEventArgs& args)
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

	bool ConfigsWindow_OnConfigCatItemClick(BaseMenuElement* item, const UiMouseEventArgs& args)
	{
		if (!item || !Instance) return false;
		if (args.Action != UiMouseEnum::LeftClick) return false;

		MenuItem* itm = static_cast<MenuItem*>(item);
		if (itm)
		{
			int index = *itm->GetData<int>();
			if (index < 0) index = 0;
			if (index >= Instance->ConfigPanelIndexMax) index = Instance->ConfigPanelIndexMax - 1;
			Instance->ConfigPanelIndex = index;
		}
		return true;
	}

	void ChangeConfigValue(zSTRING& valueName, float changeValue)
	{
		zCPar_Symbol* sym = parser->GetSymbol(valueName);
		auto pair = ExtraConfigsData.Find(valueName);

		if (!sym || !pair) return;
		ExtraConfigData& data = *pair;

		float currentValue;
		if (sym->type == zPAR_TYPE_FLOAT) currentValue = sym->single_floatdata;		
		else if (sym->type == zPAR_TYPE_INT) currentValue = static_cast<float>(sym->single_intdata);
		else { return; }

		currentValue += changeValue;
		if (currentValue > static_cast<float>(data.MaxValue)) currentValue = static_cast<float>(data.MaxValue);
		if (currentValue < static_cast<float>(data.MinValue)) currentValue = static_cast<float>(data.MinValue);

		if (sym->type == zPAR_TYPE_FLOAT) sym->SetValue(currentValue, 0);		
		else sym->SetValue(static_cast<int>(currentValue), 0);
	}

	bool ConfigsWindow_OnConfigToggleButtonClick(BaseMenuElement* item, const UiMouseEventArgs& args)
	{
		if (!item || !Instance) return false;
		if (args.Action != UiMouseEnum::LeftClick) return false;

		MenuItem* itm = static_cast<MenuItem*>(item);
		if (itm)
		{
			ConfigPanelArgs* data = itm->GetData<ConfigPanelArgs>();
			if (!data) return true;

			zCPar_Symbol* sym = parser->GetSymbol(data->ValueSymbol);
			if (!sym) return true;

			bool currentValue = false;
			switch (sym->type)
			{
				case zPAR_TYPE_INT:
					currentValue = sym->single_intdata != 0;
					sym->SetValue(currentValue ? 0 : 1, 0);
					break;

				case zPAR_TYPE_FLOAT:
					currentValue = sym->single_floatdata != 0;
					sym->SetValue(currentValue ? 0.0f : 1.0f, 0);
					break;
			}
		}
		return true;
	}
	bool ConfigsWindow_OnConfigPlusButtonClick(BaseMenuElement* item, const UiMouseEventArgs& args)
	{
		if (!item || !Instance) return false;
		if (args.Action != UiMouseEnum::LeftClick) return false;
		
		MenuItem* itm = static_cast<MenuItem*>(item);
		if (itm)
		{
			ConfigPanelArgs* data = itm->GetData<ConfigPanelArgs>();
			if (!data) return true;

			float changeValue = args.IsAltPressed ? 1000.0f : 1.0f;
			if (args.IsShiftPressed) changeValue *= 10.0f;
			
			ChangeConfigValue(data->ValueSymbol, changeValue);
		}
		return true;
	}
	bool ConfigsWindow_OnConfigMinusButtonClick(BaseMenuElement* item, const UiMouseEventArgs& args)
	{
		if (!item || !Instance) return false;
		if (args.Action != UiMouseEnum::LeftClick) return false;

		MenuItem* itm = static_cast<MenuItem*>(item);
		if (itm)
		{
			ConfigPanelArgs* data = itm->GetData<ConfigPanelArgs>();
			if (!data) return true;

			float changeValue = args.IsAltPressed ? -1000.0f : -1.0f;
			if (args.IsShiftPressed) changeValue *= 10.0f;

			ChangeConfigValue(data->ValueSymbol, changeValue);
		}
		return true;
	}

	void ConfigsWindow_OnConfigToggleButtonUpdate(BaseUiElement* button)
	{
		if (!button || !Instance) return;

		MenuItem* itm = static_cast<MenuItem*>(button);
		if (itm)
		{
			ConfigPanelArgs* data = itm->GetData<ConfigPanelArgs>();
			if (!data) return;

			UpdateCheckButtonStyle(itm, data->ValueSymbol);
			if (!data->IsEditable)
			{
				itm->IsEnabled = data->IsEditable;
				return;
			}			
		}
	}
	void ConfigsWindow_OnConfigPlusButtonUpdate(BaseUiElement* button)
	{
		if (!button || !Instance) return;

		MenuItem* itm = static_cast<MenuItem*>(button);
		if (itm)
		{
			ConfigPanelArgs* argsData = itm->GetData<ConfigPanelArgs>();
			if (!argsData) return;

			if (!argsData->IsEditable)
			{
				itm->IsEnabled = argsData->IsEditable;
				return;
			}

			zCPar_Symbol* sym = parser->GetSymbol(argsData->ValueSymbol);
			auto pair = ExtraConfigsData.Find(argsData->ValueSymbol);

			if (!sym || !pair) return;
			ExtraConfigData& data = *pair;

			float currentValue;
			switch (sym->type)
			{
				case zPAR_TYPE_INT:
					currentValue = static_cast<float>(sym->single_intdata);
					break;

				case zPAR_TYPE_FLOAT:
					currentValue = sym->single_floatdata;
					break;

				default:
					return;
			}

			itm->IsEnabled = currentValue < data.MaxValue;
		}
	}
	void ConfigsWindow_OnConfigMinusButtonUpdate(BaseUiElement* button)
	{
		if (!button || !Instance) return;

		MenuItem* itm = static_cast<MenuItem*>(button);
		if (itm)
		{
			ConfigPanelArgs* argsData = itm->GetData<ConfigPanelArgs>();
			if (!argsData) return;

			if (!argsData->IsEditable)
			{
				itm->IsEnabled = argsData->IsEditable;
				return;
			}

			zCPar_Symbol* sym = parser->GetSymbol(argsData->ValueSymbol);
			auto pair = ExtraConfigsData.Find(argsData->ValueSymbol);

			if (!sym || !pair) return;
			ExtraConfigData& data = *pair;

			float currentValue;
			switch (sym->type)
			{
			case zPAR_TYPE_INT:
				currentValue = static_cast<float>(sym->single_intdata);
				break;

			case zPAR_TYPE_FLOAT:
				currentValue = sym->single_floatdata;
				break;

			default:
				return;
			}

			itm->IsEnabled = currentValue > data.MinValue;
		}
	}

	void ConfigsWindow_OnCurrentDiffPresetsPanelUpdate(BaseUiElement* button)
	{
		if (!button || !Instance) return;
		MenuItem* itm = static_cast<MenuItem*>(button);
		if (itm)
		{
			zSTRING presetId = parser->GetSymbol("StExt_CurrentUserConfigs")->stringdata;			
			const ConfigPresetData* config = GetConfigPreset(presetId);
			if (config)
			{
				itm->Text = CurrentDiffPresetStr + config->Text + " (" + config->Name + ")";
				ParseHexColor(config->TextColor.ToChar(), itm->TextColor_Default);
			}
			else 
			{
				itm->Text = CurrentDiffPresetStr + "???";
				itm->TextColor_Default = TextColor_Regular_Default;
			}
		}
	}
	void ConfigsWindow_OnCurrentItemsPresetsPanelUpdate(BaseUiElement* button)
	{
		if (!button || !Instance) return;
		MenuItem* itm = static_cast<MenuItem*>(button);
		if (itm)
		{
			zSTRING itemsPresetName = parser->GetSymbol("StExt_CurrentItemGeneratorConfigs")->stringdata;
			itm->Text = CurrentItemsPresetStr + (itemsPresetName.IsEmpty() ? "ItemGeneratorConfigs.json" : itemsPresetName);
			zSTRING presetId = parser->GetSymbol("StExt_CurrentUserConfigs")->stringdata;
			const ConfigPresetData* config = GetConfigPreset(presetId);
			if (config)
				ParseHexColor(config->TextColor.ToChar(), itm->TextColor_Default);			
			else itm->TextColor_Default = TextColor_Regular_Default;			
		}
	}

	void ConfigsWindow_OnExportConfigsButtonUpdate(BaseUiElement* button)
	{
		if (!button || !Instance) return;
		MenuItem* itm = static_cast<MenuItem*>(button);
		if (itm)
		{
			if (--ConfigsExportButtonCooldown < 0) ConfigsExportButtonCooldown = 0;
			itm->IsEnabled = ConfigsExportButtonCooldown <= 0;
		}
	}
	bool ConfigsWindow_OnExportConfigsButtonClick(BaseMenuElement* item, const UiMouseEventArgs& args)
	{
		if (!item || !Instance) return false;
		if (args.Action != UiMouseEnum::LeftClick) return false;
		static int exportScriptFuncIndex = parser->GetIndex("StExt_ExportCurrentConfigs_Script");

		MenuItem* itm = static_cast<MenuItem*>(item);
		if (itm && (ConfigsExportButtonCooldown <= 0))
		{
			parser->CallFunc(exportScriptFuncIndex);
			ConfigsExportButtonCooldown = 120;
		}
		return true;
	}

	bool ConfigsWindow_OnSelectConfigsButtonClick(BaseMenuElement* item, const UiMouseEventArgs& args)
	{
		if (!item || !Instance) return false;
		if (args.Action != UiMouseEnum::LeftClick) return false;
		static int selectScriptFuncIndex = parser->GetIndex("StExt_SelectConfigs_Script");
		static int returnStringSymIndex = parser->GetIndex("StExt_ReturnString");

		MenuItem* itm = static_cast<MenuItem*>(item);
		if (itm)
		{
			uint configIndex = *itm->GetData<uint>();
			parser->GetSymbol(returnStringSymIndex)->SetValue(GameConfigsPresets[configIndex].Name, 0);
			parser->CallFunc(selectScriptFuncIndex);
		}
		return true;
	}


	void ConfigsWindow_OnClose(MenuWindow* wnd)
	{
		if (!wnd || !Instance) return;
		auto skinSym = parser->GetSymbol("StExt_UpdateSkin");
		if (skinSym)
			skinSym->SetValue(true, 0);
	}

	//----------------------------------------------------------------------
	//								TABS PANEL
	//----------------------------------------------------------------------

	void ConfigsWindow::InitTabs()
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

		const int tabCount = parser->GetSymbol("StExt_ConfigsMenu_TabName")->ele;
		const float margin = 0.025f;
		const float gap = 0.01f;
		const float tabWidth = 0.25f;
		const float totalTabsWidth = tabCount * tabWidth + (tabCount - 1) * gap;

		TabIndexMax = tabCount;
		float x = 0.5f - totalTabsWidth * 0.5f;
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
					itm->Text = parser->GetSymbol("StExt_ConfigsMenu_TabName")->stringdata[i];
					itm->Font = MenuFont_Header;
					itm->TextColor_Default = TextColor_HeaderTab_Default;
					itm->TextColor_Hovered = TextColor_HeaderTab_Hovered;
					itm->TextColor_Selected = TextColor_HeaderTab_Selected;
					itm->TextColor_Disabled = TextColor_HeaderTab_Disabled;
					itm->BehaviorFlags = UiElementBehaviorFlags::Hoverable | UiElementBehaviorFlags::Selectable | UiElementBehaviorFlags::Interactable | UiElementBehaviorFlags::Checkable;
					itm->SetOwnedData<int>(i);
					itm->OnMouseEvent = ConfigsWindow_OnTabButtonClick;
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

	//----------------------------------------------------------------------
	//							CONFIGS PANEL
	//----------------------------------------------------------------------

	inline BaseMenuPanel* BuildConfigPanel(const ConfigPanelArgs& args)
	{
		BaseMenuPanel* panel = new BaseMenuPanel();
		panel->OnInit = [args](BaseUiElement* item)
		{
			item->Name = "ConfigPanel_" + args.ValueSymbol;
			item->SizeX = 0.98f;
			item->SizeY = 0.045f;
			item->PosX = 0.01f;
			item->PosY = args.PosY; 

			if (auto* itm = static_cast<BaseMenuPanel*>(item)) {
				itm->BehaviorFlags = UiElementBehaviorFlags::Hoverable | UiElementBehaviorFlags::Interactable;
			}
		};
		panel->SizeY = 0.05f;


		MenuItem* titleItem = new MenuItem();
		titleItem->OnInit = [args](BaseUiElement* item)
		{
			item->Name = "ConfigPanel_Title_" + args.ValueSymbol;
			item->SizeX = 0.69f;
			item->SizeY = 0.98f;
			item->PosX = 0.01f;
			item->PosY = 0.01f;

			if (auto* itm = static_cast<MenuItem*>(item))
			{
				zCPar_Symbol* nameSym = parser->GetSymbol(args.TitleSymbol);
				itm->Text = nameSym ? nameSym->stringdata : 
					!args.TitleSymbol.IsEmpty() ? args.TitleSymbol : zString_Unknown;
				itm->BehaviorFlags = UiElementBehaviorFlags::Hoverable | UiElementBehaviorFlags::Interactable;
				itm->HorizontalAlign = UiContentAlignEnum::Begin;
			}
		};		
		

		if (args.DisplayType == Value_Type_YesNo)
		{
			MenuItem* toggleButton = new MenuItem();
			toggleButton->OnInit = [args](BaseUiElement* item)
			{
				item->Name = "ConfigPanel_ToggleButton_" + args.ValueSymbol;
				item->SizeX = 0.10f;
				item->SizeY = 0.95f;
				item->PosX = 0.72f;
				item->PosY = 0.025f;
				item->BgTexture = UiElement_ButtonTexture;

				if (auto* itm = static_cast<MenuItem*>(item))
				{
					itm->Text = "";
					itm->Font = MenuFont_Sys_Default;
					itm->IsEnabled = args.IsEditable;
					itm->SetOwnedData<ConfigPanelArgs>(args);
					itm->BehaviorFlags = UiElementBehaviorFlags::Hoverable | UiElementBehaviorFlags::Selectable | UiElementBehaviorFlags::Interactable;
					itm->OnUpdate = ConfigsWindow_OnConfigToggleButtonUpdate;
					itm->OnMouseEvent = ConfigsWindow_OnConfigToggleButtonClick;
				}
			};
			panel->AddItem(toggleButton);
		}
		else
		{
			MenuItem* minusButton = new MenuItem();
			minusButton->OnInit = [args](BaseUiElement* item)
			{
				item->Name = "ConfigPanel_MinusButton_" + args.ValueSymbol;
				item->SizeX = 0.05f;
				item->SizeY = 0.95f;
				item->PosX = 0.72f;
				item->PosY = 0.025f;
				item->BgTexture = UiElement_ButtonSquareTexture;

				if (auto* itm = static_cast<MenuItem*>(item))
				{
					itm->Text = "-";
					itm->Font = MenuFont_Symbol_Default;
					itm->TextColor_Hovered = TextColor_No;
					itm->SetOwnedData<ConfigPanelArgs>(args);
					itm->BehaviorFlags = UiElementBehaviorFlags::Hoverable | UiElementBehaviorFlags::Selectable | UiElementBehaviorFlags::Interactable;
					itm->OnUpdate = ConfigsWindow_OnConfigMinusButtonUpdate;
					itm->OnMouseEvent = ConfigsWindow_OnConfigMinusButtonClick;
				}
			};

			MenuItem* plusButton = new MenuItem();
			plusButton->OnInit = [args](BaseUiElement* item)
			{
				item->Name = "ConfigPanel_PlusButton_" + args.ValueSymbol;
				item->SizeX = 0.05f;
				item->SizeY = 0.95f;
				item->PosX = 0.77f;
				item->PosY = 0.025f;
				item->BgTexture = UiElement_ButtonSquareTexture;

				if (auto* itm = static_cast<MenuItem*>(item))
				{
					itm->Text = "+";
					itm->Font = MenuFont_Symbol_Default;
					itm->TextColor_Hovered = TextColor_Yes;
					itm->SetOwnedData<ConfigPanelArgs>(args);
					itm->BehaviorFlags = UiElementBehaviorFlags::Hoverable | UiElementBehaviorFlags::Selectable | UiElementBehaviorFlags::Interactable;
					itm->OnUpdate = ConfigsWindow_OnConfigPlusButtonUpdate;
					itm->OnMouseEvent = ConfigsWindow_OnConfigPlusButtonClick;
				}
			};

			panel->AddItem(minusButton);
			panel->AddItem(plusButton);
		}


		MenuValueItem* valueItem = new MenuValueItem();
		valueItem->OnInit = [args](BaseUiElement* item)
		{
			item->Name = "ConfigPanel_Value_" + args.ValueSymbol;
			item->SizeX = 0.15f;
			item->SizeY = 0.98f;
			item->PosX = 0.84f;
			item->PosY = 0.01f;

			if (auto* itm = static_cast<MenuValueItem*>(item))
			{
				itm->PrimaryValueArrayIndex = 0;
				itm->PrimaryValueName = args.ValueSymbol;
				itm->PrimaryValueDisplayType = (UiValueDisplayType)args.DisplayType;
				itm->BehaviorFlags = UiElementBehaviorFlags::Hoverable | UiElementBehaviorFlags::Interactable;
			}
		};


		panel->AddItem(titleItem);
		panel->AddItem(valueItem);
		return panel;
	}


	void ConfigsWindow::InitConfigsTab()
	{
		ConfigsTabPanel = new BaseMenuPanel();
		ConfigsTabPanel->OnInit = [](BaseUiElement* item)
		{
			item->Name = "ConfigsTabPanel";
			item->SizeX = 0.98f;
			item->SizeY = 0.88f;
			item->PosX = 0.01f;
			item->PosY = 0.11f;
			item->BgTexture = UiElement_PanelNoBorderTexture;
		};
		AddItem(ConfigsTabPanel);

		MenuListView* configsCatListView = new MenuListView();
		configsCatListView->OnInit = [](BaseUiElement* item)
		{
			item->Name = "ConfigsCatListView";
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
		ConfigsTabPanel->AddItem(configsCatListView);

		zCPar_Symbol* configCatatNameArraySym = parser->GetSymbol("StExt_Str_Config_Category_Name");
		ConfigPanelIndexMax = configCatatNameArraySym->ele;

		for (int catIndex = 0; catIndex < ConfigPanelIndexMax; ++catIndex)
		{
			MenuItem* configsCatListItem = new MenuItem();
			configsCatListItem->OnInit = [catIndex](BaseUiElement* item)
			{
				item->Name = "ConfigsCatListItem_" + Z(catIndex);
				item->SizeX = 0.98f;
				item->SizeY = 0.05f;
				item->PosX = 0.01f;
				item->PosY = 0.01f;

				if (auto* itm = static_cast<MenuItem*>(item))
				{
					itm->IsSelected = catIndex == 0;
					itm->Text = parser->GetSymbol("StExt_Str_Config_Category_Name")->stringdata[catIndex];
					itm->BehaviorFlags = UiElementBehaviorFlags::Hoverable | UiElementBehaviorFlags::Selectable | UiElementBehaviorFlags::Interactable | UiElementBehaviorFlags::Checkable;
					itm->SetOwnedData<int>(catIndex);
					itm->OnMouseEvent = ConfigsWindow_OnConfigCatItemClick;
				}
			};
			configsCatListView->AddItem(configsCatListItem);

			MenuScrollPanel* configsCatPanel = new MenuScrollPanel();
			configsCatPanel->OnInit = [catIndex](BaseUiElement* item)
			{
				item->Name = "ConfigsCatPanel_" + Z(catIndex);
				item->SizeX = 0.79f;
				item->SizeY = 0.98f;
				item->PosX = 0.20f;
				item->PosY = 0.01f;
				item->IsHiden = true;
			};
			ConfigsPanels.InsertEnd(configsCatPanel);
			ConfigsTabPanel->AddItem(configsCatPanel);

			Array<ExtraConfigData*> configsForCategory;
			for (auto& configData : ExtraConfigsData)
			{
				if (configData.ConfigGroup != catIndex)
					continue;

				configsForCategory.InsertEnd(&configData);
			}

			std::sort(configsForCategory.begin(), configsForCategory.end(), 
				[](const ExtraConfigData* a, const ExtraConfigData* b) { return a->DisplayOrder < b->DisplayOrder; });

			float yOffset = 0.01f;
			for (ExtraConfigData* configData : configsForCategory)
			{
				ConfigPanelArgs configArgs = ConfigPanelArgs();
				configArgs.TitleSymbol = configData->DescSymbol;
				configArgs.ValueSymbol = configData->ValueSymbol;
				configArgs.DisplayType = configData->DisplayType;
				configArgs.IsEditable = configData->IsEditable;
				configArgs.PosY = yOffset;

				BaseMenuPanel* configPanel = BuildConfigPanel(configArgs);
				yOffset += configPanel->SizeY + 0.005f;
				configsCatPanel->AddItem(configPanel);
			}
		}
		ConfigsTabPanel->Init();
	}

	//----------------------------------------------------------------------
	//								MODINFO
	//----------------------------------------------------------------------

	inline MenuItem* BuildThxPanel(const zSTRING line, const float posY, const int id, const bool isMajor)
	{
		MenuItem* titleItem = new MenuItem();
		titleItem->OnInit = [line, posY, id, isMajor](BaseUiElement* item)
		{
			item->Name = "ThxPanel_" + Z(id);
			item->SizeX = 0.98f;
			item->SizeY = 0.03f;
			item->PosX = 0.01f;
			item->PosY = posY;

			if (auto* itm = static_cast<MenuItem*>(item))
			{
				itm->Text = line;
				itm->BehaviorFlags = UiElementBehaviorFlags::Hoverable | UiElementBehaviorFlags::Interactable;
				if(isMajor)
					itm->TextColor_Default = TextColor_Header_Default;
				itm->HorizontalAlign = UiContentAlignEnum::Center;
				itm->VerticalAlign = UiContentAlignEnum::Center;
			}
		};
		return titleItem;
	}

	inline MenuItem* BuildCopyrightPanel(const zSTRING line, const float posY, const int id, const bool isHeader = false)
	{
		MenuItem* titleItem = new MenuItem();
		titleItem->OnInit = [line, posY, id, isHeader](BaseUiElement* item)
		{
			item->Name = "CopyrightPanel_" + Z(id);
			item->SizeX = 0.98f;
			item->SizeY = isHeader ? 0.05f : 0.03f;
			item->PosX = 0.01f;
			item->PosY = posY;
			item->BgTexture = UiElement_PanelNoBorderTexture;

			if (auto* itm = static_cast<MenuItem*>(item))
			{
				itm->Text = line;
				if (isHeader)
					itm->Font = MenuFont_Header;
				itm->BehaviorFlags = UiElementBehaviorFlags::Hoverable | UiElementBehaviorFlags::Interactable;
				itm->TextColor_Default = zCOLOR(130, 80, 250);
				itm->HorizontalAlign = UiContentAlignEnum::Center;
				itm->VerticalAlign = UiContentAlignEnum::Center;
			}
		};
		return titleItem;
	}


	void ConfigsWindow::InitModInfoTab()
	{
		ModInfoPanel = new MenuScrollPanel();
		ModInfoPanel->OnInit = [](BaseUiElement* item)
		{
			item->Name = "ModInfoPanel";
			item->SizeX = 0.98f;
			item->SizeY = 0.88f;
			item->PosX = 0.01f;
			item->PosY = 0.11f;
			item->BgTexture = UiElement_PanelNoBorderTexture;
		};
		AddItem(ModInfoPanel);
		float yOffset = 0.025f;

		MenuItem* currentDiffPresetItem = new MenuItem();
		currentDiffPresetItem->OnInit = [yOffset](BaseUiElement* item)
		{
			item->Name = "DiffPresetItem";
			item->SizeX = 0.99f;
			item->SizeY = 0.03f;
			item->PosX = 0.01f;
			item->PosY = yOffset;

			if (auto* itm = static_cast<MenuItem*>(item))
			{
				itm->Text = "";
				itm->TextColor_Default = TextColor_Regular_Default;
				itm->OnUpdate = ConfigsWindow_OnCurrentDiffPresetsPanelUpdate;
			}
		};
		ModInfoPanel->AddItem(currentDiffPresetItem);
		yOffset += 0.04f;

		MenuItem* currentItemsPresetItem = new MenuItem();
		currentItemsPresetItem->OnInit = [yOffset](BaseUiElement* item)
		{
			item->Name = "ItemsPresetItem";
			item->SizeX = 0.99f;
			item->SizeY = 0.03f;
			item->PosX = 0.01f;
			item->PosY = yOffset;

			if (auto* itm = static_cast<MenuItem*>(item))
			{
				itm->Text = "";
				itm->TextColor_Default = TextColor_Regular_Default;
				itm->OnUpdate = ConfigsWindow_OnCurrentItemsPresetsPanelUpdate;
			}
		};
		ModInfoPanel->AddItem(currentItemsPresetItem);

		yOffset += 0.05f;
		ModInfoPanel->AddItem(BuildHSeparator(yOffset));
		yOffset += 0.05f;

		MenuItem* loadConfigsTitleItem = new MenuItem();
		loadConfigsTitleItem->OnInit = [yOffset](BaseUiElement* item)
		{
			item->Name = "LoadConfigsTitle";
			item->SizeX = 0.98f;
			item->SizeY = 0.05f;
			item->PosX = 0.01f;
			item->PosY = yOffset;

			if (auto* itm = static_cast<MenuItem*>(item))
			{
				itm->Text = parser->GetSymbol("StExt_Str_ModMenu_PresetsSection")->stringdata;
				itm->HorizontalAlign = UiContentAlignEnum::Center;
				itm->VerticalAlign = UiContentAlignEnum::Center;
			}
		};
		ModInfoPanel->AddItem(loadConfigsTitleItem);
		yOffset += 0.07f;

		for (uint i = 0; i < GameConfigsPresets.GetNum(); i++)
		{
			MenuItem* selectConfigBtn = new MenuItem();
			selectConfigBtn->OnInit = [yOffset, i](BaseUiElement* item)
			{
				item->Name = "ExportConfigsButton";
				item->SizeX = 0.30f;
				item->SizeY = 0.06f;
				item->PosX = 0.35f;
				item->PosY = yOffset;
				item->BgTexture = UiElement_ButtonTexture;

				if (auto* itm = static_cast<MenuItem*>(item))
				{
					itm->Text = GameConfigsPresets[i].Text;
					ParseHexColor(GameConfigsPresets[i].TextColor.ToChar(), itm->TextColor_Default);
					itm->BehaviorFlags = UiElementBehaviorFlags::Hoverable | UiElementBehaviorFlags::Selectable | UiElementBehaviorFlags::Interactable;
					itm->SetOwnedData<uint>(i);
					itm->OnMouseEvent = ConfigsWindow_OnSelectConfigsButtonClick;
				}
			};
			ModInfoPanel->AddItem(selectConfigBtn);
			yOffset += 0.07f;
		}
		yOffset += 0.03f;

		MenuItem* exportConfigsBtn = new MenuItem();
		exportConfigsBtn->OnInit = [yOffset](BaseUiElement* item)
		{
			item->Name = "ExportConfigsButton";
			item->SizeX = 0.40f;
			item->SizeY = 0.07f;
			item->PosX = 0.30f;
			item->PosY = yOffset;
			item->BgTexture = UiElement_ButtonTexture;

			if (auto* itm = static_cast<MenuItem*>(item))
			{
				itm->Text = parser->GetSymbol("StExt_Str_ModMenu_ExportCurrentConfigs")->stringdata;
				itm->BehaviorFlags = UiElementBehaviorFlags::Hoverable | UiElementBehaviorFlags::Selectable | UiElementBehaviorFlags::Interactable;
				itm->OnMouseEvent = ConfigsWindow_OnExportConfigsButtonClick;
				itm->OnUpdate = ConfigsWindow_OnExportConfigsButtonUpdate;
			}
		};
		ModInfoPanel->AddItem(exportConfigsBtn);
		yOffset += 0.125f;
		
		ModInfoPanel->AddItem(BuildHSeparator(yOffset));
		yOffset += 0.01f;

		ModInfoPanel->AddItem(BuildCopyrightPanel(ModVersionString, yOffset, 0, true));
		yOffset += 0.05f;
		ModInfoPanel->AddItem(BuildCopyrightPanel("Created by StonedWizzard.", yOffset, 1));
		yOffset += 0.035f;

		if (ModPluginsInfo.GetNum() > 0)
		{
			yOffset += 0.0020f;
			ModInfoPanel->AddItem(BuildCopyrightPanel("Active mod plugins:", yOffset, 2));
			yOffset += 0.0320f;
			for (uint i = 0; i < ModPluginsInfo.GetNum(); i++)
			{
				const zSTRING pluginInfo = Z(ModPluginsInfo[i].Name + " [" + ModPluginsInfo[i].Version + "] by " + ModPluginsInfo[i].Author);
				ModInfoPanel->AddItem(BuildCopyrightPanel(pluginInfo, yOffset, 3 + i));
				yOffset += 0.03f;
			}
			ModInfoPanel->AddItem(BuildCopyrightPanel("", yOffset, 999));
			yOffset += 0.03f;
		}
		yOffset += 0.04f;

		MenuItem* headerItem = new MenuItem();
		headerItem->OnInit = [yOffset](BaseUiElement* item)
		{
			item->Name = "ThxHeader";
			item->SizeX = 0.99f;
			item->SizeY = 0.05f;
			item->PosX = 0.01f;
			item->PosY = yOffset;

			if (auto* itm = static_cast<MenuItem*>(item))
			{
				itm->Text = "Special thanks to everyone who contributed to the development of this mod:";
				itm->TextColor_Default = TextColor_Regular_Default;
			}
		};
		ModInfoPanel->AddItem(headerItem);
		yOffset += 0.055f;

		const uint count = sizeof(ThxList) / sizeof(ThxList[0]);
		for (uint i = 0; i < count; ++i)
		{
			MenuItem* itm = BuildThxPanel(ThxList[i], yOffset, i, i < 7);
			ModInfoPanel->AddItem(itm);
			yOffset += 0.035f;
		}

		ModInfoPanel->Init();
	}

	//----------------------------------------------------------------------
	//								WINDOW
	//----------------------------------------------------------------------


	ConfigsWindow::ConfigsWindow() 
	{
		ConfigsPanels = Array<MenuScrollPanel*>();
		TabPanel = Null;
		ConfigsTabPanel = Null;
		ModInfoPanel = Null;
	}

	inline BaseMenuPanel* ConfigsWindow::GetTabPanel(const int index)
	{
		switch (index)
		{
			case 0: return ConfigsTabPanel;
			case 1: return ModInfoPanel;
		}
		return Null;
	}

	void ConfigsWindow::Resize() { MenuWindow::Resize(); }

	void ConfigsWindow::Init()
	{
		OnInit = [](BaseUiElement* item) {
			item->Name = "ConfigsWindow";
			item->BgTexture = UiElement_PanelTexture;
			item->Resize();
		};
		MenuWindow::Init();
		OnClose = ConfigsWindow_OnClose;

		InitTabs();
		InitConfigsTab();
		InitModInfoTab();

		Instance = this;
	}

	void ConfigsWindow::Draw() { MenuWindow::Draw(); }

	void ConfigsWindow::Update() 
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

		if (currentTabPanel == ConfigsTabPanel)
		{
			for (int i = 0; i < ConfigPanelIndexMax; ++i)
			{
				BaseMenuPanel* panel = ConfigsPanels[i];
				if (panel == Null) continue;
				panel->IsHiden = ConfigPanelIndex != i;
			}
		}
	}


	ConfigsWindow::~ConfigsWindow()
	{
		ConfigsPanels.Clear();
		TabPanel = Null;
		ConfigsTabPanel = Null;
		ModInfoPanel = Null;
	}
}