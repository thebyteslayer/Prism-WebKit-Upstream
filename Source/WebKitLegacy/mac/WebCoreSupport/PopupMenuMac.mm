/*
 * Copyright (C) 2006-2023 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#import "PopupMenuMac.h"

#import "WebDelegateImplementationCaching.h"
#import "WebFrameInternal.h"
#import "WebUIDelegatePrivate.h"
#import <WebCore/IntRect.h>
#import <WebCore/AXObjectCache.h>
#import <WebCore/Chrome.h>
#import <WebCore/ChromeClient.h>
#import <WebCore/EventHandler.h>
#import <WebCore/Font.h>
#import <WebCore/FrameInlines.h>
#import <WebCore/LocalFrame.h>
#import <WebCore/LocalFrameView.h>
#import <WebCore/Page.h>
#import <WebCore/PopupMenuClient.h>
#import <pal/spi/mac/NSCellSPI.h>
#import <pal/system/mac/PopupMenu.h>
#import <wtf/BlockObjCExceptions.h>

using namespace WebCore;

PopupMenuMac::PopupMenuMac(PopupMenuClient* client)
    : m_client(client)
{
}

PopupMenuMac::~PopupMenuMac()
{
    [m_popup setControlView:nil];
}

void PopupMenuMac::clear()
{
    [m_popup removeAllItems];
}

void PopupMenuMac::populate()
{
    if (m_popup)
        clear();
    else {
        m_popup = adoptNS([[NSPopUpButtonCell alloc] initTextCell:@"" pullsDown:!m_client->shouldPopOver()]);
        [m_popup setUsesItemFromMenu:NO];
        [m_popup setAutoenablesItems:NO];
    }
    
    // For pullDown menus the first item is hidden.
    if (!m_client->shouldPopOver())
        [m_popup addItemWithTitle:@""];

    TextDirection menuTextDirection = m_client->menuStyle().textDirection();
    [m_popup setUserInterfaceLayoutDirection:menuTextDirection == TextDirection::LTR ? NSUserInterfaceLayoutDirectionLeftToRight : NSUserInterfaceLayoutDirectionRightToLeft];

    ASSERT(m_client);
    int size = m_client->listSize();

    for (int i = 0; i < size; i++) {
        if (m_client->itemIsSeparator(i)) {
            [[m_popup menu] addItem:[NSMenuItem separatorItem]];
            continue;
        }

        PopupMenuStyle style = m_client->itemStyle(i);
        RetainPtr<NSMutableDictionary> attributes = adoptNS([[NSMutableDictionary alloc] init]);
        if (style.font() != FontCascade()) {
            RetainPtr<CTFontRef> font = style.font().primaryFont()->getCTFont();
            if (!font) {
                CGFloat size = style.font().primaryFont()->platformData().size();
                font = adoptCF(CTFontCreateUIFontForLanguage(isFontWeightBold(style.font().weight()) ? kCTFontUIFontEmphasizedSystem : kCTFontUIFontSystem, size, nullptr));
            }
            [attributes setObject:(__bridge NSFont *)(font.get()) forKey:NSFontAttributeName];
        }

        RetainPtr<NSMutableParagraphStyle> paragraphStyle = adoptNS([[NSParagraphStyle defaultParagraphStyle] mutableCopy]);
        [paragraphStyle setAlignment:menuTextDirection == TextDirection::LTR ? NSTextAlignmentLeft : NSTextAlignmentRight];
        NSWritingDirection writingDirection = style.textDirection() == TextDirection::LTR ? NSWritingDirectionLeftToRight : NSWritingDirectionRightToLeft;
        [paragraphStyle setBaseWritingDirection:writingDirection];
        if (style.hasTextDirectionOverride()) {
            auto writingDirectionValue = static_cast<NSInteger>(writingDirection) + static_cast<NSInteger>(NSWritingDirectionOverride);
            RetainPtr<NSNumber> writingDirectionNumber = adoptNS([[NSNumber alloc] initWithInteger:writingDirectionValue]);
            RetainPtr<NSArray> writingDirectionArray = adoptNS([[NSArray alloc] initWithObjects:writingDirectionNumber.get(), nil]);
            [attributes setObject:writingDirectionArray.get() forKey:NSWritingDirectionAttributeName];
        }
        [attributes setObject:paragraphStyle.get() forKey:NSParagraphStyleAttributeName];

        // FIXME: Add support for styling the foreground and background colors.
        // FIXME: Find a way to customize text color when an item is highlighted.
        RetainPtr<NSAttributedString> string = adoptNS([[NSAttributedString alloc] initWithString:m_client->itemText(i).createNSString().get() attributes:attributes.get()]);

        [m_popup addItemWithTitle:@""];
        NSMenuItem *menuItem = [m_popup lastItem];
        [menuItem setAttributedTitle:string.get()];
        // We set the title as well as the attributed title here. The attributed title will be displayed in the menu,
        // but typeahead will use the non-attributed string that doesn't contain any leading or trailing whitespace.
        [menuItem setTitle:[[string string] stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceCharacterSet]]];
        [menuItem setEnabled:m_client->itemIsEnabled(i)];
        [menuItem setToolTip:m_client->itemToolTip(i).createNSString().get()];

ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        // Allow the accessible text of the item to be overridden if necessary.
        if (AXObjectCache::accessibilityEnabled()) {
            RetainPtr accessibilityOverride = m_client->itemAccessibilityText(i).createNSString();
            if ([accessibilityOverride length])
                [menuItem accessibilitySetOverrideValue:accessibilityOverride.get() forAttribute:NSAccessibilityDescriptionAttribute];
        }
ALLOW_DEPRECATED_DECLARATIONS_END
    }
}

void PopupMenuMac::show(const IntRect& r, LocalFrameView& frameView, int selectedIndex)
{
    populate();
    int numItems = [m_popup numberOfItems];
    if (numItems <= 0) {
        if (m_client)
            m_client->popupDidHide();
        return;
    }
    ASSERT(numItems > selectedIndex);

    // Workaround for crazy bug where a selectedIndex of -1 for a menu with only 1 item will cause a blank menu.
    if (selectedIndex == -1 && numItems == 2 && !m_client->shouldPopOver() && ![[m_popup itemAtIndex:1] isEnabled])
        selectedIndex = 0;

    NSView* view = frameView.documentView();

    TextDirection textDirection = m_client->menuStyle().textDirection();

    [m_popup attachPopUpWithFrame:r inView:view];
    [m_popup selectItemAtIndex:selectedIndex];
    [m_popup setUserInterfaceLayoutDirection:textDirection == TextDirection::LTR ? NSUserInterfaceLayoutDirectionLeftToRight : NSUserInterfaceLayoutDirectionRightToLeft];

    NSMenu *menu = [m_popup menu];
    [menu setUserInterfaceLayoutDirection:textDirection == TextDirection::LTR ? NSUserInterfaceLayoutDirectionLeftToRight : NSUserInterfaceLayoutDirectionRightToLeft];

    NSPoint location;
    CTFontRef font = m_client->menuStyle().font().primaryFont()->getCTFont();

    // These values were borrowed from AppKit to match their placement of the menu.
    const int popOverHorizontalAdjust = -13;
    const int popUnderHorizontalAdjust = 6;
    const int popUnderVerticalAdjust = 6;
    if (m_client->shouldPopOver()) {
        NSRect titleFrame = [m_popup titleRectForBounds:r];
        if (titleFrame.size.width <= 0 || titleFrame.size.height <= 0)
            titleFrame = r;
        float vertOffset = roundf((NSMaxY(r) - NSMaxY(titleFrame)) + NSHeight(titleFrame));
        // Adjust for fonts other than the system font.
        auto defaultFont = adoptCF(CTFontCreateUIFontForLanguage(kCTFontUIFontSystem, CTFontGetSize(font), nil));
        vertOffset += CTFontGetDescent(font) - CTFontGetDescent(defaultFont.get());
        vertOffset = fminf(NSHeight(r), vertOffset);
        if (textDirection == TextDirection::LTR)
            location = NSMakePoint(NSMinX(r) + popOverHorizontalAdjust, NSMaxY(r) - vertOffset);
        else
            location = NSMakePoint(NSMaxX(r) - popOverHorizontalAdjust, NSMaxY(r) - vertOffset);
    } else {
        if (textDirection == TextDirection::LTR)
            location = NSMakePoint(NSMinX(r) + popUnderHorizontalAdjust, NSMaxY(r) + popUnderVerticalAdjust);
        else
            location = NSMakePoint(NSMaxX(r) - popUnderHorizontalAdjust, NSMaxY(r) + popUnderVerticalAdjust);
    }
    // Save the current event that triggered the popup, so we can clean up our event
    // state after the NSMenu goes away.
    Ref frame { frameView.frame() };
    RetainPtr<NSEvent> event = frame->eventHandler().currentNSEvent();
    
    Ref<PopupMenuMac> protector(*this);

    RetainPtr<NSView> dummyView = adoptNS([[NSView alloc] initWithFrame:r]);
    [dummyView.get() setUserInterfaceLayoutDirection:textDirection == TextDirection::LTR ? NSUserInterfaceLayoutDirectionLeftToRight : NSUserInterfaceLayoutDirectionRightToLeft];
    [view addSubview:dummyView.get()];
    location = [dummyView convertPoint:location fromView:view];
    
    if (Page* page = frame->page()) {
        WebView* webView = kit(page);
        BEGIN_BLOCK_OBJC_EXCEPTIONS
        CallUIDelegate(webView, @selector(webView:willPopupMenu:), menu);
        END_BLOCK_OBJC_EXCEPTIONS
    }

    NSControlSize controlSize;
    switch (m_client->menuStyle().menuSize()) {
    case PopupMenuStyle::Size::Normal:
        controlSize = NSControlSizeRegular;
        break;
    case PopupMenuStyle::Size::Small:
        controlSize = NSControlSizeSmall;
        break;
    case PopupMenuStyle::Size::Mini:
        controlSize = NSControlSizeMini;
        break;
    case PopupMenuStyle::Size::Large:
        controlSize = NSControlSizeLarge;
        break;
    }

    PAL::popUpMenu(menu, location, roundf(NSWidth(r)), dummyView.get(), selectedIndex, (__bridge NSFont *)font, controlSize, !m_client->menuStyle().hasDefaultAppearance());

    [m_popup dismissPopUp];
    [dummyView removeFromSuperview];

    if (!m_client)
        return;

    int newIndex = [m_popup indexOfSelectedItem];
    m_client->popupDidHide();

    // Adjust newIndex for hidden first item.
    if (!m_client->shouldPopOver())
        newIndex--;

    if (selectedIndex != newIndex && newIndex >= 0)
        m_client->valueChanged(newIndex);

    // Give the frame a chance to fix up its event state, since the popup eats all the
    // events during tracking.
    frame->eventHandler().sendFakeEventsAfterWidgetTracking(event.get());
}

void PopupMenuMac::hide()
{
    [[m_popup menu] cancelTracking];
    if (m_client)
        m_client->popupDidHide();
}

void PopupMenuMac::updateFromElement()
{
}

void PopupMenuMac::disconnectClient()
{
    m_client = 0;
}
