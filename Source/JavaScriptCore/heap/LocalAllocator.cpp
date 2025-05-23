/*
 * Copyright (C) 2018-2019 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "LocalAllocator.h"

#include "AllocatingScope.h"
#include "FreeListInlines.h"
#include "GCDeferralContext.h"
#include "LocalAllocatorInlines.h"
#include "Options.h"
#include "SuperSampler.h"

namespace JSC {

LocalAllocator::LocalAllocator(BlockDirectory* directory)
    : m_directory(directory)
    , m_freeList(directory->m_cellSize)
{
    Locker locker { directory->m_localAllocatorsLock };
    directory->m_localAllocators.append(this);
}

void LocalAllocator::reset()
{
    m_freeList.clear();
    m_currentBlock = nullptr;
    m_lastActiveBlock = nullptr;
    m_allocationCursor = 0;
}

LocalAllocator::~LocalAllocator()
{
    if (isOnList()) {
        Locker locker { m_directory->m_localAllocatorsLock };
        remove();
    }
    
    bool ok = true;
    if (!m_freeList.allocationWillFail()) {
        dataLog("FATAL: ", RawPointer(this), "->~LocalAllocator has non-empty free-list.\n");
        ok = false;
    }
    if (m_currentBlock) {
        dataLog("FATAL: ", RawPointer(this), "->~LocalAllocator has non-null current block.\n");
        ok = false;
    }
    if (m_lastActiveBlock) {
        dataLog("FATAL: ", RawPointer(this), "->~LocalAllocator has non-null last active block.\n");
        ok = false;
    }
    RELEASE_ASSERT(ok);
}

void LocalAllocator::stopAllocating()
{
    ASSERT(!m_lastActiveBlock);
    if (!m_currentBlock) {
        ASSERT(m_freeList.allocationWillFail());
        return;
    }
    
    m_currentBlock->stopAllocating(m_freeList);
    m_lastActiveBlock = m_currentBlock;
    m_currentBlock = nullptr;
    m_freeList.clear();
}

void LocalAllocator::resumeAllocating()
{
    if (!m_lastActiveBlock)
        return;

    m_lastActiveBlock->resumeAllocating(m_freeList);
    m_currentBlock = m_lastActiveBlock;
    m_lastActiveBlock = nullptr;
}

void LocalAllocator::prepareForAllocation()
{
    reset();
}

void LocalAllocator::stopAllocatingForGood()
{
    stopAllocating();
    reset();
}

void* LocalAllocator::allocateSlowCase(JSC::Heap& heap, size_t cellSize, GCDeferralContext* deferralContext, AllocationFailureMode failureMode)
{
    SuperSamplerScope superSamplerScope(false);
    ASSERT(heap.vm().currentThreadIsHoldingAPILock());
    doTestCollectionsIfNeeded(heap, deferralContext);

    ASSERT(!m_directory->markedSpace().isIterating());
    heap.didAllocate(m_freeList.originalSize());
    
    didConsumeFreeList();
    
    AllocatingScope helpingHeap(heap);

    heap.collectIfNecessaryOrDefer(deferralContext);
    
    // Goofy corner case: the GC called a callback and now this directory has a currentBlock. This only
    // happens when running WebKit tests, which inject a callback into the GC's finalization.
    if (m_currentBlock) [[unlikely]]
        return allocate(heap, cellSize, deferralContext, failureMode);
    
    void* result = tryAllocateWithoutCollecting(cellSize);
    
    if (result) [[likely]]
        return result;

    // FIXME GlobalGC: Need to synchronize here to when allocating from the BlockDirectory in the server.

    Subspace* subspace = m_directory->m_subspace;
    if (subspace->isIsoSubspace()) {
        if (void* result = static_cast<IsoSubspace*>(subspace)->tryAllocateLowerTierPrecise(cellSize))
            return result;
    }

    ASSERT(!subspace->isPreciseOnly());
    ASSERT_WITH_MESSAGE(cellSize == m_directory->cellSize(), "non-preciseOnly allocations should match allocator's the size class");
    MarkedBlock::Handle* block = m_directory->tryAllocateBlock(heap);
    if (!block) {
        if (failureMode == AllocationFailureMode::Assert)
            RELEASE_ASSERT_NOT_REACHED();
        else
            return nullptr;
    }
    m_directory->addBlock(block);
    result = allocateIn(block, cellSize);
    ASSERT(result);
    return result;
}

void LocalAllocator::didConsumeFreeList()
{
    if (m_currentBlock)
        m_currentBlock->didConsumeFreeList();
    
    m_freeList.clear();
    m_currentBlock = nullptr;
}

void* LocalAllocator::tryAllocateWithoutCollecting(size_t cellSize)
{
    // FIXME: GlobalGC
    // FIXME: If we wanted this to be used for real multi-threaded allocations then we would have to
    // come up with some concurrency protocol here. That protocol would need to be able to handle:
    //
    // - The basic case of multiple LocalAllocators trying to do an allocationCursor search on the
    //   same bitvector. That probably needs the bitvector lock at least.
    //
    // - The harder case of some LocalAllocator triggering a steal from a different BlockDirectory
    //   via a search in the AlignedMemoryAllocator's list. Who knows what locks that needs.
    // 
    // One way to make this work is to have a single per-Heap lock that protects all mutator lock
    // allocation slow paths. That would probably be scalable enough for years. It would certainly be
    // for using TLC allocation from JIT threads.
    // https://bugs.webkit.org/show_bug.cgi?id=181635
    
    SuperSamplerScope superSamplerScope(false);
    
    ASSERT(!m_currentBlock);
    ASSERT(m_freeList.allocationWillFail());
    
    for (;;) {
        MarkedBlock::Handle* block = m_directory->findBlockForAllocation(*this);
        if (!block)
            break;

        if (void* result = tryAllocateIn(block, cellSize))
            return result;
    }
    
    if (Options::stealEmptyBlocksFromOtherAllocators()) {
        if (MarkedBlock::Handle* block = m_directory->m_subspace->findEmptyBlockToSteal()) {
            RELEASE_ASSERT(block->alignedMemoryAllocator() == m_directory->m_subspace->alignedMemoryAllocator());
            
            block->sweep(nullptr);
            
            // It's good that this clears canAllocateButNotEmpty as well as all other bits,
            // because there is a remote chance that a block may have both canAllocateButNotEmpty
            // and empty set at the same time.
            block->removeFromDirectory();
            m_directory->addBlock(block);
            return allocateIn(block, cellSize);
        }
    }
    
    return nullptr;
}

void* LocalAllocator::allocateIn(MarkedBlock::Handle* block, size_t cellSize)
{
    void* result = tryAllocateIn(block, cellSize);
    RELEASE_ASSERT(result);
    return result;
}

void* LocalAllocator::tryAllocateIn(MarkedBlock::Handle* block, size_t cellSize)
{
    ASSERT(block);
    ASSERT(!block->isFreeListed());
    m_directory->assertIsMutatorOrMutatorIsStopped();
    ASSERT(m_directory->isInUse(block));
    
    block->sweep(&m_freeList);
    
    // It's possible to stumble on a completely full block. Marking tries to retire these, but
    // that algorithm is racy and may forget to do it sometimes.
    if (m_freeList.allocationWillFail()) {
        ASSERT(block->isFreeListed());
        block->unsweepWithNoNewlyAllocated();
        ASSERT(!block->isFreeListed());
        ASSERT(!m_directory->isEmpty(block));
        ASSERT(!m_directory->isCanAllocateButNotEmpty(block));
        return nullptr;
    }
    
    m_currentBlock = block;
    
    void* result = m_freeList.allocateWithCellSize(
        []() -> HeapCell* {
            RELEASE_ASSERT_NOT_REACHED();
            return nullptr;
        }, cellSize);

    // FIXME: We should make this work with thread safety analysis.
    m_directory->m_bits.setIsEden(m_currentBlock->index(), true);
    m_directory->markedSpace().didAllocateInBlock(m_currentBlock);
    return result;
}

void LocalAllocator::doTestCollectionsIfNeeded(JSC::Heap& heap, GCDeferralContext* deferralContext)
{
    if (!Options::slowPathAllocsBetweenGCs()) [[likely]]
        return;

    static unsigned allocationCount = 0;
    if (!allocationCount) {
        if (!heap.isDeferred()) {
            if (deferralContext)
                deferralContext->m_shouldGC = true;
            else
                heap.collectNow(Sync, CollectionScope::Full);
        }
    }
    if (++allocationCount >= Options::slowPathAllocsBetweenGCs())
        allocationCount = 0;
}

} // namespace JSC

