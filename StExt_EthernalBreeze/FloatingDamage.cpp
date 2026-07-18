#include <UnionAfx.h>
#include <StonedExtension.h>
#include <vector>
#include <cstdio>

// Diablo/Souls-style floating damage numbers over hit enemies. Crash-safe by
// design: a label anchors to a COPY of the target's world position at hit time
// (no NPC pointer is stored), so a target dying/despawning can never dangle.
// Pattern adapted from Union_AlterDamage's CPopupDamageLabel; rendering mirrors
// our own ModInfo view lifecycle (persistent screen child, ClrPrintwin/Print).

namespace Gothic_II_Addon
{
	// TEMP DIAG (remove once the weapon glow is solved): dump the emitter shape of the
	// base game's weapon-enchant particle FX. Our own glow only ever lights the hilt, and
	// so does the base FX when we attach it via item.effect - yet in Returning the very
	// same enchant covers the blade. zCParticleEmitter maps 1:1 onto the script class, so
	// creating the instance tells us exactly which shape/mesh the working effect uses.
	static void StExt_PfxDiagLog(const zSTRING& msg)
	{
		FILE* f = fopen("stext_pfx.log", "a");
		if (f) { fputs(msg.ToChar(), f); fputc('\n', f); fclose(f); }
	}

	static void StExt_DumpPfxShape(const char* name)
	{
		const int idx = parser->GetIndex(zSTRING(name));
		if (idx == Invalid) { StExt_PfxDiagLog(zSTRING(name) + " -> NOT FOUND"); return; }

		zCParticleEmitter em;              // ctor builds the zSTRINGs before the parser fills them
		parser->CreateInstance(idx, &em);
		StExt_PfxDiagLog(zSTRING(name)
			+ " | shpType=" + em.shpType_S
			+ " | FOR=" + em.shpFOR_S
			+ " | offset=" + em.shpOffsetVec_S
			+ " | dim=" + em.shpDim_S
			+ " | mesh='" + em.shpMesh_S + "'"
			+ " | isVolume=" + zSTRING(em.shpIsVolume)
			+ " | meshRender=" + zSTRING(em.shpMeshRender_B)
			+ " | distrib=" + em.shpDistribType_S);
	}

	// Symbol type ids: 0 void, 1 float, 2 int, 3 string, 4 class, 5 func, 6 proto, 7 instance
	static void StExt_DumpSymbol(const char* name)
	{
		zCPar_Symbol* s = parser->GetSymbol(zSTRING(name));
		if (!s) { StExt_PfxDiagLog(zSTRING("SYM ") + name + " -> NOT FOUND"); return; }
		zSTRING line = zSTRING("SYM ") + name
			+ " | type=" + zSTRING((int)s->type)
			+ " | ele=" + zSTRING((int)s->ele)
			+ " | flags=" + zSTRING((int)s->flags);
		if (s->type == 2 && s->ele <= 1) line += zSTRING(" | int=") + zSTRING(s->single_intdata);
		StExt_PfxDiagLog(line);
	}

	void StExt_DumpWeaponGlowPfx()
	{
		static bool done = false;
		if (done) return;
		done = true;

		StExt_PfxDiagLog(zSTRING("=== weapon-glow PFX shapes ==="));
		// Adanos-warrior enchant: this is the HILT-only look we get today.
		StExt_DumpPfxShape("MFX_AW_ENCHANT_FIRE");
		StExt_DumpPfxShape("MFX_AW_ENCHANT_ORIGIN_FIRE");
		// Fire Weapon spell: the iconic burning BLADE - this is the coverage we want.
		StExt_DumpPfxShape("MFX_FIREWEAPON_ORIGIN");
		StExt_DumpPfxShape("MFX_FIREWEAPON_60S_INIT");
		// ours, only lights the hilt:
		StExt_DumpPfxShape("MFX_STEXT_WGLOW_FIRE");

		// Gallahad's enchant: parametric (colour/pps/type), builds its FX at runtime and
		// paints the BLADE. Learn its signature so we can drive it per element.
		StExt_PfxDiagLog(zSTRING("=== Gallahad visual system ==="));
		StExt_DumpSymbol("RX_ApplyVisualGallahad");
		StExt_DumpSymbol("RX_ApplyVisualGallahad.par0");
		StExt_DumpSymbol("RX_RemoveVisualGallahad.par0");
		StExt_DumpSymbol("RX_Gallahad_Visual_Color");
		StExt_DumpSymbol("RX_Gallahad_Visual_PPS");
		StExt_DumpSymbol("RX_Gallahad_Visual_Type");
		StExt_DumpSymbol("RX_Gallahad_Visual_WeaponType");
	}

