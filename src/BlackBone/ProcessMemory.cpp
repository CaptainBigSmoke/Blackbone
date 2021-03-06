#include "ProcessMemory.h"
#include "ProcessCore.h"

namespace blackbone
{

ProcessMemory::ProcessMemory( class ProcessCore& core )
    : _core( core )
{
}

ProcessMemory::~ProcessMemory()
{
}

/// <summary>
/// Allocate new memory block
/// </summary>
/// <param name="size">Block size</param>
/// <param name="protection">Memory protection</param>
/// <param name="desired">Desired base address of new block</param>
/// <returns>Memory block. If failed - returned block will be invalid</returns>
MemBlock ProcessMemory::Allocate( size_t size, DWORD protection /*= PAGE_EXECUTE_READWRITE*/, ptr_t desired /*= 0*/ )
{
    return MemBlock::Allocate( *this, size, desired, protection );
}

/// <summary>
/// Free memory
/// </summary>
/// <param name="pAddr">Memory address to release.</param>
/// <param name="size">Region size</param>
/// <returns>Status</returns>
NTSTATUS ProcessMemory::Free( ptr_t pAddr, size_t size /*= 0*/ )
{
    return _core.native()->VirualFreeExT( pAddr, size, MEM_RELEASE );
}

/// <summary>
/// Get memory region info
/// </summary>
/// <param name="pAddr">Memory address</param>
/// <param name="pInfo">Retrieved info</param>
/// <returns>Status</returns>
NTSTATUS ProcessMemory::Query( ptr_t pAddr, PMEMORY_BASIC_INFORMATION64 pInfo )
{
    return _core.native()->VirtualQueryExT( pAddr, pInfo );
}

/// <summary>
/// Change memory protection
/// </summary>
/// <param name="pAddr">Memory address</param>
/// <param name="size">Region size</param>
/// <param name="flProtect">New memory protection flags</param>
/// <param name="pOld">Old protection flags</param>
/// <returns>Status</returns>
NTSTATUS ProcessMemory::Protect( ptr_t pAddr, size_t size, DWORD flProtect, DWORD *pOld /*= NULL*/ )
{
    DWORD junk = 0;
    if (pOld == nullptr)
        pOld = &junk;

    return _core.native()->VirtualProtectExT( pAddr, size, CastProtection( flProtect, _core.DEP() ), pOld );
}

/// <summary>
/// Read data
/// </summary>
/// <param name="dwAddress">Memoey address to read from</param>
/// <param name="dwSize">Size of data to read</param>
/// <param name="pResult">Output buffer</param>
/// <param name="handleHoles">
/// If true, function will try to read all committed pages in range ignoring uncommitted ones.
/// Otherwise function will fail if there is at least one non-committed page in region.
/// </param>
/// <returns>Status</returns>
NTSTATUS ProcessMemory::Read( ptr_t dwAddress, size_t dwSize, PVOID pResult, bool handleHoles /*= false*/ )
{
    DWORD64 dwRead = 0;

    if (dwAddress == 0)
        return LastNtStatus( STATUS_INVALID_ADDRESS );

    LastNtStatus( STATUS_SUCCESS );

    // Simple read
    if (!handleHoles)
    {
        return _core.native()->ReadProcessMemoryT( dwAddress, pResult, dwSize, &dwRead );
    }
    // Read all committed memory regions
    else
    {
        MEMORY_BASIC_INFORMATION64 mbi = { 0 };

        for (ptr_t memptr = dwAddress; memptr < dwAddress + dwSize; memptr = mbi.BaseAddress + mbi.RegionSize)
        {
            if (_core.native()->VirtualQueryExT( memptr, &mbi ) != STATUS_SUCCESS)
                continue;

            // Filter empty regions
            if (mbi.State != MEM_COMMIT || mbi.Protect == PAGE_NOACCESS)
                continue;

            uint64_t region_ptr = memptr - dwAddress;

            if (_core.native()->ReadProcessMemoryT( mbi.BaseAddress,
                reinterpret_cast<uint8_t*>(pResult) + region_ptr,
                static_cast<size_t>(mbi.RegionSize),
                &dwRead ) != STATUS_SUCCESS)
            {
                return LastNtStatus();
            }
        }
    }

    return STATUS_SUCCESS;
}

/// <summary>
/// Write data
/// </summary>
/// <param name="pAddress">Memory address to write to</param>
/// <param name="dwSize">Size of data to write</param>
/// <param name="pData">Buffer to write</param>
/// <returns>Status</returns>
NTSTATUS ProcessMemory::Write( ptr_t pAddress, size_t dwSize, const void* pData )
{
    return _core.native()->WriteProcessMemoryT( pAddress, pData, dwSize );
}

}