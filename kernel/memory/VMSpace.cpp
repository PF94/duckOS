/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright © 2016-2022 Byteduck */

#include "VMSpace.h"
#include "MemoryManager.h"
#include "AnonymousVMObject.h"
#include "../kstd/cstring.h"

const VMProt VMSpace::default_prot = {
	.read = true,
	.write = true,
	.execute = true,
	.cow = false
};

VMSpace::VMSpace(VirtualAddress start, size_t size, PageDirectory& page_directory):
	m_start(start),
	m_size(size),
	m_region_map(new VMSpaceRegion {.start = start, .size = size, .used = false, .next = nullptr, .prev = nullptr}),
	m_page_directory(page_directory)
{}

VMSpace::~VMSpace() {
	auto cur_region = m_region_map;
	while(cur_region) {
		auto next = cur_region->next;
		delete cur_region;
		cur_region = next;
	}
}

kstd::Arc<VMSpace> VMSpace::fork(PageDirectory& page_directory, kstd::vector<kstd::Arc<VMRegion>>& regions_vec) {
	LOCK(m_lock);
	auto new_space = kstd::Arc<VMSpace>(new VMSpace(m_start, m_size, page_directory));
	new_space->m_used = m_used;
	delete new_space->m_region_map;

	// Clone regions
	auto cur_region = m_region_map;
	VMSpaceRegion* prev_new_region = nullptr;
	while(cur_region) {
		// Clone the VMSpaceRegion
		auto new_region = new VMSpaceRegion(*cur_region);
		if(cur_region == m_region_map)
			new_space->m_region_map = new_region;
		new_region->prev = prev_new_region;
		if(prev_new_region)
			prev_new_region->next = new_region;
		prev_new_region = new_region;

		// Clone the vmRegion
		if(cur_region->vmRegion) {
			auto region = cur_region->vmRegion;
			if(region->object()->is_anonymous()) {
				auto action = kstd::static_pointer_cast<AnonymousVMObject>(region->object())->fork_action();

				// If the object should become CoW on fork, do so
				if(action == AnonymousVMObject::ForkAction::BecomeCoW) {
					// Remap anonymous writeable regions as CoW
					if(region->prot().write) {
						region->set_cow(true);
						m_page_directory.map(*region);
					}

					// Map into new space
					auto new_vmRegion = kstd::Arc<VMRegion>(new VMRegion(region->object(), new_space, region->range(), region->object_start(), region->prot()));
					page_directory.map(*new_vmRegion);
					new_region->vmRegion = new_vmRegion.get();
					regions_vec.push_back(new_vmRegion);
				}
			} else {
				ASSERT(false);
			}
		}

		cur_region = cur_region->next;
	}

	return new_space;
}

ResultRet<kstd::Arc<VMRegion>> VMSpace::map_object(kstd::Arc<VMObject> object, VMProt prot, VirtualRange range, VirtualAddress object_start) {
	// Use the size of the range if defined, or the remainder of the object size if not.
	if(!range.size)
		range.size = object->size() - object_start;

	// Validate alignments and size
	if(range.start % PAGE_SIZE != 0 || range.size % PAGE_SIZE != 0 || object_start % PAGE_SIZE != 0 || object_start + range.size > object->size())
		return Result(EINVAL);

	// Allocate the space region appropriately
	VMSpaceRegion* region;
	if(range.start)
		region = TRY(alloc_space_at(range.size, range.start));
	else
		region = TRY(alloc_space(range.size));

	// Create and map the region
	auto vmRegion = kstd::make_shared<VMRegion>(
			object,
			self(),
			VirtualRange {region->start, object->size()},
			object_start,
			prot);
	region->vmRegion = vmRegion.get();
	m_page_directory.map(*vmRegion);
	return vmRegion;
}

ResultRet<kstd::Arc<VMRegion>> VMSpace::map_stack(kstd::Arc<VMObject> object, VMProt prot) {
	LOCK(m_lock);

	// Find the endmost region with space in it
	auto cur_region = m_region_map;
	while(cur_region->next)
		cur_region = cur_region->next;
	while((cur_region->used || cur_region->size < object->size()) && cur_region)
		cur_region = cur_region->prev;
	if(!cur_region)
		return Result(ENOMEM);
	return map_object(object, prot, {cur_region->end() - object->size(), object->size()});
}

