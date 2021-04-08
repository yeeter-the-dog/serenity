/*
 * Copyright (c) 2021, Liav A. <liavalb@hotmail.co.il>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <AK/HashMap.h>
#include <AK/NonnullOwnPtrVector.h>
#include <AK/OwnPtr.h>
#include <AK/Types.h>
#include <Kernel/ACPI/Definitions.h>
#include <Kernel/PCI/Access.h>
#include <Kernel/SpinLock.h>
#include <Kernel/VM/AnonymousVMObject.h>
#include <Kernel/VM/PhysicalRegion.h>
#include <Kernel/VM/Region.h>
#include <Kernel/VM/VMObject.h>

namespace Kernel {
namespace PCI {

#define PCI_MMIO_CONFIG_SPACE_SIZE 4096

class MMIOAccess : public Access {
public:
    class MMIOSegment {
    public:
        MMIOSegment(PhysicalAddress, u8, u8);
        u8 get_start_bus() const;
        u8 get_end_bus() const;
        size_t get_size() const;
        PhysicalAddress get_paddr() const;

    private:
        PhysicalAddress m_base_addr;
        u8 m_start_bus;
        u8 m_end_bus;
    };
    static void initialize(PhysicalAddress mcfg);

private:
    PhysicalAddress determine_memory_mapped_bus_region(u32 segment, u8 bus) const;
    void map_bus_region(u32, u8);
    VirtualAddress get_device_configuration_space(Address address);
    SpinLock<u8> m_access_lock;
    u8 m_mapped_bus { 0 };
    OwnPtr<Region> m_mapped_region;

protected:
    explicit MMIOAccess(PhysicalAddress mcfg);

    virtual const char* access_type() const override { return "MMIOAccess"; };
    virtual u32 segment_count() const override;
    virtual void enumerate_hardware(Function<void(Address, ID)>) override;
    virtual void write8_field(Address address, u32, u8) override;
    virtual void write16_field(Address address, u32, u16) override;
    virtual void write32_field(Address address, u32, u32) override;
    virtual u8 read8_field(Address address, u32) override;
    virtual u16 read16_field(Address address, u32) override;
    virtual u32 read32_field(Address address, u32) override;

    virtual u8 segment_start_bus(u32) const override;
    virtual u8 segment_end_bus(u32) const override;

    PhysicalAddress m_mcfg;
    HashMap<u16, MMIOSegment> m_segments;
};

}
}
