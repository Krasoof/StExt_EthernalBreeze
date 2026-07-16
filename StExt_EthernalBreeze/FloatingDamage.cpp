#include <UnionAfx.h>
#include <StonedExtension.h>
#include <vector>

// Diablo/Souls-style floating damage numbers over hit enemies. Crash-safe by
// design: a label anchors to a COPY of the target's world position at hit time
// (no NPC pointer is stored), so a target dying/despawning can never dangle.
// Pattern adapted from Union_AlterDamage's CPopupDamageLabel; rendering mirrors
// our own ModInfo view lifecycle (persistent screen child, ClrPrintwin/Print).

namespace Gothic_II_Addon
{
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