Result VMSpace::unmap_region(VMRegion& region) {
	m_lock.acquire();
	VMSpaceRegion* cur_region = m_region_map;
	while(cur_region) {
		if(cur_region->vmRegion == &region) {
			if(cur_region->vmRegion) {
				cur_region->vmRegion->m_space.reset();
				m_page_directory.unmap(*cur_region->vmRegion);
				m_lock.release();
				auto free_res = free_region(cur_region);
				ASSERT(!free_res.is_error());
				return free_res;
			}
			m_lock.release();
			return Result(ENOENT);
		}
		cur_region = cur_region->next;
	}
	m_lock.release();
	return Result(ENOENT);
}

Result VMSpace::unmap_region(VirtualAddress address) {
	m_lock.acquire();
	VMSpaceRegion* cur_region = m_region_map;
	while(cur_region) {
		if(cur_region->start == address) {
			if(cur_region->vmRegion) {
				cur_region->vmRegion->m_space.reset();
				m_page_directory.unmap(*cur_region->vmRegion);
				m_lock.release();
				auto free_res = free_region(cur_region);
				ASSERT(!free_res.is_error());
				return free_res;
			}
			m_lock.release();
			return Result(ENOENT);
		}
		cur_region = cur_region->next;
	}
	m_lock.release();
	return Result(ENOENT);
}

ResultRet<kstd::Arc<VMRegion>> VMSpace::get_region_at(VirtualAddress address) {
	LOCK(m_lock);
	VMSpaceRegion* cur_region = m_region_map;
	while(cur_region) {
		if(cur_region->start == address) {
			if(cur_region->vmRegion)
				return cur_region->vmRegion->self();
			return Result(ENOENT);
		}
		cur_region = cur_region->next;
	}
	return Result(ENOENT);
}

ResultRet<kstd::Arc<VMRegion>> VMSpace::get_region_containing(VirtualAddress address) {
	LOCK(m_lock);
	VMSpaceRegion* cur_region = m_region_map;
	while(cur_region) {
		if(cur_region->contains(address)) {
			if(cur_region->vmRegion)
				return cur_region->vmRegion->self();
			return Result(ENOENT);
		}
		cur_region = cur_region->next;
	}
	return Result(ENOENT);
}

Result VMSpace::reserve_region(VirtualAddress start, size_t size) {
	LOCK(m_lock);
	return alloc_space_at(size, start).result();
}

Result VMSpace::try_pagefault(VirtualAddress error_pos) {
	LOCK(m_lock);
	auto cur_region = m_region_map;
	while(cur_region) {
		if(cur_region->contains(error_pos)) {
			auto vmRegion = cur_region->vmRegion;
			if(!vmRegion)
				return Result(EINVAL);
			// Check if the region is CoW.
			if(vmRegion->is_cow() && vmRegion->object()->is_anonymous()) {
				// TODO: Check if we're mapped first? We should be.
				auto new_object = TRY(AnonymousVMObject::alloc(vmRegion->object()->size()));
				auto mapped_object = MM.map_object(new_object);
				memcpy((void*) mapped_object->start(), (void*) vmRegion->start(), new_object->size());
				vmRegion->m_object = new_object;
				vmRegion->set_cow(false);
				m_page_directory.map(*vmRegion);
				return Result(SUCCESS);
			}
			return Result(EINVAL);
		}
		cur_region = cur_region->next;
	}

	return Result(ENOENT);
}

ResultRet<VirtualAddress> VMSpace::find_free_space(size_t size) {
	LOCK(m_lock);
	auto cur_region = m_region_map;
	while(cur_region) {
		if(!cur_region->used && cur_region->size >= size)
			return cur_region->start;
		cur_region = cur_region->next;
	}
	return Result(ENOMEM);
}

size_t VMSpace::calculate_regular_anonymous_total() {
	LOCK(m_lock);
	size_t total = 0;
	auto cur_region = m_region_map;
	while(cur_region) {
		if(cur_region->used) {
			auto& object = cur_region->vmRegion->m_object;
			if(object->is_anonymous()) {
				auto anon_object = kstd::static_pointer_cast<AnonymousVMObject>(object);
				if(!anon_object->is_shared())
					total += anon_object->size();
			}
		}
		cur_region = cur_region->next;
	}
	return total;
}

