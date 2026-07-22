// Supported with union (c) 2020 Union team

// User API for oCNpc
// Add your methods here

// HOOKS
int oCNpc::EV_DamagePerFrame_StExt(oCMsgDamage* msg);

void oCNpc::OnDamage_StExt(oSDamageDescriptor& desc);

void oCNpc::OnDamage_Sound_StExt(oSDamageDescriptor& desc);

void oCNpc::DisplayCannotUse_StExt();

void oCNpc::ChangeAttribute_StExt(int attribute, int value);


//void oCNpc::Archive_StExt(zCArchiver& ar);

void oCNpc::Unarchive_StExt(zCArchiver& ar);

void oCNpc::InitByScript_StExt(int instance, int inSaveGame);

void oCNpc::RemoveItemEffects_StExt(oCItem* item);

void oCNpc::AddItemEffects_StExt(oCItem* item);

void oCNpc::ProcessNpc_StExt();

void oCNpc::UnequipItem_StExt(oCItem* item);

void oCNpc::EquipArmor_StExt(oCItem* item);

int oCNpc::CanUse_StExt(oCItem* item);

bool oCNpc::CanEquipAdditionalArmor(oCItem* item);

void oCNpc::GetEquippedArmors(Array<oCItem*>& array);

TNpcSlot* oCNpc::CreateAdditionalItemSlot(const zSTRING& name);

void oCNpc::EquipAdditionalArmorItem(oCItem* item);

void oCNpc::UnequipAdditionalArmorItem(const int wear);

void oCNpc::PutAdditionalArmorItemToSlot(oCItem* item);

void oCNpc::RemoveAdditionalArmorItemFromSlot(oCItem* item);

void oCNpc::StopAllVoices_StExt();

int oCNpc::UpdateNextVoice_StExt();

int oCNpc::EV_PlaySound_StExt(oCMsgConversation*);

int oCNpc::EV_OutputSVM_StExt(oCMsgConversation*);

void oCNpc::OpenInventory_StExt(int mode);

void oCNpc::OnMessage_StExt(zCEventMessage* msg, zCVob* vob);

void DoDie_StExt(oCNpc* atk);

int oCNpc::EV_AttackForward_StExt(oCMsgAttack* msg);
int oCNpc::EV_AttackLeft_StExt(oCMsgAttack* msg);
int oCNpc::EV_AttackRight_StExt(oCMsgAttack* msg);
int oCNpc::EV_AttackRun_StExt(oCMsgAttack* msg);
int oCNpc::EV_AttackFinish_StExt(oCMsgAttack* msg);
int oCNpc::EV_Parade_StExt(oCMsgAttack* msg);

int oCNpc::EV_ShootAt_StExt(oCMsgAttack* msg);
int oCNpc::EV_Defend_StExt(oCMsgAttack* msg);

int oCNpc::DoTakeVob_StExt(zCVob* vob);

