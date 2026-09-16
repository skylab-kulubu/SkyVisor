#include "ept.h"
#include "hypervisor.h"

UINT64 initialize_eptp()
{
    ept_pointer* eptp = (ept_pointer*)ExAllocatePool2(POOL_FLAG_NON_PAGED, PAGE_SIZE, DRIVER_POOLTAG);
    if (!eptp)
    {
        DbgPrint(DRIVER_DBG "Failed to allocate EPT pointer.\n");
        return 0;
    }
    
    ept_pml4e* ept_pml4 = (ept_pml4e*)ExAllocatePool2(POOL_FLAG_NON_PAGED, PAGE_SIZE, DRIVER_POOLTAG);
    if (!ept_pml4)
    {
        ExFreePoolWithTag(eptp, DRIVER_POOLTAG);
        DbgPrint(DRIVER_DBG "Failed to allocate EPT PML4.\n");
        return 0;
    }

    ept_pdpte* ept_pdpt = (ept_pdpte*)ExAllocatePool2(POOL_FLAG_NON_PAGED, PAGE_SIZE, DRIVER_POOLTAG);
    if (!ept_pdpt)
    {
        ExFreePoolWithTag(eptp, DRIVER_POOLTAG);
        ExFreePoolWithTag(ept_pml4, DRIVER_POOLTAG);
        DbgPrint(DRIVER_DBG "Failed to allocate EPT PDPT.\n");
        return 0;
    }

    ept_pde* ept_pd = (ept_pde*)ExAllocatePool2(POOL_FLAG_NON_PAGED, PAGE_SIZE, DRIVER_POOLTAG);
    if (!ept_pd)
    {
        ExFreePoolWithTag(eptp, DRIVER_POOLTAG);
        ExFreePoolWithTag(ept_pml4, DRIVER_POOLTAG);
        ExFreePoolWithTag(ept_pdpt, DRIVER_POOLTAG);
        DbgPrint(DRIVER_DBG "Failed to allocate EPT PD.\n");
        return 0;
    }

    ept_pte* ept_pt = (ept_pte*)ExAllocatePool2(POOL_FLAG_NON_PAGED, PAGE_SIZE, DRIVER_POOLTAG);
    if (!ept_pt)
    {
        ExFreePoolWithTag(eptp, DRIVER_POOLTAG);
        ExFreePoolWithTag(ept_pml4, DRIVER_POOLTAG);
        ExFreePoolWithTag(ept_pdpt, DRIVER_POOLTAG);
        ExFreePoolWithTag(ept_pd, DRIVER_POOLTAG);
        DbgPrint(DRIVER_DBG "Failed to allocate EPT PT.\n");
        return 0;
    }

    int pages_to_allocate = 10;
    UINT64 guest_memory = (UINT64)ExAllocatePool2(POOL_FLAG_NON_PAGED, PAGE_SIZE * pages_to_allocate, DRIVER_POOLTAG);
    if (!guest_memory)
    {
        ExFreePoolWithTag(eptp, DRIVER_POOLTAG);
        ExFreePoolWithTag(ept_pml4, DRIVER_POOLTAG);
        ExFreePoolWithTag(ept_pdpt, DRIVER_POOLTAG);
        ExFreePoolWithTag(ept_pd, DRIVER_POOLTAG);
        ExFreePoolWithTag(ept_pt, DRIVER_POOLTAG);
        DbgPrint(DRIVER_DBG "Failed to allocate guest pages.\n");
        return 0;
    }

    for (size_t i = 0; i < pages_to_allocate; ++i)
    {
        ept_pt[i].accessed = 0;
        ept_pt[i].dirty = 0;
        ept_pt[i].memory_type = 6; // cacheable
        ept_pt[i].execute_access = 1;
        ept_pt[i].user_mode_execute = 0;
        ept_pt[i].ignore_pat = 0;
        ept_pt[i].page_frame_number = MmGetPhysicalAddress((void*)(guest_memory + (i * PAGE_SIZE))).QuadPart >> 12;
        ept_pt[i].read_access = 1;
        ept_pt[i].suppress_ve = 0;
        ept_pt[i].write_access = 1;
    }

    ept_pd->accessed = 0;
    ept_pd->execute_access = 1;
    ept_pd->user_mode_execute = 0;
    ept_pd->page_frame_number = MmGetPhysicalAddress((void*)(ept_pt)).QuadPart >> 12;
    ept_pd->read_access = 1;
    ept_pd->write_access = 1;

    ept_pdpt->accessed = 0;
    ept_pdpt->execute_access = 1;
    ept_pdpt->user_mode_execute = 0;
    ept_pdpt->page_frame_number = MmGetPhysicalAddress((void*)(ept_pd)).QuadPart >> 12;
    ept_pdpt->read_access = 1;
    ept_pdpt->write_access = 1;

    ept_pml4->accessed = 0;
    ept_pml4->execute_access = 1;
    ept_pml4->user_mode_execute = 0;
    ept_pml4->page_frame_number = MmGetPhysicalAddress((void*)(ept_pdpt)).QuadPart >> 12;
    ept_pml4->read_access = 1;
    ept_pml4->write_access = 1;

    eptp->enable_access_and_dirty_flags = 1;
    eptp->memory_type = 6;
    eptp->page_walk_length = 3;
    eptp->page_frame_number = MmGetPhysicalAddress((void*)(ept_pml4)).QuadPart >> 12;


    UINT64 eptp_phys = MmGetPhysicalAddress(eptp).QuadPart;
    DbgPrint(DRIVER_DBG "Allocated EPTP at virtual address: %llx\n", eptp);
    DbgPrint(DRIVER_DBG "Allocated EPTP at physical address: %llx\n", eptp_phys);

    return eptp_phys;
}