ResultRet<VMSpace::VMSpaceRegion*> VMSpace::alloc_space(size_t size) {
	ASSERT(size % PAGE_SIZE == 0);

	/**
	 * We allocate a new region if we need one BEFORE iterating through the regions, because there's a chance we'll
	 * need to allocate more pages for the heap and if we're in the middle of iterating through regions when that
	 * happens, it could get ugly.
	 */
	auto new_region = new VMSpaceRegion;

	{
		LOCK(m_lock);
		auto cur_region = m_region_map;
		while(cur_region) {
			if(cur_region->used || cur_region->size < size) {
				cur_region = cur_region->next;
				continue;
			}

			if(cur_region->size == size) {
				cur_region->used = true;
				m_used += cur_region->size;
				delete new_region;
				return cur_region;
			}

			*new_region = VMSpaceRegion {
					.start = cur_region->start,
					.size = size,
					.used = true,
					.next = cur_region,
					.prev = cur_region->prev
			};

			if(cur_region->prev)
				cur_region->prev->next = new_region;

			cur_region->start += size;
			cur_region->size -= size;
			cur_region->prev = new_region;
			m_used += new_region->size;

			if(m_region_map == cur_region)
				m_region_map = new_region;
			return new_region;
		}
	}

	delete new_region;
	return Result(ENOMEM);
}

ResultRet<VMSpace::VMSpaceRegion*> VMSpace::alloc_space_at(size_t size, VirtualAddress address) {
	ASSERT(address % PAGE_SIZE == 0);
	ASSERT(size % PAGE_SIZE == 0);

	/**
	 * We allocate new regions if we need one BEFORE iterating through the regions, because there's a chance we'll
	 * need to allocate more pages for the heap and if we're in the middle of iterating through regions when that
	 * happens, it could get ugly.
	 */
	auto new_region_before = new VMSpaceRegion;
	auto new_region_after = new VMSpaceRegion;

	{
		LOCK(m_lock);
		auto cur_region = m_region_map;
		while(cur_region) {
			if(cur_region->contains(address)) {
				if(cur_region->used) {
					delete new_region_before;
					delete new_region_after;
					return Result(ENOMEM);
				}

				if(cur_region->size == size) {
					cur_region->used = true;
					m_used += cur_region->size;
					delete new_region_before;
					delete new_region_after;
					return cur_region;
				}

				if(cur_region->size - (address - cur_region->start) >= size) {
					// Create new region before if needed
					if(cur_region->start < address) {
						*new_region_before = VMSpaceRegion {
								.start = cur_region->start,
								.size = address - cur_region->start,
								.used = false,
								.next = cur_region,
								.prev = cur_region->prev
						};
						if(cur_region->prev)
							cur_region->prev->next = new_region_before;
						cur_region->prev = new_region_before;
						if(m_region_map == cur_region)
							m_region_map = new_region_before;
					} else {
						delete new_region_before;
					}

					// Create new region after if needed
					if(cur_region->end() > address + size) {
						*new_region_after = VMSpaceRegion {
								.start = address + size,
								.size = cur_region->end() - (address + size),
								.used = false,
								.next = cur_region->next,
								.prev = cur_region
						};
						if(cur_region->next)
							cur_region->next->prev = new_region_after;
						cur_region->next = new_region_after;
					} else {
						delete new_region_after;
					}

					cur_region->start = address;
					cur_region->size = size;
					cur_region->used = true;
					m_used += cur_region->size;
					return cur_region;
				}

				return Result(ENOMEM);
			}

			cur_region = cur_region->next;
		}
	}

	return Result(ENOMEM);
}

Result VMSpace::free_region(VMSpaceRegion* region) {
	VMSpaceRegion* to_delete[2] = {nullptr, nullptr};
	{
		LOCK(m_lock);
		region->used = false;
		region->vmRegion = nullptr;
		m_used -= region->size;

		// Merge previous region if needed
		if(region->prev && !region->prev->used) {
			to_delete[0] = region->prev;
			region->prev = region->prev->prev;
			if(to_delete[0]->prev)
				to_delete[0]->prev->next = region;
			region->start -= to_delete[0]->size;
			region->size += to_delete[0]->size;
			if(m_region_map == to_delete[0])
				m_region_map = region;
		}

		// Merge next region if needed
		if(region->next && !region->next->used) {
			to_delete[1] = region->next;
			region->next = region->next->next;
			if(to_delete[1]->next)
				to_delete[1]->next->prev = region;
			region->size += to_delete[1]->size;
		}
	}

	// We do this while not holding the lock just in case this triggers a page free in the allocator.
	delete to_delete[0];
	delete to_delete[1];

	return Result(SUCCESS);
}
