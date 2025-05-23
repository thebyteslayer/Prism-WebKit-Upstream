/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "ScrollingCoordinatorMac.h"

#if ENABLE(ASYNC_SCROLLING) && ENABLE(SCROLLING_THREAD)

#import "LocalFrameInlines.h"
#import "LocalFrameView.h"
#import "Logging.h"
#import "Page.h"
#import "PlatformCALayerContentsDelayedReleaser.h"
#import "PlatformWheelEvent.h"
#import "Region.h"
#import "ScrollingStateTree.h"
#import "ScrollingThread.h"
#import "ScrollingTreeFixedNodeCocoa.h"
#import "ScrollingTreeFrameScrollingNodeMac.h"
#import "ScrollingTreeMac.h"
#import "ScrollingTreeStickyNodeCocoa.h"
#import "TiledBacking.h"
#import <wtf/MainThread.h>

namespace WebCore {

Ref<ScrollingCoordinator> ScrollingCoordinator::create(Page* page)
{
    return adoptRef(*new ScrollingCoordinatorMac(page));
}

ScrollingCoordinatorMac::ScrollingCoordinatorMac(Page* page)
    : ThreadedScrollingCoordinator(page)
{
    setScrollingTree(ScrollingTreeMac::create(*this));
}

ScrollingCoordinatorMac::~ScrollingCoordinatorMac()
{
    ASSERT(!scrollingTree());
}

void ScrollingCoordinatorMac::commitTreeStateIfNeeded()
{
    ThreadedScrollingCoordinator::commitTreeStateIfNeeded();
    updateTiledScrollingIndicator();
}

void ScrollingCoordinatorMac::willStartPlatformRenderingUpdate()
{
    PlatformCALayerContentsDelayedReleaser::singleton().mainThreadCommitWillStart();
}

void ScrollingCoordinatorMac::didCompletePlatformRenderingUpdate()
{
    downcast<ScrollingTreeMac>(scrollingTree())->didCompletePlatformRenderingUpdate();
    PlatformCALayerContentsDelayedReleaser::singleton().mainThreadCommitDidEnd();
}

void ScrollingCoordinatorMac::updateTiledScrollingIndicator()
{
    RefPtr localMainFrame = page()->localMainFrame();
    if (!localMainFrame)
        return;

    RefPtr frameView = localMainFrame->view();
    if (!frameView)
        return;
    
    CheckedPtr tiledBacking = frameView->tiledBacking();
    if (!tiledBacking)
        return;

    ScrollingModeIndication indicatorMode;
    if (shouldUpdateScrollLayerPositionSynchronously(*frameView))
        indicatorMode = SynchronousScrollingBecauseOfStyleIndication;
    else
        indicatorMode = AsyncScrollingIndication;
    
    tiledBacking->setScrollingModeIndication(indicatorMode);
}

} // namespace WebCore

#endif // ENABLE(ASYNC_SCROLLING) && ENABLE(SCROLLING_THREAD)
