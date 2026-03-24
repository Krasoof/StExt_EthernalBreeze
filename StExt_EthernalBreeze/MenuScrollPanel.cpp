#include <StonedExtension.h>

namespace Gothic_II_Addon
{
    MenuScrollPanel::MenuScrollPanel() { }

    void MenuScrollPanel::Init()
    {
        ScrollOffset = 0;
        ScrollOffsetBefore = Invalid;
        ScrollCanvasSize = 0;

        BaseMenuPanel::Init();
        UpdateCanvasSize();
    }

    void MenuScrollPanel::Resize() { BaseMenuPanel::Resize(); }

    void MenuScrollPanel::Draw() { BaseMenuPanel::Draw(); }

    void MenuScrollPanel::Update()
    {
        BaseMenuElement::Update();
        ItemsCount = Items.GetNum();

        const int panelTop = GlobalPosY;
        const int panelBottom = panelTop + GlobalSizeY;
        const float scrollOffsetAbs = ScrollOffset * ScreenToRelativePixDelta;

        for (uint i = 0; i < ItemsCount; ++i)
        {
            BaseMenuElement* item = Items[i];
            if (!item) continue;

            if (ScrollOffset != ScrollOffsetBefore)
            {
                item->PosY = item->InitialPosY - scrollOffsetAbs;
                item->Resize();

                int itemTop = item->GlobalPosY;
                int itemBottom = itemTop + item->GlobalSizeY;
                item->IsVisible = ((itemTop >= panelTop) && (itemBottom <= panelBottom));

                if (item->IsVisible)
                {
                    if (item->View && View && !View->childs.IsIn(item->View))
                        View->InsertItem(item->View);
                }
                else
                {
                    if (item->View && View && View->childs.IsIn(item->View))
                    {
                        item->View->Blit();
                        item->View->ClrPrintwin();
                        View->RemoveItem(item->View);
                    }
                }
            }
            item->Update();
        }
        ScrollOffsetBefore = ScrollOffset;
    }

    void MenuScrollPanel::UpdateCanvasSize()
    {
        if (ItemsCount == 0)
        {
            ScrollCanvasSize = GlobalSizeY;
            return;
        }

        float maxBottom = 0.0f;
        for (uint i = 0; i < Items.GetNum(); ++i)
        {
            BaseMenuElement* item = Items[i];
            if (!item) continue;

            float bottom = item->InitialPosY + item->InitialSizeY;
            if (bottom > maxBottom) maxBottom = bottom;
        }

        ScrollCanvasSize = static_cast<int>((maxBottom * ScreenVBufferSize) - (GlobalSizeY * 0.5f));
        if (ScrollCanvasSize < GlobalSizeY)
            ScrollCanvasSize = GlobalSizeY;
    }

    void MenuScrollPanel::Scroll(int delta)
    {        
        UpdateCanvasSize();
        ScrollOffset += delta * -1;
        if (ScrollOffset < 0) ScrollOffset = 0;
        if (ScrollOffset > ScrollCanvasSize) ScrollOffset = ScrollCanvasSize;
    }

    bool MenuScrollPanel::HandleMouse(const UiMouseEventArgs& args)
    {
        if (!IsEnabled || !IsVisible) return false;

        const bool isHit = (args.X > GlobalRectLeft && args.X < GlobalRectRight) && (args.Y > GlobalRectTop && args.Y < GlobalRectBottom);
        if (isHit && args.Action == UiMouseEnum::Scroll)
        {
            int scrollDelta = static_cast<int>((args.ScrollDelta * ModMenuWindow_ScrollShiftMult) * ModMenuWindow_ScrollMult);
            Scroll(scrollDelta);
        }

        return BaseMenuPanel::HandleMouse(args);
    }

    bool MenuScrollPanel::HandleKey(const UiKeyEventArgs& args) 
    { 
        int scrollDelta = 0;
        if (args.Action == UiKeyEnum::Down)
            scrollDelta = static_cast<int>((-128 * ModMenuWindow_ScrollShiftMult) * ModMenuWindow_ScrollMult);
        if (args.Action == UiKeyEnum::Up)
            scrollDelta = static_cast<int>((128 * ModMenuWindow_ScrollShiftMult) * ModMenuWindow_ScrollMult);

        if (scrollDelta != 0) {
            Scroll(scrollDelta);
            return true;
        }
        return false; 
    }

    void MenuScrollPanel::GetScrollOffset(int& scrollOffset, int& scrollOffsetBefore) { scrollOffset = ScrollOffset; scrollOffsetBefore = ScrollOffsetBefore; }
    void MenuScrollPanel::SetScrollOffset(int scrollOffset, int scrollOffsetBefore)
    {
        ScrollOffset = ValidateValue(scrollOffset, 0, ScrollCanvasSize);
        ScrollOffsetBefore = scrollOffsetBefore;
    }

    MenuScrollPanel::~MenuScrollPanel() { }
}