	struct StExtFloatDmg
	{
		zVEC3 worldPos;
		int   damage;
		float spawnTime;
		bool  isCrit;
	};

	static std::vector<StExtFloatDmg> StExt_FloatDmgList;
	static zCView* StExt_FloatDmgView = Null;

	static const float FLOATDMG_LIFE = 1.2f;    // seconds on screen
	static const float FLOATDMG_RISE = 110.0f;  // world-units risen over its life
	static const float FLOATDMG_HEAD = 75.0f;   // spawn height above npc origin
	static const size_t FLOATDMG_MAX = 48;      // hard cap (safety)

	void StExt_SpawnFloatingDamage(oCNpc* target, int damage, bool isCrit)
	{
		if (!target || damage <= 0) return;
		if (StExt_FloatDmgList.size() >= FLOATDMG_MAX) return;

		StExtFloatDmg fd;
		fd.worldPos = target->GetPositionWorld();
		fd.worldPos[1] += FLOATDMG_HEAD;
		fd.damage = damage;
		fd.spawnTime = ztimer->totalTimeFloat / 1000.0f;
		fd.isCrit = isCrit;
		StExt_FloatDmgList.push_back(fd);
	}

	void StExt_FloatingDamage_Clear()
	{
		StExt_FloatDmgList.clear();
	}

	static bool StExt_WorldToView(const zVEC3& wp, zCView* view, int& vx, int& vy)
	{
		zCCamera* cam = ogame ? ogame->GetCamera() : Null;
		if (!cam) return false;

		zVEC3 viewPos = cam->GetTransform(zCAM_TRAFO_VIEW) * wp;
		if (viewPos[2] <= cam->nearClipZ) return false;   // behind the camera

		float px = 0.0f, py = 0.0f;
		cam->Project(&viewPos, px, py);
		vx = view->anx((int)(px + 0.5f));
		vy = view->any((int)(py + 0.5f));
		return true;
	}

	void StExt_FloatingDamage_Loop()
	{
		if (!ogame || !screen || !zrenderer) return;

		if (StExt_FloatDmgList.empty())
		{
			if (StExt_FloatDmgView && screen->childs.IsIn(StExt_FloatDmgView))
				StExt_FloatDmgView->ClrPrintwin();
			return;
		}

		if (!StExt_FloatDmgView)
		{
			StExt_FloatDmgView = new zCView(0, 0, ScreenVBufferSize, ScreenVBufferSize);
			StExt_FloatDmgView->SetAlphaBlendFunc(zRND_ALPHA_FUNC_BLEND);
		}
		if (!screen->childs.IsIn(StExt_FloatDmgView))
			screen->InsertItem(StExt_FloatDmgView, FALSE);

		StExt_FloatDmgView->ClrPrintwin();

		const float now = ztimer->totalTimeFloat / 1000.0f;
		for (int i = (int)StExt_FloatDmgList.size() - 1; i >= 0; --i)
		{
			StExtFloatDmg& fd = StExt_FloatDmgList[i];
			const float life = now - fd.spawnTime;
			if (life < 0.0f || life > FLOATDMG_LIFE)
			{
				StExt_FloatDmgList.erase(StExt_FloatDmgList.begin() + i);
				continue;
			}

			const float t = life / FLOATDMG_LIFE;   // 0..1
			zVEC3 p = fd.worldPos;
			p[1] += FLOATDMG_RISE * t;

			int vx = 0, vy = 0;
			if (!StExt_WorldToView(p, StExt_FloatDmgView, vx, vy))
				continue;

			zSTRING txt = zSTRING(fd.damage);
			const int alpha = (int)(255.0f * (1.0f - t * t));   // quadratic fade-out
			const zCOLOR col = fd.isCrit ? zCOLOR(255, 150, 30, alpha) : zCOLOR(255, 235, 130, alpha);
			StExt_FloatDmgView->SetFontColor(col);
			const int w = StExt_FloatDmgView->FontSize(txt);
			StExt_FloatDmgView->Print(vx - w / 2, vy, txt);
		}
	}
}
